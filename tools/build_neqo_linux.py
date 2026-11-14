"""Build the standalone Neqo HTTP/3 bridge against Firefox's vendored sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from provenance import write_if_changed
import re
import shutil
import subprocess
import tomllib


def prepare(root, firefox):
    build = root / "build/necko-quic"
    crates = build / "crates"
    crates.mkdir(parents=True, exist_ok=True)
    inventory = {}
    for name in ("neqo-common", "neqo-http3", "neqo-qpack", "neqo-transport", "nss-rs", "mtu"):
        source = firefox / "third_party/rust" / name
        target = crates / name
        if not target.exists():
            shutil.copytree(source, target)
        for path in source.rglob("*"):
            if path.is_file():
                inventory[f"{name}/{path.relative_to(source).as_posix()}"] = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest = (source / "Cargo.toml").read_text()
        manifest = manifest.replace('git = "https://github.com/mozilla/nss-rs"', 'path = "../nss-rs"')
        # These copies build Linux standalone only; Cargo otherwise resolves
        # Windows/macOS and Gecko-only dependencies absent from the donor vendor.
        blocks = re.split(r'(?m)(?=^\[)', manifest)
        manifest = ''.join(b for b in blocks if not (
            b.startswith('[target.') and ('cfg(windows)' in b.splitlines()[0] or
                'target_os = "macos"' in b.splitlines()[0]) or
            b.startswith('[build-dependencies.mozbuild]')))
        manifest = manifest.replace('gecko = ["mozbuild"]', 'gecko = []')
        write_if_changed(target / "Cargo.toml", manifest)
    # Narrow embedding ports: expose the owned NSS descriptor before starting
    # the handshake so the standalone adapter can apply its TLS profile.
    agent = crates / "nss-rs/src/agent.rs"
    text = (firefox / "third_party/rust/nss-rs/src/agent.rs").read_text()
    text = text.replace('impl SecretAgent {', 'impl SecretAgent {\n'
        '    pub fn standalone_fd(&mut self) -> *mut std::ffi::c_void { self.fd.cast() }', 1)
    write_if_changed(agent, text)
    connection = crates / "neqo-transport/src/connection/mod.rs"
    text = (firefox / "third_party/rust/neqo-transport/src/connection/mod.rs").read_text()
    text = text.replace('    pub fn set_certificate_compression<',
        '    pub fn standalone_tls_fd(&mut self) -> *mut std::ffi::c_void {\n'
        '        self.crypto.tls_mut().standalone_fd()\n    }\n\n'
        '    pub fn set_certificate_compression<', 1)
    text = text.replace('    pub fn standalone_tls_fd(',
        '    pub fn standalone_tp_style(&mut self, style: u8) {\n'
        '        self.tps.borrow_mut().local_mut().standalone_wire_style = style;\n    }\n\n'
        '    pub fn standalone_tls_fd(', 1)
    write_if_changed(connection, text)
    tparams = crates / "neqo-transport/src/tparams.rs"
    text = (firefox / "third_party/rust/neqo-transport/src/tparams.rs").read_text()
    text = text.replace('pub struct TransportParameters {',
        'pub struct TransportParameters {\n    pub standalone_wire_style: u8,', 1)
    old = """        for (i, v) in self.params.iter().filter(|(i, v)| f(*i, v.as_ref())) {
            if let Some(tp) = v {
                tp.encode(enc, i);
            }
        }"""
    new = """        let mut entries: Vec<_> = self.params.iter()
            .filter(|(i,v)| f(*i,v.as_ref()) && v.is_some()).collect();
        if self.standalone_wire_style != 0 {
            // NSS entropy; Fisher-Yates with rejection avoids modulo bias.
            for end in (1..entries.len()).rev() {
                let bound = (end + 1) as u64;
                let limit = u64::MAX - u64::MAX % bound;
                let index = loop {
                    let value = u64::from_ne_bytes(random::<8>());
                    if value < limit { break (value % bound) as usize; }
                };
                entries.swap(end,index);
            }
        }
        for (i,v) in entries {
            if i == TransportParameterId::VersionInformation {
                if self.standalone_wire_style == 3 { continue; }
                if self.standalone_wire_style == 2 {
                    let mut entry = Encoder::default();
                    v.as_ref().unwrap().encode(&mut entry,i);
                    let mut decoded = Decoder::new(entry.as_ref());
                    decoded.decode_varint();
                    enc.encode_varint(0xff73db_u64);
                    enc.encode(decoded.decode_remainder());
                    continue;
                }
            }
            v.as_ref().unwrap().encode(enc,i);
        }"""
    if old not in text:
        raise ValueError("Neqo transport-parameter embedding anchor changed")
    write_if_changed(tparams, text.replace(old,new,1))

    source = root / "native/necko_posix/quic"
    # Reuse Firefox's compatibility crates when its vendor inventory has moved
    # ahead of a transitive requirement (bindgen, toml, Windows-only crates).
    patches = tomllib.loads((firefox / "Cargo.toml").read_text())["patch"]["crates-io"]
    patch_lines = []
    for name, values in patches.items():
        if "path" not in values or name not in ("bindgen", "toml_0_9", "windows-targets"):
            continue
        fields = {k: v for k, v in values.items() if k in ("path", "package", "version")}
        fields["path"] = str(firefox / fields["path"])
        patch_lines.append(json.dumps(name) + '={' + ','.join(k + '=' + json.dumps(v) for k, v in fields.items()) + '}\n')
    manifest = ('[package]\nname="necko_quic"\nversion="0.1.0"\nedition="2024"\n'
        '[lib]\ncrate-type=["cdylib"]\npath=' + json.dumps(str(source / "lib.rs")) + '\n'
        '[dependencies]\nserde_json="1"\nbase64="0.22"\nlibc="0.2"\n'
        'neqo-http3={path="crates/neqo-http3",default-features=false}\n'
        'neqo-transport={path="crates/neqo-transport",default-features=false}\n'
        'neqo-common={path="crates/neqo-common",default-features=false}\n'
        'nss={package="nss-rs",path="crates/nss-rs",default-features=false}\n'
        '[patch.crates-io]\n' + ''.join(patch_lines) +
        '[profile.release]\nopt-level=2\ncodegen-units=1\n')
    write_if_changed(build / "Cargo.toml", manifest)
    lock = source / "Cargo.lock"
    if lock.is_file():
        write_if_changed(build / "Cargo.lock", lock.read_text())
    config = build / ".cargo/config.toml"
    config.parent.mkdir(exist_ok=True)
    write_if_changed(config, '[source.crates-io]\nreplace-with="firefox"\n[source.firefox]\ndirectory=' +
        json.dumps(str(firefox / "third_party/rust")) + '\n')
    records = build / "neqo-upstream.json"
    write_if_changed(records, json.dumps({"firefox": "160.0", "neqo": "0.33.0", "nss-rs": "0.16.1",
        "sha256": inventory}, indent=2) + '\n')
    sdk = root / "build/necko/source/dist"
    if not sdk.exists():
        sdk.symlink_to(root / "build/necko/dist", target_is_directory=True)
    return build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefox-source", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--output", type=Path, required=True,
                        help="the helper's necko package directory (tools/build_necko_linux.py)")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = prepare(root, args.firefox_source.resolve())
    if args.prepare_only:
        return
    env = dict(os.environ, NSS_DIR=str(root / "build/necko/source/nss"), NSS_PREBUILT="1",
               RUSTFLAGS="-C link-arg=-Wl,-rpath,$ORIGIN")
    command = ["cargo", "build", "--release", "--offline", "-j", str(args.jobs)]
    if (root / "native/necko_posix/quic/Cargo.lock").is_file():
        command.append("--locked")
    subprocess.run(command,
        cwd=build, env=env, check=True)
    target = args.output / "libnecko_quic.so"
    if not target.is_file() or target.read_bytes() != (build / "target/release/libnecko_quic.so").read_bytes():
        shutil.copy2(build / "target/release/libnecko_quic.so", target)
    print(target)


if __name__ == "__main__":
    main()
