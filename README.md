# Yet Another PYthon JIT
Still under early development. Check support for python syntax at `supp_tab.tsv`.

## Design considerations
1. It shall be suitable for use with all major python versions (3.8, 3.9, 3.10).
2. It shall support all valid python, and does not need to infer all static types as in `numba`.
3. JIT should be fast, unlike `numba` or oher LLVM-based JIT's. Thus, [MIR](https://github.com/vnmakarov/mir) is employed as the backend.
4. In most cases yapyjit shall be a plug-and-enable library to speed up python execution. It is a library so all CPython extensions are inherently compatible, unlike `PyPy`.

## Benchmarks
The benchmarks are from [python/pyperformance](https://github.com/python/pyperformance). More optimizations would be implemented in the compiler, and thus the results are subject to change.

Surprisingly, only with very basic transpiling from python code to machine code (that calls relevant CPython functions), as well as a bit of inline caching of dynamic calls in python, it already shows a significant amount of speed-up. `yapyjit` is even faster than PyPy on several tasks. The current goal is to make a working tracing JIT from here on. (`async`/`yield` functionality is postponed in the schedule by now.)

On Intel Core i7-4700MQ (benchmarks are listed in alphabetical order, time is in milliseconds):
| Benchmark | CPython38 | CPy38 + yapyjit | Speed-up (100% → x%) |
| :---: | :---: | :---: | :---: |
| float | 233 ± 5 | 114 ± 4 | 48.8% |
| mdp | 6225 ± 367 | 5749 ± 144 | 92.4% |
| nbody | 333 ± 24 | 127 ± 6 | 38.1% |
| scimark_fft | 758 ± 37 | 471 ± 30 | 62.0% |
| scimark_lu | 302 ± 22 | 285 ± 19 | 94.5% |
| scimark_monte_carlo | 210 ± 13 | 168 ± 11 | 80.0% |
| scimark_sor | 410 ± 25 | 287 ± 19 | 70.0% |
| scimark_sparse_mat_mult | 10 ± 1 | 7 ± 0 | 74.4% |
| spectral_norm | 333 ± 20 | 124 ± 7 | 37.3% |


## Installation
### Prebuilt wheels
Prebuilt wheels for x64 windows, linux and Mac OS can be downloaded from github actions artifacts.

### Development/Build from source
1. Ensure that you have correctly set up for building native CPython extensions.
2. Clone this repository, and run
```sh
python setup.py bdist_wheel
```
3. Install the wheel in the `dist` folder.

## Usage
Just import `yapyjit` and use `@yapyjit.jit` to decorate functions you'd like to JIT. The syntax also supports member functions, class methods and static methods. Notice that it is still in an early stage of development. If unsupported python syntax is found an error will be raised. Also expect other bugs in the compiler. You are welcome to open an issue if you find one.
