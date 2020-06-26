<p align="center">
  <img src="examples/figs/sibilla_logo.jpg" width="225" height="265">
</p>

![C/C++ CI](https://github.com/sibyl-team/sib/workflows/C/C++%20CI/badge.svg)

# Sibilla 

> SIB: [S]tatistical [I]nference in Epidemics via [B]elief Propagation

Message passing algorithm for individual-level inference in epidemics. SIB assumes that the epidemic process is generated by a compartmental model (currently a generalized semi-continuous time SIR model is implemented). Among other things, SIB computes the probability of each individual to be susceptible, infected or recovered at a given time from a list of contacts and partial observations. The method is based on Belief Propagation equations for partial trajectories.
See [notes](https://github.com/sibyl-team/sib/blob/master/notes/bpnotes.pdf) and [article](https://www.nature.com/articles/srep27538) for more details.

## Requirements

  - A C++11 compiler
  - python3
  - [pybind11](https://github.com/pybind/pybind11)
  - header only boost libraries

## Install
you can choose one of the following procedures:

* make

    1. Adjust `Makefile`, if needed.
    2. type: `make`
    
* pip install

    1. Adjust the setup.py, if needed.
    2. `pip install .`

You'll obtain a standalone CLI ./sib, plus a dynamic library containing a python module `sib`.

When not installing with `pip`, make sure to include the `sib` folder into the python path.

## Quick start

 Have a look at this [notebook](./examples/dummy_test.ipynb).

 ## Reference
 This code implements and expands the method described in this [paper](https://www.nature.com/articles/srep27538).


## Documentation

### python module

#### Classes

```python
sib.Params(
  prob_i = Uniform(1.0), 
  prob_r = Exponential(0.1), 
  pseed = 0.01, 
  psus = 0.5, 
  softconstraint = 0):
    
    Settings the parameters of the model used for inference.

    Parameters
    ----------
    prob_i : Function - optional
        function to compute the probability of infection, 
        depending on the difference (t-ti), ti time of infection.
        [default: Uniform]
    prob_r : Function - optional
        function to compute the probability of recovery, 
        depending on the difference (t-ti), ti time of infection.
        [default: exponential decay with mu = 0.1]
    pseed : Float - optional
        Prior to be source of infections (infected at starting times).
        [default 1e-2]
    psus: Float - optional
        Prior to be susceptible.
        [default: 0.5]
    softconstraint: Float
        Soft the observations constraints.

  
    Returns
    -------
    None
```

```python
sib.FactorGraph(
  Params,
  contacts,
  observations,
  individuals = 0
)
    Construct the factor graph from the list of contacts,
    the lists of observations, the Parameters of the model.
    Eventually is possible to set infectiousness and 
    recoverability priors on each individual.

    Parameters
    ----------
    Params: Class sib.Params -- optional
        see sib.Params
    contacts: list<tuple(int, int, int, float)>
        list of single direct contacts (for bidirectional contacts add also
        the inverse contact). Order of tuple contact: (node_i, node_j, time, lambda)
        The lambdas are the instantaneous probability of infection.
    observations: list<tuple(int, int, int)>
        list of single observation on single node a time t. Order of observation tuple:
        (node_i, state, time)
        Where state could be 0, 1 or 2 representing susceptible, infected, recovery.
    individuals: list(tuple(int, prior_t, prior_r)) [optional]
        List of tuple where each tuple are the prior on the infectiousness and recoverability of node "i".

    return
    ------
      Class FactorGraph

```

```python
sib.iterate(
  f,
  maxit=100,
  tol=1e-3, 
  damping=0.0,
  callback=(lambda t, err, f: print(t, err, flush=True))
)
Iterate the BP messages.
    Parameters
    ----------
    f: FactorGraph class
      The factor graph. See sib.FactorGraph
    maxit: int -- optional
      max number of iteration of update of BP equation
    tol: float -- optional
      If the error of the update of BP equation is less than `tol`the iteration stop.
    damping: float -- optional
      Damping parameter to help the convergence of BP equation.
    callback: function(iter_n, err, f)
      Function callback, called after each iteration. Take as argument, number of current iteration (iter_n), error (err), and the FactorGraph (f). 
```

## Contributions
If you want to participate write us ([sibyl-team](mailto:sibylteam@gmail.com?subject=[GitHub]%20Source%20sibilla)) or make a pull request.

## License
[Apache License 2.0](LICENSE)

## Maintainers
[The sibyl-team](https://github.com/sibyl-team):

[Alfredo Braunstein](alfredo.braunstein@polito.it), [Alessandro Ingrosso](alessingrosso@gmail.com) ([@ai_ingrosso](https://twitter.com/ai_ngrosso)), [Anna Paola Muntoni](), [Luca Dall'Asta](luca.dallasta@polito.it), [Fabio Mazza](), [Giovanni Catania](), [Indaco Biazzo](indaco.biazzo@polito.it) ([@ocadni](https://twitter.com/ocadni))

<p float="left">
<img src="examples/figs/polito_log.png" width="186" height="66">
<img src="examples/figs/columbia_logo.jpg" width="100" height="100">
</p>

## Acknowledgements
This project has been partially funded by Fondazione CRT through call "La Ricerca dei Talenti", project SIBYL.

<img src="examples/figs/fcrt-logo.png" width="200" height="52">
