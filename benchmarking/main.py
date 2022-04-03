import subprocess
import os
import sys


if __name__ == "__main__":
    for root, ds, fs in os.walk(os.path.dirname(__file__)):
        for f in sorted(fs):
            if f.endswith(".py") and f.startswith("bm"):
                fpath = os.path.join(root, f)
                os.environ["YAPYJIT_EN"] = "ENABLE"
                subprocess.run([sys.executable, fpath, "--copy-env"])
                os.environ["YAPYJIT_EN"] = "DISABLE"
                subprocess.run([sys.executable, fpath, "--copy-env"])
