"""Assemble the Windows runtime archive from a source build.

usage: python ci/package/runtime.py [--dist dist] [--out build/ci-runtime]

The base is the runtime currently pinned in runtime.json (downloaded and
checked against its SHA-256): it supplies the Necko transport, the embedded
Python and the MSVC runtime DLLs. js.exe and mozglue.dll are replaced by the
ones tools/build.py just built from the firefox/ submodule; LICENSE, NOTICE and
COMMERCIAL-LICENSE.md come from the repository. The archive is written to
OUT, and runtime.json in the working tree is updated to pin it, so that
`pip wheel .` (setup.py) packages exactly this runtime.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from wheel_runtime import download_url, validate  # noqa: E402

REPLACED = ("dist/js.exe", "dist/mozglue.dll")
LICENSE_FILES = ("LICENSE", "NOTICE", "COMMERCIAL-LICENSE.md")


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            value.update(block)
    return value.hexdigest()


def version():
    text = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    return re.search(r'^version = "([^"]+)"', text, re.M).group(1)


def fetch_base(manifest, directory):
    archive = directory / manifest["archive"]
    request = urllib.request.Request(download_url(manifest), headers={"User-Agent": "ruyijs-build"})
    with urllib.request.urlopen(request, timeout=300) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    if archive.stat().st_size != manifest["size"] or digest(archive) != manifest["sha256"]:
        raise SystemExit("base runtime archive does not match runtime.json")
    base = directory / "base"
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or ".." in path.parts or "\\" in entry.filename or ":" in entry.filename:
                raise SystemExit(f"unsafe entry {entry.filename}")
        source.extractall(base)
    validate(base)
    return base


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    parser.add_argument("--out", type=Path, default=ROOT / "build/ci-runtime")
    args = parser.parse_args()
    release = version()
    manifest_path = ROOT / "runtime.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    out = args.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    stage = out / "runtime"
    with tempfile.TemporaryDirectory(dir=out) as temporary:
        shutil.copytree(fetch_base(manifest, Path(temporary)), stage)
    for name in REPLACED:
        built = args.dist / Path(name).name
        if not built.is_file():
            raise SystemExit(f"missing build output {built}")
        shutil.copy2(built, stage / name)
    for name in LICENSE_FILES:
        shutil.copy2(ROOT / name, stage / name)
    firefox = subprocess.run(["git", "-C", str(ROOT), "ls-tree", "HEAD", "firefox"],
                             capture_output=True, text=True, check=True).stdout.split()[2]
    milestone = (ROOT / "firefox/config/milestone.txt").read_text(encoding="utf-8").split()[-1]
    (stage / "SOURCE_NOTICE.md").write_text(
        "# Runtime sources\n\n"
        f"Package: https://pypi.org/project/ruyijs/{release}/\n"
        f"dist/js.exe and dist/mozglue.dll were built from Firefox {milestone} "
        f"(github.com/mozilla-firefox/firefox at {firefox}) and the ruyijs sources\n"
        "of the same release (https://github.com/cutetailbud/ff-ruyijs).\n"
        "The Necko transport and the embedded Python under vendor/ are carried over from the\n"
        f"previously pinned runtime {manifest['archive']} (sha256 {manifest['sha256']}).\n"
        "Component license texts are in licenses/ and the embedded Python packages.\n"
        "SHA256SUMS describes this runtime.\n", encoding="utf-8", newline="\n")
    files = sorted((p for p in stage.rglob("*") if p.is_file() and p.name != "SHA256SUMS"),
                   key=lambda p: p.relative_to(stage).as_posix())
    (stage / "SHA256SUMS").write_text(
        "".join(f"{digest(p)}  {p.relative_to(stage).as_posix()}\n" for p in files),
        encoding="utf-8", newline="\n")
    validate(stage)
    archive = out / f"ruyijs-runtime-{release}-win-amd64.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as target:
        for path in files + [stage / "SHA256SUMS"]:
            target.write(path, path.relative_to(stage).as_posix())
    unpacked = sum(p.stat().st_size for p in files)
    if unpacked > manifest["unpacked_limit"]:
        raise SystemExit("runtime exceeds the unpacked limit")
    manifest.update(
        version=release, archive=archive.name, sha256=digest(archive), size=archive.stat().st_size,
        shell_sha256=digest(stage / "dist/js.exe"), inventory_sha256=digest(stage / "SHA256SUMS"),
        url=f"https://github.com/cutetailbud/ff-ruyijs/releases/download/v{release}/{archive.name}")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(manifest_path, out / "runtime.json")
    print(json.dumps({k: manifest[k] for k in ("version", "archive", "sha256", "size", "shell_sha256")}, indent=2))


if __name__ == "__main__":
    main()
