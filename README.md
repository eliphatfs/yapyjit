# Yet Another PYthon JIT
Still under early development. Check support for python syntax at `supp_tab.tsv`.

## Design considerations
1. It shall be suitable for use with all major python versions (3.8, 3.9, 3.10).
2. It shall support all valid python, and does not need to infer all static types as in `numba`.
3. JIT should be fast, unlike `numba` or oher LLVM-based JIT's. Thus, [MIR](https://github.com/vnmakarov/mir) is employed as the backend.
4. In most cases yapyjit shall be a plug-and-enable library to speed up python execution. It is a library so all CPython extensions are inherently compatible, unlike `PyPy`.

## Benchmarks
The benchmarks are from [python/pyperformance](https://github.com/python/pyperformance). More optimizations would be implemented in the compiler, and thus the results are subject to change.

Surprisingly, only with very basic transpiling from python code to machine code (that calls relevant CPython functions), as well as a bit of inline caching of dynamic calls in python, it already shows a significant amount of speed-up. `yapyjit` is even faster than PyPy on several tasks.

On Intel Core i7-4700MQ (benchmarks are listed in alphabetical order, time is in milliseconds):
|        Benchmark        | CPython38  | CPy38 + yapyjit | Speed-up (100% → x%) |
| :---------------------: | :--------: | :-------------: | :------------------: |
|          float          |  259 ± 24  |    185 ± 27     |        71.6%         |
|         mdp (*)         | 5727 ± 304 |   5553 ± 406    |        97.0%         |
|          nbody          |  374 ± 67  |    243 ± 25     |        64.8%         |
|       scimark_fft       |  823 ± 78  |    520 ± 81     |        63.2%         |
|       scimark_lu        |  327 ± 23  |    322 ± 69     |        98.6%         |
|   scimark_monte_carlo   |  231 ± 21  |    188 ± 20     |        81.0%         |
|       scimark_sor       |  450 ± 54  |    406 ± 44     |        90.3%         |
| scimark_sparse_mat_mult |   11 ± 2   |      6 ± 1      |        56.9%         |
|      spectral_norm      |  350 ± 66  |    218 ± 36     |        62.3%         |


(*): yapyjit cannot fully compile yet, various parts are still interpreted.

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
