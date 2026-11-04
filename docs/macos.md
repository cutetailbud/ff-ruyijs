# macOS arm64

Since 0.3.0 the `macosx_26_0_arm64` wheel is on PyPI. GitHub Actions builds it from the `firefox/` submodule (`.github/workflows/build-macos.yml`): SpiderMonkey, Skia and NSS come from that source, and `libnecko_core.dylib` and `libnecko_quic.dylib` are linked against that NSS. It has no xpcshell. When that binary is absent, `Context(proxy=...)` opens an HTTP CONNECT tunnel and then uses `libnecko_core`. The donor-tree path below still includes xpcshell and keeps the Necko proxy prefs.

The same facts are written in [macos.zh-CN.md](macos.zh-CN.md).

## What the macOS port adds

The shared Python API, DOM, clock, and page logic stay one source tree. macOS adds the outer shell: a Unix-socket frame, an xpcshell Necko helper, Core Text, ImageIO, and Firefox's own Skia objects from the browser donor. The C client install name is `@rpath/libruyijs_client.dylib`.

- Same API: JS contexts, DOM, the Python bridge, WebCrypto algorithms, and the static image formats.
- Acceptable platform differences: a named pipe or a Unix socket, CNG or CommonCrypto, WIC or ImageIO, DirectWrite or Core Text. GIF disposal, glyph gamma, and CJK fallback follow the system decoder. They are not pixel-identical to Windows.
- Still outside this port: the socket process stays off. Turning the prefs on makes this xpcshell exit with signal 11 while filling Mac sandbox parameters. The donor `plugin-container.app` is produced by building `ipc/app`. Direct HTTPS follows `Context(tls=)` through `libnecko_core.dylib` for ClientHello counts and for the 0.2.1 header and HTTP/2 settings table. Pages were rechecked for 100 and 159. Eight boundary versions were library GETs, not pages. A proxy stays on xpcshell when that binary exists. Without it, HTTPS uses CONNECT and cleartext uses an absolute-form request; HTTP/3 does not go through the proxy. DNS, connect, and TLS timing slots stay 0. The PyPI wheel has no xpcshell. `macos.url` is the fork release zip. Direct cleartext and HTTP/3 are in `docs/closed-route.zh-CN.md`.

## What lives where

ruyijs on macOS is the outer shell. SpiderMonkey and Necko/NSS come from one official Firefox tree, built outside this repository and never committed here.

| Piece | Where it comes from |
| --- | --- |
| `dist/js`, `dist/libmozglue.dylib` | SpiderMonkey standalone donor, then `tools/build.py` |
| Necko/NSS fallback and proxy | `xpcshell` from the browser donor |
| `libnecko_core.dylib` | `native/necko_tls/`, linked to donor NSS 3.131. Direct HTTPS, plus the cleartext socket and HTTP/1 exchange. Lookup is `dist/necko_tls`, then `dist/necko` |
| `libnecko_quic.dylib` | `native/necko_quic/`, Neqo 0.32.0 from the Firefox 159 vendor, linked to donor NSS 3.131. Darwin build is `tools/build_necko_quic.py`. The dylib is not committed |
| Frame pipe, process launch, C ABI client | this repository |
| Python helper venv | `vendor/necko-derived/python`, gitignored. A wheel uses the Python that installed it |

A maintainer checkout is two sibling directories:

```
firefox/                 official Firefox source, not part of ruyijs
  obj-js-standalone/     SpiderMonkey standalone
  obj-necko/             browser build that produces xpcshell
ff-ruyijs/               this repository
```

The default xpcshell path is the parent of this repo plus `firefox-159/obj-necko/dist/bin/xpcshell`. Rename the Firefox directory or pass `RUYIJS_XPCSHELL`. An installed wheel looks for `dist/necko/xpcshell` inside the runtime before that donor path. The fetch script defaults to `src/resource_network/necko_fetch.js`, found by walking parents of the client, or to `necko_fetch.js` beside the installed client. Override it with `RUYIJS_NECKO_SCRIPT`.

Pass the standalone objdir to `tools/build.py --donor-obj`. The matching environment variable is `RUYIJS_DONOR_OBJ`. Pass it for that build. Do not export it from a long-lived shell profile.

## Donor builds

Use the Firefox tag `FIREFOX_159_0_RELEASE`. Keep both objdirs. A later rebuild resumes in place.

SpiderMonkey standalone, `mozconfig-js`:

```
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-js-standalone
ac_add_options --enable-project=js
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --enable-rust-simd
```

Browser donor used for xpcshell, `mozconfig-necko`:

