// This file is part of sibilla : inference in epidemics with Belief Propagation
// Author: Alfredo Braunstein
// Author: Alessandro Ingrosso
// Author: Anna Paola Muntoni



#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cassert>
#include <tuple>
#include <exception>
#include <boost/math/special_functions/gamma.hpp>
#include "bp.h"
#include "cavity.h"

using namespace std;


// real_t Node::prob_g(real_t dg) const { return exp(-mu * dg); }
real_t Node::prob_g(real_t dg) const { return real_t(1) - boost::math::gamma_p(k_,dg*mu_); }


FactorGraph::FactorGraph(Params const & params,
		vector<tuple<int,int,int,real_t> > const & contacts,
		vector<tuple<int, int, int> > const & obs,
		vector<tuple<int, real_t, real_t> > const & individuals) : params(params)
{
	Tinf = -1;
	for (auto it = contacts.begin(); it != contacts.end(); ++it) {
		int i,j,t;
		real_t lambda;
		tie(i,j,t,lambda) = *it;
		Tinf = max(Tinf, t + 1);
		add_contact(i, j, t, lambda);
	}

	for (auto it = obs.begin(); it != obs.end(); ++it) {
		int i,s,t;
		tie(i,s,t) = *it;
		Tinf = max(Tinf, t + 1);
		add_obs(i, s, t);
	}

	for (auto it = individuals.begin(); it != individuals.end(); ++it) {
		int i;
		real_t k, mu;
		tie(i,k,mu) = *it;
		int a = add_node(i);
		nodes[a].k_ = k;
		nodes[a].mu_ = mu;
	}

	for (int i = 0; i < int(nodes.size()); ++i) {
		vector<int> F = nodes[i].tobs;
		for (int k = 0; k < int(nodes[i].neighs.size()); ++k) {
			vector<int> const & tij = nodes[i].neighs[k].times;
			F.insert(F.end(), tij.begin(), tij.end());
		}
		sort(F.begin(), F.end());
		F.push_back(Tinf);
		nodes[i].times.push_back(-1);
		for (int k = 0; k < int(F.size()); ++k) {
			if (nodes[i].times.back() != F[k])
				nodes[i].times.push_back(F[k]);
		}
		int ntimes = nodes[i].times.size();
		nodes[i].bt.resize(ntimes);
		nodes[i].bg.resize(ntimes);
		nodes[i].ht.resize(ntimes);
		nodes[i].hg.resize(ntimes);
		set_field(i);
		for (int k = 0; k  < int(nodes[i].neighs.size()); ++k) {
			omp_init_lock(&nodes[i].neighs[k].lock_);
			nodes[i].neighs[k].times.push_back(Tinf);
			nodes[i].neighs[k].lambdas.push_back(0.0);
			int nij = nodes[i].neighs[k].times.size();
			nodes[i].neighs[k].msg.resize(nij*nij);
		}
	}
	init();
	//showgraph();
}

int FactorGraph::find_neighbor(int i, int j) const
{
	int k = 0;
	for (; k < int(nodes[i].neighs.size()); ++k)
		if (j == nodes[i].neighs[k].index)
			break;
	return k;
}

int FactorGraph::add_node(int i)
{
	map<int,int>::iterator mit = index.find(i);
	if (mit != index.end())
		return mit->second;
	index[i] = nodes.size();
	nodes.push_back(Node(i, params.k, params.mu));
	return index[i];
}

void FactorGraph::add_obs(int i, int state, int t)
{
	Node & f = nodes[add_node(i)];
	if (f.tobs.empty() || t >= f.tobs.back()) {
		f.tobs.push_back(t);
		f.obs.push_back(state);
	} else {
		throw invalid_argument("time of observations should be ordered");
	}
}

void FactorGraph::add_contact(int i, int j, int t, real_t lambda)
{
	i = add_node(i);
	j = add_node(j);
	int ki = find_neighbor(i, j);
	int kj = find_neighbor(j, i);
	if (ki == int(nodes[i].neighs.size())) {
		assert(kj == int(nodes[j].neighs.size()));
		nodes[i].neighs.push_back(Neigh(j, kj));
		nodes[j].neighs.push_back(Neigh(i, ki));
	}
	Neigh & ni = nodes[i].neighs[ki];
	Neigh & nj = nodes[j].neighs[kj];
	if (ni.times.empty() || t > ni.times.back()) {
		ni.times.push_back(t);
		ni.lambdas.push_back(lambda);
		nj.times.push_back(t);
		nj.lambdas.push_back(0.0);
	} else if (t == ni.times.back()) {
		ni.lambdas.back() = lambda;
	} else {
		throw invalid_argument("time of contacts should be ordered");
	}
}

