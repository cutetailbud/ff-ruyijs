# Shared engine: many Contexts in one process

A plain `Context` starts its own shell process (`js.exe`) and its own Necko
helper processes. An `Engine` starts one shell process and runs every Context
you open in it as a **session** on its own thread, with its own JSContext.
Sessions run in parallel and stay as independent as Contexts with a process
of their own; they share the process and the engine's code.

```python
from ruyijs import Engine

with Engine() as engine:
    a = engine.context(proxy="http://127.0.0.1:7890")
    b = engine.context(tls="120", timeout=30)
    a.navigate("https://example.com/")
    a.advance(2000)
    print(a.eval("document.title"), b.eval("navigator.userAgent"))
```

`engine.context(...)` takes the same arguments as `Context(...)` (except
`shell`, which is the engine's), and so does `Context(engine=engine, ...)`.
Existing code that calls `Context(...)` itself becomes engine sessions inside
`use_engine`:

```python
from ruyijs import Engine, use_engine

with Engine() as engine, use_engine(engine):
    run_my_scraper()   # every Context() it creates, from any thread, is a session
```

Use each session from whichever Python thread you like; sessions do not wait
for each other.

## What each session keeps to itself

- its JavaScript heap, page, realms, iframes, Workers (still processes of
  their own, with the session's proxy and TLS profile), clock and event loop;
- cookies, storage, IndexedDB, Cache API, the CORS preflight cache;
- host functions and data (`expose`, `set`, `native`);
- the whole environment configuration: navigator, screen, canvas noise,
  WebRTC, audio, fonts, media features, language and device pixel ratio;
- its proxy and TLS profile, its Necko helper processes, their connections
  and TLS session cache;
- the timer-precision secret and the native stack: recursion goes as deep as
  in a Context with a process of its own.

A session does not see another one's globals, documents, cookies or host
functions, and a server cannot link two sessions through a shared connection
unless you ask for it with `network=` (below).

How deep recursion goes depends on where a process's stack starts, which the
system varies a little from process to process: Windows ASLR starts a main
thread's stack up to 2 KiB lower at random (about a dozen frames), and macOS
also takes a process's command line and environment out of it. Contexts differ
from one another by that much; the sessions of one engine all get the stack of
the engine's process.

## What sessions share

The process, the engine's code, SpiderMonkey's self-hosted runtime data and
helper threads, and operating-system caches such as fonts. With `network=`,
sessions also share Necko helper processes and connections.

Memory is not one of them: each session thread allocates from an arena of its
own, as Firefox's main thread does, so sessions building pages at the same time
do not wait for each other, and a session's free pages go back to the system
when it ends.

## Resource controls

| Option | Effect |
| --- | --- |
| `Engine(prewarm=1)` | Necko helpers each new session starts in the background (0-2; a plain Context on Windows and macOS starts 2, and on Linux starts them on the first request). More start on demand either way; `engine.context(prewarm=2)` sets it per session. |
| `engine.context(network="name")` | Sessions that pass the same name share helper processes, connections and TLS session resumption, like tabs of one browser profile: fewer processes, but a server can link those sessions. They must use the same `proxy` and `tls`. |
| `Engine(max_helpers=N)` | A budget for the helper processes of all sessions together. When it is spent, the least recently used idle helper of another session makes way. |

Measured on Windows with a simple page, four contexts:

| | Processes | Private memory |
| --- | --- | --- |
| four `Context`s | 12 (4 `js.exe`, 8 helpers) | 378 MiB |
| one `Engine`, `prewarm=2` | 9 (1 `js.exe`, 8 helpers) | 352 MiB |
| one `Engine` (default `prewarm=1`) | 5 (1 `js.exe`, 4 helpers) | 221 MiB |
| one `Engine`, one `network` for all four | 3 (1 `js.exe`, 2 helpers) | 151 MiB |

Most of a Context's memory is its helpers and its own JavaScript heap, so the
helper options save the most; a heavier page grows each session's heap the
same way in both modes. Thirty-two contexts loading a page with a 2,000-element
list at once, on 22 cores: 32 `Context`s took 1.8 s with 3.0 GiB of private
memory in 96 processes, 32 sessions 2.2 s with 1.7 GiB in 33, and 32 sessions
on one `network` 3.5 s (their first load starts the shared helpers; later loads
take 0.3 s) with 0.88 GiB in 7. On Windows none of these processes has a console
(`conhost.exe`) of its own.

## Failures

- A request that times out in a session raises `TimeoutError`; the session's
  running script is interrupted and the session ends. Other sessions go on.
- `close()` ends one session; `Engine.close()` closes every session and then
  the process; `engine.status()` lists the sessions still running.
- A crash of the engine process ends all of its sessions. Use separate
  engines, or plain Contexts, where one crash must not take others with it.

## Running the examples in one engine

```sh
python -m ruyijs.examples --shared akamai datadome creepjs truepeople
python -m ruyijs.examples --shared creepjs datadome -- --proxy http://127.0.0.1:7890
```

Each example runs in its own thread as a session of one engine; arguments
after `--` go to every example, and each output line starts with the
example's name.

## Protocol

`js.exe --ruyijs-host PIPE [--ruyijs-max-helpers N]` connects the control pipe
and serves JSON frames with the same framing as `--ruyijs-rpc`:

| Request | Reply |
| --- | --- |
| `{"op": "open", "pipe", "mode", "proxy", "tls", "network", "prewarm"}` | `{"session": id}`; the session connects to `pipe` and serves the Context RPC there |
| `{"op": "status"}` | `[{"session", "running", "network"}]` |
| `{"op": "interrupt", "session": id}` | stops that session's running script |
| `{"op": "close"}` | waits for every session thread, then the process exits |