```
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-necko
ac_add_options --enable-application=browser
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --disable-nodejs
ac_add_options --enable-rust-simd
ac_add_options --without-wasm-sandboxed-libraries
```

`--disable-nodejs` leaves the newtab bundle unfinished. The helper does not need that bundle. Stop when `obj-necko/dist/bin/xpcshell` runs. The linked XUL library is `obj-necko/dist/bin/XUL`.

The helper turns the socket process off. The client sets `MOZ_DISABLE_SOCKET_PROCESS=1`. `necko_fetch.js` sets `network.process.enabled` and `network.http.network_access_on_socket_process.enabled` to false. Leave those in place.

## Build the shell and the helper

The helper venv uses Python 3.12. `tools/bootstrap_macos_helper.py` looks up `python3.12` on `PATH`, then `~/.local/bin`, Homebrew, and `/usr/local/bin`. If none of those exists it stops. When `vendor/necko-derived/python/pyvenv.cfg` already exists, the script keeps that venv and installs only packages that fail to import.

From the repository root, with `MACOSX_DEPLOYMENT_TARGET=26.0`:

```sh
python3.12 -B tools/bootstrap_macos_helper.py
python3.12 -B tools/build.py --donor-obj ../firefox-159/obj-js-standalone
```

Bootstrap installs `requests`, `brotli`, `zstandard`, `h2`, `cryptography`, `backports.zstd`, and `publicsuffixlist`. Missing wheels come only from gitignored `vendor/necko-derived/wheels`, with `pip --no-index`. Fill that directory with `curl --noproxy '*'` from a mirror of the simple index. An empty directory and a missing import stop the script. The script contains no proxy address. This round did not run bootstrap again. The current helper already imports `publicsuffixlist`. `firefox-suffixes.dat` sits beside `necko_win/_firefox.py` and is copied with that directory. Direct HTTPS uses `h2` and `cryptography` with the library. `requests` still prepares the frame that oneshot already spoke on Windows. A proxy uses the donor xpcshell when that binary exists. Without it, HTTPS uses CONNECT and cleartext uses an absolute-form request. A non-HTTP URL still uses the donor xpcshell.

`tools/build.py` then copies tracked sources into the venv:

| Source | Installed path |
| --- | --- |
| `src/resource_network/necko_oneshot.py` | `ruyijs_necko/oneshot.py` |
| `src/resource_network/necko_ruyi_init.py` | `ruyijs_necko/__init__.py` |
| `src/resource_network/necko_timing.py` | `ruyijs_necko/timing.py` |
| `src/resource_network/necko_client.py` | `necko/client.py` |
| `src/resource_network/necko_adapters.py` | `necko/adapters.py` |
| `src/resource_network/necko_pkg_init.py` | `necko/__init__.py` |
| `src/resource_network/necko_core_client.py` | `necko/core_client.py` |
| `src/resource_network/webtransport_worker.py` | `ruyijs_necko/webtransport_worker.py` |
| `src/resource_network/webtransport_wire.py` | `ruyijs_necko/webtransport_wire.py` |
| `src/resource_network/webtransport_verify.py` | `ruyijs_necko/webtransport_verify.py` |
| `src/resource_network/webtransport_session.py` | `ruyijs_necko/webtransport_session.py` |
| `src/resource_network/webtransport_connection.py` | `ruyijs_necko/webtransport_connection.py` |
| `native/necko_tls/necko_win/` | `necko_win/` on the helper path |

On Windows the same build copies only `oneshot.py`. If the Darwin venv is missing, the shell still links and no release archive is created.

The shell binary is `dist/js`. Point examples at it with `RUYIJS_SHELL` set to that path, and put `python/` on `PYTHONPATH`. The client library is `libruyijs_client.dylib`.

A local wheel is tagged `macosx_26_0_arm64`. It places the shell at `_runtime/dist/js` and copies the donor xpcshell closure to `_runtime/dist/necko`, including `libnecko_quic.dylib` when that library exists. The wheel also includes `necko/closed_route.py`, `necko/core_client.py`, and `necko_win`. The wheel and `ruyijs-runtime-macos-arm64.zip` are on the maintainer fork release, not on PyPI. Regenerate the archive with `python3.12 -B tools/macos_runtime.py <zip-path>`, then store the printed size and sha256 in the `runtime.json` `macos` object together with the release URL.

## How a request moves