void FactorGraph::set_field(int i)
{
	// this assumes ordered observation times
	int it = 0;
	int tl = 0, gl = 0;
	int tu = nodes[i].times.size();
	int gu = nodes[i].times.size();
//	cout << nodes[i].index << " ";
//	for (int t = 0; t < int(nodes[i].tobs.size()); ++t) {
//		cout << nodes[i].tobs[t] << " ";
//		cout << nodes[i].obs[t] << " " << endl;
//	}
//	cout << endl;
	for (int k = 0; k < int(nodes[i].tobs.size()); ++k) {
		int state = nodes[i].obs[k];
		int tobs = nodes[i].tobs[k];
		while (nodes[i].times[it] != tobs)
			it++;
		switch (state) {
			case 0:
				tl = max(tl, it);
				gl = max(gl, it);
				break;
			case 1:
				tu = min(tu, it - 1);
				gl = max(gl, it);
				break;
			case 2:
				tu = min(tu, it - 1);
				gu = min(gu, it - 1);
				break;
			case -1:
				break;
		}
		// if (state != -1) {
		//	 cerr << "node " << nodes[i].index << " state obs " << state << " time " << tobs << " ti in [" << nodes[i].times[tl] << "," << nodes[i].times[tu] << "]" << endl;
		//	 cerr << "node " << nodes[i].index << " state obs " << state << " time " << tobs << " gi in [" << nodes[i].times[gl] << "," << nodes[i].times[gu] << "]" << endl;
		//}
	}

	// cout  << "I i: " << nodes[i].index << " " << "( " << nodes[i].times[tl] << ", " << nodes[i].times[tu] << ")" << endl;
	// cout  << "R i: " << nodes[i].index << " " << "( " << nodes[i].times[gl] << ", " << nodes[i].times[gu] << ")" << endl;
	for(int t = 0; t < int(nodes[i].ht.size()); ++t) {
		nodes[i].ht[t] = (tl <= t && t <= tu);
		nodes[i].hg[t] = (gl <= t && t <= gu);
	}
	nodes[i].ht[0] *= params.pseed;
}

void FactorGraph::show_graph()
{
	cerr << "Number of nodes " <<  int(nodes.size()) << endl;
	for(int i = 0; i < int(nodes.size()); i++) {
		cerr << "### index " << nodes[i].index << "###" << endl;
		cerr << "### in contact with " <<  int(nodes[i].neighs.size()) << "nodes" << endl;
		vector<Neigh> const & aux = nodes[i].neighs;
		for (int t = 0; t < int(nodes[i].tobs.size()); ++t)
			cerr << "### observed at time " << nodes[i].tobs[t] << endl;
		for (int j = 0; j < int(aux.size()); j++) {
			cerr << "# neighbor " << nodes[aux[j].index].index << endl;
			cerr << "# in position " << aux[j].pos << endl;
			cerr << "# in contact " << int(aux[j].times.size()) << " times, in t: ";
			for (int t = 0; t < int(aux[j].times.size()); t++)
				cerr << aux[j].times[t] << " ";
			cerr << " " << endl;
		}
	}
}

void FactorGraph::show_beliefs(ostream & ofs)
{
	for(int i = 0; i < int(nodes.size()); ++i) {
		Node & f = nodes[i];
		ofs << "node " << f.index << ":" << endl;
		for (int t = 0; t < int(f.bt.size()); ++t) {
			ofs << "    " << f.times[t] << " " << f.bt[t] << " (" << f.ht[t] << ") " << f.bg[t] << " (" << f.hg[t] << ")" << endl;
		}
	}

}

void FactorGraph::show_msg(ostream & msgfile)
{
	for(int i = 0; i < int(nodes.size()); ++i) {
		for(int j = 0; j < int(nodes[i].neighs.size()); ++j) {
			for (int n = 0; n < int(nodes[i].neighs[j].msg.size()); ++n) {
				vector<real_t> & msg = nodes[i].neighs[j].msg;
				msgfile << msg[n] << " ";
			}
			msgfile << " " << endl;
		}
	}
}

