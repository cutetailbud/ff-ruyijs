"""Collect what ruyijs links from a partial Firefox browser build.

usage: collect.py OBJDIR OUTPUT

OUTPUT keeps the objdir layout, so it can stand in for the browser objdir
(tools/build.py --firefox-gfx-obj OUTPUT):
  gfx/skia/*.obj (or *.o)  Firefox's compiled Skia
  config/autoconf.mk       records the Firefox version of those objects
  dist/bin                 nss3, softokn3, freebl3 and mozglue as Firefox ships them
  dist/lib                 their import libraries (Windows)
  dist/include/{nss,nspr}  the NSS and NSPR headers
"""
import shutil
import sys
from pathlib import Path

objdir, output = Path(sys.argv[1]), Path(sys.argv[2])
windows = sys.platform == "win32" or (objdir / "dist/bin/nss3.dll").is_file()
if output.exists():
    shutil.rmtree(output)


def copy(source, target):
    if not source.is_file():
        raise SystemExit(f"missing: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


objects = sorted((objdir / "gfx/skia").glob("*.obj" if windows else "*.o"))
if not objects:
    raise SystemExit(f"no Skia objects under {objdir / 'gfx/skia'}")
for path in objects:
    copy(path, output / "gfx/skia" / path.name)
copy(objdir / "config/autoconf.mk", output / "config/autoconf.mk")
# The vendored-crates source replacement configure wrote; the macOS HTTP/3
# library (tools/build_necko_quic.py) builds Neqo offline with it.
if (objdir / ".cargo/config.toml").is_file():
    copy(objdir / ".cargo/config.toml", output / ".cargo/config.toml")

if windows:
    binaries = ["nss3.dll", "softokn3.dll", "freebl3.dll", "mozglue.dll"]
    libraries = ["nss3.lib", "mozglue.lib"]
else:
    suffix = ".dylib" if sys.platform == "darwin" else ".so"
    binaries = [f"lib{name}{suffix}" for name in ("nss3", "softokn3", "freebl3", "mozglue")]
    libraries = []
for name in binaries:
    copy(objdir / "dist/bin" / name, output / "dist/bin" / name)
# A partial build does not run the tier that installs import libraries into
# dist/lib; they are taken from the directory that linked them.
LINKED_IN = {"nss3.lib": "security", "mozglue.lib": "mozglue/build"}
for name in libraries:
    installed = objdir / "dist/lib" / name
    copy(installed if installed.is_file() else objdir / LINKED_IN[name] / name,
         output / "dist/lib" / name)
for name in ("nss", "nspr"):
    source = objdir / "dist/include" / name
    if not source.is_dir():
        raise SystemExit(f"missing: {source}")
    shutil.copytree(source, output / "dist/include" / name)

files = [p for p in output.rglob("*") if p.is_file()]
size = sum(p.stat().st_size for p in files)
print(f"collected {len(objects)} Skia objects, {len(binaries)} NSS binaries; "
      f"{len(files)} files, {size / 2**20:.1f} MiB in {output}")