The length-prefixed frame format is the same as Windows. On macOS the pipe is a Unix socket. The shell starts `python -I -B -m ruyijs_necko.oneshot`. Direct HTTPS uses `libnecko_core.dylib`. ALPN `h2` uses the product HTTP/2 settings. ALPN `http/1.1` writes HTTP/1.1 on that socket. Another ALPN is unwired, and the router may fall back. A proxy keeps xpcshell when that binary exists; otherwise HTTPS uses CONNECT and cleartext uses an absolute-form request. Direct `http://` uses the socket and HTTP/1 exchange in `libnecko_core`. Direct `https://` uses `libnecko_core` until a remembered response `Alt-Svc` sends the next same-process request for that origin through `libnecko_quic`. Certificate verification failure does not fall back. Cookies are not stored. `redirectionLimit` stays 0, including on the xpcshell path, so a 3xx response is returned as the response. Inside one helper process, same-origin HTTP/2 can reuse one connection. The page shell still starts one oneshot per request unless `--session` is used.

Verified on direct HTTPS pages: HTTP/2, version code 20, for 100 and 159. HTTP/1.1 on this socket was unit-tested. A later direct-TLS check of `https://www.gnu.org/` returned status 200 and version 11. The earlier xpcshell path had also returned HTTP/1.1. Direct cleartext and HTTP/3 use the same export names as the Windows DLLs. A local HTTP/1.1 server returned status 200 and version 11. A local Neqo server returned version 30 on the second request after the response `Alt-Svc` was remembered. A bad certificate on that second request failed and was not status 200. Public `quic.nginx.org` did the same: version 20, then version 30, with the same body. `cloudflare.com` did not send `Alt-Svc`, so both requests stayed version 20. The donor xpcshell still returns `necko 804b0033` for plain HTTP if a request reaches it. Turning the socket process on makes this xpcshell close the pipe and write no error. Do not add another QUIC stack.

## System frameworks

The shell links CoreFoundation, CoreGraphics, CoreText, ImageIO, and Security. SHA, AES-CBC, AES-GCM, and HMAC use CommonCrypto. ECDSA and ECDH use Security.framework. GCM oneshot symbols are declared locally because the macOS 26 SDK headers omit them.

Do not add OpenSSL, FreeType, fontconfig, libpng, libjpeg, or libwebp. Image decode is ImageIO. Font metrics and glyph masks are Core Text. SVG path bounds are Core Graphics.

POSIX changes stay on the non-Windows side of the existing `#ifndef _WIN32` split. The Windows `#else` body stays byte-for-byte.

## What already works

- Unix-socket frames, worker `posix_spawn`, and the C ABI client. The client install name is `@rpath/libruyijs_client.dylib`, with `@executable_path` on the rpath. `ruyijs_create`, `ruyijs_call`, and `ruyijs_close` still resolve.
- Core Text font metrics and glyph masks. Canvas fills and `fillText` use Firefox's own Skia objects from the browser donor (`gfx/skia`). The typeface is `SkMakeTypefaceFromCTFont` on the Core Text font that face already holds. A solid fill was non-empty, and `fillText` left ink.
- ImageIO still images: PNG, JPEG, GIF, WebP, BMP, and ICO, premultiplied BGRA. A multi-frame GIF uses ImageIO's already composited display frames.
- SVG path bounds for M, L, C, Q, A, and Z.
- WebCrypto SHA, AES-CBC, AES-GCM, HMAC, ECDSA, and ECDH. An ECDH parameter-dictionary failure returns false, and the caller surfaces `OperationError`.
- Direct HTTPS through `libnecko_core.dylib`. `Context()` and `Context(tls="100")` each opened `https://example.com/` at status 200 and `h2`, and the handshake counts followed `tls=`. The Akamai example has passed with `init_real` set, over HTTP/2, on the earlier xpcshell path.
- `Context(tls=)` is copied to `NECKO_FIREFOX`. The library reads it for direct HTTPS. `necko_fetch.js` still does not. A proxy stays on xpcshell. The result is in `docs/tls-profile.md`.

CreepJS passed again after the adapter relink, including `svg` and `canvas 2d`, with no failed section.

## Known differences

These surfaces now call the same system and Firefox code the donor uses. They are not a second image or font stack.

| Surface | macOS behavior |
| --- | --- |
| Animated GIF | ImageIO display frames. Disposal is already applied, so a frame Windows clears to transparent can keep the background color. After decode, the repetition count is the GIF metadata value. |
| APNG and animated WebP | ImageIO frames, then the shared blend. Scaling uses Core Graphics and leaves pixels premultiplied. |
| Image color | The display ICC comes from `CGDisplayCopyColorSpace` and `CGColorSpaceCopyICCData`, then Firefox qcms. No profile leaves the pixels unchanged. |
| Canvas text | `CTFontCreatePathForGlyph` still builds outlines. Skia `fillText` uses `SkMakeTypefaceFromCTFont` on that same Core Text font. |
| Font alias | `Osaka-Mono` maps to `Osaka` only when the PostScript name and the family both match. That is Firefox's single-face list, not a new table. |
| COLR and CPAL | COLR version 0 and CPAL palette 0. Table bytes come from `CTFontCopyTable`. COLRv1 is not implemented. |
| Glyph gamma | `SystemTextGamma` stays 1.8 and `SystemTextContrast` stays 1.0. `gasp` and `maxp` are readable. The mask does not invent a DirectWrite rendering mode. |

