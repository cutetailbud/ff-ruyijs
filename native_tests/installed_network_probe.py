"""Run local peers against the helper modules bundled in an installed wheel."""

import argparse
import os
from pathlib import Path
import runpy
import sys

import ruyijs
from ruyijs._runtime_paths import get_runtime_path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("probe", choices=("network", "webtransport", "image"))
args = parser.parse_args()
package = Path(ruyijs.__file__).resolve().parent
runtime = get_runtime_path()
if runtime != package / "_runtime":
    raise RuntimeError("Run this probe from a fresh wheel installation")
sites = list((runtime / "vendor/necko-derived/python/lib").glob("python*/site-packages"))
if len(sites) != 1:
    raise RuntimeError("The bundled helper must have one site-packages directory")
sys.path.insert(0, str(sites[0]))
os.environ["RUYIJS_TEST_PACKAGE"] = str(package.parent)
probe = Path(__file__).with_name(args.probe + "_probe.py")
sys.argv = [str(probe), "-v"]
runpy.run_path(str(probe), run_name="__main__")
