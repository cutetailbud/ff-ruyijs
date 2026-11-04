# Linux x86_64 build and speed baseline

Measured on 2026-11-03 from the sync of upstream `d41f247` (0.3.3) into the fork, commit `a11cfd4`. No performance changes were made. The same build steps are summarized in [linux.md](linux.md). Repeat the numbers with `ci/linux_baseline.py` against the wheel this build produces.

The machine was a 4 vCPU hypervisor guest that reports itself as `Intel(R) Xeon(R) Processor` (no finer model), Ubuntu 24.04, glibc 2.39, kernel `6.12.94+`, about 16 GB RAM. Builds used `JOBS=3`.

## What the image still needed

`ci/linux/build.sh` is the build. On this image the following were not already true, and the build stops without them.

| Requirement | What was on the image | What the build used |
| --- | --- | --- |
| Clang 21 | Clang 18 | `ci/linux/build.sh deps` installs Clang 21.1.8 from apt.llvm.org. `CC=clang-21`, `CXX=clang++-21`, and `/usr/lib/llvm-21/bin` on `PATH` so the NSS build's plain `clang` is that compiler |
| libstdc++ headers Clang 21 will use | `g++-13` headers, while `libstdc++6` and `/usr/lib/gcc/x86_64-linux-gnu/14` are already GCC 14 | `g++-14` and `libstdc++-14-dev`. Without them `js/src/configure` reports "libstdc++ in use is not new enough"; the compiler error under that message is `cstddef` file not found |
| Rust | rustc 1.83.0 | Firefox 159's `firefox/python/mozboot/mozboot/util.py` requires Rust 1.90.0 or newer. `rustup toolchain install stable` provided rustc 1.99.1 (2026-10-13) |
| Python 3.14 for the Necko helper | `python3` is 3.12.3 | Python 3.14.10 from the deadsnakes PPA (`python3.14`, `python3.14-venv`, `python3.14-dev`). `tools/build_necko_linux.py` refuses anything older. The donor configure and the wheel test stay on `/usr/bin/python3` |
| `packaging` 24.2 or newer | Ubuntu's `python3-packaging` is 24.0, which satisfies the script's `packaging>=24` | setuptools 77+ imports `packaging.licenses` and stops the wheel. `python3 -m pip install --upgrade 'packaging>=24.2'` |
| `python3-venv` | not installed | `ci/linux/build.sh test` creates a venv with `/usr/bin/python3` |
| `RUNNER_TEMP` | unset | The deps and test steps expand it under `set -u`. Set `RUNNER_TEMP=/tmp` |
| Firefox source | `firefox/` empty | Shallow fetch of the submodule commit, below |

`cbindgen` 0.29.4 comes from the deps step (`cargo install`). Donor configure runs on `/usr/bin/python3`, as the script says.

## Commands

From the repository root, after the packages above:

```sh
export PATH="/usr/local/cargo/bin:/usr/lib/llvm-21/bin:$PATH"
export CC=clang-21 CXX=clang++-21 JOBS=3 RUNNER_TEMP=/tmp

# Firefox 159 source at the commit recorded by `git ls-tree HEAD firefox`.
sha=$(git ls-tree HEAD firefox | awk '{print $3}')
rm -rf firefox
git init -q firefox
git -C firefox remote add origin https://github.com/mozilla-firefox/firefox.git
git -C firefox fetch --depth 1 --no-tags origin "$sha"
git -C firefox -c checkout.workers=0 checkout -q --detach FETCH_HEAD

ci/linux/build.sh deps
ci/linux/build.sh donor
ci/linux/build.sh shell
PYTHON=python3.14 ci/linux/build.sh necko
python3 -m pip install --upgrade 'setuptools>=77' 'packaging>=24.2' wheel
ci/linux/build.sh wheel
ci/linux/build.sh test
```

`deps` is listed for a clean machine. It had already been run before the timed steps below. `PYTHON=python3.14` is required here because `python3` is 3.12; on the GitHub runner, `python3` is 3.14 and the bare `ci/linux/build.sh necko` is enough.

The submodule commit was `9a2dc53c369c7ae5172018b35988e1d2ffeff402` (2026-10-22). The checkout is 5.9 GB.

