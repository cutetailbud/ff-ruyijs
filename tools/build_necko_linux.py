"""Build the shared POSIX Necko adapters against Firefox's Linux NSS/NSPR."""

import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv

from build import install_network_helper


def run(command, **kwargs):
    subprocess.run([str(item) for item in command], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefox-source", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--http3", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux" or args.jobs < 1:
        parser.error("Linux and a positive --jobs value are required")
    root = Path(__file__).resolve().parents[1]
    firefox = args.firefox_source.resolve()
    for source in (firefox / "security/nss", firefox / "nsprpub"):
        if not source.is_dir():
            parser.error(f"Firefox donor source is missing: {source}")
    build = root / "build/necko"
    runtime = root / "vendor/necko-derived/python"
    python = runtime / "bin/python3"
    if not python.is_file():
        venv.EnvBuilder(with_pip=True).create(runtime)
    dependencies = ["requests", "brotli", "zstandard", "h2", "cryptography",
                    "publicsuffixlist", "gyp-next"]
    if sys.version_info < (3, 14):
        dependencies.append("backports.zstd")
    run([python, "-m", "pip", "install", "--disable-pip-version-check", *dependencies])
    source_root = build / "source"
    for source, name in ((firefox / "security/nss", "nss"), (firefox / "nsprpub", "nspr")):
        target = source_root / name
        if not target.exists():
            shutil.copytree(source, target, ignore=shutil.ignore_patterns("out", ".git", "obj-*"))
    environment = dict(os.environ, PATH=str(runtime / "bin") + os.pathsep + os.environ["PATH"])
    run(["bash", "build.sh", "-o", "--clang", "--disable-tests", "-j", args.jobs,
         "--dist=" + str(build / "dist")], cwd=source_root / "nss", env=environment)
    nss = build / "dist/Release"
    destination = root / "dist/necko_tls"
    destination.mkdir(parents=True, exist_ok=True)
    for library in (nss / "lib").glob("*.so"):
        shutil.copy2(library, destination / library.name)
    library = destination / "libnecko_core.so"
    run(["clang", "-shared", "-fPIC", "-O2", "-pthread", "-DXP_UNIX",
         "-fvisibility=hidden", "-I" + str(nss / "include/nspr"),
         "-I" + str(build / "dist/public/nss"),
         root / "native/necko_tls/necko_tls.c", root / "native/necko_tls/necko_exchange.c",
         "-L" + str(destination), "-Wl,-rpath,$ORIGIN", "-Wl,-z,defs",
         "-lssl3", "-lnss3", "-lnssutil3", "-lnspr4", "-o", library])
    helper = install_network_helper(root, [])
    run([python, "-I", "-B", "-c",
         "from necko.core_client import CoreClient; c=CoreClient(); "
         "assert c.available; print('shared POSIX Necko adapter: available'); c.close()"])
    if args.http3:
        from build_neqo_linux import build_quic
        build_quic(root, firefox, args.jobs)
    native = ctypes.CDLL(str(library))
    native.necko_tls_nss_version.restype = ctypes.c_char_p
    native.necko_tls_nspr_version.restype = ctypes.c_char_p
    report = {"backend": "shared-posix-necko+nss",
              "nss": native.necko_tls_nss_version().decode(),
              "nspr": native.necko_tls_nspr_version().decode(),
              "http3": (destination / "libnecko_quic.so").is_file(), "helper": helper}
    (runtime / "backend.json").write_text(json.dumps(report, indent=2) + "\n")
    licenses = root / "dist/licenses"
    licenses.mkdir(exist_ok=True)
    for source, name in (
        (firefox / "LICENSE", "mozilla-MPL.txt"),
        (firefox / "toolkit/content/license.html.mako", "firefox-license.html.mako"),
        (firefox / "security/nss/COPYING", "nss-COPYING"),
        (firefox / "nsprpub/LICENSE", "nspr-LICENSE"),
        (firefox / "gfx/skia/LICENSE", "skia-LICENSE"),
        (firefox / "gfx/skia/LICENSE-NOTICE-skia.txt", "skia-NOTICE"),
        (firefox / "gfx/skia/LICENSE-NOTICE-android.txt", "skia-android-NOTICE"),
    ):
        if not source.is_file():
            raise FileNotFoundError(source)
        shutil.copy2(source, licenses / name)
    shutil.copytree(firefox / "toolkit/content/licenses", licenses / "mozilla-third-party", dirs_exist_ok=True)
    for crate in (firefox / "third_party/rust").iterdir():
        for source in crate.glob("*"):
            if source.is_file() and source.name.upper().startswith(("LICENSE", "COPYING", "NOTICE")):
                target = licenses / "rust" / crate.name / source.name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
    print(json.dumps({key: value for key, value in report.items() if key != "helper"}))


if __name__ == "__main__":
    main()
