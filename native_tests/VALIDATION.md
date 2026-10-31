# Linux port from upstream macOS/POSIX

Date: 2026-10-31. Base: upstream `da1df2c`, including macOS PRs #4 and #6.
Platform: Linux x86_64, Python 3.14, Clang 21 and glibc 2.43. Runtime: Firefox
159 SpiderMonkey, Skia 149, NSS 3.131, NSPR 4.40 and Neqo 0.32.0.

## Results

- `python3 -B -m unittest discover -s native_tests -v`: 26 test cases passed.
  Three cases invoke isolated suites with 14 further checks: eight TLS/H1/H2/H3,
  three WebTransport and three image-codec checks. These cover real independent
  peers, not mocked network responses.
- The fragment shader supplied in the maintainer's PR #5 review compiles and
  links in `test_review_regressions.py`. Review-specific source and scope
  checks are recorded in [PR5_REVIEW.md](PR5_REVIEW.md).
- `PYTHONPATH=python python3 -B ci/smoke_test.py`: all checks passed.
- Pyflakes on the changed Python files and `git diff --check`: passed.
- `tools/build_linux.py` subsequently found no Ninja work. A subsequent
  unchanged Neqo build reported a 0.19-second Cargo phase.
- The Linux wheel was installed into a fresh virtual environment. Probes run
  from `/tmp` passed runtime discovery, callbacks, Workers, Python and C ABI
  helper discovery, and relocation to a directory containing Chinese characters.
  The installed transport and codec modules also passed the 14 independent
  helper checks. Test dependencies were installed separately; the probes used
  the transport modules and native libraries from the wheel.

Observed live source examples, with their upstream pass criteria unchanged:

| Example | Result | Elapsed |
| --- | --- | --- |
| Akamai | `passed=true`, real initial page, no script errors | 13.503 s |
| CreepJS | `ok=true`, no failed sections, script errors or console errors | 12.193 s |
| DataDome | device-check POST 200 and homepage 200; exit 0 | 8.119 s |

These are individual observations, not performance benchmarks or guarantees
about future server scores. The DataDome run initially exposed a shared adapter
bug: a TLS peer without ALPN was sent to xpcshell. A local TLS regression now
requires HTTP/1.1 on the verified connection, and the live example passed after
that fix. A separate shared WebTransport regression checks close after streams
and datagrams: information must be read before Neqo removes the session.

The old branch's DOM/input/GLSL/console changes, batch/pool APIs and extra
examples are not included. The existing DOM/input/GLSL/console implementations and example files remain
unchanged from this upstream base. Windows and macOS builds were not executed on this Linux host.
The wheel is a host-glibc `linux_x86_64` build, not manylinux.

## Reproduction

Build instructions and dependencies: [docs/linux.md](../docs/linux.md).

```sh
python3 -B tools/build_linux.py --donor-obj /path/firefox/obj-js --jobs 4
python3 -B tools/build_necko_linux.py --firefox-source /path/firefox --http3 --jobs 2
vendor/necko-derived/python/bin/python3 -m pip install -r native_tests/requirements.txt
python3 -B -m unittest discover -s native_tests -v
PYTHONPATH=python python3 -B ci/smoke_test.py
python3 -m pip wheel --no-deps . -w wheelhouse
```

Install the generated wheel in a fresh environment, install
`native_tests/requirements.txt` there for the independent peers, and use that
environment's Python with isolated mode from outside the checkout:

```sh
/path/fresh-venv/bin/python -I -B /path/ff-ruyijs/native_tests/installed_probe.py
/path/fresh-venv/bin/python -I -B /path/ff-ruyijs/native_tests/installed_network_probe.py network
/path/fresh-venv/bin/python -I -B /path/ff-ruyijs/native_tests/installed_network_probe.py webtransport
/path/fresh-venv/bin/python -I -B /path/ff-ruyijs/native_tests/installed_network_probe.py image
```

No `RUYIJS_SHELL`, `RUYIJS_NECKO_PYTHON`, `NECKO_CORE_LIBRARY`, or
`NECKO_QUIC_LIBRARY` override was used in the installation checks.
