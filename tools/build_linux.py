"""Build the native Linux runtime against a configured SpiderMonkey donor."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from provenance import write_if_changed
import shutil
import subprocess
import sys
from types import SimpleNamespace

from donor import Donor
import urlpattern_native
import rust_runtime


def build_urlpattern(root, donor, build):
    source = root / "vendor/urlpattern-native"
    directory = build / "urlpattern"
    directory.mkdir(parents=True, exist_ok=True)
    mirror = directory / "donor"
    wanted = urlpattern_native.packages(source / "Cargo.lock")
    index = urlpattern_native.donor_index(donor.src / "third_party/rust")
    missing = [key for key in wanted if key not in index]
    if missing:
        raise RuntimeError("Donor lacks locked URLPattern dependencies: " + repr(missing))
    identity = hashlib.sha256((source / "Cargo.lock").read_bytes() +
                              str(donor.src).encode()).hexdigest()
    marker = directory / "mirror.sha256"
    if not marker.is_file() or marker.read_text() != identity:
        if mirror.exists():
            shutil.rmtree(mirror)
        mirror.mkdir()
        for key in wanted:
            origin = index[key]
            target = mirror / origin.name
            shutil.copytree(origin, target)
            checksum = json.loads((target / ".cargo-checksum.json").read_text())
            (target / ".cargo-checksum.json").write_text(json.dumps({"files": {}, "package": checksum.get("package")}) + "\n")
        marker.write_text(identity)
    manifest = ("[package]\nname = \"ruyijs_urlpattern\"\nversion = \"0.1.0\"\nedition = \"2021\"\n"
                "[lib]\npath = " + json.dumps(str(source / "src/lib.rs")) +
                "\ncrate-type = [\"rlib\"]\n"
                "[dependencies]\nurlpattern = \"=0.6.0\"\nurl = \"2.5\"\n"
                "[profile.release]\nopt-level = 2\npanic = \"abort\"\ncodegen-units = 1\n")
    write_if_changed(directory / "Cargo.toml", manifest)
    write_if_changed(directory / "Cargo.lock", (source / "Cargo.lock").read_text())
    rustc, cargo = donor.substs["RUSTC"], donor.substs["CARGO"]
    rustc = rustc[0] if isinstance(rustc, list) else rustc
    cargo = cargo[0] if isinstance(cargo, list) else cargo
    environment = dict(os.environ, RUSTC=rustc)
    subprocess.run([cargo, "build", "--release", "--offline", "--locked",
                    "--manifest-path", str(directory / "Cargo.toml"),
                    "--target-dir", str(directory / "target"),
                    "--config", "source.crates-io.replace-with='donor'",
                    "--config", "source.donor.directory=" + json.dumps(str(mirror))],
                   check=True, cwd=directory, env=environment)
    # The donor already owns Rust's allocator and standard runtime. Combining
    # the crate rlibs avoids a second std/init array from a Rust staticlib.
    archive = directory / "target/release/libruyijs_urlpattern.a"
    members = sorted((directory / "target/release/deps").glob("*.rlib"))
    if archive.is_file() and all(p.stat().st_mtime_ns <= archive.stat().st_mtime_ns for p in members):
        return
    if archive.exists():
        archive.unlink()
    libraries = sorted((directory / "target/release/deps").glob("*.rlib"))
    script = "CREATE " + str(archive) + "\n" + "".join("ADDLIB " + str(path) + "\n" for path in libraries) + "SAVE\nEND\n"
    subprocess.run(["ar", "-M"], input=script, text=True, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--donor-obj", type=Path, default=os.environ.get("RUYIJS_DONOR_OBJ"), required=not os.environ.get("RUYIJS_DONOR_OBJ"))
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 4))
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--dependencies-only", action="store_true")
    args = parser.parse_args()
    if not sys.platform.startswith("linux") or args.jobs < 1:
        parser.error("Linux and a positive --jobs value are required")
    root = Path(__file__).resolve().parents[1]
    donor = Donor(args.donor_obj)
    if donor.os_arch != "Linux":
        parser.error("a Linux donor build is required")
    build = (args.build_dir or root / "build/linux").resolve()
    build.mkdir(parents=True, exist_ok=True)
    build_urlpattern(root, donor, build)
    adapter = SimpleNamespace(donor=donor, directory=build, env=dict(os.environ),
                              incremental={"disabled": False}, librarian=lambda: (shutil.which("ar"), []))
    rust_runtime.build(adapter)
    if args.dependencies_only:
        return 0
    subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=clang++",
                    "-DRUYIJS_DONOR_OBJ=" + str(donor.obj)], check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], check=True)
    subprocess.run(["strip", "--strip-debug", str(root / "dist/js")], check=True)
    subprocess.run([str(root / "dist/js"), "--version"], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
