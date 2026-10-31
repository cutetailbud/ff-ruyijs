"""Build a platform wheel containing the unchanged standalone runtime."""
from pathlib import Path
import platform
import shutil
import sys

from setuptools import Distribution, setup
from setuptools.command.bdist_wheel import bdist_wheel
from setuptools.command.build_py import build_py

sys.path.insert(0, str(Path(__file__).resolve().parent / "tools"))
from wheel_runtime import obtain_runtime, validate, digest

# Captured third-party responses and cookies used only for offline replay stay out of the wheel.
EXAMPLES_EXCLUDED = shutil.ignore_patterns("__pycache__", "*.pyc", "fixture", "interstitial_fixture",
                                           "replay.py", "interstitial.py", "interstitial_solve.py",
                                           "*_result.json")


class PlatformDistribution(Distribution):
    def has_ext_modules(self):
        return True


class BuildPython(build_py):
    def get_outputs(self, include_bytecode=True):
        outputs = list(super().get_outputs(include_bytecode=include_bytecode))
        for relative in ("ruyijs/examples", "ruyijs_necko", "necko"):
            folder = Path(self.build_lib) / relative
            if folder.exists():
                outputs.extend(str(path) for path in folder.rglob("*") if path.is_file())
        return sorted(set(outputs))

    def run(self):
        tag = _platform_tag()
        super().run()
        project = Path(__file__).resolve().parent
        target = Path(self.build_lib) / "ruyijs/_runtime"
        if tag == "win_amd64":
            runtime = obtain_runtime(project)
            if target.exists():
                validate(target)
                if digest(target / "SHA256SUMS") != digest(runtime / "SHA256SUMS"):
                    raise ValueError(f"Build directory contains another runtime version: {target}")
            else:
                shutil.copytree(runtime, target)
        elif tag == "linux_x86_64":
            from bundle_linux import bundle
            bundle(project, target)
        else:
            runtime = project / "dist"
            if not (runtime / "js").is_file() or not (runtime / "libmozglue.dylib").is_file():
                raise FileNotFoundError(
                    "macOS arm64 wheel needs dist/js and dist/libmozglue.dylib "
                    "from an aarch64-apple-darwin Firefox donor")
            for name in ("js", "libmozglue.dylib", "libruyijs_client.dylib"):
                if not (runtime / name).is_file():
                    raise FileNotFoundError(f"macOS arm64 wheel needs dist/{name}")
            if target.exists():
                shutil.rmtree(target)
            target.mkdir(parents=True)
            shell_dest = target / "dist"
            shell_dest.mkdir(parents=True)
            for name in ("js", "libmozglue.dylib", "libruyijs_client.dylib"):
                shutil.copy2(runtime / name, shell_dest / name)
            from macos_runtime import donor_bin, stage_necko
            stage_necko(target / "dist" / "necko", donor_bin(project),
                        project / "src/resource_network/necko_fetch.js")
            _install_darwin_helper(Path(self.build_lib), project)
        examples = Path(self.build_lib) / "ruyijs/examples"
        if examples.exists():
            shutil.rmtree(examples)
        shutil.copytree(Path(__file__).resolve().parent / "examples", examples, ignore=EXAMPLES_EXCLUDED)
        registry = Path(__file__).resolve().parent / "python/ruyijs/examples"
        shutil.copytree(registry, examples, dirs_exist_ok=True, ignore=EXAMPLES_EXCLUDED)
        readme = examples / "DataDome/README.md"
        if readme.is_file():
            readme.write_text(
                "> The installed package includes only the live `solve.py` "
                "(`python -m ruyijs.examples datadome`).\n"
                "> Offline replay and device-check scripts and their captured data are not distributed.\n\n"
                + readme.read_text(encoding="utf-8"), encoding="utf-8")



def _install_darwin_helper(build_lib, project):
    source = project / "src/resource_network"
    mapping = {
        build_lib / "ruyijs_necko/__init__.py": source / "necko_ruyi_init.py",
        build_lib / "ruyijs_necko/timing.py": source / "necko_timing.py",
        build_lib / "ruyijs_necko/oneshot.py": source / "necko_oneshot.py",
        build_lib / "necko/client.py": source / "necko_client.py",
        build_lib / "necko/adapters.py": source / "necko_adapters.py",
        build_lib / "necko/__init__.py": source / "necko_pkg_init.py",
        build_lib / "necko/necko_fetch.js": source / "necko_fetch.js",
        build_lib / "ruyijs_necko/webtransport_worker.py": source / "webtransport_worker.py",
        build_lib / "ruyijs_necko/webtransport_wire.py": source / "webtransport_wire.py",
        build_lib / "ruyijs_necko/webtransport_verify.py": source / "webtransport_verify.py",
        build_lib / "ruyijs_necko/webtransport_session.py": source / "webtransport_session.py",
        build_lib / "ruyijs_necko/webtransport_connection.py": source / "webtransport_connection.py",
        build_lib / "necko/closed_route.py": source / "necko_closed_route.py",
        build_lib / "necko/core_client.py": source / "necko_core_client.py",
    }
    for dest, src in mapping.items():
        if not src.is_file():
            raise FileNotFoundError(src)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(src.read_bytes())
    win_source = project / "native/necko_tls/necko_win"
    if not win_source.is_dir():
        raise FileNotFoundError(win_source)
    for item in sorted(win_source.iterdir()):
        if not item.is_file() or item.suffix == ".pyc":
            continue
        dest = build_lib / "necko_win" / item.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(item.read_bytes())


def _platform_tag():
    machine = platform.machine().lower()
    if sys.maxsize <= 2 ** 32:
        raise RuntimeError("ruyijs requires 64-bit Python")
    if sys.platform == "win32" and machine in ("amd64", "x86_64"):
        return "win_amd64"
    if sys.platform == "darwin" and machine in ("arm64", "aarch64"):
        return "macosx_26_0_arm64"
    if sys.platform == "linux" and machine in ("amd64", "x86_64"):
        return "linux_x86_64"
    raise RuntimeError("ruyijs wheels are built for Windows x64, macOS arm64 or Linux x64")


class PlatformWheel(bdist_wheel):
    def finalize_options(self):
        super().finalize_options()
        self.root_is_pure = False
        self.plat_name = _platform_tag()

    def get_tag(self):
        return "py3", "none", _platform_tag()


setup(distclass=PlatformDistribution,
      cmdclass={"build_py": BuildPython, "bdist_wheel": PlatformWheel})
