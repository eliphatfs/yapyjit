import subprocess
import os
import sys


fast = ['bm_mdp.py']


if __name__ == "__main__":
    for root, ds, fs in os.walk(os.path.dirname(__file__)):
        for f in sorted(fs):
            if f.endswith(".py") and f.startswith("bm"):
                fpath = os.path.join(root, f)
                extra_flags = [] if f not in fast else ['--fast']
                os.environ["YAPYJIT_EN"] = "ENABLE"
                os.environ["YAPYJIT_FLAGS"] = "NOWARN"
                subprocess.run([sys.executable, fpath, "--copy-env"] + extra_flags)
                os.environ["YAPYJIT_EN"] = "DISABLE"
                subprocess.run([sys.executable, fpath, "--copy-env"] + extra_flags)
