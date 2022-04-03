# Yet Another PYthon JIT
Still under development. Check support for different python grammars at `supp_tab.tsv`.

## Design considerations
1. It shall be suitable for use with all major python versions (3.8, 3.9, 3.10).
2. It shall support all valid python, and does not need to infer all static types as in `numba`.
3. JIT should be fast, unlike `numba` or oher LLVM-based JIT's. In most cases yapyjit shall be a plug-and-enable library to speed up python execution.

## Benchmarks
The benchmarks are from [python/pyperformance](https://github.com/python/pyperformance). More optimizations would be implemented in the compiler, and thus the results are subject to change.

Interestingly, only with very basic transpiling from python code to machine code (that calls various CPython functions), as well as a bit of inline caching of dynamic calls in python, it already shows a significant amount of speed-up.

On Intel Core i7-4700MQ (benchmarks are listed in alphabetical order, time is in milliseconds):
| Benchmark | CPython38 | CPy38 + yapyjit | Speed-up (100% → x%) |
| :---: | :---: | :---: | :---: |
| float | 254 ± 16 | 181 ± 13 | 71.26% |
| nbody | 371 ± 69 | 243 ± 39 | 65.50% |
| spectral_norm | 376 ± 61 | 196 ± 24 | 52.12% |
