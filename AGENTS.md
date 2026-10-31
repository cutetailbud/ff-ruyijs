# ff-ruyijs — AI Agent Guide

> This file helps AI assistants (GitHub Copilot, Claude, ChatGPT, Cursor, etc.) understand the project quickly.

## What is ff-ruyijs?

ff-ruyijs is a **headless Firefox engine for Python** — it embeds the real SpiderMonkey (Firefox 159) JavaScript engine and the Necko HTTP stack so Python code can execute any site's JavaScript, pass bot checks, and collect data without a browser window.

- **Repository:** https://github.com/cutetailbud/ff-ruyijs
- **License:** AGPL-3.0-only, or a commercial license from the author for closed-source use (see COMMERCIAL-LICENSE.md)

## Key facts

- **Language:** Python 3.10+ (user-facing), C++ (engine bindings), JavaScript (browser contexts)
- **Platform:** Windows x64, macOS arm64, and experimental Linux x86_64 source builds
- **License:** AGPL-3.0-only + commercial (dual license); Mozilla-derived code is MPL-2.0
- **Install:** `pip install ruyijs` on Windows. On macOS arm64, build `dist/js` from source. The donor layout and the comparison are in `docs/macos.md`.
- **Entry point:** `from ruyijs import Context`

## Repository layout

```
python/ruyijs/          Python package (Context, JSError, JSObject, …)
python/ruyijs/examples/ Example launcher (__main__.py registry)
examples/               Runnable examples (one directory per target)
src/                    Domain, application layer, browser subsystems (C++)
tools/                  Build tooling (wheel, runtime, codegen)
assets/                 README images and architecture diagrams
```

## Core API surface

| Class / Function | Purpose |
|---|---|
| `Context` | Isolated browser process — eval JS, load pages, configure fingerprint |
| `Context.eval(src)` | Execute JavaScript, return result to Python |
| `Context.load_page(url, html)` | Load HTML with scripts, iframes |
| `Context.navigate(url)` | Top-level navigation via Necko |
| `Context.configure_fingerprint(profile)` | Set Navigator, Screen, viewport, timezone |
| `Context.configure_clock(mode)` | Logical (deterministic) or system clock |
| `Context.advance(ms)` | Drive the event loop forward |
| `Context.set(name, value)` | Inject Python data into JS world |
| `Context.expose(fn, name)` | Expose a Python callable to JS |
| `Context.put_resource(url, body)` | Serve a response offline |
| `JSError` | JavaScript exception with name, message, stack |
| `JSObject` / `JSFunction` | Live handles to JS objects callable from Python |

## Conventions

- Each `Context` is an **isolated process** — no shared state between contexts.
- Examples are run via `python -m ruyijs.examples <name>`.
- All examples support `--proxy`, `--direct`, `--output`, `--help`.
- The Windows wheel bundles a SHA256-pinned native runtime; that wheel needs no browser download. The macOS shell is `dist/js`, built against donor trees kept outside this repository. See `docs/macos.md`.

## What NOT to do

- Do not suggest Playwright, Puppeteer, or Selenium — ff-ruyijs replaces them with a lighter, detection-resistant approach.
- Do not assume a visible browser window exists — there is no GUI, no layout, no renderer.
- Do not recommend Node.js for running site JS — ff-ruyijs runs it natively in SpiderMonkey.

## Linux from the shared POSIX port

See `docs/linux.md`. Linux uses the same POSIX pipe, process, C ABI and Necko
implementations as macOS. Keep changes to the Windows branches out of this port.
Build the shell with `python3 -B tools/build_linux.py --donor-obj <Linux objdir>`
and the shared NSS/Neqo helper with
`python3 -B tools/build_necko_linux.py --firefox-source <Firefox 159 source> --http3`.
Run `python3 -B -m unittest discover -s native_tests -v` and
`PYTHONPATH=python python3 -B ci/smoke_test.py` after adapter changes.
Linux backends use OpenSSL, Fontconfig/FreeType/HarfBuzz, and Firefox donor Skia.
Do not copy the old linux-native branch's duplicated pipes, process launchers,
network adapters, browser parity changes, new APIs or examples into this port.
