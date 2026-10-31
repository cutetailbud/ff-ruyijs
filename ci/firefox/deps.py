"""Print the objdir build steps for the Firefox parts ruyijs links, in order.

usage: deps.py OBJDIR

Run after `mach configure`: the build backend has already written each
library's object list (backend.mk `<lib>_OBJS := @<file>.list`). Output, one
step per line:
  DIR          build that directory (objects, static libraries)
  DIR TARGET   build only TARGET in DIR (a shared library link)
Object directories come first, then the shared libraries in link order
(mozglue, then nss3, then freebl3 and softokn3, which link nss3), then
gfx/skia, whose objects tools/build.py archives itself.
"""
import os
import re
import sys
from pathlib import Path

objdir = Path(sys.argv[1]).resolve()
# One step per LF-terminated line on every platform (Windows' Python would
# print CRLF, and bash keeps the CR in the last field).
sys.stdout.reconfigure(newline=chr(10))
WANTED = ("mozglue", "nss3", "freebl3", "softokn3")


def library_name(path):
    stem = Path(path).name.split(".")[0]
    return stem[3:] if stem.startswith("lib") else stem


def rel(path):
    return Path(os.path.relpath(path, objdir)).as_posix()


libraries = {}
for backend in objdir.rglob("backend.mk"):
    text = backend.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^SHARED_LIBRARY := (\S+)$", text, re.M)
    if not match or library_name(match.group(1)) not in WANTED:
        continue
    directory = backend.parent
    target = match.group(1).replace("$(DEPTH)", os.path.relpath(objdir, directory).replace(os.sep, "/"))
    # The object list file, referenced per the platform's EXPAND_LIBS_LIST_STYLE:
    # "@f.list" (list), "-Wl,@f.list" (linkerlist), "-Wl,-filelist,f.list"
    # (filelist). A library linking only its own directory's objects has none.
    listing = re.search(r"^\S+_OBJS := (?:-Wl,-filelist,|-Wl,@|@)(\S+\.list)$", text, re.M)
    objects = []
    if listing:
        list_file = Path(listing.group(1))
        list_file = list_file if list_file.is_absolute() else directory / list_file
        for line in list_file.read_text(encoding="utf-8", errors="replace").splitlines():
            entry = line.strip().strip('"')
            if entry:
                path = Path(entry) if Path(entry).is_absolute() else directory / entry
                objects.append(path.resolve())
    linked = [Path(value.replace("$(DEPTH)", str(objdir))).resolve()
              for value in re.findall(r"^(?:SHARED|STATIC)_LIBS \+= (\S+)$", text, re.M)]
    libraries[library_name(match.group(1))] = dict(dir=rel(directory), target=target,
                                                    objects=objects, linked=linked)

missing = [name for name in WANTED if name not in libraries]
if missing:
    raise SystemExit(f"no backend.mk links {missing} under {objdir}")

link_dirs = {info["dir"] for info in libraries.values()}
emitted = set()
# Per library, in link order: its object directories, then its link step.
# Building an object directory may link a sibling library whose directory
# name extends it (freebl_freebl -> freebl_freebl3 on macOS), so nothing of
# a later library is built before the libraries it links exist.
for name in WANTED:
    info = libraries[name]
    needed = [rel(path.parent) for path in info["objects"]]
    # A static library such as pure_virtual is built by its own directory;
    # the import libraries of the wanted DLLs come from their link step.
    needed += [rel(path.parent) for path in info["linked"] if library_name(path) not in WANTED]
    # A directory that prefixes a link directory goes last in its group: by
    # then every object of the library it may link is built.
    prefixes = lambda d: any(other != d and other.startswith(d) for other in link_dirs)
    for d in sorted(set(needed), key=lambda d: (prefixes(d), d)):
        if d not in link_dirs and d not in emitted:
            print(d)
            emitted.add(d)
    print(info["dir"], info["target"])
print("gfx/skia")
