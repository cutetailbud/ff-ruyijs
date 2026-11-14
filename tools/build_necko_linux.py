"""Build the Linux Necko helper: native NSS/NSPR and the standalone Necko adapter.

The helper is the Python environment the shell starts for its HTTP(S) requests
(src/resource_network/necko_helper.cpp looks for
vendor/necko-derived/python/bin/python3 beside dist/, as on macOS). It holds:

- the `necko` and `ruyijs_necko` packages of the runtime runtime.json pins,
  verified by SHA-256 and extracted unchanged except for the Linux patches
  below, with src/resource_network/necko_oneshot.py as ruyijs_necko/oneshot.py
  (tools/build.py installs the same file on Windows and macOS);
- NSS 3.132 and NSPR 4.40 built from the Firefox source tree;
- libnecko_core.so, the Linux implementation of the adapter's native ABI
  (native/necko_posix/src), and with --http3 libnecko_quic.so
  (tools/build_neqo_linux.py).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request
import venv
import zipfile

import compression_native

DEPENDENCIES = ["requests==2.34.2", "urllib3==2.7.0", "h2==4.4.1",
                "hpack==4.2.0", "hyperframe==6.1.0", "brotli==1.2.0",
                "cryptography==50.0.1", "publicsuffixlist==1.0.2.20260922"]
UPSTREAM = "vendor/necko-derived/python/Lib/site-packages/"
# The adapter's Windows paths and one HTTP/3 cleanup signature, as exact text
# edits (native/necko_posix/README.md). A changed upstream file fails the build.
PATCHES = {
    "ruyijs_necko/brotli_native.py": [
        ("import ctypes as ct\n", "import ctypes as ct\nimport os\n"),
        ('        path = Path(__file__).with_name("ruyijs_brotli.dll")\n',
         '        path = (Path(__file__).with_name("ruyijs_brotli.dll") if os.name == "nt" else\n'
         '                Path(__file__).parent.parent / "necko/libnecko_core.so")\n'),
    ],
    "ruyijs_necko/launch.py": [
        ("import os\n", "import os\nimport sys\n"),
        ('    executable = runtime / "python/python.exe"\n',
         '    executable = runtime / "python/python.exe" if os.name == "nt" else Path(sys.executable)\n'),
    ],
    "ruyijs_necko/connection_lifetime.py": [
        ("def h3_finished(connection, udp):\n", "def h3_finished(connection, udp, selector=None):\n"),
        ("        connection._ruyijs_retained_udp = udp\n",
         "        connection._ruyijs_retained_udp = udp\n"
         "    if selector is not None:\n"
         "        connection._ruyijs_retained_selector = selector\n"),
        ('    for attribute in ("_ruyijs_retained_udp", "_wake_read", "_wake_write", "_dll_directory"):\n',
         '    for attribute in ("_ruyijs_retained_selector", "_ruyijs_retained_udp", "_wake_read", '
         '"_wake_write", "_dll_directory"):\n'),
    ],
    # The Linux QUIC bridge selects the transport-parameter style per release.
    "necko/_http3.py": [
        ('                profile=PROFILES[self.firefox]["quic"],\n',
         '                profile=PROFILES[self.firefox]["quic"],\n'
         '                firefox=self.firefox,\n'),
    ],
}


def run(args, **kwargs):
    subprocess.run(list(map(str, args)), check=True, **kwargs)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def runtime_archive(root, build):
    """The runtime runtime.json pins, downloaded once and checked."""
    manifest = json.loads((root / "runtime.json").read_text(encoding="utf-8"))
    archive = build / manifest["archive"]
    if not archive.is_file():
        request = urllib.request.Request(manifest["url"], headers={"User-Agent": "ruyijs-build"})
        with urllib.request.urlopen(request, timeout=300) as source, archive.open("wb") as output:
            shutil.copyfileobj(source, output)
    if archive.stat().st_size != manifest["size"] or digest(archive.read_bytes()) != manifest["sha256"]:
        archive.unlink()
        raise ValueError("Pinned runtime archive size or SHA-256 differs from runtime.json")
    return manifest, archive


def install_packages(root, build, site):
    manifest, archive = runtime_archive(root, build)
    inventory = {}
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            if not entry.filename.startswith(UPSTREAM):
                continue
            relative = Path(entry.filename[len(UPSTREAM):])
            if (not relative.parts or relative.parts[0] not in ("necko", "ruyijs_necko") or
                    relative.suffix.lower() not in (".py", ".json", ".pem", ".dat", ".html", ".txt")):
                continue
            content = source.read(entry)
            inventory[relative.as_posix()] = digest(content)
            text_name = relative.as_posix()
            if text_name in PATCHES:
                text = content.decode("utf-8").replace("\r\n", "\n")
                for old, new in PATCHES[text_name]:
                    if text.count(old) != 1:
                        raise ValueError(f"Linux patch anchor changed in {text_name}: {old!r}")
                    text = text.replace(old, new)
                content = text.encode("utf-8")
            target = site / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
    missing = sorted(set(PATCHES) - set(inventory))
    if missing:
        raise ValueError(f"Pinned runtime lacks {missing}")
    oneshot = site / "ruyijs_necko/oneshot.py"
    oneshot.write_bytes((root / "src/resource_network/necko_oneshot.py").read_bytes())
    (build / "upstream.json").write_text(json.dumps({
        "archive": manifest["url"], "sha256": manifest["sha256"], "files": inventory,
        "patched": sorted(PATCHES), "replaced": {"ruyijs_necko/oneshot.py":
                                                 "src/resource_network/necko_oneshot.py"}},
        indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefox-source", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--http3", action="store_true", help="also build Firefox's Neqo HTTP/3 bridge")
    args = parser.parse_args()
    if not sys.platform.startswith("linux") or sys.version_info < (3, 14) or args.jobs < 1:
        parser.error("Linux, Python 3.14+ and a positive jobs count are required")
    root = Path(__file__).resolve().parents[1]
    build = root / "build/necko"
    build.mkdir(parents=True, exist_ok=True)
    runtime = root / "vendor/necko-derived/python"
    if not (runtime / "bin/pip").exists():
        venv.EnvBuilder(with_pip=True).create(runtime)
    python = runtime / "bin/python3"
    run([python, "-m", "pip", "install", "--disable-pip-version-check", *DEPENDENCIES, "gyp-next==0.21.0"])
    site = Path(subprocess.check_output(
        [python, "-c", "import sysconfig; print(sysconfig.get_path('purelib'))"], text=True).strip())
    install_packages(root, build, site)
    source = build / "source"
    for origin, name in ((args.firefox_source / "security/nss", "nss"),
                         (args.firefox_source / "nsprpub", "nspr")):
        destination = source / name
        if not destination.exists():
            shutil.copytree(origin, destination, ignore=shutil.ignore_patterns("out", ".git", "obj-*"))
    if args.prepare_only:
        return
    env = dict(os.environ, PATH=str(runtime / "bin") + os.pathsep + os.environ["PATH"])
    run(["bash", "build.sh", "-o", "--clang", "--disable-tests", "-j", args.jobs,
         "--dist=" + str(build / "dist")], cwd=source / "nss", env=env)
    nss = build / "dist/Release"
    target = site / "necko"
    for library in (nss / "lib").glob("*.so"):
        shutil.copy2(library, target / library.name)
    vendor = root / "native/necko_posix"
    # The Brotli 1.2.0 decoder of vendor/compression-native, which Windows and
    # macOS compile too; ruyijs_necko/brotli_native.py accepts only that version.
    compression = root / "vendor/compression-native"
    compression_native.frozen_inputs(compression)
    (build / "brotli").mkdir(exist_ok=True)
    decoder = []
    for unit in compression_native.brotli_units(compression):
        if unit.startswith(("common/", "dec/")):
            output = build / "brotli" / (unit.replace("/", "_")[:-2] + ".o")
            run([os.environ.get("CC", "clang"), "-std=c11", "-fPIC", "-O2",
                 *compression_native.brotli_flags(compression),
                 "-c", compression / "brotli" / unit, "-o", output])
            decoder.append(output)
    run([os.environ.get("CXX", "clang++"), "-std=c++20", "-shared", "-fPIC", "-O2", "-pthread",
         "-I" + str(nss / "include/nspr"), "-I" + str(build / "dist/public/nss"),
         "-I" + str(compression / "brotli/include"),
         *sorted((vendor / "src").glob("*.cpp")), *decoder, "-L" + str(target),
         "-Wl,-rpath,$ORIGIN", "-Wl,-z,defs", "-lssl3", "-lnss3", "-lnssutil3", "-lnspr4",
         "-o", target / "libnecko_core.so"])
    run([python, "-I", "-B", "-c", "from necko._transport import _load_library; "
         "n=_load_library(); print('NSS', n.necko_tls_nss_version().decode(), "
         "'NSPR', n.necko_tls_nspr_version().decode())"])
    if args.http3:
        run([sys.executable, "-B", root / "tools/build_neqo_linux.py",
             "--firefox-source", args.firefox_source, "--jobs", min(args.jobs, 4),
             "--output", target])
    http3 = (target / "libnecko_quic.so").is_file()
    (runtime / "backend.json").write_text(json.dumps({"backend": "standalone-necko+nss",
        "nss": "3.132", "http": ["http/1.1", "h2"] + (["h3"] if http3 else []), "http3": http3,
        "python": sys.version.split()[0]}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
