# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.4.0] - 2026-11-14

### Changed

- Firefox 160.0. The wheels are built from the official Firefox 160.0 release (`firefox/` submodule at
  `FIREFOX_160_0_RELEASE`), and the Windows Necko transport is necko-standalone 0.8.3 (NSS 3.132, Neqo
  0.33.0), with the local HTTP/1.1 and HTTP/3 patches re-applied and both native libraries rebuilt. The
  User-Agent is `rv:160.0 … Firefox/160.0`, `tls=` accepts 100–160 and defaults to 160, and
  `ci/hello_check.py` compares against a ClientHello captured from the official Firefox 160.0 build (the
  same as 159's in every compared field). The macOS runtime is unchanged.
- The web platform surface follows Firefox 160 on Windows: the speech recognition interfaces
  (`SpeechRecognition`, `SpeechGrammar`, `SpeechGrammarList` with their `webkit` aliases,
  `SpeechRecognitionAlternative`, `SpeechRecognitionErrorEvent`, `SpeechRecognitionEvent`,
  `SpeechRecognitionPhrase`, `SpeechRecognitionResult`, `SpeechRecognitionResultList`, and
  `SpeechRecognition.available()` / `install()`); `WebTransport.responseHeaders`;
  `WebTransportDatagramDuplexStream.incomingMaxBufferedDatagrams` / `outgoingMaxBufferedDatagrams` in place of
  the high-water marks; the `Highlight` and `HighlightRegistry` member order. With no recognition service,
  `start()` waits as Firefox does without a model or a microphone.
- Geometry reads after an inline style or attribute change reuse the unchanged parts of the last layout
  instead of laying out the whole page again: 1,500 siblings, `offsetHeight` after `setAttribute` 213 ms
  to 0.5 ms (#13, PR #33 by ladydd).

### Fixed

- Aborting an in-flight `fetch()` with an `AbortController`, or calling `XMLHttpRequest.abort()` / `open()`,
  stops the response transfer: the Necko helper carrying the request ends and the server sees the
  connection close. The request was rejected but its response kept downloading to the end (#43).
- Changing a text node with `data`, `nodeValue`, `appendData`, `replaceData` and the other CharacterData
  methods updates later geometry reads; they returned the layout from before the change.

## [0.3.7] - 2026-11-10

### Fixed

- Page timers, posted messages and animation frames run while the `load` event waits on slow images or style
  sheets. A 50 ms `setTimeout` on a page with a 2 s image ran after the image and after `load`; it now runs
  at about 50 ms, and `load` still fires once the image is done, as in Firefox (#31).

## [0.3.6] - 2026-11-07

### Fixed

- Aborting a fetch that is still in flight rejects it with `AbortError` instead of crashing `js.exe`
  (0xC0000005 on Windows, "pipe closed during a frame" on Linux). Pages that abort pending fetches, such as
  Twitch's login page with its Kasada SDK, now load (#30).
- A pending Web Animations play/pause task and the finish timer run; they never matched their own callback
  before and so never fired.

## [0.3.5] - 2026-11-06

### Changed

- Layout reads a node without the sibling scan or the name strings it does not use (#22), resolves `element.style`
  without a full node snapshot and finds a reflected facade through an index instead of a list walk (#27), and
  reads its known box properties from their slots rather than by name (#29). Geometry is unchanged.
- `getComputedStyle` is cached while the document revision and cascade stamp are unchanged, so repeated reads of
  the same element do not recompute the cascade (#23).
- `querySelectorAll` defers each match's wrapper until it is indexed, and `getElementById` answers from an element
  id index instead of a tree scan (#24). A custom-element upgrade reads only the tag and connection (#25).
- A large `innerHTML` replacement purges the detached tree's dirty pages back to the allocator (#26).

### Fixed

- `getElementById` returns null for an element that was removed from the document but is still referenced by
  script, matching Firefox; the id index no longer reports a detached node (acceptance fix for #24).

## [0.3.4] - 2026-11-05

### Changed

- DOM calls find a node's wrapper from the object's own slot instead of walking every live wrapper (#16), and
  event targets are found through an index instead of a scan; appending a node that is not in a tree no longer
  copies its new parent's child list (#17). Building 8,000 elements with a listener each: 2.3 s instead of 3.4.
- Layout resolves `auto`, `none` and `0px` without parsing them (#18) and finds the properties it reads on every
  box by id, without a second edge pass per child (#19). Geometry is unchanged.
- On Linux and macOS a plain Context starts its Necko helpers on the first request instead of prewarming two,
  and drops them before the shell exits (#20, #21). Windows and Engine sessions keep their prewarm setting.
- `docs/linux-baseline.md` and `ci/linux_baseline.py`: Ubuntu 24.04 build notes and a speed baseline (#15).

### Fixed

- macOS: the xpcshell fallback returns the response headers, and finds a donor build in more checkout layouts
  (#14). Redirects still reach the page one hop at a time on every route.

## [0.3.3] - 2026-11-02

### Changed

- The Necko helpers start from bytecode. The Python bundled for them on Windows and Linux held its standard
  library as sources, and pip compiles the bundled packages for whichever Python installs the wheel (rarely the
  helpers' 3.14), so each helper compiled about 350 modules when it started, and so did each DNS resolver child a
  helper starts for a new host. The runtime now carries bytecode of the bundled Python (unchecked-hash `.pyc`).
  On Windows, with the wheel installed by Python 3.12: a helper starts in 0.55 CPU-seconds instead of 2.4 and
  uses about 3 MiB less (5 MiB after its first HTTPS request); opening 32 Contexts takes 40 CPU-seconds instead
  of 218; the first request to a new host is about 0.45 s faster, and a page that needs many helpers loads
  sooner (github.com: 9.4 s instead of 16.8). `ci/helper_check.py` fails when a helper compiles a module.
- The bundled helper Python leaves out modules the helper cannot use: on Windows Tk, SQLite and winsound
  (5.4 MB); on Linux the static libpython and its build files (73 MB), the build Python's pip, CPython's test
  modules, and the Tk, curses, readline, dbm and SQLite extension modules, with the libraries they brought into
  the bundle (libtcl, libtk, libX11).

### Fixed

- Geometry reads (`offsetHeight`, `getBoundingClientRect()`, `scrollHeight`, ...) after a style change no
  longer restyle elements the change cannot reach, and lay out faster (#13, reported by @jp19980). A child keeps
  its computed style when its parent's change leaves what the parent passes it unchanged (inherited and custom
  properties, display, font sizes), as Firefox does, unless the child `inherit`s a property that does not
  inherit. Layout shares computed styles instead of copying them, finds properties in a sorted table, remembers
  resolved lengths, and keeps a text's measurement while neither the text nor its font changed. With 1,500 divs,
  reading an input's `offsetHeight` after changing a later sibling's padding took 415 ms and now takes 57 ms on
  the Windows machine we measured (500 divs: 132 to 17.6 ms; 100: 28 to 4.7 ms).
- Geometry read after a mutation missed two changes until the page was laid out from scratch: a custom property
  changed on an element did not reach children that read it through `var()`, and a line-height changed from a
  length to a number (`20px` to `1.25`) did not reach children with another font size. `ci/layout_check.py`
  compares geometry read after a mutation with a fresh layout of the mutated page.

## [0.3.2] - 2026-11-02

### Fixed

- `Engine`: sessions no longer wait for each other's memory allocations. Each session thread allocates from a
  jemalloc arena of its own, as Firefox's main thread does, and gives its free pages back when it ends. 32
  sessions loading a page at once took 8.0 s and 156 CPU-seconds, now 2.2 s and 17 (32 Contexts: 1.8 s). Linux
  builds use glibc's allocator, which already gives each thread an arena.

### Changed

- Windows: `js.exe`, its Necko helpers and worker processes start without a console (`DETACHED_PROCESS`), so
  none of them brings a `conhost.exe` of its own (about 6 MiB each). A Context now runs in 3 processes instead of
  6 and uses about 17 MiB less; an engine session uses one process and about 6 MiB less.
  `ci/engine_check.py` checks it.

## [0.3.1] - 2026-11-01

### Added

- `Engine`: many independent Contexts as sessions of one shell process (`engine.context(...)`,
  `Context(engine=...)`, `use_engine(engine)`; docs/engine.md). Each session runs on its own thread with its
  own JSContext, heap, page, clock, cookies, storage, host functions, environment, proxy, TLS profile and
  Necko helpers, in parallel, with the native stack of a process of its own. A timeout or `close()` ends only
  that session. `prewarm`, `network=` (shared helpers and connections) and `max_helpers` bound the helper
  processes: four sessions measured 219 MiB in 5 processes against 385 MiB in 12 for four Contexts.
  `python -m ruyijs.examples --shared NAME ...` runs several examples at once in one engine, and every
  example's `main()` takes an optional argument list.
- macOS: `Context(proxy=...)` works in a wheel without xpcshell (#7 by @Facetomyself). HTTPS opens an
  HTTP CONNECT tunnel and then uses `libnecko_core`; cleartext HTTP sends an absolute-form request through
  the proxy; HTTP/3 does not go through it. With xpcshell present, the proxy stays on the Necko prefs as
  on Windows. Proxy credentials in the URL are not sent on this path.
- macOS: `tools/build_necko_darwin.py` builds the Windows Necko adapter (the `necko` and `ruyijs_necko`
  packages with `libnecko_core.dylib` and the Neqo bridge from `native/necko_posix`) for a local build;
  `Context` uses it when `dist/necko-adapter` or `build/necko-darwin` exists (#9 by @Facetomyself). The
  PyPI wheel is unchanged.
- CI: `ci/hello_check.py` compares each wheel's TLS ClientHello with one captured from Firefox 159.0
  (`ci/firefox159-hello.json`); it fails the Windows, macOS and Linux builds on any difference.
- macOS: text is shaped with Core Text, as Firefox 159 shapes it on macOS, so system fonts' AAT `kern`
  tables and glyph-path ink bounds match Firefox. Windows and Linux keep HarfBuzz (#8 by @isesword).
- `DOMException.prototype` has Firefox's `result`, `filename`, `lineNumber`, `columnNumber`, `data` and
  `stack` accessors, in Firefox's order before the constants. `data` exists on Window only and `stack` is
  `[Replaceable]` (#8 by @isesword).
- `CSS.supports` answers 26 more property/value pairs the way Firefox 159 does, among them `accent-color`,
  `aspect-ratio`, `backdrop-filter`, `container-type`, `text-wrap: balance` and `content-visibility`
  (#8 by @isesword).
- `RUYIJS_CF_ACCESS_LOG=<file>` appends the host-object reads a page makes (navigator and screen fields,
  document visibility, `CSS.supports`, `matchMedia`) to that file; `tools/signal_parity.py` collects the
  same reads from Firefox 159. Off by default (#8 by @isesword).
- Local macOS builds: `tools/build.py --profile firefox-environment-macos.json` bakes a macOS Firefox 159
  profile, and `--necko-nss-lib` links `libnecko_core` against a local NSS. CI and the wheels keep the
  default profile (#8 by @isesword).
- The `truepeople` example: Cloudflare's managed challenge on the Necko stack
  (`python -m ruyijs.examples truepeople`) (#8 by @isesword).

### Changed

- `atob("Charleval")` throws an `InvalidCharacterError` DOMException whose message is `hello,Charleval`,
  on Window and in Workers; every other input behaves as before. Firefox throws its ordinary
  invalid-character error for this string.

### Fixed

- macOS: HTTP/2 responses no longer stall. `necko_tls_pending` peeks at the socket the way the Windows
  `necko_core.dll` does, instead of asking NSS only for already decrypted bytes (#10 by @sbbbsbbb).
- macOS: the ClientHello is Firefox 159's: ALPN offers `h2` first, the profile's cipher order, and
  `status_request`, `delegated_credentials` and a 32-byte legacy session id (#10 by @sbbbsbbb).
- macOS: an HTTP CONNECT response with only a status line is accepted, and `libnecko_quic` initializes NSS
  when its first command arrives before `necko_quic_init` (#10 by @sbbbsbbb).
- Linux: the ClientHello is Firefox 159's. ALPN offers `h2` before `http/1.1` (NSS moves the first protocol
  to the end, so `http/1.1` is passed first) and the `post_handshake_auth` extension is no longer sent.
- macOS: an HTTPS server that negotiates no ALPN is read as HTTP/1.1 on `libnecko_core` instead of falling
  back to xpcshell, which the PyPI wheel does not have (#5 by @Starktomy).
- macOS: closing a WebTransport session keeps its final statistics; they are read before Neqo removes the
  session (#5 by @Starktomy).

## [0.3.0] - 2026-11-01

### Added

- Wheels for Windows x64, macOS arm64 and Linux x86_64 on PyPI, built by GitHub Actions from the official
  Firefox 159.0 source, referenced as the `firefox/` submodule (`mozilla-firefox/firefox` at
  `FIREFOX_159_0_RELEASE`). One run of `.github/workflows/wheels.yml` builds SpiderMonkey, Skia, NSS and
  mozglue on each runner (`ci/firefox/`, `ci/linux/build.sh`), then ruyijs and the wheel, and tests the
  installed wheel in a fresh venv offline (`ci/smoke_test.py`) and online (`ci/network_smoke.py`, one page
  load through Necko). No Firefox tree outside the repository is needed.
- macOS arm64 on PyPI (`macosx_26_0_arm64`, macOS 26 or later). Direct HTTPS uses `libnecko_core.dylib`
  and `libnecko_quic.dylib`, linked against the NSS built from the same source. The wheel has no xpcshell,
  so proxies are not available on macOS yet. `h2`, `cryptography`, `publicsuffixlist` and `backports.zstd`
  join the Darwin dependencies of the Necko helper.
- Linux x86_64 (`manylinux_2_39_x86_64`, glibc 2.39 or later; #5 by @Starktomy): a native ELF shell on the
  POSIX process, pipe and socket layer shared with macOS; fontconfig/FreeType/HarfBuzz text, OpenSSL 3
  WebCrypto, libwebp, LittleCMS2, stb_image and the vendored libavif; the Necko helper on NSS 3.131 /
  NSPR 4.40 and Neqo built from the Firefox source, with its own Python 3.14 (`native/necko_posix/`,
  `docs/linux.md`).
- `docs/macos.md` and `docs/macos.zh-CN.md` record the 0.2.1 comparison. The C client install name is `@rpath`. `Context(tls=)` reaches the xpcshell environment and is not read there. Same-process HTTP/3 can be version 30 after a remembered `Alt-Svc`. The socket process stays off.
- macOS arm64 source build of the shell. SpiderMonkey and Necko stay in an official Firefox donor tree outside this repository. Maintainer steps and the verified HTTP/1.1 and HTTP/2 path are in `docs/macos.md` and `docs/macos.zh-CN.md`.
- macOS image and font adapters on the existing system frameworks: ImageIO display frames, Core Text text paths, the Firefox single-face font alias, COLR version 0, and display ICC through Firefox qcms.
- A local macOS wheel lays the shell out at `_runtime/dist/js` and copies the donor xpcshell closure to `_runtime/dist/necko`, including `libnecko_quic.dylib` when that library exists. The wheel also includes `necko/closed_route.py`, `necko/core_client.py`, and `necko_win`. `requests`, `brotli`, and `zstandard` are Darwin package dependencies. `runtime.json` stores the archive size, sha256, and the fork-release URL. That local wheel keeps the xpcshell closure; the PyPI wheel is built without it.
- `tools/bootstrap_macos_helper.py` creates the gitignored Necko helper venv with Python 3.12 and the packages `requests`, `brotli`, and `zstandard`. An existing venv is left in place.

### Changed

- The Windows runtime's `js.exe` and `mozglue.dll` are built from the submodule on CI. The Necko transport,
  the embedded Python and the MSVC runtime DLLs are carried over from the 0.2.1 runtime archive;
  `SOURCE_NOTICE.md` in the runtime records both.
- `tools/build.py` resolves its path arguments, so relative `--donor-obj` and `--firefox-gfx-obj` paths
  work. `tools/build_necko_quic.py` and the NSPR headers fall back to the `firefox/` submodule.
- On Darwin, `tools/build.py` installs the tracked xpcshell adapter (`oneshot`, package init, timing, client, and adapters) into the helper venv. Windows still receives `oneshot.py` only. A missing helper venv does not block linking `dist/js` and does not create a release archive.
- Darwin canvas links Firefox's Skia objects from the browser donor and builds text typefaces with `SkMakeTypefaceFromCTFont`. The `--timing-v3` trailer now carries eight values. DNS, connect, and TLS slots stay 0 because this adapter does not measure them.

### Fixed

- Firefox 159 parity: CSSOM declarations serialize as Servo does; the cascade handles `@layer`,
  `@property`, flow-relative properties, `!important` longhands over `var()` shorthands and
  blockification; cross-origin iframe navigation timing follows `Timing-Allow-Origin`; a `display:none`
  iframe has no scroll range; `HTMLElement.spellcheck`; text flex items, percentage resolution and
  replaced elements in layout; the OS locale for untagged text, canvas `ctx.lang`, Skia mask gamma and
  colour conversion, SVG text positions, `EXT_clip_control` and `EXT_texture_mirror_clamp_to_edge`.
- The Windows family/language cache key in `font_collection.cpp` is a `'\0'` separator again, so that half compiles. The macOS key stays a newline. `requests`, `brotli`, and `zstandard` stay Darwin dependencies.
- macOS iframe loads map Necko ticks with `steady_clock` milliseconds, the same clock `page_navigation.cpp` already uses. Windows keeps `QueryPerformanceCounter`.

### Changed

- **License:** ff-ruyijs is now dual-licensed under the AGPL-3.0-only or a commercial
  license (`COMMERCIAL-LICENSE.md`), replacing the ff-ruyijs Source Available License
  v1.0. Releases up to 0.2.1 keep the license they shipped with; Mozilla-derived code
  stays MPL-2.0. Contributions are accepted under the AGPL-3.0 plus a relicensing grant
  (`CONTRIBUTING.md`).

## [0.2.1] - 2026-10-30

### Fixed

- Timer precision is Firefox 159's: `performance.now()`, `timeOrigin`,
  `Date.now()`, `Event.timeStamp` and every navigation / resource time are
  clamped to 1 ms with Firefox's jittered rounding (`nsRFPService`), one seed
  per document. `Event.timeStamp` is on the document's performance timeline.
- Navigation and resource timing report the measured DNS, connect and TLS
  phases from Necko (a reused connection reads `fetchStart`, as in Firefox);
  a browser-initiated load's `fetchStart` precedes `navigationStart`.
  `deliveryType` is in Firefox's member order; unreached milestones read 0.
- Necko resolves IP literals without a lookup and caches names
  (`network.dnsCacheExpiration` 60 s, 1600 entries).
- Layout: CSS margin collapsing, 17 px classic scrollbars on the viewport and
  scroll containers, root `clientWidth/Height` and `scrollWidth/Height` from
  the scrollport, `scrollTopMax` / `scrollLeftMax`, `clientTop` /
  `clientLeft`, `window.scrollMaxX/Y`.
- Fonts: SimSun line height (bad-underline metrics), the Arial default when no
  listed family is installed, Firefox's per-script system fallback list.
- SVG text `getBBox`: font box versus frame descent, and no glyph ink below
  20 px (`text-rendering: auto`).
- Example result files no longer ship in the wheel.

## [0.2.0] - 2026-10-29

### Added

- Canvas 2D draws through Firefox 159's own Skia (`gfx/skia`), set up like
  Firefox's `DrawTargetSkia`: fills, strokes, paths, `clip`, gradients,
  patterns, `roundRect`, `drawImage`, shadows and `fillText` / `strokeText`.
- `Context(tls="100".."159")` selects the Firefox release whose TLS
  ClientHello, HTTP/2 settings and HTTP/3 priorities Necko sends.
- `DOMMatrix` / `DOMMatrixReadOnly`: every operation, `fromMatrix`,
  `fromFloat32/64Array`, CSS transform strings, `transformPoint`, `toString`.

### Fixed

- `Origin` and the body `Content-Type` on fetch / XHR / `sendBeacon`;
  `Sec-Fetch-Site` across redirects; `window.originAgentCluster`.
- `window.event` during microtasks queued by an event listener.
- Font fallback by language group, element `lang`, SVG presentation
  attributes and SVG text `getBBox`; `ctx.font` read-back.
- `python -m ruyijs.examples` lists the examples that ship; DataDome output on
  a Chinese Windows console; the Akamai example keeps its User-Agent and TLS
  version together (`--tls`).

### Changed

- The bundled runtime is rebuilt on Firefox 159 (SpiderMonkey, Necko, Skia).

## [0.1.9] - 2026-10-29

### Added

- README: document native TLS fingerprint switching across Firefox 100–159
  (Necko + NSS). Both English and Chinese READMEs updated.

## [0.1.8] - 2026-10-28

### Removed

- The `expedia-taap` example. It shipped a real account as its default login;
  the credentials are no longer in the source tree or the package.

### Changed

- Wheels are built from tracked files only, so local untracked example
  directories are no longer packaged. The bundled runtime is unchanged from
  0.1.7.

## [0.1.7] - 2026-10-28

### Fixed

- External scripts, stylesheets and preloads hung until the RPC timeout under
  Wine (#1). Wine loaded its builtin `msvcp140.dll` instead of the MSVC one
  shipped beside `js.exe`; its `condition_variable` waits never woke for
  completions pushed by the background transport threads. Under Wine,
  `Context` now sets the per-program override
  `HKCU\Software\Wine\AppDefaults\js.exe\DllOverrides` to
  `msvcp140=native,builtin` (an existing value is kept; native Windows is
  untouched). The 8 s / 10 s caps added in d33062f are reverted.
  (`python/ruyijs/_context.py`)

### Added

- The native build tools (`tools/build.py` and its modules), the third-party
  native sources compiled into `js.exe` (`vendor/compression-native`,
  `vendor/image-native`, `vendor/urlpattern-native`) and the environment
  baseline `profiles/firefox-environment.json` are public again (#3). Local
  paths are no longer hard-coded; pass `--donor-obj` etc. or the matching
  `RUYIJS_*` environment variables. Without a Firefox browser build, the
  recorded `tools/generated/` headers stand in for its font preferences and
  WebIDL member order, so a SpiderMonkey build of `FIREFOX_158_0_RELEASE` is
  enough (see `tools/README.md`).
- The page's `ruyijs` helper object (`createHostFunction`,
  `createHostCollection`, ...) is defined in `src/runtime/host_helpers.cpp` when
  the SpiderMonkey donor has no call tracing, so `src/` builds against official
  Firefox sources; a donor with tracing keeps its own implementation.
- The prebuilt runtime is attached to the GitHub release
  (`ruyijs-runtime-0.1.7-win-amd64.zip`), and `runtime.json` downloads it from
  there.

### Changed

- Bundled runtime rebuilt (`js.exe` SHA-256 `766e3742…`); build-machine paths
  are no longer embedded in `js.exe`, `necko_quic.dll` or the runtime
  manifests.

## [0.1.5] - 2026-10-27

### Fixed

- Host-initiated `Context.navigate()` deadlocked under Wine: the navigation was
  queued as SpiderMonkey tasks (`DeliverNavigation` / `DeliverCommit`) that are
  never delivered between host RPC calls under Wine, so `advance` waited on the
  pending task until the RPC timeout. The host `navigate` action now commits
  synchronously on the call, the same way the host `load` action already does
  (which is why `load_page` was unaffected). In-page navigation (`location.href`,
  `location.assign`, reload) and form submission keep Firefox's async two-task
  model, so the calling script still continues first. Reported in #1.
  (`src/runtime/page_navigation.cpp`, `src/runtime/page_load.*`)

### Changed

- Bundled runtime rebuilt with the navigation fix (`js.exe` SHA-256 `30581e7a…`)
  and re-pinned in `runtime.json`; the embedded Python and other runtime
  components are unchanged.

## [0.1.4] - 2026-10-26

### Fixed

- `python -m ruyijs.examples <name>` failed with "示例文件不存在" from an installed
  wheel: the example locator used a sentinel file (`turnstile/turnstile.py`) that is
  not shipped, so it never recognized the installed layout and looked in the wrong
  directory. It now uses `google/google_search.py`, which ships. Affected every
  example when run from the installed package.

### Removed

- `datadome` and `turnstile` entries from the examples CLI registry; their files are
  not distributed, so they could never run.

## [0.1.2] - 2026-10-22

### Added

- Examples ship inside the wheel as `ruyijs.examples`; run `python -m ruyijs.examples` to list them.
  DataDome offline replay captures are not included.

### Changed

- Bundled runtime rebuilt from the current sources (`js.exe` SHA-256 `0b827e48…`).

## [0.1.1] - 2026-10-22

### Added

- `Context.navigate(url)`: a browser-initiated top-level navigation (no Referer, `Sec-Fetch-Site: none`).

### Changed

- Bundled runtime rebuilt from the current SpiderMonkey shell (`js.exe` SHA-256 `c1e89f22…`), including
  the parser and event fixes made after 0.1.0.
- Published on PyPI as a Windows x64 wheel: `python -m pip install ruyijs`.

## [0.1.0] - 2026-10-21

### Added

- Installable Windows x64 Python wheel with a bundled, SHA256-pinned runtime.
- Source installation through Gitee, version metadata and `get_runtime_path()`.
- Installed-package examples and Python 3.10 / 3.14 integration verification.

- First public release of ruyijs: a SpiderMonkey (Firefox 158) runtime for Windows x64 with a
  Python binding, the `ruyijs_client.dll` C ABI and a CLI.
- A native browser environment across 38 bounded contexts, with a validated fingerprint/profile
  configuration.
- Page loading with iframes, a deterministic clock and event loop, trusted input, and console and API logs.
- The Necko network transport (HTTP/1.1, HTTP/2, HTTP/3) with per-context proxies.
- A portable release export with embedded Python, licenses, a source archive and checksums.
