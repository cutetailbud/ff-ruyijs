"""Build the upstream Neqo bridge on Linux using the shared ClientHello hooks."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tomllib

from build_necko_quic import _patch_nss, _patch_transport
from provenance import write_if_changed


def build_quic(root, firefox, jobs=2):
    build = root / "build/necko-quic-linux"
    crates = build / "crates"
    crates.mkdir(parents=True, exist_ok=True)
    names = ("neqo-common", "neqo-http3", "neqo-qpack", "neqo-transport", "nss-rs", "mtu")
    identity = hashlib.sha256(str(firefox).encode())
    for tool in (Path(__file__), root / "tools/build_necko_quic.py"):
        identity.update(tool.read_bytes())
    for name in names:
        for source in sorted((firefox / "third_party/rust" / name).rglob("*")):
            if source.is_file():
                identity.update(str(source.relative_to(firefox)).encode() + b"\0")
                identity.update(source.read_bytes())
    signature = identity.hexdigest()
    stamp = build / "overlay.sha256"
    if not stamp.is_file() or stamp.read_text() != signature:
        for name in names:
            source = firefox / "third_party/rust" / name
            target = crates / name
            if target.exists():
                shutil.rmtree(target)
            for original in source.rglob("*"):
                if original.is_file():
                    copied = target / original.relative_to(source)
                    copied.parent.mkdir(parents=True, exist_ok=True)
                    data = original.read_bytes()
                    if not copied.is_file() or copied.read_bytes() != data:
                        copied.write_bytes(data)
            manifest = (source / "Cargo.toml").read_text()
            manifest = manifest.replace('git = "https://github.com/mozilla/nss-rs"', 'path = "../nss-rs"')
            blocks = re.split(r"(?m)(?=^\[)", manifest)
            manifest = "".join(block for block in blocks if not (
                block.startswith("[target.") and ("cfg(windows)" in block.splitlines()[0] or
                    'target_os = "macos"' in block.splitlines()[0]) or
                block.startswith("[build-dependencies.mozbuild]")))
            write_if_changed(target / "Cargo.toml", manifest.replace('gecko = ["mozbuild"]', 'gecko = []'))
        _patch_nss(crates / "nss-rs")
        _patch_transport(crates / "neqo-transport")
        stamp.write_text(signature)
    patches = tomllib.loads((firefox / "Cargo.toml").read_text())["patch"]["crates-io"]
    entries = []
    for name, values in patches.items():
        if "path" not in values or name not in ("bindgen", "toml_0_9", "windows-targets"):
            continue
        fields = {key: value for key, value in values.items() if key in ("path", "package", "version")}
        fields["path"] = str(firefox / fields["path"])
        entries.append(json.dumps(name) + "={" + ",".join(
            key + "=" + json.dumps(value) for key, value in fields.items()) + "}\n")
    manifest = ('[package]\nname="necko_quic"\nversion="0.0.0"\nedition="2024"\n'
        '[lib]\ncrate-type=["cdylib"]\npath=' + json.dumps(str(root / "native/necko_quic/src/lib.rs")) + '\n'
        '[dependencies]\nserde_json="1"\nlibc="0.2"\n'
        'neqo-http3={path="crates/neqo-http3",default-features=false}\n'
        'neqo-transport={path="crates/neqo-transport",default-features=false}\n'
        'neqo-common={path="crates/neqo-common",default-features=false}\n'
        'nss={package="nss-rs",path="crates/nss-rs",default-features=false}\n'
        '[patch.crates-io]\n' + "".join(entries) +
        '[profile.release]\nopt-level=2\ncodegen-units=1\n')
    write_if_changed(build / "Cargo.toml", manifest)
    lock = root / "native/necko_quic/Cargo-linux.lock"
    if lock.is_file():
        write_if_changed(build / "Cargo.lock", lock.read_text())
    config = build / ".cargo/config.toml"
    config.parent.mkdir(exist_ok=True)
    write_if_changed(config, '[source.crates-io]\nreplace-with="firefox"\n[source.firefox]\ndirectory=' +
                     json.dumps(str(firefox / "third_party/rust")) + '\n')
    sdk = root / "build/necko/source/dist"
    if not sdk.exists():
        sdk.symlink_to(root / "build/necko/dist", target_is_directory=True)
    environment = dict(os.environ, NSS_DIR=str(root / "build/necko/source/nss"), NSS_PREBUILT="1",
        RUSTFLAGS="-C link-arg=-Wl,-rpath,$ORIGIN -C link-arg=-Wl,-z,defs "
                  "-C link-arg=-lz -C link-arg=-lbrotlidec -C link-arg=-lzstd")
    command = ["cargo", "build", "--release", "--offline", "-j", str(jobs)]
    if lock.is_file():
        command.append("--locked")
    subprocess.run(command, cwd=build, env=environment, check=True)
    target = root / "dist/necko_tls/libnecko_quic.so"
    shutil.copy2(build / "target/release/libnecko_quic.so", target)
    write_if_changed(lock, (build / "Cargo.lock").read_text())
    print(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefox-source", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    build_quic(Path(__file__).resolve().parents[1], args.firefox_source.resolve(), args.jobs)


if __name__ == "__main__":
    main()