## Build time and size

Wall time for the successful run. The first donor configure was thrown away after the missing `cstddef` failure; it had not compiled anything.

| Step | Wall time | Result |
| --- | --- | --- |
| `donor` | 8 min 59 s | `obj/js-linux`, 2.0 GB. `dist/bin/js` in that objdir is the SpiderMonkey shell |
| `shell` | 6 min 42 s | `dist/js` is 65,950,688 bytes, plus `dist/libruyijs_client.so`. `dist/` is 63 MB |
| `necko` | 1 min 47 s | NSS 3.131, NSPR 4.40, `libnecko_core.so`, `libnecko_quic.so`. `backend.json` reports `http/1.1`, `h2`, `h3`, Python 3.14.10. `build/` is 1.1 GB |
| `wheel` | 12 s | `wheelhouse/ruyijs-0.3.3-py3-none-manylinux_2_39_x86_64.whl`, 56,196,494 bytes |
| `test` | 15 s | All checks passed. See below |

`vendor/necko-derived` after the helper build is 90 MB. A full tree of firefox + obj + build + helper + dist is about 9.2 GB, in line with the "about 11 GB" note in `linux.md`.

## Tests

`ci/linux/build.sh test` installs that wheel into a fresh venv and runs, offline except for the two network checks:

- `ci/smoke_test.py`
- `ci/helper_check.py` — the helper compiled no module at start; interpreter start and imports were 0.16 CPU-seconds
- `ci/layout_check.py` — passed; it printed `offsetHeight` after a style change on 1,500 divs as 18.9 ms (median of 5)
- `ci/atob_check.py`
- `ci/network_smoke.py` — `https://example.com/` loaded
- `ci/proxy_smoke.py`
- `ci/hello_check.py` — ClientHello matched Firefox 159 (`ci/firefox159-hello.json`), including ALPN `h2`, `http/1.1`
- `ci/engine_check.py --network` — four sessions in one process, including a proxied load of example.com

## Speed and memory

Run from outside the checkout so the wheel is used, not `dist/js` in the tree:

```sh
cd /tmp
env -u PYTHONPATH -u RUYIJS_SHELL -u LD_LIBRARY_PATH \
  /path/to/venv/bin/python /path/to/ci/linux_baseline.py --network
```

One run of that command produced the table below. Medians are what the script prints. Where a cost moves around, the range from this run and from the same script's earlier runs on this machine is noted.

| What | Result |
| --- | --- |
| Context startup, time to `eval("1+1")` | Eight back-to-back contexts: 41.9, 38.1, 80.1, 63.2, 78.1, 51.8, 87.3, 108.9 ms. The first two sit near 40 ms; later ones are slower. A separate loop of 10 contexts with a 50 ms pause between them stayed at 35–53 ms, median 38 ms |
| `eval("1+1")` after warmup, 400 calls | median 21 µs, p95 25 µs (min 18, max 134) |
| Integer loop, 4×10^8 iterations, result returned | median 208 ms (156–212). About 1.9×10^9 iterations/s |
| Build 2,000 objects, `JSON.stringify` + `JSON.parse` | median 1.2 ms |
| `load_page` of 100 divs, context already up | median 5.3 ms (2.1–7.3) |
| Build 1,500 text divs, then read `offsetHeight` after a padding change | build 23.4 ms; read median 15.6 ms (15.1–16.5). The test's own layout check printed 18.9 ms for the same read |
| `Engine` + 4 sessions, each `eval("1+1")` | 189 ms |
| Same 4×10^8 loop, one session vs four at once | one session 206 ms; four sessions finished in 206 ms wall (the slowest session). They ran together |
| `https://example.com/` to `readyState == "complete"` | 532 ms at the end of this run. Three cold processes measured apart from the script: 975, 1031, and 1196 ms. A second navigation in that same context: 432, 451, and 595 ms |
| `http://127.0.0.1/` to a local page, three contexts | first navigation 211, 230, and 236 ms; second navigation in the same context 42, 41, and 40 ms. An earlier run of the same helper was 163–173 ms cold and 40 ms warm |

