"""Rust std/global allocator for ruyijs Rust rlibs when donor jsrust keeps them private.

The zlib and URLPattern rlibs carry no std of their own; they resolve std,
alloc and the allocator shim against whatever the link provides. Up to Firefox
158 the donor jsrust.lib exported that runtime. Firefox 159 builds jsrust with
-Clto, which internalizes std, so the shell would link with every std symbol
undefined. In that case one empty staticlib built by the donor rustc supplies
exactly that runtime once; without donor LTO nothing is added, so jsrust stays
the only provider and no symbol is defined twice.

LTO still leaves a few std symbols global in jsrust (rust_eh_personality,
std::panicking::EMPTY_PANIC). The std object of this staticlib defines the same
ones, so they are renamed to private names there: the std copy keeps using its
own, and every other reference resolves to the identical jsrust definition.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import shutil


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def donor_lto(donor):
    spec = donor.obj / 'js/src/rust/.cargo-library-spec.json'
    if not spec.is_file():
        return False
    return bool(json.loads(spec.read_text('utf8')).get('edge', {}).get('lto'))


def strong_globals(nm, path):
    listed = subprocess.run([nm, '--defined-only', '-g', str(path)], capture_output=True, text=True).stdout
    # Constant-pool COMDATs (__xmm@, __real@, ??_C@) merge at link time and never collide.
    return {fields[2] for fields in (line.split() for line in listed.splitlines())
            if len(fields) == 3 and fields[1] in 'TDRB' and not fields[2].startswith(
                ('.weak.', '__imp_', '__xmm@', '__real@', '??_C@'))}


def privatize(native, library, directory, jsrust):
    """Rename std globals that donor jsrust also defines; return the renames."""
    tools = Path(native.librarian()[0]).parent
    if native.donor.os_arch == "Linux":
        return privatize_linux(library, directory, jsrust)
    nm, ar, objcopy = (str(tools / name) for name in ('llvm-nm.exe', 'llvm-ar.exe', 'llvm-objcopy.exe'))
    members = subprocess.run([ar, 't', str(library)], capture_output=True, text=True, check=True).stdout.split()
    std = [m for m in members if m.startswith('std-') and m.endswith('.o')]
    if len(std) != 1:
        raise ValueError('rust runtime must hold exactly one std object')
    subprocess.run([ar, 'x', str(library), std[0]], cwd=directory, check=True)
    obj = directory / std[0]
    shared = sorted(strong_globals(nm, obj) & strong_globals(nm, jsrust))
    renames = {name: 'ruyijs_private_' + hashlib.sha256(name.encode()).hexdigest()[:16] for name in shared}
    if renames:
        subprocess.run([objcopy, *[f'--redefine-sym={a}={b}' for a, b in renames.items()], str(obj)], check=True)
        subprocess.run([ar, 'r', str(library), std[0]], cwd=directory, check=True)
    obj.unlink()
    return renames


def privatize_linux(library, directory, jsrust):
    nm, ar, objcopy = (shutil.which(name) for name in ("nm", "ar", "objcopy"))
    members = subprocess.run([ar, "t", str(library)], capture_output=True, text=True, check=True).stdout.split()
    objects = [name for name in members if name.startswith("std-") and name.endswith(".o")]
    if not objects:
        raise ValueError("Rust runtime has no std objects")
    existing = strong_globals(nm, jsrust)
    renames = {}
    for member in objects:
        subprocess.run([ar, "x", str(library), member], cwd=directory, check=True)
        obj = directory / member
        names = {name: "ruyijs_private_" + hashlib.sha256(name.encode()).hexdigest()[:16]
                 for name in sorted(strong_globals(nm, obj) & existing)}
        if names:
            subprocess.run([objcopy, *[f"--redefine-sym={a}={b}" for a, b in names.items()], str(obj)], check=True)
            subprocess.run([ar, "r", str(library), member], cwd=directory, check=True)
            renames.update(names)
        obj.unlink()
    return renames


def build(native):
    if not donor_lto(native.donor):
        return []
    directory = native.directory / 'rust-runtime'
    directory.mkdir(parents=True, exist_ok=True)
    rustc = native.donor.substs['RUSTC']
    if isinstance(rustc, list):
        rustc = rustc[0]
    if not Path(rustc).is_file():
        raise RuntimeError('The configured donor rustc is required: ' + rustc)
    jsrust = [p for p in native.donor.libraries() if p.name == ('jsrust.lib' if native.donor.windows else 'libjsrust.a')]
    if len(jsrust) != 1:
        raise ValueError('donor must link exactly one jsrust.lib')
    version = subprocess.run([rustc, '--version'], capture_output=True, text=True, check=True).stdout
    # Same codegen as the rlibs that depend on it (compression_native/urlpattern).
    command = [rustc, '--edition=2021', '--target=' + native.donor.rust_target, '--crate-type=staticlib',
               '--crate-name=ruyijs_rust_runtime', '-C', 'opt-level=2', '-C', 'panic=abort',
               '-C', 'target-feature=-crt-static', '-C', 'codegen-units=1']
    identity = {'builder': digest(__file__), 'rustc': version, 'rustcSHA256': digest(rustc),
                'jsrustSHA256': digest(jsrust[0]), 'command': command[1:]}
    signature = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    library = directory / ('ruyijs_rust_runtime.lib' if native.donor.windows else 'libruyijs_rust_runtime.a')
    stamp = directory / 'build.json'
    if stamp.is_file():
        old = json.loads(stamp.read_text('utf8'))
        if not native.incremental['disabled'] and old.get('signature') == signature and \
                library.is_file() and digest(library) == old.get('output'):
            return [library]
    source = directory / 'lib.rs'
    source.write_text('// Empty crate: the staticlib bundles std, alloc and the allocator shim.\n', 'utf8')
    command = [*command, str(source), '-o', str(library)]
    completed = subprocess.run(command, cwd=directory, env=native.env, capture_output=True, text=True)
    (directory / 'rustc.log').write_text(completed.stdout + completed.stderr, 'utf8')
    if completed.returncode:
        raise RuntimeError('rust runtime: ' + (completed.stdout + completed.stderr)[-10000:])
    renames = privatize(native, library, directory, jsrust[0])
    stamp.write_text(json.dumps({'signature': signature, 'identity': identity, 'command': command,
                                 'renamed': renames, 'output': digest(library)}, indent=2) + '\n', 'utf8')
    return [library]
