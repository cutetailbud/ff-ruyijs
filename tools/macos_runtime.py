"""Stage the macOS Necko files a donor xpcshell already loads.

Firefox links these with @rpath and an @executable_path rpath. Copying them
beside xpcshell keeps that layout. install_name_tool is not used.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

NECKO_FILES = (
    "xpcshell",
    "XUL",
    "libnss3.dylib",
    "libmozglue.dylib",
    "libgkcodecs.dylib",
    "liblgpllibs.dylib",
    "ChannelPrefs",
    "libfreebl3.dylib",
    "libsoftokn3.dylib",
)
NECKO_DIRS = ("modules", "components", "defaults")
SHELL_FILES = ("js", "libmozglue.dylib", "libruyijs_client.dylib")


def donor_bin(project: Path) -> Path:
    override = os.environ.get("RUYIJS_XPCSHELL")
    if override:
        return Path(override).resolve().parent
    return project.parent / "firefox-159" / "obj-necko" / "dist" / "bin"


# Firefox's NSS beside libnecko_core.dylib and libnecko_quic.dylib (both have
# an @loader_path rpath); tools/build.py --necko-core-only links them into
# dist/necko_tls.
NSS_FILES = ("libnss3.dylib", "libmozglue.dylib", "libfreebl3.dylib", "libsoftokn3.dylib")


def stage_necko(dest: Path, donor: Path, script: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    tls_dist = script.parents[2] / "dist" / "necko_tls"
    if not (donor / "xpcshell").is_file():
        # No browser build with xpcshell (e.g. CI, where XUL does not fit the
        # runners): direct HTTPS, HTTP/3 and cleartext use the Necko libraries;
        # a proxy request raises NeckoError("Necko donor xpcshell was not found").
        for name in ("libnecko_core.dylib", *NSS_FILES):
            if not (tls_dist / name).exists():
                raise FileNotFoundError(f"dist/necko_tls is missing {name}")
            shutil.copy2(tls_dist / name, dest / name)
        quic = tls_dist / "libnecko_quic.dylib"
        if quic.is_file():
            shutil.copy2(quic, dest / "libnecko_quic.dylib")
        print("macOS wheel: no xpcshell donor; proxies are not available")
        return
    for name in NECKO_FILES:
        source = donor / name
        if not source.is_file():
            raise FileNotFoundError(f"Necko donor is missing {name}")
        shutil.copy2(source, dest / name)
    for folder in NECKO_DIRS:
        source = donor / folder
        if not source.is_dir():
            raise FileNotFoundError(f"Necko donor is missing {folder}")
        shutil.copytree(source, dest / folder)
    if not script.is_file():
        raise FileNotFoundError("Necko fetch script is missing")
    shutil.copy2(script, dest / "necko_fetch.js")
    core = tls_dist / "libnecko_core.dylib"
    if core.is_file():
        shutil.copy2(core, dest / "libnecko_core.dylib")
    quic = tls_dist / "libnecko_quic.dylib"
    if quic.is_file():
        shutil.copy2(quic, dest / "libnecko_quic.dylib")


def write_runtime_zip(project: Path, archive: Path) -> None:
    shell = project / "dist"
    for name in SHELL_FILES:
        if not (shell / name).is_file():
            raise FileNotFoundError(f"macOS runtime is missing dist/{name}")
    archive.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ruyi-macos-runtime-") as temporary:
        root = Path(temporary)
        dist = root / "dist"
        dist.mkdir()
        for name in SHELL_FILES:
            shutil.copy2(shell / name, dist / name)
        stage_necko(
            dist / "necko",
            donor_bin(project),
            project / "src/resource_network/necko_fetch.js",
        )
        staged = sum(1 for path in root.rglob('*') if path.is_file())
        print('staged_files', staged, flush=True)
        print('compressing', flush=True)
        if archive.exists():
            archive.unlink()
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as packed:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    packed.write(path, path.relative_to(root).as_posix())
    print("size", archive.stat().st_size)
    print("sha256", hashlib.sha256(archive.read_bytes()).hexdigest())


def main() -> None:
    project = Path(__file__).resolve().parents[1]
    archive = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/ruyijs-runtime-macos-arm64.zip")
    write_runtime_zip(project, archive)


if __name__ == "__main__":
    main()