import os
import sys
import shutil
import pyperf
import subprocess
import collections


fast = ['bm_mdp.py']


if __name__ == "__main__":
    res_dir = os.path.join(os.path.dirname(__file__), "results")
    shutil.rmtree(res_dir, ignore_errors=True)
    os.makedirs(res_dir, exist_ok=True)
    res_dict = collections.defaultdict(list)
    for root, ds, fs in os.walk(os.path.dirname(__file__)):
        for f in sorted(fs):
            if f.endswith(".py") and f.startswith("bm"):
                fpath = os.path.join(root, f)
                extra_flags = [] if f not in fast else ['--fast']
                os.environ["YAPYJIT_EN"] = "ENABLE"
                os.environ["YAPYJIT_FLAGS"] = "NOWARN"
                common_args = [
                    sys.executable, fpath,
                    "--copy-env",
                    "-o", os.path.join(res_dir, f + ".jit.json"),
                ]
                subprocess.run(common_args + extra_flags)
                os.environ["YAPYJIT_EN"] = "DISABLE"
                common_args = [
                    sys.executable, fpath,
                    "--copy-env",
                    "-o", os.path.join(res_dir, f + ".nojit.json"),
                ]
                subprocess.run(common_args + extra_flags)
                jit = pyperf.BenchmarkSuite.load(os.path.join(res_dir, f + ".jit.json"))
                nojit = pyperf.BenchmarkSuite.load(os.path.join(res_dir, f + ".nojit.json"))
                for bm in jit:
                    name = '_'.join(bm.get_name().split("_")[:-1])
                    res_dict[name].append((bm.mean(), bm.stdev()))
                for bm in nojit:
                    name = '_'.join(bm.get_name().split("_")[:-1])
                    res_dict[name].append((bm.mean(), bm.stdev()))
    with open(os.path.join(res_dir, "summary.md"), "w", encoding='utf-8') as f:
        print(f"| Benchmark | CPython38 | CPy38 + yapyjit | Speed-up (100% → x%) |", file=f)
        print(f"| :---: | :---: | :---: | :---: |", file=f)
        for name, (jit, nojit) in sorted(res_dict.items()):
            print(f"| {name} | {nojit[0] * 1000:.0f} ± {nojit[1] * 1000:.0f} | {jit[0] * 1000:.0f} ± {jit[1] * 1000:.0f} | {jit[0] / nojit[0] * 100:.1f}% |", file=f)
