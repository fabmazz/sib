#include "bp.h"
#include <vector>
#include <exception>

using namespace std;

void drop_time(FactorGraph & fg, int t)
{
        fg.drop_contacts(t);
        int n = fg.nodes.size();
        for (int i = 0; i < n; ++i) {
                Node & f = fg.nodes[i];
                if (t == f.times[1]) {
                        f.bt.erase(f.bt.begin());
                        f.bg.erase(f.bg.begin());
                        f.ht.erase(f.ht.begin());
                        f.hg.erase(f.hg.begin());
			f.times.erase(f.times.begin() + 1);
			int m = f.neighs.size();
			for (int j = 0; j < m; ++j) {
				Neigh & v = f.neighs[j];
				for (int k = 0; k < int(v.t.size()); ++k) {
					--v.t[k];
				}
			}
                }
		f.times[0] = t;
		for (auto it = f.obs.begin(); it != f.obs.end();) {
			if (t == get<0>(*it))
				f.obs.erase(it);
			else
				++it;
		}
        }
}


real_t get_h(real_t b, real_t bs)
{
	if (b == 0) {
		if (bs == 0)
			return 1.0;
		throw domain_error("singularity error");;
	}
	return bs / b;
}

std::tuple<int, real_t, real_t>
drop_sc(FactorGraph & fg, int t, int maxit_bp, real_t tol_bp, real_t damping_bp, int maxit_sc, real_t tol_sc, real_t damping_sc)
{
	int n = fg.nodes.size();
	vector<vector<real_t>> bts(n);
	vector<vector<real_t>> bgs(n);
	for (int i = 0; i < n; ++i) {
		bts[i] = fg.nodes[i].bt;
		bgs[i] = fg.nodes[i].bg;
	}
	fg.drop_contacts(t);
	real_t err_bp = 0.0;
	real_t err_sc = 0.0;
	int it;
	for (it = 0; it < maxit_sc; ++it) {
		err_bp = 0.0;
		for (int k = 0; k < maxit_bp; ++k) {
			err_bp = max(err_bp, fg.iteration(damping_bp));
			if (err_bp < tol_bp)
				break;
		}
		err_sc = 0.0;
#pragma omp parallel for reduction(max:err_sc)
		for (int i = 0; i < n; ++i) {
			Node & f = fg.nodes[i];
			for (int ti = 0; ti < int(f.bt.size()); ++ti) {
				err_sc = max(err_sc, bts[i][ti] - f.bt[ti]);
				err_sc = max(err_sc, bgs[i][ti] - f.bg[ti]);
				f.ht[ti] *= pow(get_h(f.bt[ti], bts[i][ti]), damping_sc);
				f.hg[ti] *= pow(get_h(f.bg[ti], bgs[i][ti]), damping_sc);
			}
		}
		if (max(err_bp, err_sc) < tol_sc)
			break;
	}
	return make_tuple(it, err_bp, err_sc);
}