void norm_msg(vector<real_t> & msg)
{
	real_t S = 0;
	for(int n = 0; n < int(msg.size()); ++n)
		S += msg[n];
	assert(S > 0);
	for(int n = 0; n < int(msg.size()); ++n)
		msg[n] /= S;
}

real_t setmes(vector<real_t> & from, vector<real_t> & to, real_t damp)
{
	int n = from.size();
	real_t s = 0;
	for (int i = 0; i < n; ++i) {
		s += from[i];
	}
	real_t err = 0;
	for (int i = 0; i < n; ++i) {
		from[i] /= s;
		err = max(err, abs(from[i] - to[i]));
		to[i] = damp*to[i] + (1-damp)*from[i];
	}
	return err;
}

int Sij(Node const & f, Neigh const & v, int sij, int gi)
{
	// here gi stands for the ti + gi index
	return v.times[sij] <= f.times[gi] ? sij : v.times.size() - 1;
}

inline int idx(int sij, int sji, int qj)
{
	return sji + qj * sij;
}


ostream & operator<<(ostream & o, vector<real_t> const & m)
{
	o << "{";
	for (int i=0; i<int(m.size()); ++i){
		o << m[i] << " ";
	}
	o << "}";
	return o;
}

void FactorGraph::init()
{
	for(int i = 0; i < int(nodes.size()); ++i) {
		for(int j = 0; j < int(nodes[i].neighs.size()); ++j) {
			vector<real_t> & msg = nodes[i].neighs[j].msg;
			for(int ss = 0; ss < int(msg.size()); ++ss) 
				msg[ss] = 1;
		}
	}
}

