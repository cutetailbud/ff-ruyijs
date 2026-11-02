"""Check that the Necko helper of an installed ruyijs wheel starts light.

A Context starts Necko helper processes, and a helper starts a resolver child
for each host it looks up. On Windows and Linux they run on the Python bundled
with the wheel: whichever Python installed the wheel, every module they import
must load from bytecode of that bundled Python, none compiled from source at
start (which cost each helper 2 CPU-seconds), and the bundled Python must not
carry modules the helper cannot use. On macOS the helper runs on the Python
that installed ruyijs, whose bytecode pip compiles. Exit status is non-zero on
the first failed check.
"""
import json
import subprocess
import sys
from pathlib import Path

import ruyijs

# Run by the helper's interpreter: what a helper and a resolver child import.
PROBE = r'''
import importlib.util, json, sys, time
compiled = []
def audit(event, args):
    if event == "compile" and isinstance(args[1], str) and args[1].endswith(".py"):
        compiled.append(args[1])
sys.addaudithook(audit)
for name in ("ruyijs_necko.oneshot", "ruyijs_necko.resolver_child", "ruyijs_necko.webtransport_worker"):
    if importlib.util.find_spec(name):
        __import__(name)
print(json.dumps({"compiled": compiled, "modules": len(sys.modules), "cpu": time.process_time(),
                  "version": sys.version.split()[0]}))
'''


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def main():
    runtime = Path(ruyijs.get_runtime_path())
    embedded = runtime / "vendor/necko-derived/python"
    if sys.platform == "win32":
        python = embedded / "python.exe"
    elif sys.platform.startswith("linux"):
        python = embedded / "bin/python3"
    else:
        print("skipped: the helper runs on the Python that installed ruyijs")
        return
    result = subprocess.run([str(python), "-I", "-B", "-c", PROBE], capture_output=True, text=True, check=True)
    probe = json.loads(result.stdout.strip().splitlines()[-1])
    check("the helper compiles no module at start", not probe["compiled"],
          f"Python {probe['version']}, {probe['modules']} modules, {len(probe['compiled'])} compiled: "
          f"{[Path(name).name for name in probe['compiled'][:8]]}")
    print(f"     helper interpreter start and imports: {probe['cpu']:.2f} CPU-s")
    if sys.platform == "win32":
        import zipfile
        archive = next(embedded.glob("python3*.zip"))
        with zipfile.ZipFile(archive) as stdlib:
            sources = [name for name in stdlib.namelist() if name.endswith(".py")]
        check("the standard library zip holds bytecode only", not sources, sources[:5])
        unused = [name for name in ("_tkinter.pyd", "tcl86t.dll", "tk86t.dll", "_sqlite3.pyd", "sqlite3.dll",
                                    "winsound.pyd") if (embedded / "DLLs" / name).exists()]
        check("no Tk, SQLite or winsound modules", not unused, unused)
    elif sys.platform.startswith("linux"):
        stdlib = next((embedded / "lib").glob("python3.*"))
        unused = [path.name for path in stdlib.glob("config-*")]
        unused += [path.name for path in (stdlib / "lib-dynload").iterdir()
                   if path.name.startswith(("_test", "_ctypes_test", "_tkinter", "_curses", "readline",
                                            "_sqlite3", "_dbm", "_gdbm", "xxlimited", "xxsubtype"))]
        unused += [name for name in ("pip", "ensurepip") if (stdlib / "site-packages" / name).exists()]
        check("no build files, test, Tk, curses, readline, dbm, SQLite or pip modules", not unused, unused)
        libraries = sorted(path.name for path in (runtime / "lib").iterdir())
        tk = [name for name in libraries if name.startswith(("libtcl", "libtk"))]
        check("no Tk libraries bundled for _tkinter", not tk, tk)
    print("helper checks passed")


if __name__ == "__main__":
    main()
