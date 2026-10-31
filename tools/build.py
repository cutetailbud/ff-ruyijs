"""Build only a local standalone shell; the Firefox donor remains read-only."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from donor import Donor
from generate import generate
from client import build_client
from native import Native
from provenance import digest, inventory, save_hash_cache, verify_unchanged, write_if_changed
import product_sources
import geometry_fonts
import binding_order
import sanitizer_data
import compression_native
import urlpattern_native
import image_native
import rust_runtime
import skia_native


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    # Local donor and Firefox paths come from arguments or environment, never
    # from a machine-specific default.
    parser.add_argument("--donor-obj", default=os.environ.get("RUYIJS_DONOR_OBJ"),
                        help="SpiderMonkey standalone obj dir (env RUYIJS_DONOR_OBJ)")
    parser.add_argument("--toolchain-bin", help="explicit independent LLVM bin")
    parser.add_argument("--vs-root", help="explicit VS root with VC and Windows Kits")
    parser.add_argument("--runtime-dll-dir", help="matching MSVC CRT redistributable")
    parser.add_argument("--firefox-font-prefs", default=os.environ.get("RUYIJS_FIREFOX_FONT_PREFS"),
                        help="browser build dist/bin/greprefs.js (env RUYIJS_FIREFOX_FONT_PREFS; "
                             "default: tools/generated/firefox_font_prefs.h)")
    parser.add_argument("--firefox-bindings", default=os.environ.get("RUYIJS_FIREFOX_BINDINGS"),
                        help="generated Firefox dom/bindings directory (prototype member order; "
                             "env RUYIJS_FIREFOX_BINDINGS; default: tools/generated)")
    parser.add_argument("--firefox-gfx-obj", default=os.environ.get("RUYIJS_FIREFOX_GFX_OBJ"),
                        help="Firefox browser build objdir whose gfx/skia objects draw canvas "
                             "(env RUYIJS_FIREFOX_GFX_OBJ)")
    parser.add_argument("--pristine", action="store_true", help="relink donor objects only")
    parser.add_argument("--prepare-only", action="store_true", help="generate and record inputs")
    parser.add_argument("--no-incremental", action="store_true",
                        help="recompile all local translation units")
    parser.add_argument("--reuse-objects", action="store_true",
                        help="reuse every existing local object without recompiling")
    parser.add_argument("--jobs", type=int, choices=range(1, 129), default=min(os.cpu_count() or 4, 128),
                        help="independent compiler processes (default: CPU count)")
    parser.add_argument("--necko-core-only", action="store_true",
                        help="build dist/necko_tls/libnecko_core.dylib and skip the shell")
    args = parser.parse_args()
    if args.necko_core_only:
        if not args.firefox_gfx_obj:
            parser.error("--firefox-gfx-obj (or RUYIJS_FIREFOX_GFX_OBJ) is required")
        return args
    if not args.donor_obj:
        parser.error("--donor-obj (or RUYIJS_DONOR_OBJ) is required")
    if not args.pristine and not args.firefox_gfx_obj:
        parser.error("--firefox-gfx-obj (or RUYIJS_FIREFOX_GFX_OBJ) is required")
    return args


def local_directory(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f"build output is outside ruyijs: {path}")
    path.mkdir(parents=True, exist_ok=True)
    return path


def build_necko_core(root, necko_obj):
    """Link libnecko_core.dylib against the donor NSS next to xpcshell.

    The dylib's rpath is @loader_path. Symlinks beside it let a local probe
    load NSS without rewriting the donor binaries.
    """
    necko_obj = Path(necko_obj)
    libdir = necko_obj / "dist" / "bin"
    include = necko_obj / "dist" / "include"
    nspr_source = necko_obj.parent / "nsprpub" / "pr" / "include"
    sources = [
        root / "native" / "necko_tls" / "necko_tls.c",
        root / "native" / "necko_tls" / "necko_exchange.c",
    ]
    for source in sources:
        if not source.is_file():
            raise FileNotFoundError(source)
    if not (libdir / "libnss3.dylib").is_file():
        raise FileNotFoundError(libdir / "libnss3.dylib")
    if not (nspr_source / "obsolete" / "protypes.h").is_file():
        raise FileNotFoundError(nspr_source / "obsolete" / "protypes.h")
    out = local_directory(root, "dist/necko_tls")
    output = out / "libnecko_core.dylib"
    command = [
        "clang", "-dynamiclib", "-O2", "-Wall", "-Wextra", "-fvisibility=hidden",
        "-DXP_DARWIN", "-DXP_UNIX", "-D__APPLE__",
        "-arch", "arm64",
        "-I", str(include / "nss"),
        "-I", str(include / "nspr"),
        "-I", str(nspr_source),
        *[str(source) for source in sources],
        "-L", str(libdir),
        "-lnss3",
        "-Wl,-rpath,@loader_path",
        "-Wl,-install_name,@rpath/libnecko_core.dylib",
        "-o", str(output),
    ]
    subprocess.run(command, check=True)
    for name in ("libnss3.dylib", "libmozglue.dylib", "libfreebl3.dylib", "libsoftokn3.dylib"):
        link = out / name
        if link.is_symlink() or link.exists():
            link.unlink()
        link.symlink_to((libdir / name).resolve())
    return output


def install_network_helper(root, deployed):
    """Copy owned Necko helper sources into an existing venv.

    Windows receives oneshot.py only. POSIX also receives the shared library/xpcshell
    adapter. A missing venv is reported and does not create a release package.
    """
    oneshot_source = root / "src/resource_network/necko_oneshot.py"
    venv = root / "vendor/necko-derived/python"
    package_dirs = []
    windows_package = venv / "Lib/site-packages/ruyijs_necko"
    if windows_package.is_dir():
        package_dirs.append(windows_package)
    package_dirs += [path for path in sorted(venv.glob("lib/python*/site-packages/ruyijs_necko"))
                     if path.is_dir()]
    if sys.platform in ("darwin", "linux"):
        for site in sorted(venv.glob("lib/python*/site-packages")):
            if not site.is_dir():
                continue
            package = site / "ruyijs_necko"
            necko_dir = site / "necko"
            package.mkdir(exist_ok=True)
            necko_dir.mkdir(exist_ok=True)
            if package not in package_dirs:
                package_dirs.append(package)
    deployed_oneshot = None
    for package in package_dirs:
        if not package.is_dir():
            continue
        oneshot = package / "oneshot.py"
        oneshot.write_bytes(oneshot_source.read_bytes())
        deployed.append(oneshot)
        deployed_oneshot = oneshot
    report = {"source": str(oneshot_source),
              "deployed": str(deployed_oneshot) if deployed_oneshot else None}
    if deployed_oneshot is not None:
        report["sha256"] = digest(deployed_oneshot)
    if sys.platform not in ("darwin", "linux"):
        return report
    sites = []
    for package in package_dirs:
        necko_dir = package.parent / "necko"
        if package.is_dir() and necko_dir.is_dir():
            sites.append((package, necko_dir))
    if not sites:
        report["missing"] = "helper venv"
        print("helper venv is missing; js stays linked and no release package was created")
        return report
    sources = (
        root / "src/resource_network/necko_ruyi_init.py",
        root / "src/resource_network/necko_timing.py",
        root / "src/resource_network/necko_client.py",
        root / "src/resource_network/necko_adapters.py",
        root / "src/resource_network/necko_pkg_init.py",
    )
    installed = []
    for package, necko_dir in sites:
        targets = (
            (package / "__init__.py", sources[0]),
            (package / "timing.py", sources[1]),
            (necko_dir / "client.py", sources[2]),
            (necko_dir / "adapters.py", sources[3]),
            (necko_dir / "__init__.py", sources[4]),
        )
        for target, source in targets:
            target.write_bytes(source.read_bytes())
            deployed.append(target)
            installed.append(target)
        core_source = root / "src/resource_network/necko_core_client.py"
        if core_source.is_file():
            copied = necko_dir / "core_client.py"
            copied.write_bytes(core_source.read_bytes())
            deployed.append(copied)
            installed.append(copied)
        closed_source = root / "src/resource_network/necko_closed_route.py"
        if closed_source.is_file():
            copied = necko_dir / "closed_route.py"
            copied.write_bytes(closed_source.read_bytes())
            deployed.append(copied)
            installed.append(copied)
        for name in (
            "webtransport_worker.py",
            "webtransport_wire.py",
            "webtransport_verify.py",
            "webtransport_session.py",
            "webtransport_connection.py",
        ):
            source = root / "src/resource_network" / name
            copied = package / name
            copied.write_bytes(source.read_bytes())
            deployed.append(copied)
            installed.append(copied)
        win_source = root / "native/necko_tls/necko_win"
        if win_source.is_dir():
            win_dest = necko_dir.parent / "necko_win"
            win_dest.mkdir(exist_ok=True)
            for source in sorted(win_source.iterdir()):
                if not source.is_file() or source.suffix == ".pyc":
                    continue
                copied = win_dest / source.name
                copied.write_bytes(source.read_bytes())
                deployed.append(copied)
                installed.append(copied)
    report[sys.platform] = [str(path) for path in installed]
    return report


def main():
    args = arguments()
    root = Path(__file__).resolve().parents[1]
    if args.necko_core_only:
        if sys.platform != "darwin":
            raise SystemExit("libnecko_core is built on Darwin")
        build_necko_core(root, args.firefox_gfx_obj)
        print("dist/necko_tls/libnecko_core.dylib")
        return 0
    mode = "pristine" if args.pristine else "runtime"
    directory = local_directory(root, "build/" + mode)
    destination = local_directory(root, os.environ.get("RUYIJS_DESTINATION", "dist/pristine" if args.pristine else "dist"))
    artifacts = local_directory(root, "artifacts")
    report_path = artifacts / f"build-{mode}.json"
    report = {"mode": mode, "status": "started", "commands": []}
    native = None
    try:
        donor = Donor(args.donor_obj)
        native = Native(donor, root, directory, args)
        objects = donor.objects()
        inputs = donor.source_inputs() + objects + donor.libraries()
        inputs += [donor.obj / "dist/bin" / donor.shell_name,
                   donor.obj / "dist/bin" / donor.glue_name]
        inputs += list((root / "tools").glob("*.py"))
        sources = []
        if not args.pristine:
            unit, source_inputs = generate(donor, root, directory)
            sources = [(unit, "shell0"), *product_sources.sources(root)]
            # Both inputs come from a full Firefox browser build. Without one,
            # use the headers recorded from Firefox 159 in tools/generated.
            recorded = root / "tools/generated"
            font_unit, font_inputs = geometry_fonts.prepare(
                donor, directory, args.firefox_font_prefs, recorded / "firefox_font_prefs.h")
            if args.firefox_bindings:
                inputs += binding_order.prepare(args.firefox_bindings, directory)
            else:
                order = directory / "firefox_binding_order.h"
                write_if_changed(order, (recorded / "firefox_binding_order.h").read_text("utf-8"), "utf-8")
                inputs += [recorded / "firefox_binding_order.h", order]
            inputs += sanitizer_data.prepare(donor.src, directory)
            inputs += skia_native.inputs(donor, args.firefox_gfx_obj)
            sources.append(font_unit)
            inputs += font_inputs
            inputs += source_inputs + product_sources.inputs(root)
            if (root / "profiles/firefox-environment.json").is_file():
                profile_inputs = product_sources.profile(root, directory)
                # The header is generated inside build/runtime and is an output
                # of this invocation. Verify the source profile, not its
                # rewritten output timestamp, as a donor input.
                inputs += [path for path in profile_inputs
                           if path != directory / "firefox_environment_profile.h"]
            inputs += [unit, directory / "js.cpp", directory / "runtime_assets.h"]
            inputs += list((root / "python").rglob("*.py"))
            # Documentation is recorded when present; a public checkout omits
            # internal design notes, and they never affect the compiled output.
            inputs += [path for path in (root / "README.md", root / "ARCHITECTURE.md")
                       if path.is_file()]
            inputs += list((root / "docs").rglob("*.md"))
            inputs += [path for path in (root / "vendor/compression-native").rglob("*") if path.is_file()]
            inputs += [path for path in (root / "vendor/image-native").rglob("*") if path.is_file()]
        shell_name = os.environ.get("RUYIJS_OUTPUT_NAME", donor.shell_name)
        report.update(donor=str(donor.obj), source=str(donor.src),
                      executable=str(destination / shell_name),
                      inputs=inventory(inputs),
                      compiler=native.compiler(), linker=native.linker(),
                      include=native.env.get("INCLUDE", ""),
                      library_paths=donor.libpaths(args.vs_root),
                      runtime_dll_dir=args.runtime_dll_dir)
        if args.prepare_only:
            report["status"] = "prepared"
            return 0
        native.check_tools(compile_needed=bool(sources))
        if sources:
            original = donor.shell / ("Unified_cpp_js_src_shell0" + donor.object_suffix)
            if objects.count(original) != 1:
                raise ValueError("shell0 object must occur once in donor list")
            objects.remove(original)
            def compile_unit(unit):
                source, name = unit
                return native.compile(source, name,
                        exceptions=product_sources.needs_exceptions(source))
            with ThreadPoolExecutor(max_workers=args.jobs) as pool:
                objects += list(pool.map(compile_unit, sources))
        executable = destination / shell_name
        if not args.pristine:
            objects += compression_native.build(native)
            objects += urlpattern_native.build(native)
            if sys.platform != "darwin":
                objects += rust_runtime.build(native)
            objects += skia_native.build(native, args.firefox_gfx_obj)
            objects += image_native.build(native)
            report["imageCodecs"] = json.loads((directory / "image-native/build.json").read_text("utf-8"))
            report["compressionCodecs"] = json.loads(
                (directory / "compression-native/build.json").read_text("utf-8"))
        linked = native.link(objects, executable)
        deployed = native.deploy(destination)
        if sources:
            # The residency proof reads the whole executable; reuse it while unchanged.
            residency = directory / "residency.json"
            stamp = [executable.stat().st_size, executable.stat().st_mtime_ns]
            previous = json.loads(residency.read_text("utf-8")) if residency.is_file() else {}
            if linked or previous.get("stamp") != stamp:
                previous = {"stamp": stamp,
                            "domains": product_sources.residency(root, executable)}
                residency.write_text(json.dumps(previous), "utf-8")
            report["residency"] = previous["domains"]
        if not args.pristine:
            report["client"] = build_client(native, destination)
            if sys.platform == "darwin":
                build_necko_core(root, args.firefox_gfx_obj)
                report["neckoCore"] = "dist/necko_tls/libnecko_core.dylib"
            # The Necko Python runtime ships with releases, not the source tree.
            # A checkout without the helper venv still links the shell.
            report["networkHelper"] = install_network_helper(root, deployed)
        report["incremental"] = native.incremental
        verify_unchanged(report["inputs"], rehash=True)
        version = subprocess.run([str(executable), "--version"], cwd=destination,
                                 env=native.env, text=True, capture_output=True,
                                 timeout=30, check=True)
        report.update(status="built", version=version.stdout.strip(),
                      sha256=digest(executable), deployed=inventory(deployed),
                      toolchain_versions=native.versions)
        print(version.stdout.strip())
        print(executable)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
        print(str(error), file=sys.stderr)
        return 1
    finally:
        save_hash_cache()
        if native:
            report["commands"] = native.commands
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"Evidence: {report_path}")


if __name__ == "__main__":
    raise SystemExit(main())