real_t FactorGraph::update(int i)
{
	Node & fac_node = nodes[i];
	int const num_neighs = fac_node.neighs.size();
	vector<vector<real_t> > UU(num_neighs);
	vector<vector<real_t> > HH(num_neighs);
	int const qi_ = fac_node.bt.size();

	vector<real_t> ut(qi_);
	vector<real_t> ug(qi_);

	for (int j = 0; j < num_neighs; ++j) {
		Neigh & v = nodes[fac_node.neighs[j].index]
						.neighs[fac_node.neighs[j].pos];
		omp_set_lock(&v.lock_);
		HH[j] = v.msg;
		omp_unset_lock(&v.lock_);
		UU[j].resize(v.msg.size());
	}
	// proba tji >= ti for each j
	vector<real_t> C0(num_neighs);
	// proba tji > ti for each j
	vector<real_t> C1(num_neighs);

	Cavity<real_t> P0(C0, 1., multiplies<real_t>()); //multiplies is C++14 std syntax, operator *
	Cavity<real_t> P1(C1, 1., multiplies<real_t>());
	vector<int> min_in(num_neighs), min_out(num_neighs);
	real_t za = 0.0;
	for (int ti = 0; ti < qi_; ++ti) {
		for (int j = 0; j < num_neighs; ++j) {
			Neigh const & v = fac_node.neighs[j];
			int const qj = v.times.size();
			min_in[j] = qj - 1;
			min_out[j] = qj - 1;
			for (int s = qj - 1; s >= 0 && v.times[s] >= fac_node.times[ti]; --s) {
				// smallest tji >= ti
				min_in[j] = s;
				if (v.times[s] > fac_node.times[ti]) {
					// smallest tji > ti
					min_out[j] = s;
				}
			}
		}

		for (int gi = ti; gi < qi_; ++gi) {
			fill(C0.begin(), C0.end(), 0.0);
			fill(C1.begin(), C1.end(), 0.0);

			for (int j = 0; j < num_neighs; ++j) {
				Neigh const & v = fac_node.neighs[j];
				vector<real_t> const & h = HH[j];
				int const qj = v.times.size();
				for (int sji = min_in[j]; sji < qj; ++sji) {
					real_t pi = 1;
					for (int s = min_out[j]; s < qj - 1; ++s) {
						int const sij = Sij(fac_node, v, s, gi);
						real_t const p = pi * v.lambdas[s] * h[idx(sji, sij, qj)];
						C0[j] += p;
						if (v.times[sji] > fac_node.times[ti])
							C1[j] += p;
						pi *= 1 - v.lambdas[s];
					}
					int const sij = Sij(fac_node, v, qj - 1, gi);
					real_t const p = pi * h[idx(sji, sij, qj)];
					C0[j] += p;
					if (v.times[sji] > fac_node.times[ti])
						C1[j] += p;
				}
			}

			P0.initialize(C0.begin(), C0.end(), 1.0, multiplies<real_t>());
			P1.initialize(C1.begin(), C1.end(), 1.0, multiplies<real_t>());

			//messages to ti, gi
			real_t const g_prob = fac_node.prob_g(fac_node.times[gi] - fac_node.times[ti]) - (gi + 1 == qi_ ? 0.0 : fac_node.prob_g(fac_node.times[gi + 1] - fac_node.times[ti]));
			real_t const a = g_prob  * (ti == 0 || ti == qi_ - 1 ? P0.full() : P0.full() - P1.full());

			ug[gi] += fac_node.ht[ti] * a;
			ut[ti] += fac_node.hg[gi] * a;
			za += fac_node.ht[ti] * fac_node.hg[gi] * a;

			//messages to sij, sji
			for (int j = 0; j < num_neighs; ++j) {
				Neigh const & v = fac_node.neighs[j];
				int const qj = v.times.size();
				real_t const p0 = P0[j];
				real_t const p01 = p0 - P1[j];
				for (int sji = min_in[j]; sji < qj; ++sji) {
					real_t pi = fac_node.ht[ti] * fac_node.hg[gi] * g_prob * (ti == 0 || v.times[sji] == fac_node.times[ti] ? p0 : p01);
					for (int s = min_out[j]; s < qj - 1; ++s) {
						real_t & Uij = UU[j][idx(Sij(fac_node, v, s, gi), sji, qj)];
						Uij += pi * v.lambdas[s];
						pi *= 1 - v.lambdas[s];
					}
					UU[j][idx(Sij(fac_node, v, qj - 1, gi), sji, qj)] += pi;
				}
			}
		}
	}
	fac_node.f_ = -log(za);
	//apply external fields on t,h
	for (int t = 0; t < qi_; ++t) {
		ut[t] *= fac_node.ht[t];
		ug[t] *= fac_node.hg[t];
	}
	//compute marginals on t,g
	real_t diff = max(setmes(ut, fac_node.bt, params.damping), 
						setmes(ug, fac_node.bg, params.damping));
	for (int j = 0; j < num_neighs; ++j) {
		Neigh & v = fac_node.neighs[j];
		omp_set_lock(&v.lock_);
		// diff = max(diff, setmes(UU[j], v.msg, params.damping));
		setmes(UU[j], v.msg, params.damping);
		omp_unset_lock(&v.lock_);

		real_t zj = 0; // z_{(sij,sji)}}
		int const qj = v.times.size();
		for (int sij = 0; sij < qj; ++sij) {
			for (int sji = 0; sji < qj; ++sji) {
				zj += HH[j][idx(sij, sji, qj)]*v.msg[idx(sji, sij, qj)];
			}
		}
		fac_node.f_ += 0.5*log(zj); // half is cancelled by z_{a,(sij,sji)}
	}

	return diff;

}

real_t FactorGraph::iteration()
{
	int const N = nodes.size();
	real_t err = 0.0;
	vector<int> perm(N);
	for(int i = 0; i < N; ++i)
		perm[i] = i;
	random_shuffle(perm.begin(), perm.end());
#pragma omp parallel for reduction(max:err)
	for(int i = 0; i < N; ++i)
		err = max(err, update(perm[i]));
	return err;
}

real_t FactorGraph::loglikelihood() const
{
	real_t L = 0;
	for(auto nit = nodes.begin(), nend = nodes.end(); nit != nend; ++nit)
		L -= nit->f_;
	return L;
}

real_t FactorGraph::iterate(int maxit, real_t tol)
{
	real_t err = std::numeric_limits<real_t>::infinity();
	for (int it = 1; it <= maxit; ++it) {
		err = iteration();
		cout << "it: " << it << " err: " << err << endl;
		if (err < tol)
			break;
	}
	return err;
}
