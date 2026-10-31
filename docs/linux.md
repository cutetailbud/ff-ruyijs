# Linux x86_64

This port starts at upstream `da1df2c`, including the macOS work in PRs #4 and
#6. It follows the [maintainer's review of PR #5](https://github.com/cutetailbud/ff-ruyijs/pull/5#pullrequestreview-5722324778).
The earlier `linux-native` branch remains available separately.

Linux directly compiles the upstream POSIX runtime pipe, asynchronous framing,
Worker process launcher, C client, Necko process/pipe/session owners, and network
adapters. There is one POSIX implementation per subsystem. Linux does not add
`pipe_posix.cpp`, `process_posix.cpp`, or another C client lifecycle implementation.
The Windows branches retain upstream behavior.

The TLS adapter is the same `native/necko_tls` C source used by macOS. HTTP/3
and WebTransport use the same `native/necko_quic/src/lib.rs` and the shared
ClientHello hooks from `tools/build_necko_quic.py`. Helpers are staged by the
same `install_network_helper()` function into
`vendor/necko-derived/python`. Linux adds ELF names/rpaths, Linux socket and
spawn flags, and native certificate-compression decoders.

Core Text, ImageIO and CommonCrypto are macOS system interfaces. Linux supplies
Fontconfig/FreeType/HarfBuzz fonts, native image codecs, and OpenSSL WebCrypto.
Canvas uses Firefox 159's Skia raster sources, with the upstream Canvas owner.
Decoded premultiplied BGRA is converted at the Linux Canvas bridge, preserving
the shared image-frame contract.

The old branch's DOM/input/GLSL/console changes, `eval_many`, `ContextPool`,
client hints and extra examples are outside this port. They require separate
review; neither their behavior nor their validation is claimed here.

## Build

Use a read-only Firefox `FIREFOX_159_0_RELEASE` source tree with a configured,
built Linux x86_64 SpiderMonkey standalone objdir. It must have `dist/js`,
`config.status.json`, and the shell object list. Donor Rust crates are used
offline; no donor files are modified.

The tested host uses glibc, Clang, CMake 3.24+, Ninja, Rust/Cargo, Python 3.14,
`pkg-config`, and development packages for OpenSSL 3, Fontconfig, FreeType,
HarfBuzz (including subset), libavif, Brotli, zlib, WebP, LittleCMS2, and stb.
The NSS builder also installs `gyp-next` in its private Python environment.
Python 3.11+ is needed for the HTTP/3 build tool; the public package API remains
Python 3.10+.

```sh
python3 -B tools/build_linux.py --donor-obj /path/firefox/obj-js --jobs 4
python3 -B tools/build_necko_linux.py --firefox-source /path/firefox --http3 --jobs 2
PYTHONPATH=python python3 -B ci/smoke_test.py
```

Outputs are `dist/js`, `dist/libruyijs_client.so`, and
`dist/necko_tls/{libnecko_core,libnecko_quic}.so`. Each Context uses its own
process and private socket. `RUYIJS_SHELL` can select an explicit shell.
`NECKO_EXTRA_CA` adds a local CA for development without disabling certificate
or hostname validation.

The upstream proxy/fallback route requires a separately built donor xpcshell
selected with `RUYIJS_XPCSHELL`. Direct HTTP/HTTPS/H3 and WebTransport use the
native adapters. TLS peers without ALPN use HTTP/1.1 on that verified connection.
Requests outside the adapter's supported protocols still take the upstream
xpcshell fallback. Proxy compatibility has not been
verified in this port. The upstream direct adapter does not implement client
certificate identities. Full connection statistics and browser rendering
equivalence are not established by these checks.

## Tests and packaging

```sh
vendor/necko-derived/python/bin/python3 -m pip install -r native_tests/requirements.txt
python3 -B -m unittest discover -s native_tests -v
python3 -m pip wheel --no-deps . -w wheelhouse
```

Native tests cover both ends of the shared Unix framing protocol, fragmented
and maximum-size frames, bounded failures, C ABI callbacks/reentry/timeouts,
Workers, the Python bridge, font shaping, color glyphs, Canvas/image alpha,
known-answer WebCrypto, and local HTTP navigation. Independent TLS/H2/H3 and
WebTransport peers test actual traffic, certificate rejection, ClientHello
profiles, H2 reuse, Alt-Svc upgrades, large binary bodies, streams, datagrams,
send groups and key exporters. The shared POSIX WebTransport owner snapshots
session information before Neqo removes the session on close.

The Linux wheel bundles the native shell/client, an isolated relocatable
Python helper, the shared transport modules, NSS/Neqo, and non-glibc ELF
dependencies. Install `patchelf` before building it. The tag is
`py3-none-linux_x86_64`; this is a host-glibc build, not a manylinux wheel.
It is not published to PyPI. Windows and macOS builds were not executed on
this Linux host.

Observed validation results and installation reproduction are in
[native_tests/VALIDATION.md](../native_tests/VALIDATION.md).
