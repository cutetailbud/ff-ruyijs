<div align="center">

# 🦊 ruyiJS

### Run any site's JavaScript from Python — with the real Firefox engine.

**A new-generation, minimalist browser purpose-built for AI-driven automation.**<br>
The authentic Firefox JavaScript engine and networking stack, with no window and no renderer —<br>
made for web scraping, JS reverse engineering, data collection, and getting past bot checks.

[![PyPI](https://img.shields.io/pypi/v/ruyijs?label=pypi&color=111111&logo=pypi&logoColor=white)](https://pypi.org/project/ruyijs/) [![Downloads](https://img.shields.io/pepy/dt/ruyijs?label=downloads&color=111111&logo=pypi&logoColor=white)](https://pepy.tech/projects/ruyijs) [![Python](https://img.shields.io/badge/Python-%E2%89%A53.10-111111?logo=python&logoColor=white)](https://pypi.org/project/ruyijs/) [![License](https://img.shields.io/badge/license-AGPL--3.0%20%2B%20commercial-111111)](LICENSE) [![Windows](https://img.shields.io/badge/Windows%20x64-0078D4?logo=windows&logoColor=white)](https://pypi.org/project/ruyijs/) [![macOS](https://img.shields.io/badge/macOS%20arm64-000000?logo=apple&logoColor=white)](https://pypi.org/project/ruyijs/) [![Linux](https://img.shields.io/badge/Linux%20x86__64-FCC624?logo=linux&logoColor=black)](https://pypi.org/project/ruyijs/) [![LinuxDO](https://img.shields.io/badge/LinuxDO-community-FFB003?logo=discourse&logoColor=white)](https://linux.do/t/topic/3078486)

<sub>Sponsored by</sub>

<a href="https://www.fastaitoken.com"><img src="assets/sponsor-fastaitoken.png" height="42" alt="FastAIToken"></a>&nbsp;&nbsp;&nbsp;<a href="https://www.fastaitoken.com" title="Claude"><img src="assets/models/claude.svg" width="42" height="42" alt="Claude"></a>&nbsp;&nbsp;<a href="https://www.fastaitoken.com" title="GPT"><img src="assets/models/gpt.svg" width="42" height="42" alt="GPT"></a>&nbsp;&nbsp;<a href="https://www.fastaitoken.com" title="Grok"><img src="assets/models/grok.svg" width="42" height="42" alt="Grok"></a>&nbsp;&nbsp;<a href="https://www.fastaitoken.com" title="DeepSeek"><img src="assets/models/deepseek.svg" width="42" height="42" alt="DeepSeek"></a>&nbsp;&nbsp;<a href="https://www.fastaitoken.com" title="GLM"><img src="assets/models/glm.svg" width="42" height="42" alt="GLM"></a>

<sub>Contributors</sub>

<a href="https://github.com/Facetomyself" title="Facetomyself · #4 macOS arm64 shell, #6 QUIC profiles, #7 macOS proxy, #9 macOS Necko adapter, #14-21 DOM and layout speed, #22-29 DOM, layout and style speed"><img src="assets/contributors/Facetomyself.png" width="48" height="48" alt="Facetomyself"></a>&nbsp;&nbsp;<a href="https://github.com/Starktomy" title="Starktomy · #5 Linux runtime"><img src="assets/contributors/Starktomy.png" width="48" height="48" alt="Starktomy"></a>&nbsp;&nbsp;<a href="https://github.com/isesword" title="isesword · #8 macOS Core Text shaping and Firefox parity"><img src="assets/contributors/isesword.png" width="48" height="48" alt="isesword"></a>&nbsp;&nbsp;<a href="https://github.com/sbbbsbbb" title="sbbbsbbb · #10 macOS TLS and HTTP/2"><img src="assets/contributors/sbbbsbbb.png" width="48" height="48" alt="sbbbsbbb"></a>

**[Install](#-installation)** &nbsp;·&nbsp; **[Features](#-features)** &nbsp;·&nbsp; **[Verification systems](#-verification-systems-it-defeats)** &nbsp;·&nbsp; **[Examples](#-examples)** &nbsp;·&nbsp; **[FAQ](#-faq--troubleshooting)** &nbsp;·&nbsp; **[Contact](#-contact)** &nbsp;·&nbsp; **[LinuxDO](https://linux.do/t/topic/3078486)** &nbsp;·&nbsp; **English** · [简体中文](README.zh-CN.md)

> **⚠️ Disclaimer:** This project is for researching next-generation AI browsers. Any form of brute-force reverse engineering is strictly rejected. The source code is open source under the AGPL-3.0; closed-source or proprietary use requires a commercial license.

<br>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/architecture-dark.svg">
  <img src="assets/architecture.svg" width="880" alt="ruyiJS architecture: your Python code drives a ruyiJS Context — an isolated process holding the SpiderMonkey JS engine, 38 browser contexts, and the Necko network stack — which talks to the target site.">
</picture>

</div>

<br>

## Is ruyiJS for you?

Headless Chrome and Playwright hand you a whole browser: a GPU-rendering, window-managing, memory-hungry process that anti-bot vendors have spent years learning to detect. ruyiJS takes the opposite path — it embeds only the Firefox internals that matter for automation and leaves the rest behind:

- **The real engine, not an impersonation.** Genuine SpiderMonkey (Firefox 160) and the Necko network stack execute a site's JavaScript, challenges, signatures, and fingerprints exactly as Firefox would.
- **Truly headless.** No window, no layout, no renderer. A pure JavaScript + Web API environment that starts fast and stays light.
- **Isolated and reproducible.** Each `Context` is its own process with a persistent JavaScript world and its own proxy, so sessions never leak into each other.
- **Offline when you need it.** Serve scripts and responses yourself and replay a whole captured flow with no network.

If you scrape, reverse-engineer web JavaScript, or collect data behind a challenge, it's for you. If a plain HTTP client already gets your pages, you don't need it.

Wheels for **Windows x64**, **macOS arm64** and **Linux x86_64** are on PyPI for **Python 3.10+**. GitHub Actions builds each of them from the official Firefox 160.0 source ([build from source](#build-from-source)).

## 📦 Installation

```sh
python -m pip install ruyijs
```

**Requirements:** 64-bit Python 3.10+ on Windows x64, on macOS 26 or later (Apple Silicon), or on Linux x86_64 with glibc 2.39 or later (Ubuntu 24.04, Debian 13, Fedora 40 and newer).

The Windows wheel ships the SHA256-pinned SpiderMonkey + Necko runtime (~42 MB). Local scripts run offline right after install — no browser download, no external dependencies.

**macOS arm64:** the `macosx_26_0_arm64` wheel loads pages over direct HTTPS through `libnecko_core.dylib` and `libnecko_quic.dylib`, linked against the NSS of the same Firefox source. It has no xpcshell. `Context(proxy=...)` uses an HTTP CONNECT tunnel into `libnecko_core` when that binary is absent. The Necko helper runs on the Python that installed ruyijs; its packages are dependencies of the macOS wheel. Details and the remaining gaps are in [docs/macos.md](docs/macos.md).

**Linux x86_64:** the `manylinux_2_39_x86_64` wheel is a native ELF build, contributed by [@Starktomy](https://github.com/Starktomy) in #5. Text uses fontconfig, FreeType and HarfBuzz with the fonts installed on the machine, WebCrypto uses OpenSSL 3, and the Necko helper (NSS 3.131, Neqo for HTTP/3) brings its own Python. See [docs/linux.md](docs/linux.md).

To upgrade:
```sh
python -m pip install --upgrade ruyijs
```

To verify the installation:
```python
import ruyijs
print(ruyijs.__version__)   # 0.4.0
```

### Build from source

```sh
git clone --recurse-submodules https://github.com/cutetailbud/ff-ruyijs
```

The `firefox/` submodule is the official Firefox 160.0 release (`mozilla-firefox/firefox` at `FIREFOX_160_0_RELEASE`, a shallow clone). The workflows that build the PyPI wheels are the reference for a local build: [`.github/workflows/wheels.yml`](.github/workflows/wheels.yml) runs `ci/firefox/build.sh js|browser|toolchain` and `tools/build.py` on Windows and macOS, and the steps of `ci/linux/build.sh` on Linux.

## 🚀 Quick start

**1 · Open a context and run JavaScript.** Variables and functions persist across `eval` calls within the same context.

```python
from ruyijs import Context

with Context() as context:
    context.eval("let count = 40")
    print(context.eval("count += 2"))        # 42
    print(context.eval("navigator.userAgent"))
```

**3 · Point it at a real target.** Configure the environment, load the page, and let the site's own JavaScript mint whatever token it demands.

```python
with Context(proxy="http://127.0.0.1:7890") as context:
    context.configure_fingerprint({
        "navigator": {"language": "zh-CN", "hardwareConcurrency": 8},
        "locale": {"timezone": "Asia/Shanghai"},
    })
    context.load_page("https://target.example/", html)
    token = context.eval("window.__sign(payload)")   # run the page's signer
    print(token)
```

## 🏗️ How it fits together

```
Python  ──►  ruyiJS Context (isolated process)  ──►  Target site
                 ├─ SpiderMonkey — Firefox 160 JS engine
                 ├─ 38 browser contexts — DOM · Canvas · Workers · iframes · …
                 ├─ Skia — Firefox 160 2D graphics behind Canvas
                 └─ Necko — HTTP/1.1 · 2 · 3, per-context proxy
```

Your Python code drives a `Context`. Inside it, the genuine SpiderMonkey engine runs the page's scripts against a real DOM and 38 bounded browser subsystems, while in-page `fetch`/XHR ride the Necko transport. The implementation lives under [`src/`](src) (domain, application, and every browser subsystem) and [`python/ruyijs/`](python/ruyijs) (the binding).

## ✨ Features

### Run a site's JavaScript

Execute scripts in a persistent context, load real HTML with iframes and classic scripts, and query the resulting DOM. References to live JS objects come back as `JSObject` / `JSFunction` you can call from Python.

| Capability | How |
| :-- | :-- |
| Execute JS, keep state | `context.eval(src)` — variables and functions persist |
| Get a callable back | `context.eval(src, to_py=False)` → `JSFunction` |
| Load a page | `context.load_page(url, html)` — relative URLs resolve against `url` |
| Top-level navigation | `context.navigate(url)` — no Referer, `Sec-Fetch-Site: none` |
| Query the DOM | `context.eval("document.querySelector('#x').textContent")` |

```python
with Context() as context:
    context.set("config", {"offset": 2})
    add = context.eval("(value) => value + config.offset", to_py=False)
    print(add(40))  # 42
```

### Configure the browser environment

Set Navigator, Screen, viewport, timezone, and locale through one validated profile surface. Commit it before the first page script runs.

```python
with Context() as context:
    context.configure_fingerprint({
        "navigator": {"language": "zh-CN", "hardwareConcurrency": 8},
        "screen": {"width": 1920, "height": 1080},
        "viewport": {"width": 1280, "height": 720},
        "locale": {"timezone": "Asia/Shanghai"},
    })
    print(context.eval("[navigator.language, screen.width]"))  # ['zh-CN', 1920]
```

### Native network over Necko

In-page `fetch` and XHR ride real HTTP/1.1, 2, and 3, through a proxy scoped to the context. Serve responses yourself with `put_resource()` to replay an entire captured flow offline. Inspect every request with `net_log()`. On macOS, pages load over direct HTTPS. A proxy uses the donor xpcshell when it is present, and an HTTP CONNECT tunnel into `libnecko_core` when it is not (`docs/macos.md`).

### Native TLS fingerprint — switchable across Firefox 100 – 160

The network stack is Firefox's own **Necko + NSS** (the same TLS library real Firefox uses). The TLS ClientHello — cipher suites, extensions, ALPN, key shares, and their ordering — is produced by the real NSS library, not an emulation, so bot-detection systems that inspect TLS cannot distinguish ruyiJS traffic from a genuine Firefox session.

The Necko transport can **freely switch between Firefox 100 – 160 TLS profiles**. Each profile reproduces that version's exact ClientHello, giving you a different JA3 / JA4 fingerprint on the wire — useful for rotating browser identities, evading per-version blocks, or testing how a site reacts to different Firefox releases.

Choose the release per context with `tls=` (`"100"`–`"160"`; default 160). On Windows this sets the TLS ClientHello, HTTP/2 settings and HTTP/3 priorities of that context's Necko transport; set a matching user agent with `configure_fingerprint` when the page should agree. On macOS, direct HTTPS uses arm64 `libnecko_core.dylib` when that library reports NSS 3.131. The shell prefers `dist/necko_tls/libnecko_core.dylib`, then the staged `dist/necko` copy. An existing `NECKO_CORE_LIBRARY` wins. `Context()` and `Context(tls="100")` both opened `https://example.com/` at status 200 and protocol `h2`. The measured handshake was 15 ciphers, 5 groups, and 3 key shares for 159, and 17, 6, and 2 for 100. Library GETs at the cluster boundaries 100, 119, 132, 135, 151, 154, 156, and 159 also returned 200 and `h2`, and the counts matched the profile table. The other versions were not each opened as a page. Omitted request headers and the HTTP/2 settings follow the 0.2.1 product table for that `tls=` value. A header the caller sets wins. HTTP/3 fields in that table are unused, and HTTP/3 is unchanged. A proxy, a missing library, or a different NSS build stays on the donor xpcshell. The proxy was not retested in this run. Plain HTTP still returns `necko 804b0033` from that donor (`docs/tls-profile.md`). The PyPI macOS wheel has no xpcshell, so the proxy case is not available there.

```python
with Context(proxy="http://127.0.0.1:7890", timeout=30, tls="120") as context:
    context.load_page("https://example.com/", "")
    context.eval("globalThis.r = null; fetch('/api').then(x => x.json()).then(v => r = v)")
    context.advance(25)               # drive the event loop until it settles
    print(context.eval("r"))
```

### Canvas on Firefox's native Skia

Canvas 2D draws through Firefox's own official **Skia** (`gfx/skia` from the Firefox 160 release), not a re-implementation.

### Python ↔ JavaScript bridge

Pass data in with `set()`, get callable JS functions out, and expose Python functions to the running script. Workers, Service Workers, and iframes are included.

```python
with Context() as context:
    context.expose(lambda a, b: a + b, "add")
    print(context.eval("add(20, 22)"))  # 42
```

### Many independent sessions in one process

A `Context` starts its own process. An `Engine` runs many Contexts as sessions of one process instead, each on its own thread with its own heap, page, clock, cookies, storage, proxy and TLS profile, in parallel. `prewarm` and `network=` trim the Necko helper processes, which are most of a Context's memory: four sessions measured 221 MiB in 5 processes against 378 MiB in 12 for four Contexts, and 151 MiB in 3 when they share one network ([docs/engine.md](docs/engine.md)).

```python
from ruyijs import Context, Engine, use_engine

with Engine() as engine:
    a = engine.context(proxy="http://127.0.0.1:7890")
    b = engine.context(tls="120")
    with use_engine(engine):
        c = Context()          # code that creates its own Contexts joins the engine too
```

### Deterministic clock

Drive timers with a logical clock for perfectly reproducible runs, or use the system clock for live work. The host advances time explicitly.

```python
with Context() as context:
    context.configure_clock("logical", epoch_ms=1700000000000)
    context.eval("globalThis.result = 0; setTimeout(() => { result = 42 }, 10)")
    context.advance(10)
    print(context.eval("result"))  # 42
```

### Errors and logs

JavaScript exceptions surface as `JSError` with name, message, and stack. `console_log()` and `net_log()` return the captured console and network records.

```python
from ruyijs import Context, JSError

with Context(timeout=10) as context:
    try:
        context.eval("throw new TypeError('bad input')", filename="demo.js")
    except JSError as error:
        print(error.name, error.message)  # TypeError bad input
        print(error.stack)
```

## 🛡️ Verification systems it defeats

Each example drives the site's own JavaScript to produce the parameters it demands, then completes the flow end to end. All ship runnable in the package: `python -m ruyijs.examples <name>`.

| System | Category | Example |
| :-- | :-- | :-- |
| **Akamai Bot Manager 3.x** | sensor_data + sbsd challenge | `akamai` |
| **DataDome** | Interstitial device-check (plv2 + plv3) | `datadome` |
| **CreepJS** | Fingerprint audit — passes every section | `creepjs` |

> 🔒 **Strictly prohibited for any unlawful use.** ruyiJS is provided **solely for research into a new generation of AI browsers.** Live results depend on current network conditions and each site's present responses.

## 🧪 Examples

Examples ship inside the package — no need to clone the repo:

```sh
python -m ruyijs.examples                      # list every example and its install path
python -m ruyijs.examples akamai               # Akamai Bot Manager 3.x
python -m ruyijs.examples datadome             # DataDome interstitial device-check
python -m ruyijs.examples creepjs              # CreepJS fingerprint audit
python -m ruyijs.examples --shared akamai datadome creepjs   # several at once, one shared engine
```

Every example supports `--direct` (direct connection) and `--proxy http://127.0.0.1:7890`. Add `--help` after a name for all options, and `--output result.json` to save results. Each example directory ships its own README.

## 📚 Documentation & API

| | |
| :-- | :-- |
| **`Context.eval` / `set` / `expose`** | Execute JS, pass data in, expose Python callbacks |
| **`load_page` / `navigate` / `put_resource`** | Load HTML, top-level navigation, serve resources offline |
| **`configure_fingerprint` / `configure_environment`** | Navigator, Screen, viewport, timezone, locale |
| **`configure_clock` / `advance` / `tick`** | Logical vs. system clock, drive the event loop |
| **`JSError` / `console_log` / `net_log`** | Errors as exceptions, console and network records |
| **`JSObject` / `JSFunction` / `get_runtime_path`** | Live JS handles, bundled-runtime path |
| **`Engine` / `engine.context` / `use_engine`** | Many independent Contexts as sessions of one process |

## 📏 Scope & limitations

ruyiJS provides JavaScript execution and a broad Web API environment on **Windows x64**, **macOS arm64** and **Linux x86_64**. It has **no browser window and no full page layout or rendering** — it is a tool for automation, analysis, and research. Use it in compliance with the terms of service of any site you interact with and with applicable law. The platform differences are in [docs/macos.md](docs/macos.md) and [docs/linux.md](docs/linux.md).

**Related:** [**ruyiHttp**](https://gitee.com/losenine888/ruyiHttp) — a Requests-style Python HTTP library built on Firefox's native networking components.

## ❓ FAQ / Troubleshooting

<details>
<summary><b>What Python versions are supported?</b></summary>

64-bit Python 3.10 and above on every platform, tested up to Python 3.14. On macOS the Necko helper runs on the interpreter that installed ruyijs; the Linux wheel brings its own Python 3.14 for it.
</details>

<details>
<summary><b>Does it work on macOS or Linux?</b></summary>

Yes. PyPI has a `macosx_26_0_arm64` wheel (macOS 26 or later) and a `manylinux_2_39_x86_64` wheel (glibc 2.39 or later). The macOS wheel has no xpcshell; since 0.3.1 `Context(proxy=...)` uses an HTTP CONNECT tunnel there. See [docs/macos.md](docs/macos.md) and [docs/linux.md](docs/linux.md).

The JS engine is Mozilla's official **SpiderMonkey** and the networking stack is Mozilla's official **Necko** — both are built from the official Firefox source, referenced as the `firefox/` submodule.
</details>

<details>
<summary><b>Do I need to download a browser?</b></summary>

No. Every wheel bundles the SpiderMonkey + Necko runtime built from the Firefox 160.0 source, and `pip install ruyijs` needs no browser. On macOS, pip also installs the Python packages of the Necko helper.
</details>

<details>
<summary><b>Context hangs or doesn't respond</b></summary>

- Make sure you use the `with` statement (`with Context() as ctx:`) so the process is cleaned up properly.
- If using a proxy, verify it is reachable: `context = Context(proxy="http://127.0.0.1:7890")`.
- Increase the timeout: `Context(timeout=60)`.
- On Wine, upgrade to ≥ 0.1.5 which fixes a known navigate deadlock.
</details>

<details>
<summary><b>JSError: script error</b></summary>

The site's JavaScript threw an exception. Inspect `error.name`, `error.message`, and `error.stack` to locate the cause. If the page relies on APIs not yet implemented, the stack trace will point to the missing call.
</details>

<details>
<summary><b>How do I use a proxy?</b></summary>

```python
with Context(proxy="http://127.0.0.1:7890") as ctx:
    ctx.navigate("https://example.com/")
```

HTTP, HTTPS, and SOCKS5 proxies are supported per-context.
</details>

<details>
<summary><b>How do I run examples?</b></summary>

```sh
python -m ruyijs.examples              # list all examples
python -m ruyijs.examples akamai --proxy http://127.0.0.1:7890
python -m ruyijs.examples creepjs
```
</details>

## 💬 Contact

Questions, permission requests, commercial licensing, or just to talk shop — add the author on WeChat, or scan to join the discussion group.

<table>
<tr>
<td align="center"><img src="assets/wechat-qr.jpg" alt="WeChat QR — 如意" width="220"><br><b>WeChat:</b> 如意</td>
<td align="center"><img src="assets/wechat-group-qr.jpg" alt="Group — AI浏览器ruyijs过检测交流群3" width="220"><br><b>Group:</b> AI浏览器ruyijs过检测交流群3</td>
</tr>
</table>

> The group QR refreshes periodically; if it has expired, add the author on WeChat (如意) for an invite.

## 📄 License

ff-ruyijs is **dual-licensed**:

- **[AGPL-3.0](LICENSE)** — free for any use, commercial included, as long as you comply: if you distribute ff-ruyijs or a modified version, **or offer it to users over a network** (a hosted service or API), you must publish the complete source of your version under the AGPL-3.0.
- **[Commercial license](COMMERCIAL-LICENSE.md)** — required to use ff-ruyijs in closed-source products or services, or to keep your modifications private.

For a commercial license, contact via WeChat (ID: **如意**) or open an issue on [GitHub](https://github.com/cutetailbud/ff-ruyijs) titled "Commercial license".

Mozilla-derived code (SpiderMonkey, Necko) stays under the MPL-2.0. Releases up to 0.2.1 keep the license they shipped with.

See [`NOTICE`](NOTICE) for third-party attributions.

If ff-ruyijs saved you from reimplementing a browser, a ⭐ helps others find it.
