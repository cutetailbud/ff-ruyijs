"""URLPattern native dependency: the Firefox-vendored urlpattern 0.6.0 crate and
its resolved dependency set, built offline with the donor rustc/cargo behind the
ruyijs C ABI glue in vendor/urlpattern-native (docs/owners/WEB_PLATFORM.md).

The donor third_party/rust tree stays read-only. Firefox patches some vendored
crates without updating cargo's per-file checksums, so the exact packages named
by the frozen Cargo.lock are mirrored into the build directory with only their
package checksum kept. Only target rlibs are archived: the donor jsrust library
already provides the one Rust std and global allocator.
"""
import hashlib
import json
import shutil
import subprocess
import tomllib
from pathlib import Path

def target_triple(native):
    return native.donor.rust_target


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tree_digest(root):
    h = hashlib.sha256()
    for path in sorted(p for p in Path(root).rglob('*') if p.is_file()):
        h.update(str(path.relative_to(root)).replace('\\', '/').encode())
        h.update(digest(path).encode())
    return h.hexdigest()


def packages(lock_path):
    lock = tomllib.loads(Path(lock_path).read_text('utf8'))
    return sorted((p['name'], p['version']) for p in lock['package'] if 'source' in p)


def donor_index(rust):
    index = {}
    for directory in rust.iterdir():
        manifest = directory / 'Cargo.toml'
        if not manifest.is_file():
            continue
        try:
            package = tomllib.loads(manifest.read_text('utf8'))['package']
        except (tomllib.TOMLDecodeError, KeyError, UnicodeDecodeError):
            continue
        index[(package['name'], package['version'])] = directory
    return index


def build(native):
    source = native.root / 'vendor/urlpattern-native'
    directory = native.directory / 'urlpattern-native'
    directory.mkdir(parents=True, exist_ok=True)
    rustc = native.donor.substs['RUSTC']
    cargo = native.donor.substs['CARGO']
    rustc = rustc[0] if isinstance(rustc, list) else rustc
    cargo = cargo[0] if isinstance(cargo, list) else cargo
    for tool in (rustc, cargo):
        if not Path(tool).is_file():
            raise RuntimeError('The configured donor Rust tool is required: ' + tool)
    librarian, lib_prefix = native.librarian()
    wanted = packages(source / 'Cargo.lock')
    index = donor_index(native.donor.src / 'third_party/rust')
    missing = [key for key in wanted if key not in index]
    if missing:
        raise RuntimeError('Donor third_party/rust lacks locked packages: ' + repr(missing))
    identity = {
        'glue': tree_digest(source / 'src'), 'manifest': digest(source / 'Cargo.toml'),
        'lock': digest(source / 'Cargo.lock'), 'builder': digest(__file__),
        'packages': {name + '@' + version: json.loads(
            (index[(name, version)] / '.cargo-checksum.json').read_text('utf8')).get('package')
            for name, version in wanted},
        'rustcSHA256': digest(rustc), 'cargoSHA256': digest(cargo),
        'librarianSHA256': digest(librarian), 'target': target_triple(native)}
    signature = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    library = native.static_library(directory, 'ruyijs_urlpattern')
    stamp = directory / 'build.json'
    if stamp.is_file():
        old = json.loads(stamp.read_text('utf8'))
        if not native.incremental['disabled'] and old.get('signature') == signature and \
                library.is_file() and digest(library) == old.get('output'):
            return [library]
    mirror = directory / 'donor-mirror'
    if mirror.exists():
        shutil.rmtree(mirror)
    mirror.mkdir()
    for key in wanted:
        origin = index[key]
        shutil.copytree(origin, mirror / origin.name)
        checksum = json.loads((origin / '.cargo-checksum.json').read_text('utf8'))
        (mirror / origin.name / '.cargo-checksum.json').write_text(
            json.dumps({'files': {}, 'package': checksum.get('package')}), 'utf8')
    env = native.env.copy()
    env['RUSTC'] = rustc
    env['CARGO_HOME'] = str(directory / 'cargo-home')
    target_dir = directory / 'target'
    # Every rlib in deps is archived, so rlibs left by an earlier rustc or
    # donor would be linked beside the new ones; start from an empty target.
    if target_dir.exists():
        shutil.rmtree(target_dir)
    command = [cargo, 'build', '--release', '--offline', '--locked', '--target', target_triple(native),
               '--manifest-path', str(source / 'Cargo.toml'), '--target-dir', str(target_dir),
               '--config', "source.crates-io.replace-with='donor'",
               '--config', "source.donor.directory='" + mirror.as_posix() + "'"]
    completed = subprocess.run(command, cwd=directory, env=env, capture_output=True, text=True)
    (directory / 'cargo.log').write_text(completed.stdout + completed.stderr, 'utf8')
    if completed.returncode:
        raise RuntimeError('urlpattern cargo build: ' + (completed.stdout + completed.stderr)[-10000:])
    rlibs = sorted(str(p) for p in (target_dir / target_triple(native) / 'release/deps').glob('*.rlib'))
    if library.exists():
        library.unlink()
    archived = subprocess.run(native.archive(library, rlibs), cwd=directory, env=env,
                              capture_output=True, text=True)
    (directory / 'archive.log').write_text(archived.stdout + archived.stderr, 'utf8')
    if archived.returncode:
        raise RuntimeError('urlpattern archive: ' + (archived.stdout + archived.stderr)[-10000:])
    if not library.is_file():
        cargo_log = (directory / 'cargo.log').read_text('utf8', errors='replace')
        raise RuntimeError(f'urlpattern archive wrote no {library.name} from {len(rlibs)} rlibs '
                           f'in {target_dir / target_triple(native)}: '
                           + (archived.stdout + archived.stderr)[-3000:] + '\ncargo: ' + cargo_log[-6000:])
    stamp.write_text(json.dumps({'signature': signature, 'identity': identity, 'command': command,
                                 'rlibs': [Path(p).name for p in rlibs], 'output': digest(library)},
                                indent=2) + '\n', 'utf8')
    return [library]
