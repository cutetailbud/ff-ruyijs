"""Bundle the built ELF runtime and a relocatable, isolated NSS/Neqo helper.

This is a host-glibc Linux bundle, not a manylinux build. All other ELF dependency
paths are made relative. Never copy the build venv's absolute .pth or symlinks.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from packaging.requirements import Requirement
from packaging.utils import canonicalize_name
DEPENDENCIES = ["requests", "brotli", "zstandard", "h2", "cryptography", "publicsuffixlist"]

GLIBC = re.compile(r'^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl|libnss_\w+)\.so[.\d]*$')


def command(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def ignore(directory, names):
    return [name for name in names if name in ('__pycache__', 'test', 'tests', 'idlelib', 'tkinter', 'ensurepip')
            or name.endswith(('.pyc', '.pyo'))]


def helper(project, target):
    python = project / 'vendor/necko-derived/python/bin/python3'
    if not (python.parent.parent / 'backend.json').is_file():
        raise FileNotFoundError('Build the native NSS/Neqo helper before bundling Linux')
    inventory = json.loads(command(python, '-I', '-c', '''
import importlib.metadata as m,json,sys,sysconfig
print(json.dumps(dict(executable=sys.executable,stdlib=sysconfig.get_path('stdlib'),
 distributions=[dict(name=d.metadata['Name'],requires=d.requires or [],
 files=[str(d.locate_file(p)) for p in d.files or []]) for d in m.distributions()])))
'''))
    runtime = target / 'vendor/necko-derived/python'
    (runtime / 'bin').mkdir(parents=True)
    shutil.copy2(Path(inventory['executable']).resolve(), runtime / 'bin/python3')
    stdlib = runtime / 'lib' / Path(inventory['stdlib']).name
    shutil.copytree(inventory['stdlib'], stdlib, ignore=ignore)
    site = stdlib / 'site-packages'
    site.mkdir(exist_ok=True)
    packages = {canonicalize_name(d['name']): d for d in inventory['distributions']}
    pending = [Requirement(item).name for item in DEPENDENCIES]
    if 'backports-zstd' in packages:
        pending.append('backports.zstd')
    base = python.parent.parent / 'lib' / stdlib.name / 'site-packages'
    selected = set()
    while pending:
        name = canonicalize_name(pending.pop())
        if name in selected:
            continue
        selected.add(name)
        package = packages[name]
        for value in package['requires']:
            req = Requirement(value)
            if req.marker is None or req.marker.evaluate({'extra': ''}):
                pending.append(req.name)
        for value in package['files']:
            source = Path(value)
            # Distribution metadata paths can point outside site-packages (bin/).
            try:
                relative = source.relative_to(base)
            except ValueError:
                continue
            if '..' in relative.parts or '__pycache__' in relative.parts or source.suffix == '.pyc' or not source.is_file():
                continue
            destination = site / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    for package in ('necko', 'ruyijs_necko', 'necko_win'):
        shutil.copytree(base / package, site / package, ignore=ignore)
    (runtime / 'bin/python3._pth').write_text(
        f'../lib/{stdlib.name}\n../lib/{stdlib.name}/lib-dynload\n../lib/{stdlib.name}/site-packages\nimport site\n')
    shutil.copy2(python.parent.parent / 'backend.json', runtime / 'backend.json')
    licenses = target / 'licenses'
    licenses.mkdir(exist_ok=True)
    for path in Path('/usr/share/doc').glob('python3.*/copyright'):
        shutil.copy2(path, licenses / (path.parent.name + '-copyright'))



def libraries(target):
    patchelf = shutil.which('patchelf')
    if not patchelf:
        raise RuntimeError('Install patchelf before bundling Linux')
    binaries = [p for p in target.rglob('*') if p.is_file() and not p.is_symlink() and
                ('.so' in p.name or p.name in ('js', 'python3'))]
    lib = target / 'lib'
    lib.mkdir(exist_ok=True)
    for binary in binaries:
        relative = os.path.relpath(lib, binary.parent)
        command(patchelf, '--set-rpath', '$ORIGIN:$ORIGIN/' + relative, binary)
    bundled = []
    for binary in binaries:
        deps = command('ldd', binary)
        if 'not found' in deps:
            raise RuntimeError(f'Unresolved dependency of {binary}: {deps}')
        for line in deps.splitlines():
            match = re.search(r'^\s*(\S+) => (/\S+)', line)
            if not match or GLIBC.match(match[1]):
                continue
            name, path = match.groups()
            source = Path(path)
            if source.is_relative_to(target) or (lib / name).exists():
                continue
            shutil.copy2(source.resolve(), lib / name)
            bundled.append(name)
    for binary in lib.iterdir():
        relative = os.path.relpath(lib, binary.parent)
        command(patchelf, '--set-rpath', '$ORIGIN:$ORIGIN/' + relative, binary)
    licenses = target / 'licenses/system'
    licenses.mkdir(parents=True, exist_ok=True)
    for name in bundled:
        try:
            owner = command('dpkg-query', '-S', '*/' + name).splitlines()[0].split(': ')[0]
            package = owner.split(':')[0]
            source = Path('/usr/share/doc') / package / 'copyright'
            if source.is_file():
                shutil.copy2(source, licenses / (package + '-copyright'))
        except subprocess.CalledProcessError:
            pass
    return bundled


def bundle(project, target):
    project, target = project.resolve(), target.resolve()
    if project.is_relative_to(target):
        raise ValueError("Output must not contain the source checkout")
    if target.exists():
        shutil.rmtree(target)
    (target / 'dist').mkdir(parents=True)
    for name in ('js', 'libruyijs_client.so'):
        shutil.copy2(project / 'dist' / name, target / 'dist' / name)
        command('strip', '--strip-debug', target / 'dist' / name)
    shutil.copy2(project / 'src/client/ruyijs.h', target / 'dist/ruyijs.h')
    for name in ('LICENSE', 'NOTICE', 'COMMERCIAL-LICENSE.md'):
        shutil.copy2(project / name, target / name)
    shutil.copytree(project / 'dist/necko_tls', target / 'dist/necko_tls')
    helper(project, target)
    shutil.copytree(project / 'dist/licenses', target / 'licenses', dirs_exist_ok=True)
    bundled = libraries(target)
    glibc = command('getconf', 'GNU_LIBC_VERSION')
    has_quic = (target / 'dist/necko_tls/libnecko_quic.so').is_file()
    graphics = 'system Skia' if any(name.startswith('libskia.') for name in bundled) else 'Firefox 159 Skia 149 raster'
    metadata = dict(platform='linux_x86_64', engine='SpiderMonkey 159', transport='shared-posix-necko+nss',
                    http=['http/1.1', 'h2'] + (['h3', 'WebTransport'] if has_quic else []), webcrypto='OpenSSL',
                    graphics=graphics, build_glibc=glibc, libraries=sorted(bundled))
    for package in ('libstb-dev', 'libharfbuzz-dev', 'libfreetype-dev', 'libfontconfig-dev'):
        path = Path('/usr/share/doc') / package / 'copyright'
        if path.is_file():
            shutil.copy2(path, target / 'licenses' / (package + '-copyright'))
    metadata['sha256'] = {str(p.relative_to(target)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in target.rglob('*') if p.is_file()}
    (target / 'runtime.json').write_text(json.dumps(metadata, indent=2) + '\n')
    command(target / 'vendor/necko-derived/python/bin/python3', '-I', '-B', '-c',
            'import ruyijs_necko.oneshot; from necko.core_client import CoreClient;'
            'c=CoreClient(); assert c.available; c.close()')
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(bundle(Path(__file__).resolve().parents[1], args.output))
