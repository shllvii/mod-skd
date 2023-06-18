# Stall-free Module Scheduling with Buffer Optimization

> The repo contains experimental implementation for the selected analysis methods of Cement HDL 

## Algorithm Impl

`mskd.[cc|h]` contains the implementatin according to algrithm description in the [report](private-yet).


## How to run

We've copied the externel prerequisite to the sub-directory  `externel/`, so that the following commands may be enough for the reproduction.
```sh
make test # compile binary, generate data, and run
make analyze # extract results from produced json files
```

If you met any trouble during the reproduction, please consider replace the "gurobi things" under `externel/` with those working well on your platform.
If the trouble cannot be solved, please contact the author (shallwexiao@gmail.com), who probably cannot help at all either.
