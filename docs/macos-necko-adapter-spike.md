# Darwin Necko adapter spike

This branch runs the Windows runtime's `necko` / `ruyijs_necko` packages on macOS. It is not a maintenance release. `main`, `release/macos`, `tls/necko-core-arm64`, and `macos-core-proxy` stay where they were. Nothing here is pushed, and no pull request is opened.

Products stay in `dist/necko-adapter/` after `stage_runtime`, with the build tree still under gitignored `build/necko-darwin/`. `vendor/necko-derived/python` and `dist/js` were not replaced. The shell used for the smoke is the existing `dist/js`. When `RUYIJS_NECKO_PYTHON` is unset, `Context` uses `dist/necko-adapter/python/bin/python3` if that interpreter exists, otherwise the build-tree interpreter. An explicit `RUYIJS_NECKO_PYTHON` still wins. The packaged `dist/necko_tls` library remains the fallback when neither interpreter exists. `tools/macos_runtime.py` copies `dist/necko-adapter` into a local runtime zip beside the shell. A local archive built from this checkout is 122860289 bytes, SHA-256 `4984979b66122a934e95977da9917dc87ad1bd6cac31d16b9172ad9fac3c7553`. It contains `dist/js`, `dist/necko-adapter/python`, `libnecko_core.dylib`, and `libnecko_quic.dylib`. The published `runtime.json` pin is unchanged.

## What is installed

`tools/build_necko_darwin.py` downloads the `runtime.json` 0.3.5 Windows archive, checks its size and SHA-256, and extracts the text files of `necko` and `ruyijs_necko` into a Python 3.14 virtualenv. The four Linux patch anchors are applied with `libnecko_core.dylib` instead of `libnecko_core.so`. `ruyijs_necko/oneshot.py` is `src/resource_network/necko_oneshot.py`. Python 3.12 cannot import this adapter: `ruyijs_necko/codec.py` imports `compression.zstd`, which the 3.14 standard library provides. The extracted `_tls_profiles.py` is the same 4883 bytes as `native/necko_tls/necko_win/_tls_profiles.py`.

The core library is `native/necko_posix/src` linked against the existing `../firefox-159/obj-necko` NSS. No NSS rebuild was started. Darwin has no `SOCK_NONBLOCK` or `SOCK_CLOEXEC`, so the socket opens with `fcntl` and `SO_NOSIGPIPE`. `SSL_DataPending` is a local symbol in the folded `libnss3.dylib`, so `necko_tls_pending` reports whether `PR_Recv` can peek one byte. The HTTP/2 pump only tests that sign.

HTTP/3 is `native/necko_posix/quic`, the same Neqo 0.32.0 bridge Linux builds. `tools/build_necko_darwin.py --quic-only` copies `nss-rs` and the Neqo transport sources under `build/necko-darwin/`, adds the `standalone_tls_fd` and transport-parameter hooks there, and links `libnecko_quic.dylib` with the existing obj-necko NSS, `@loader_path`, and `@rpath/libnecko_quic.dylib`. The donor tree and `dist/necko_tls` are not modified. `necko_quic_version` returns the integer 32. A certificate error on the HTTP/3 attempt fails that request; the TCP backup from the adapter's connect race is not returned in its place.

The TLS option set follows the recovered `necko_core.dll` path in `native/necko_tls/necko_tls.c`. NSS defaults already leave OCSP stapling, delegated credentials, post-handshake auth, and the TLS 1.3 compatibility session id off. Enabling them added ClientHello extensions the captured Firefox hellos do not carry.

## What the smoke showed

`hello_probe.py` against the spike dylib, NSS 3.131:

| Profile | Ciphers | Groups | Key shares | Record | SCT | ECH | Compression |
|---|---|---|---|---|---|---|---|
| 100 | 17 | 6 | 2 (32, 65) | 517 | no | no | 0 |
| 159 | 15 | 5 | 3 (1216, 32, 65) | 1832 | yes | yes | 3 |

The extension list and those lengths match `dist/necko_tls/libnecko_core.dylib` for both profiles. Version 159's cipher order matches too. Version 100's cipher order follows the profile table (`0xc00a`, `0xc009`, `0xc013`, `0xc014`). The existing library's preference loop emits `0xc009`, `0xc013`, `0xc00a`, `0xc014` for that profile. The same probe against the spike library also matched the profile table for 119, 132, 135, 151, 154, and 156: cipher, group, and key-share counts were 17/6/2, 17/7/3, 17/7/3, 16/7/3, 15/7/3, and 15/5/3.

Through the existing shell, without setting `RUYIJS_NECKO_PYTHON`, and with `RUYIJS_XPCSHELL` pointed at the donor binary:

- The helper process loaded modules from `build/necko-darwin/python/lib/python3.14/site-packages`.
- `navigate('https://example.com/')` produced `document.title` `Example Domain` and `nextHopProtocol` `h2`. Establishment timing was non-zero: `domainLookupStart` 67, `connectStart` 87, `secureConnectionStart` 266.
- A local server that negotiates only `http/1.1` produced `nextHopProtocol` `http/1.1`.
- `open_response` returned status 302 for a redirect. The page then followed it: `location.href` ended on `/land` and the title was `Landed`.
- An HTTPS request through `http://user:secret@127.0.0.1:<port>` sent one `CONNECT example.com:443 HTTP/1.1` with `Proxy-Authorization: Basic dXNlcjpzZWNyZXQ=`, loaded `Example Domain`, and stayed on `h2`. No new `xpcshell` process appeared. A cleartext request through an HTTP proxy sends `GET http://example.com/ HTTP/1.1`.
- A proxy URL with a path was refused before the proxy saw a connection, and the document title stayed empty. `https://self-signed.badssl.com/` failed certificate validation.

HTTP/3 used a local `neqo-server` and a TCP server that advertised it with `Alt-Svc`. In one adapter session the first response was status 200, version 11, and 64 bytes. The second response was status 200, version 30, and the same 64 bytes. The page shell in one `Context` reported `nextHopProtocol` `http/1.1`, then `h3`. An alternate endpoint whose certificate failed validation made the second request raise `SSLError`; the status was not 200. A remembered `Alt-Svc` is used for the next request instead of being dropped when TCP wins the race. Public `https://quic.nginx.org/` in one `Context` was `h2` with title `NGINX QUIC Preview`, then `h3` with the same title.

WebTransport used the already running echo at `127.0.0.1:9443`. `python -m ruyijs_necko.oneshot --webtransport` returned a begin event of kind 0. With the leaf certificate's SHA-256, the session sent datagram `ping` and received `ping`. With PKI and no hash, verification failed because that certificate has no subjectAltName.

Loading the Firefox roots prints a `CryptographyDeprecationWarning` for one certificate serial that is not positive. The page still verified.

## Still open

- `necko_tls_pending` can say that plaintext is present. It cannot return the buffered byte count while `SSL_DataPending` stays a local symbol. The recovered DLL returns the same 0-or-1 result.
- Firefox 100's cipher order follows the Windows profile table, not the older macOS preference loop.
- This note is the trial record. It does not change `docs/macos.md` or `runtime.json`.
