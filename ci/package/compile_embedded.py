"""Compile the bytecode of the runtime's embedded Python with that Python itself.

usage: <runtime>/vendor/necko-derived/python/python.exe -I -B ci/package/compile_embedded.py

The embedded Python runs every Necko helper and each DNS resolver child a
helper starts. Its standard library zip held sources only, and pip compiles the
bundled packages for whichever Python installs the wheel, so a helper compiled
about 350 modules each time it started: 2 to 2.4 CPU-seconds. This replaces
each source in python3XX.zip with its bytecode, as the python.org embeddable
package ships it, and writes bytecode next to the sources in Lib/site-packages.
Both are unchecked-hash .pyc files: they hold whatever the files' modification
times, and the helpers (which run with -B) never rewrite them. Running it again
recompiles the packages and leaves the compiled zip as it is.
"""
import compileall
import py_compile
import sys
import tempfile
import zipfile
from pathlib import Path

UNCHECKED = py_compile.PycInvalidationMode.UNCHECKED_HASH


def compile_zip(archive):
    """Replace every .py in the standard library zip with its .pyc."""
    compiled = []
    with tempfile.TemporaryDirectory(dir=archive.parent) as work:
        work = Path(work)
        target, script, bytecode = work / archive.name, work / "module.py", work / "module.pyc"
        with zipfile.ZipFile(archive) as source, zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as output:
            for info in source.infolist():
                data = source.read(info)
                if not info.filename.endswith(".py"):
                    output.writestr(info, data)
                    continue
                script.write_bytes(data)
                # dfile: tracebacks name the module by its place in the zip.
                py_compile.compile(str(script), cfile=str(bytecode), dfile=info.filename, doraise=True,
                                   invalidation_mode=UNCHECKED)
                entry = zipfile.ZipInfo(info.filename + "c", info.date_time)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = info.external_attr
                output.writestr(entry, bytecode.read_bytes())
                compiled.append(info.filename)
        target.replace(archive)
    return compiled


def main():
    home = Path(sys.executable).resolve().parent
    archive = home / f"python{sys.version_info.major}{sys.version_info.minor}.zip"
    site = home / "Lib" / "site-packages"
    if not archive.is_file() or not site.is_dir():
        raise SystemExit(f"{home} is not an embedded Python with {archive.name} and Lib/site-packages")
    compiled = compile_zip(archive)
    # ddir keeps the build directory out of the bytecode; the import system
    # names each module by its installed path when it loads it.
    if not compileall.compile_dir(str(site), ddir="Lib/site-packages", quiet=1, force=True,
                                  invalidation_mode=UNCHECKED, workers=1):
        raise SystemExit("compiling Lib/site-packages failed")
    packages = sum(1 for _ in site.rglob("*.pyc"))
    print(f"{archive.name}: {len(compiled)} modules compiled; Lib/site-packages: {packages} .pyc")


if __name__ == "__main__":
    main()
