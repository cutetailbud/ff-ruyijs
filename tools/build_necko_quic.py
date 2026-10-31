"""Build libnecko_quic.dylib from the Firefox 159 Neqo 0.32.0 crates.

Cargo stays offline. Git dependencies inside the donor Neqo crates are
rewritten in a temporary overlay so resolution does not try to clone
nss-rs. The donor tree itself is not modified. The target directory is
outside this repository.

`python tools/build_necko_quic.py library` builds only the dylib. That
path copies nss-rs and neqo-transport into a separate overlay, then
adds the ClientHello hooks the stock crates do not expose. It does not
remove `/tmp/neqo-overlay` and it does not build neqo-server.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


OVERLAY_CRATES = (
    "neqo-bin",
    "neqo-common",
    "neqo-http3",
    "neqo-qpack",
    "neqo-transport",
    "neqo-udp",
    "mtu",
    "test-fixture",
)

_NSS_BLOCK = re.compile(
    r'\[dependencies\.nss\]\n'
    r'version = "0\.16"\n'
    r'git = "https://github.com/mozilla/nss-rs"\n'
    r'package = "nss-rs"\n'
)
_FIXTURE_BLOCK = re.compile(
    r'\[dependencies\.nss-test-fixture\]\n'
    r'version = "0\.1"\n'
    r'git = "https://github.com/mozilla/nss-rs"\n'
    r'package = "test-fixture"\n'
)


def _donor(root: Path) -> Path:
    override = os.environ.get("RUYIJS_DONOR_OBJ", "")
    if override:
        return Path(override)
    return root.parent / "firefox-159" / "obj-necko"


def _link_tree(donor: Path) -> Path:
    layout = Path("/tmp/neqo-nss")
    if layout.exists():
        shutil.rmtree(layout)
    dist = layout / "dist"
    public = dist / "public" / "nss"
    nspr = dist / "Release" / "include" / "nspr"
    libs = dist / "Release" / "lib"
    public.parent.mkdir(parents=True)
    nspr.parent.mkdir(parents=True)
    libs.mkdir(parents=True)
    include_nss = (donor / "dist" / "include" / "nss").resolve()
    include_nspr = (donor / "dist" / "include" / "nspr").resolve()
    if not include_nss.is_dir() or not include_nspr.is_dir():
        raise SystemExit("donor NSS headers are missing")
    public.symlink_to(include_nss)
    nspr.symlink_to(include_nspr)
    bin_dir = (donor / "dist" / "bin").resolve()
    folded = bin_dir / "libnss3.dylib"
    for name in (
        "libnss3.dylib",
        "libssl3.dylib",
        "libnssutil3.dylib",
        "libnspr4.dylib",
        "libplc4.dylib",
        "libplds4.dylib",
    ):
        (libs / name).symlink_to(folded)
    for name in ("libfreebl3.dylib", "libsoftokn3.dylib"):
        source = bin_dir / name
        if source.is_file():
            (libs / name).symlink_to(source)
    (layout / "nss").mkdir()
    return layout / "nss"


def _libclang() -> str | None:
    candidates = (
        Path("/opt/homebrew/opt/llvm/lib"),
        Path("/usr/local/opt/llvm/lib"),
        Path("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib"),
    )
    for directory in candidates:
        if any(directory.glob("libclang*.dylib")):
            return str(directory)
    return None


def _vendor_config(donor: Path) -> Path:
    """Keep the crates.io vendor map and drop git source replacements.

    A git replacement against the vendor directory requires the Firefox
    workspace lock. The overlay removes those git dependencies instead.
    """
    source = donor / ".cargo" / "config.toml"
    if not source.is_file():
        raise SystemExit("donor cargo vendor config is missing")
    kept = []
    skipping = False
    for line in source.read_text(encoding="utf-8").splitlines():
        if line.startswith("["):
            skipping = "git+" in line
        if not skipping:
            kept.append(line)
    path = Path("/tmp/necko-quic-cargo-config.toml")
    path.write_bytes(("\n".join(kept) + "\n").encode("utf-8"))
    return path


def _patches(root: Path) -> str:
    text = (root / "native" / "necko_quic" / "Cargo.toml").read_text(encoding="utf-8")
    start = text.find("[patch.crates-io]")
    if start < 0:
        raise SystemExit("crate patch table is missing")
    end = text.find('\n[patch."', start + 1)
    section = text[start:] if end < 0 else text[start:end]
    firefox = (root.parent / "firefox-159").resolve().as_posix()
    return section.replace("../../../firefox-159", firefox).rstrip() + "\n"


def _strip_dev_dependencies(text: str) -> str:
    """Dev-dependencies of the server package are not in the Firefox vendor."""
    kept = []
    skipping = False
    for line in text.splitlines(keepends=True):
        if line.startswith("["):
            skipping = line.startswith("[dev-dependencies")
        if not skipping:
            kept.append(line)
    return "".join(kept)


def _materialize(src: Path, dest: Path, nss_path: Path, fixture_path: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    original = (src / "Cargo.toml").read_text(encoding="utf-8")
    if "\n[workspace]" in "\n" + original or original.startswith("[workspace]"):
        raise SystemExit(f"{src.name} Cargo.toml declares a workspace")
    rewritten, _ = _NSS_BLOCK.subn(
        "[dependencies.nss]\n"
        f'path = "{nss_path.as_posix()}"\n'
        'package = "nss-rs"\n',
        original,
    )
    rewritten, _ = _FIXTURE_BLOCK.subn(
        "[dependencies.nss-test-fixture]\n"
        f'path = "{fixture_path.as_posix()}"\n',
        rewritten,
    )
    rewritten = rewritten.replace(
        '[dependencies.nss-rs]\npath = ".."\n',
        '[dependencies.nss-rs]\n'
        f'path = "{nss_path.as_posix()}"\n',
    )
    if src.name == "neqo-bin":
        rewritten = _strip_dev_dependencies(rewritten)
        rewritten = re.sub(r"\nbench = \[\n(?:.*\n)*?\]\n", "\n", rewritten, count=1)
        kept = []
        skipping = False
        for line in rewritten.splitlines(keepends=True):
            if line.startswith("[[bench]]"):
                skipping = True
                continue
            if skipping and line.startswith("["):
                skipping = False
            if not skipping:
                kept.append(line)
        rewritten = "".join(kept)
    if 'git = "https://github.com/mozilla/nss-rs"' in rewritten:
        raise SystemExit(f"git nss dependency remains in {src.name}")
    (dest / "Cargo.toml").write_bytes(rewritten.encode("utf-8"))
    for child in src.iterdir():
        if child.name in {"Cargo.toml", "Cargo.lock", "target"}:
            continue
        (dest / child.name).symlink_to(child.resolve())


def _overlay(donor_rust: Path, nss_path: Path, overlay: Path | None = None) -> Path:
    overlay = overlay or Path("/tmp/neqo-overlay")
    if overlay.exists():
        shutil.rmtree(overlay)
    overlay.mkdir()
    fixture = overlay / "nss-test-fixture"
    (fixture / "src").mkdir(parents=True)
    (fixture / "Cargo.toml").write_bytes(
        b'[package]\nname = "nss-test-fixture"\nversion = "0.1.0"\nedition = "2021"\n'
        b'publish = false\n\n[lib]\npath = "src/lib.rs"\n'
    )
    (fixture / "src" / "lib.rs").write_bytes(
        b"use std::path::PathBuf;\n\n"
        b"pub fn db_path() -> PathBuf {\n"
        b"    PathBuf::from(\"/tmp/neqo-missing-db\")\n"
        b"}\n"
    )
    for name in OVERLAY_CRATES:
        src = donor_rust / name
        if not src.is_dir():
            raise SystemExit(f"donor crate is missing: {name}")
        _materialize(src, overlay / name, nss_path, fixture)
    return overlay


def _workspace(
    root: Path,
    nss_path: Path,
    overlay: Path | None = None,
    workspace: Path | None = None,
) -> Path:
    overlay = overlay or Path("/tmp/neqo-overlay")
    workspace = workspace or Path("/tmp/neqo-ws")
    if workspace.exists():
        shutil.rmtree(workspace)
    necko = workspace / "necko"
    necko.mkdir(parents=True)
    (necko / "src").symlink_to((root / "native" / "necko_quic" / "src").resolve())
    package = (
        "[package]\n"
        'name = "necko_quic"\n'
        'version = "0.0.0"\n'
        'edition = "2021"\n'
        "publish = false\n\n"
        "[lib]\n"
        'crate-type = ["cdylib"]\n\n'
        "[dependencies]\n"
        f'neqo-common = {{ path = "{(overlay / "neqo-common").as_posix()}" }}\n'
        f'neqo-http3 = {{ path = "{(overlay / "neqo-http3").as_posix()}" }}\n'
        f'neqo-transport = {{ path = "{(overlay / "neqo-transport").as_posix()}" }}\n'
        f'nss = {{ package = "nss-rs", path = "{nss_path.as_posix()}" }}\n'
        'serde_json = "1"\n'
        'libc = "0.2"\n'
    )
    (necko / "Cargo.toml").write_bytes(package.encode("utf-8"))
    manifest = (
        "[workspace]\n"
        'resolver = "2"\n'
        'members = ["necko"]\n\n'
        + _patches(root)
    )
    (workspace / "Cargo.toml").write_bytes(manifest.encode("utf-8"))
    return workspace


def _run(command, env, cwd) -> None:
    print(" ".join(command[:6]), flush=True)
    completed = subprocess.run(command, cwd=cwd, env=env)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)


def _server_workspace(root: Path) -> Path:
    manifest = Path("/tmp/neqo-overlay/Cargo.toml")
    excluded = ", ".join(
        f'"{name}"'
        for name in (*OVERLAY_CRATES, "nss-test-fixture")
        if name != "neqo-bin"
    )
    text = (
        "[workspace]\n"
        'resolver = "2"\n'
        'members = ["neqo-bin"]\n'
        f"exclude = [{excluded}]\n\n"
        + _patches(root)
    )
    manifest.write_bytes(text.encode("utf-8"))
    return manifest


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    donor = _donor(root)
    if not (donor / "dist" / "bin" / "libnss3.dylib").is_file():
        raise SystemExit("donor NSS library is missing")
    donor_rust = root.parent / "firefox-159" / "third_party" / "rust"
    nss_crate = (donor_rust / "nss-rs").resolve()
    if not (nss_crate / "Cargo.toml").is_file():
        raise SystemExit("vendored nss-rs crate is missing")
    config = _vendor_config(donor)
    nss_dir = _link_tree(donor)
    overlay = _overlay(donor_rust, nss_crate)
    workspace = _workspace(root, nss_crate, overlay)
    out = root / "dist" / "necko_tls"
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["CARGO_NET_OFFLINE"] = "true"
    env["CARGO_TARGET_DIR"] = "/tmp/necko-quic-target"
    env["NSS_DIR"] = str(nss_dir)
    env["NSS_PREBUILT"] = "1"
    libclang = _libclang()
    if libclang:
        env["LIBCLANG_PATH"] = libclang
    try:
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        sdk = ""
    if sdk:
        env["SDKROOT"] = sdk
    cargo = env.get("CARGO", "cargo")
    common = [cargo, "build", "--offline", "--release", "--config", str(config)]
    lib_env = dict(env)
    lib_env["RUSTFLAGS"] = (
        "-C link-arg=-Wl,-rpath,@loader_path "
        "-C link-arg=-Wl,-install_name,@rpath/libnecko_quic.dylib"
    )
    _run(
        [*common, "--manifest-path", str(workspace / "Cargo.toml"), "-p", "necko_quic"],
        lib_env,
        workspace,
    )
    built = Path("/tmp/necko-quic-target/release/libnecko_quic.dylib")
    if not built.is_file():
        raise SystemExit("libnecko_quic.dylib was not produced")
    shutil.copy2(built, out / "libnecko_quic.dylib")
    server_env = dict(env)
    server_env["RUSTFLAGS"] = "-C link-arg=-Wl,-rpath,@loader_path"
    server_manifest = _server_workspace(root)
    _run(
        [
            *common,
            "--manifest-path",
            str(server_manifest),
            "-p",
            "neqo-bin",
            "--bin",
            "neqo-server",
        ],
        server_env,
        server_manifest.parent,
    )
    server = Path("/tmp/necko-quic-target/release/neqo-server")
    if not server.is_file():
        raise SystemExit("neqo-server was not produced")
    shutil.copy2(server, out / "neqo-server")
    os.chmod(out / "neqo-server", 0o755)
    bin_dir = (donor / "dist" / "bin").resolve()
    for name in ("libnss3.dylib", "libmozglue.dylib", "libfreebl3.dylib", "libsoftokn3.dylib"):
        link = out / name
        if link.is_symlink() or link.exists():
            continue
        target = bin_dir / name
        if target.is_file():
            link.symlink_to(target)
    print("necko_quic=ok", flush=True)


def _copy_tree(src: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest, ignore=shutil.ignore_patterns("target", ".git"))


def _replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"patch anchor count {count} in {path.name}")
    path.write_bytes(text.replace(old, new, 1).encode("utf-8"))


def _unwrap_src(crate: Path) -> None:
    src = crate / "src"
    # Linux overlays already own their source directory.
    if src.is_dir() and not src.is_symlink():
        return
    if not src.is_symlink():
        raise SystemExit(f"{crate.name} src is not a symlink")
    target = src.resolve()
    src.unlink()
    shutil.copytree(target, src)


def _patch_nss(nss_copy: Path) -> None:
    agent = nss_copy / "src" / "agent.rs"
    _replace_once(
        agent,
        "/// The maximum number of tickets to remember for a given connection.\n",
        "unsafe extern \"C\" {\n"
        "    fn SSL_SignatureSchemePrefSet(\n"
        "        fd: *mut prio::PRFileDesc,\n"
        "        schemes: *const u32,\n"
        "        count: ::std::os::raw::c_uint,\n"
        "    ) -> crate::SECStatus;\n"
        "}\n\n"
        "/// The maximum number of tickets to remember for a given connection.\n",
    )
    _replace_once(
        agent,
        "    pub fn send_additional_key_shares(&mut self, count: usize) -> Res<()> {\n"
        "        secstatus_to_res(unsafe {\n"
        "            ssl::SSL_SendAdditionalKeyShares(self.fd, c_uint::try_from(count)?)\n"
        "        })\n"
        "    }\n",
        "    pub fn send_additional_key_shares(&mut self, count: usize) -> Res<()> {\n"
        "        secstatus_to_res(unsafe {\n"
        "            ssl::SSL_SendAdditionalKeyShares(self.fd, c_uint::try_from(count)?)\n"
        "        })\n"
        "    }\n"
        "\n"
        "    /// Set the signature schemes offered in the client hello.\n"
        "    ///\n"
        "    /// # Errors\n"
        "    ///\n"
        "    /// When NSS rejects the list or the count does not fit.\n"
        "    pub fn set_signature_schemes(&mut self, schemes: &[u16]) -> Res<()> {\n"
        "        let values: Vec<u32> = schemes.iter().copied().map(u32::from).collect();\n"
        "        secstatus_to_res(unsafe {\n"
        "            SSL_SignatureSchemePrefSet(\n"
        "                self.fd,\n"
        "                values.as_ptr(),\n"
        "                c_uint::try_from(values.len())?,\n"
        "            )\n"
        "        })\n"
        "    }\n",
    )


def _patch_transport(crate: Path) -> None:
    _unwrap_src(crate)
    source = crate / "src"
    (source / "hello_adjust.rs").write_bytes(
        b"use std::cell::Cell;\n"
        b"\n"
        b"thread_local! {\n"
        b"    static STYLE: Cell<u8> = const { Cell::new(0) };\n"
        b"    static BONUS: Cell<usize> = const { Cell::new(0) };\n"
        b"}\n"
        b"\n"
        b"pub fn set_client_hello_adjustments(style: u8, mtu_bonus: usize) {\n"
        b"    STYLE.with(|cell| cell.set(style));\n"
        b"    BONUS.with(|cell| cell.set(mtu_bonus));\n"
        b"}\n"
        b"\n"
        b"pub fn style() -> u8 {\n"
        b"    STYLE.with(Cell::get)\n"
        b"}\n"
        b"\n"
        b"pub fn mtu_bonus() -> usize {\n"
        b"    BONUS.with(Cell::get)\n"
        b"}\n"
    )
    _replace_once(
        source / "lib.rs",
        "pub mod tparams;\n",
        "pub mod tparams;\nmod hello_adjust;\n",
    )
    _replace_once(
        source / "lib.rs",
        "    version::Version,\n};\n#[cfg(feature = \"bench\")]\n",
        "    version::Version,\n};\n"
        "pub use hello_adjust::set_client_hello_adjustments;\n"
        "#[cfg(feature = \"bench\")]\n",
    )
    _replace_once(
        source / "connection" / "mod.rs",
        "        self.crypto.tls_mut().send_additional_key_shares(count)?;\n"
        "        Ok(())\n"
        "    }\n",
        "        self.crypto.tls_mut().send_additional_key_shares(count)?;\n"
        "        Ok(())\n"
        "    }\n"
        "\n"
        "    /// Set signature schemes while the handshake has not started.\n"
        "    ///\n"
        "    /// # Errors\n"
        "    ///\n"
        "    /// When the connection is no longer in `State::Init`, or NSS rejects the list.\n"
        "    pub fn set_signature_schemes(&mut self, schemes: &[u16]) -> Res<()> {\n"
        "        if self.state != State::Init {\n"
        "            qerror!(\n"
        "                \"[{self}] Cannot set signature schemes in state {:?}\",\n"
        "                self.state\n"
        "            );\n"
        "            return Err(Error::ConnectionState);\n"
        "        }\n"
        "        self.crypto.tls_mut().set_signature_schemes(schemes)?;\n"
        "        Ok(())\n"
        "    }\n",
    )
    _replace_once(
        source / "tparams.rs",
        "    fn encode_filtered<F, B: Buffer>(&self, f: F, enc: &mut Encoder<B>)\n"
        "    where\n"
        "        F: Fn(TransportParameterId, Option<&TransportParameter>) -> bool,\n"
        "    {\n"
        "        for (i, v) in self.params.iter().filter(|(i, v)| f(*i, v.as_ref())) {\n"
        "            if let Some(tp) = v {\n"
        "                tp.encode(enc, i);\n"
        "            }\n"
        "        }\n"
        "    }\n",
        "    fn encode_filtered<F, B: Buffer>(&self, f: F, enc: &mut Encoder<B>)\n"
        "    where\n"
        "        F: Fn(TransportParameterId, Option<&TransportParameter>) -> bool,\n"
        "    {\n"
        "        let style = crate::hello_adjust::style();\n"
        "        let mut rows: Vec<(TransportParameterId, &TransportParameter)> = self\n"
        "            .params\n"
        "            .iter()\n"
        "            .filter(|(i, v)| f(*i, v.as_ref()))\n"
        "            .filter_map(|(i, v)| v.as_ref().map(|tp| (i, tp)))\n"
        "            .filter(|(i, _)| style != 3 || *i != TransportParameterId::VersionInformation)\n"
        "            .collect();\n"
        "        if matches!(style, 1 | 2 | 3) && rows.len() > 1 {\n"
        "            for index in (1..rows.len()).rev() {\n"
        "                let sample = random::<4>();\n"
        "                let pick = usize::try_from(u32::from_be_bytes(sample)).unwrap_or(0)\n"
        "                    % (index + 1);\n"
        "                rows.swap(index, pick);\n"
        "            }\n"
        "        }\n"
        "        for (i, tp) in rows {\n"
        "            if style == 2 && i == TransportParameterId::VersionInformation {\n"
        "                encode_draft_version_information(tp, enc);\n"
        "            } else {\n"
        "                tp.encode(enc, i);\n"
        "            }\n"
        "        }\n"
        "    }\n",
    )
    _replace_once(
        source / "tparams.rs",
        "impl TransportParameters {\n",
        "fn encode_draft_version_information<B: Buffer>(\n"
        "    tp: &TransportParameter,\n"
        "    enc: &mut Encoder<B>,\n"
        ") {\n"
        "    let TransportParameter::Versions { current, other } = tp else {\n"
        "        tp.encode(enc, TransportParameterId::VersionInformation);\n"
        "        return;\n"
        "    };\n"
        "    enc.encode_varint(0xff73db_u64);\n"
        "    enc.encode_vvec_with(|enc_inner| {\n"
        "        enc_inner.encode_uint(4, *current);\n"
        "        for version in other {\n"
        "            enc_inner.encode_uint(4, *version);\n"
        "        }\n"
        "    });\n"
        "}\n"
        "\n"
        "impl TransportParameters {\n",
    )
    _replace_once(
        source / "connection" / "mod.rs",
        "        if encoder.len() >= profile.limit() {\n"
        "            return;\n"
        "        }\n"
        "\n"
        "        qdebug!(\n"
        "            \"[{self}] pad Initial from {} to {}\",\n"
        "            encoder.len(),\n"
        "            profile.limit()\n"
        "        );\n"
        "        let pad_amount = profile.limit() - encoder.len();\n"
        "        initial.track_padding(pad_amount);\n",
        "        let target = if self.conn_params.scone_enabled() {\n"
        "            profile.limit()\n"
        "        } else {\n"
        "            profile\n"
        "                .limit()\n"
        "                .saturating_add(crate::hello_adjust::mtu_bonus())\n"
        "        };\n"
        "        if encoder.len() >= target {\n"
        "            return;\n"
        "        }\n"
        "\n"
        "        qdebug!(\n"
        "            \"[{self}] pad Initial from {} to {}\",\n"
        "            encoder.len(),\n"
        "            target\n"
        "        );\n"
        "        let pad_amount = target - encoder.len();\n"
        "        initial.track_padding(pad_amount);\n",
    )
    _replace_once(
        source / "connection" / "mod.rs",
        "        } else {\n"
        "            encoder.pad_to(profile.limit(), 0);\n"
        "        }\n",
        "        } else {\n"
        "            encoder.pad_to(target, 0);\n"
        "        }\n",
    )


def _build_zstd(root: Path) -> Path:
    lib = root.parent / "firefox-159" / "third_party" / "zstd" / "lib"
    out = Path("/tmp/neqo-hello-zstd")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir()
    names = (
        ("common", "debug.c"),
        ("common", "entropy_common.c"),
        ("common", "error_private.c"),
        ("common", "fse_decompress.c"),
        ("common", "xxhash.c"),
        ("common", "zstd_common.c"),
        ("decompress", "huf_decompress.c"),
        ("decompress", "zstd_ddict.c"),
        ("decompress", "zstd_decompress.c"),
        ("decompress", "zstd_decompress_block.c"),
    )
    objects = []
    for folder, name in names:
        source = lib / folder / name
        if not source.is_file():
            raise SystemExit(f"zstd source is missing: {name}")
        obj = out / f"{source.stem}.o"
        command = [
            "xcrun",
            "--sdk",
            "macosx",
            "clang",
            "-c",
            "-O2",
            "-DZSTD_DISABLE_ASM",
            "-I",
            str(lib),
            "-I",
            str(lib / "common"),
            "-I",
            str(lib / "decompress"),
            "-o",
            str(obj),
            str(source),
        ]
        _run(command, os.environ.copy(), out)
        objects.append(obj)
    archive = out / "libnecko_zstd.a"
    _run(
        ["xcrun", "--sdk", "macosx", "ar", "rcs", str(archive), *[str(item) for item in objects]],
        os.environ.copy(),
        out,
    )
    if not archive.is_file():
        raise SystemExit("libnecko_zstd.a was not produced")
    return archive


def build_library() -> None:
    root = Path(__file__).resolve().parents[1]
    donor = _donor(root)
    if not (donor / "dist" / "bin" / "libnss3.dylib").is_file():
        raise SystemExit("donor NSS library is missing")
    donor_rust = root.parent / "firefox-159" / "third_party" / "rust"
    nss_crate = (donor_rust / "nss-rs").resolve()
    if not (nss_crate / "Cargo.toml").is_file():
        raise SystemExit("vendored nss-rs crate is missing")
    nss_copy = Path("/tmp/neqo-hello-nss")
    _copy_tree(nss_crate, nss_copy)
    _patch_nss(nss_copy)
    config = _vendor_config(donor)
    nss_dir = _link_tree(donor)
    overlay = _overlay(donor_rust, nss_copy, Path("/tmp/neqo-hello-overlay"))
    _patch_transport(overlay / "neqo-transport")
    workspace = _workspace(root, nss_copy, overlay, Path("/tmp/neqo-hello-ws"))
    zstd = _build_zstd(root)
    out = root / "dist" / "necko_tls"
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in list(env):
        if key.startswith("DYLD_"):
            env.pop(key, None)
    env["CARGO_NET_OFFLINE"] = "true"
    env["CARGO_TARGET_DIR"] = "/tmp/necko-quic-target"
    env["NSS_DIR"] = str(nss_dir)
    env["NSS_PREBUILT"] = "1"
    libclang = _libclang()
    if libclang:
        env["LIBCLANG_PATH"] = libclang
    try:
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        sdk = ""
    if sdk:
        env["SDKROOT"] = sdk
    cargo = env.get("CARGO", "cargo")
    env["RUSTFLAGS"] = (
        "-C link-arg=-Wl,-rpath,@loader_path "
        "-C link-arg=-Wl,-install_name,@rpath/libnecko_quic.dylib "
        "-C link-arg=-lcompression "
        f"-C link-arg={zstd.as_posix()}"
    )
    _run(
        [
            cargo,
            "build",
            "--offline",
            "--release",
            "--config",
            str(config),
            "--manifest-path",
            str(workspace / "Cargo.toml"),
            "-p",
            "necko_quic",
        ],
        env,
        workspace,
    )
    built = Path("/tmp/necko-quic-target/release/libnecko_quic.dylib")
    if not built.is_file():
        raise SystemExit("libnecko_quic.dylib was not produced")
    shutil.copy2(built, out / "libnecko_quic.dylib")
    print("necko_quic_library=ok", flush=True)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "library":
        build_library()
    else:
        main()

