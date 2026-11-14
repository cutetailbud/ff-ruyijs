# Linux x86_64

Since 0.3.0 PyPI has a `manylinux_2_39_x86_64` wheel. It needs glibc 2.39 or later (Ubuntu 24.04, Debian 13,
Fedora 40 and newer). The Linux port is [@Starktomy](https://github.com/Starktomy)'s pull request #5, integrated on
the POSIX process, pipe and socket layer that macOS uses.

The same facts are written in [linux.zh-CN.md](linux.zh-CN.md).

## Platform backends

| Component | Linux backend |
| --- | --- |
| JavaScript, Intl, Wasm | SpiderMonkey from the Firefox 160.0 source (the `firefox/` submodule) |
| DOM, iframe realms, events, Worker | The shared ruyijs sources, on POSIX processes and Unix sockets |
| Canvas | Firefox 160's own Skia, compiled from the submodule |
| Fonts | fontconfig, FreeType and HarfBuzz, with the fonts installed on the machine |
| WebCrypto | OpenSSL 3 |
| Images | libwebp, LittleCMS2 and stb_image; AVIF through the vendored libavif, dav1d and libyuv |
| HTTP(S), HTTP/3 | The Necko helper on NSS 3.131 / NSPR 4.40 built from the submodule, with Neqo for HTTP/3. It runs on a Python 3.14 bundled in the wheel |
| C ABI | `libruyijs_client.so` |

Text is drawn with the fonts installed on the machine, so canvas text and font metrics differ from Windows unless the
same fonts are present. The wheel bundles the libraries it links, except glibc; the fonts and the fontconfig
configuration come from the system.

## Build

The PyPI wheel is built by `.github/workflows/build-linux.yml` on Ubuntu 24.04. To build it locally, run the steps of
`ci/linux/build.sh` in order, from a checkout that has the `firefox/` submodule:

```sh
ci/linux/build.sh deps     # apt packages, clang 21 (apt.llvm.org), cbindgen
ci/linux/build.sh donor    # SpiderMonkey objdir -> obj/js-linux
ci/linux/build.sh shell    # dist/js and dist/libruyijs_client.so
ci/linux/build.sh necko    # NSS/NSPR, libnecko_core, Neqo and the Necko helper (Python 3.14)
ci/linux/build.sh wheel    # wheelhouse/ruyijs-*-manylinux_2_39_x86_64.whl
ci/linux/build.sh test     # installs the wheel in a fresh venv and runs the smoke tests
```

The donor step runs configure on the distribution Python (`/usr/bin/python3`), the necko step needs Python 3.14 as
`python3`. A full build uses about 11 GB of disk. The Necko adapter and its limits are described in
[native/necko_posix/README.md](../native/necko_posix/README.md). One measured build, the smoke tests, and a speed
baseline are written up in [linux-baseline.md](linux-baseline.md).