Resident set after the process tree had settled (a context's two helpers had appeared):

| State | `js` | helper `python3` | Total |
| --- | --- | --- | --- |
| One context, only `eval("1+1")` | 43 MiB | 2 processes, 86 MiB | 129 MiB |
| Same context after the 1,500-div page | 78 MiB | 2 processes, 86 MiB | 164 MiB |
| One engine, four sessions | 62 MiB | 4 processes, 172 MiB | 234 MiB |

The two helpers are there before any request. Closing the context does not take them with it immediately: right after `close()`, both were still alive; at 0.1 s they were still alive; at 0.2 s they were gone. Opening contexts in a tight loop therefore stacks helpers (2, then 4, then 6 were observed) and the later startups above are that case.

## Where the time went

`perf` from Ubuntu's `linux-tools` does not match kernel `6.12.94+`, so it cannot sample. Stacks were taken with `gdb -p <shell_pid> -batch -ex bt` while `ci/linux_baseline.py --hold <workload>` ran. Attaching stops the process, so this is a coarse sample, not a cycle count. The same frames came up often enough to name the hotspot.

**Geometry read** (`--hold geometry`, 82 stacks). The hot path is box layout, not the pipe. The recurring chains were `BoxStyle::Get` called from `BoxLength`, `BoxFlow::AdjoinsChildren`, `BoxEdges`, and `BoxAbsolute`, and `BoxFlow::Children` calling `BoxFlow::Place` calling `BoxFlow::Children` again. A separate 103-stack pass had `BoxStyle::Get` or `memcmp` in the leaf frame 26 times. `Pipe::Read` appeared twice in the 82. `BoxStyle::Get` (`src/domain/styling/box_layout_values.cpp`) binary-searches a `vector` of strings, with a thread-local hint so a repeated literal is one comparison. 0.3.3 added that hint. It is still the hottest leaf in this read.

**Building 1,500 text divs** (61 stacks, a loop of `createElement` / `textContent` / `appendChild`). 47 stacks were in `document_model::State::Find(JSObject*)`, called from `Receiver` / `Call`. That function walks a linked list of every live wrapper (`src/document_model/realm.cpp`). `Find(NodeKey)` is a hash map. The object lookup is not. Font shaping did not show up in this sample; the list walk did.

**The 4×10^8 loop** (41 stacks). 40 landed in `free` or in code with no symbol, which is where SpiderMonkey's JIT code lives, and one landed in `Pipe::Read`. The 208 ms is JavaScript execution. The host round trip is the 21 µs eval, not this loop.

## Candidates to look at next

These are targets only. Nothing here was changed.

1. **`State::Find(JSObject*)` is a linear scan.** 47 of 61 samples while building 1,500 text nodes, and that build costs about 23 ms. `Find(NodeKey)` is already indexed. An index from wrapper object to node would cut the DOM-call cost that grows with the number of live nodes.

2. **`BoxStyle::Get` and the `BoxFlow::Place` / `Children` walk dominate a geometry read.** About 16 ms to read `offsetHeight` after changing one element's padding, on 1,500 divs, and the samples sit in string lookup plus the recursive place/children walk. The hint cache is already there. The next gain is fewer lookups per box (stable property ids, or not re-reading edges a padding change cannot affect), which is a larger change than another cache.

3. **A context that never uses the network still starts two Necko helpers, and they outlive `close()` by about 100–200 ms.** They are 86 MiB of the 129 MiB idle total. A tight loop of contexts accumulates those processes and startup moves from about 40 ms to 60–110 ms. The first request to localhost is about 160–240 ms and the second, in the same context, is about 40 ms, so most of that first request is the cold helper rather than HTTP to `127.0.0.1`.

4. **Cold `https://example.com/` is about 0.5–1.2 s, and a second load in the same context is about 0.4–0.6 s.** The localhost numbers put the stack's own warm request near 40 ms and its cold request near 200 ms, so the rest of example.com is the network. The part still inside ruyijs is the cold helper from item 3, not the page load after the helper is up.

The eval round trip (about 21 µs) and the integer loop (JIT code, about 1.9×10^9 iterations/s, and four engine sessions finish in the same wall time as one) are not useful host-side targets on this machine.
