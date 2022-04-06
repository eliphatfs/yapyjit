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
|          float          |  262 ± 30  |    134 ± 12     |        51.1%         |
|         mdp (*)         | 5940 ± 689 |   5399 ± 749    |        90.9%         |
|          nbody          |  355 ± 61  |    243 ± 41     |        68.3%         |
|       scimark_fft       | 897 ± 152  |    507 ± 80     |        56.6%         |
|       scimark_lu        |  345 ± 51  |    293 ± 45     |        85.2%         |
|   scimark_monte_carlo   |  240 ± 29  |    189 ± 26     |        78.7%         |
|       scimark_sor       |  452 ± 62  |    361 ± 59     |        79.9%         |
| scimark_sparse_mat_mult |   11 ± 2   |      7 ± 2      |        65.6%         |
|      spectral_norm      |  385 ± 53  |    223 ± 29     |        58.0%         |



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