`src/web_media/image_codec.cpp` and `src/web_crypto/digest_backend.cpp` are real ImageIO and CommonCrypto code. They are not stubs.

The DataDome example's homepage result is a server score. It is not a missing Mac binding, and the example's pass condition stays as it is.

## Comparison with Windows

Reviewed against upstream `a626670`. That range includes the 0.2.1 shell, the CSSOM and iframe timing commits, and the AGPL-3.0-only license change. A row marked "same source" is unfinished on both platforms. This table describes the merged macOS shell. The previous local wheel and runtime zip were not rebuilt, and that zip is not a 0.2.1 archive.

| Surface | Windows | macOS |
| --- | --- | --- |
| JS context, DOM, fingerprint, clock, Python bridge | Implemented | Same API |
| Necko HTTP/1.1 and HTTP/2 | Implemented | Direct HTTPS pages this round returned status 200 and version 20 (`h2`) for 100 and 159. HTTP/1.1 is written when ALPN is `http/1.1`. That parser was unit-tested. A later direct-TLS check of `https://www.gnu.org/` returned status 200 and version 11. Direct cleartext uses the recovered HTTP/1 exchange and, on a local server, returned status 200 and version 11. xpcshell itself still returns `necko 804b0033` |
| HTTP/3 | `necko_quic.dll` and Neqo 0.32.0 | `libnecko_quic.dylib` uses the Firefox 159 Neqo 0.32.0 crates and donor NSS 3.131. Same-process proof: first response version 11, second request version 30 after response `Alt-Svc`. A bad certificate failed. Public `quic.nginx.org` went from version 20 to version 30 with the same body. The socket process stays off. The page shell starts a new oneshot per request unless `--session` is used, so the cache does not cross those processes |
| TLS profiles 100–159 | `Context(tls=)` | Direct HTTPS uses `libnecko_core.dylib`. Pages 100 and 159 followed the profile counts: 17 ciphers, 6 groups, and 2 key shares for 100, and 15, 5, and 3 for 159. Eight boundary library GETs matched the table and returned 200 and `h2`. Omitted headers and HTTP/2 settings follow the 0.2.1 table. Pages were not opened for all sixty versions (`docs/tls-profile.md`) |
| WebCrypto SHA, AES-CBC, AES-GCM, HMAC, ECDSA, ECDH | CNG | CommonCrypto and Security.framework |
| Still images: PNG, JPEG, GIF, WebP, BMP, ICO | WIC | ImageIO, premultiplied BGRA |
| Animated GIF | WIC disposal. A cleared frame can be transparent | ImageIO display frames. That area can keep the background color. Repetition comes from the GIF metadata |
| APNG and animated WebP | Shared blend | ImageIO frames, then the shared blend. Core Graphics scaling leaves pixels premultiplied |
| Image color | Display profile, then qcms | `CGDisplayCopyColorSpace` and `CGColorSpaceCopyICCData`, then qcms. No profile leaves the pixels unchanged |
| Canvas text path and `fillText` | Skia, DirectWrite typeface | Skia objects from the browser donor. The typeface is `SkMakeTypefaceFromCTFont`. Outlines still use `CTFontCreatePathForGlyph` |
| `strokeText` | Paints nothing | Paints nothing |
| Font alias | The platform font list | `Osaka-Mono` maps to `Osaka` when the PostScript name and the family both match. That entry is Firefox's single-face list |
| COLR and CPAL | Version 0, palette 0 | Version 0, palette 0. Table bytes come from `CTFontCopyTable` |
| Glyph gamma | DirectWrite rendering mode | `SystemTextGamma` is 1.8 and `SystemTextContrast` is 1.0. `gasp` and `maxp` are readable |
| WebTransport | Shared C++ frames. The helper modules are in the tree. A 0.2.1 install still uses `necko._http3.Connection` and `necko_quic.dll` | Same frames on `libnecko_quic.dylib`. `webtransport=true` only on the long-lived session. Ordinary HTTP/3 stays `webtransport=False`. Hashes stay ECDSA P-256 and at most 14 days. The page shell launched `oneshot --webtransport` against local `127.0.0.1:9443`: negotiated, status 200, and datagram `ping` echoed. Without hashes, PKI verification failed. QUIC ClientHello follows the 100-159 fork points. Local Initial counts matched. Records are not byte-identical, and there is no JA3. |
| iframe document load | Shared source. Necko ticks map through QueryPerformanceCounter | Same source. Ticks map through `steady_clock` milliseconds, as in `page_navigation.cpp`. One iframe document returned status 200 and `h2` |
| WebCodecs, audio decode, WebGPU, customized built-ins, XML, form target | Same source | Interface only, or an explicit unimplemented error |
| DataDome homepage | Server score | Server score. The example pass condition is unchanged |
| Install | PyPI `win_amd64` wheel for 0.2.1 | `macosx_26_0_arm64` wheel and `ruyijs-runtime-macos-arm64.zip` are on the fork release `v0.2.1`. `runtime.json` `macos.url` points at that zip. They are not on PyPI |
| C ABI client | DLL beside the shell | `libruyijs_client.dylib` install name `@rpath/libruyijs_client.dylib`, rpath `@executable_path`. XUL, xpcshell, and NSS names stay as Firefox wrote them |

