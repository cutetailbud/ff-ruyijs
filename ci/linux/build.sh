#!/usr/bin/env bash
# Linux x86_64 build of ruyijs against the Firefox 160.0 source in firefox/
# (the submodule; FIREFOX_SOURCE overrides the location). Each step is one step
# of ci/linux/build-linux.yml; run them in order on Ubuntu 24.04:
#
#   ci/linux/build.sh deps     apt packages, clang 21 (apt.llvm.org), cbindgen
#   ci/linux/build.sh donor    SpiderMonkey standalone objdir   -> obj/js-linux
#   ci/linux/build.sh shell    dist/js, dist/libruyijs_client.so  (tools/build_linux.py)
#   ci/linux/build.sh necko    NSS/NSPR, libnecko_core, Neqo and the helper
#                              (tools/build_necko_linux.py --http3; Python 3.14)
#   ci/linux/build.sh wheel    the manylinux wheel                -> wheelhouse/
#   ci/linux/build.sh test     installs the wheel in a fresh venv; ci/smoke_test.py,
#                              ci/atob_check.py, ci/fetch_abort_check.py,
#                              ci/fetch_abort_transfer_check.py, ci/load_timer_check.py,
#                              ci/network_smoke.py, ci/proxy_smoke.py,
#                              ci/hello_check.py and ci/engine_check.py
set -euo pipefail
step="${1:?usage: build.sh deps|donor|shell|necko|wheel|test}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
firefox="${FIREFOX_SOURCE:-$root/firefox}"
donor="${RUYIJS_DONOR_OBJ:-$root/obj/js-linux}"
jobs="${JOBS:-$(nproc)}"
LLVM=21
# clang 21 from apt.llvm.org (the deps step); CC, CXX and LLVM_OBJDUMP may name
# another clang 21 installation instead.
export CC="${CC:-clang-$LLVM}" CXX="${CXX:-clang++-$LLVM}"
objdump="${LLVM_OBJDUMP:-llvm-objdump-$LLVM}"
python="${PYTHON:-python3}"

case "$step" in
  deps)
    sudo apt-get update -qq
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      cmake ninja-build pkg-config patchelf libffi-dev zlib1g-dev \
      libfontconfig-dev libfreetype-dev libharfbuzz-dev libssl-dev \
      libwebp-dev liblcms2-dev libstb-dev
    if ! command -v "clang-$LLVM" >/dev/null; then
      curl -fsSL https://apt.llvm.org/llvm.sh -o "$RUNNER_TEMP/llvm.sh"
      sudo bash "$RUNNER_TEMP/llvm.sh" "$LLVM"
    fi
    # llvm-objdump for the donor configure; libclang for the bindgen of Neqo's
    # NSS bindings. NSS's own build.sh runs plain `clang`, so the necko step
    # wants /usr/lib/llvm-21/bin first on PATH (the workflow adds it).
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      "llvm-$LLVM" "libclang-$LLVM-dev"
    command -v cbindgen >/dev/null || cargo install cbindgen --version 0.29.4 --locked
    "clang++-$LLVM" --version | head -1; rustc --version; cbindgen --version; cmake --version | head -1 ;;
  donor)
    # The configuration Starktomy's Linux port (pull request #5) was validated with.
    # configure and make run on the distribution Python (3.12 on Ubuntu 24.04),
    # not on the 3.14 the helper needs.
    mkdir -p "$donor"
    cd "$donor"
    PYTHON3="${DONOR_PYTHON:-/usr/bin/python3}" LLVM_OBJDUMP="$objdump" \
      "$firefox/js/src/configure" --enable-project=js \
      --enable-optimize --disable-debug --disable-tests --disable-jemalloc \
      --enable-shared-js --with-system-zlib
    make -j"$jobs" ;;
  shell)
    "$python" -B tools/build_linux.py --donor-obj "$donor" --jobs "$jobs"
    ls -l dist/js dist/libruyijs_client.so ;;
  necko)
    "$python" -B tools/build_necko_linux.py --firefox-source "$firefox" --jobs "$jobs" --http3
    cat vendor/necko-derived/python/backend.json ;;
  wheel)
    "$python" -m pip install --quiet "setuptools>=77" wheel "packaging>=24"
    "$python" -m pip wheel --no-deps --no-build-isolation -w wheelhouse .
    ls -l wheelhouse ;;
  test)
    # A fresh venv on the distribution Python rather than the 3.14 that built the
    # helper, and no LD_LIBRARY_PATH (setup-python sets one): the bundle must
    # resolve every library itself.
    venv="$RUNNER_TEMP/wheel-test"
    rm -rf "$venv"
    "${TEST_PYTHON:-/usr/bin/python3}" -m venv "$venv"
    "$venv/bin/python" -m pip install --quiet --no-index wheelhouse/*.whl
    cd "$RUNNER_TEMP"
    clean=(env -u PYTHONPATH -u RUYIJS_SHELL -u LD_LIBRARY_PATH)
    runtime="$("${clean[@]}" "$venv/bin/python" -c "import ruyijs; print(ruyijs.get_runtime_path())")"
    "${clean[@]}" "$venv/bin/python" -c "import ruyijs; print(ruyijs.__version__, '$runtime')"
    "${clean[@]}" "$runtime/vendor/necko-derived/python/bin/python3" -c \
      "import sys, ssl, necko, ruyijs_necko; print('helper', sys.version.split()[0], ssl.OPENSSL_VERSION)"
    "${clean[@]}" "$venv/bin/python" "$root/ci/smoke_test.py"
    # The Necko helper starts from bytecode and carries no unusable modules.
    "${clean[@]}" "$venv/bin/python" "$root/ci/helper_check.py"
    # Geometry after a mutation equals a fresh layout of the mutated page.
    "${clean[@]}" "$venv/bin/python" "$root/ci/layout_check.py"
    "${clean[@]}" "$venv/bin/python" "$root/ci/atob_check.py"
    "${clean[@]}" "$venv/bin/python" "$root/ci/fetch_abort_check.py"
    "${clean[@]}" "$venv/bin/python" "$root/ci/fetch_abort_transfer_check.py"
    "${clean[@]}" "$venv/bin/python" "$root/ci/load_timer_check.py"
    # One real page load through the bundled Necko helper, direct and through
    # a local CONNECT proxy.
    "${clean[@]}" "$venv/bin/python" "$root/ci/network_smoke.py"
    "${clean[@]}" "$venv/bin/python" "$root/ci/proxy_smoke.py"
    # The ClientHello must be Firefox 160's (ci/firefox160-hello.json).
    "${clean[@]}" "$venv/bin/python" "$root/ci/hello_check.py"
    # Several Contexts as sessions of one shared engine process.
    "${clean[@]}" "$venv/bin/python" "$root/ci/engine_check.py" --network ;;
  *)
    echo "unknown step: $step" >&2; exit 1 ;;
esac