`image_wic.h` still contains a POSIX stub. macOS decode goes through ImageIO in `image_codec.cpp` and `image_gif.cpp`.

## Follow-up plan

The shell stays linked for `macosx_26_0_arm64`. Linux, Intel Macs, and macOS older than 26 stay outside this plan. New behavior comes from the Firefox donor or the system frameworks already linked: ImageIO, Core Text, Core Graphics, CommonCrypto, and Security.

1. Local cleartext and HTTP/3 now use the Windows export names. The same-process proof passed twice, and public `quic.nginx.org` returned version 30 on the second request. The socket process stays off. Turning the prefs on still makes this xpcshell exit with signal 11 while filling Mac sandbox parameters. WebTransport now uses that same library for one long-lived session. The page shell launched `oneshot --webtransport` against a local echo at `127.0.0.1:9443`: negotiated, status 200, and the datagram `ping` echoed. Without certificate hashes, PKI verification of that certificate failed. A page request without `--session` does not keep the `Alt-Svc` cache. That process boundary comes from the shared oneshot.
2. TLS profiles. Direct HTTPS follows `tls=` for ClientHello counts and for the product header and HTTP/2 settings table. Pages were rechecked for 100 and 159. Record length is not byte-identical. Do not add a JA3 switch.
3. C client install name is done. `otool -D` prints `@rpath/libruyijs_client.dylib`, and the rpath is `@executable_path`.
4. Image pixels. Keep ImageIO. A fixture can record the GIF background difference. Change pixels only when ImageIO already exposes the matching disposal.
5. Font aliases. `Osaka-Mono` to `Osaka` is the only Mac alias. Mac family selection stays the Core Text cascade. The upstream pref-lang, SimSun, and Arial lists stay on the Windows side of `font_collection.cpp`. The Windows cache separator stays the source escape `'\0'`, so the file stays text.
6. Publish. `runtime.json` `macos.url` is the fork release zip for `v0.2.1`. The same release holds `ruyijs-0.2.1-py3-none-macosx_26_0_arm64.whl`. Neither file is on PyPI. A clean install imported. Direct HTTPS returned status 200 and version 20. A local HTTP/3 second request returned status 200 and version 30. `libnecko_quic.dylib` loaded from the packaged `dist/necko` directory. COLRv1, Linux, Intel Macs, and macOS older than 26 stay out.

## What not to commit

Do not commit the Firefox tree, either objdir, `dist/js`, the helper venv, the macOS runtime zip, `libnecko_quic.dylib`, `neqo-server`, `native/necko_quic/Cargo.lock`, cookies, tokens, proxy credentials, or response bodies. The macOS `runtime.json` object stores size, sha256, and the fork-release URL. Do not commit the zip itself.

## Reporting a macOS issue

Include the ruyijs commit, the macOS version, the Xcode version, the Python version, the Firefox tag, and which of the two objdirs the build used.

## Maintainer fork

Upstream `main` stays the Windows release line. On the macOS maintainer fork, fixes land on `release/macos` first and that fork's `main` fast-forwards to the same commit. An upstream update is merged on a branch cut from `release/macos`, reviewed, and merged back. It is not merged into that fork's `main` directly.

## Closed route

Cleartext HTTP and HTTP/3 follow the Windows exports. The notes are in `docs/closed-route.zh-CN.md`. The socket process stays off.
