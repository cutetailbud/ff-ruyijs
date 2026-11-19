"""Check the HTTP time limits Context(http_timeout=..., http_read_timeout=...) sets (issue #45).

A page request had a fixed 15 s budget in the Necko helper and a fixed 20 s
redirect chain in the page, so a response that kept sending data for 36 s
failed at 15 s and Context(timeout=60) could not help: that is the host's
command wait. Uses a local HTTP server only:

- a response sending a chunk every 0.6 s for 22 s completes with http_timeout=40
  (past both former limits);
- http_timeout=2 ends a 4 s response at about 2 s;
- http_read_timeout=1 ends a response that pauses for 2.5 s;
- an Engine session's http_timeout reaches its helpers (2 s) and its redirect
  chain (1 s per hop + 5 s for the chain: ten 0.8 s hops stop near 6 s);
- out-of-range values are refused before any process starts.

Exit status is non-zero on failure.
"""
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from ruyijs import Context, Engine

CHUNK = b"data: x\n\n"


class Slow(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        url = urlsplit(self.path)
        query = {k: float(v[0]) for k, v in parse_qs(url.query).items()}
        if url.path == "/hop":
            time.sleep(query["delay"])
            index, count = int(query["i"]), int(query["n"])
            if index < count:
                self.send_response(302)
                self.send_header("Location", f"/hop?i={index + 1}&n={count}&delay={query['delay']}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            body = b"done"
        elif url.path == "/stream":
            count, gap = int(query["n"]), query["gap"]
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(CHUNK) * count))
            self.end_headers()
            try:
                for i in range(count):
                    if i:
                        time.sleep(gap)
                    self.wfile.write(CHUNK)
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass
            return
        else:
            body = b"<!doctype html><title>local</title>"
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


FETCH = """globalThis.reply = null;
fetch(%r).then(async r => { try { reply = {status: r.status, bytes: (await r.text()).length}; }
                            catch (e) { reply = {error: e.name}; } },
               e => { reply = {error: e.name}; }); void 0;"""


def fetch(ctx, base, path, limit=60):
    ctx.navigate(base + "/")
    started = time.monotonic()
    ctx.eval(FETCH % path)
    while ctx.eval("reply === null"):
        if time.monotonic() - started > limit:
            return None, time.monotonic() - started
        ctx.advance(50, max_tasks=1000)
    return ctx.eval("reply"), time.monotonic() - started


failures = []


def check(name, condition, detail):
    print(f"{'ok  ' if condition else 'FAIL'} {name}  ({detail})", flush=True)
    if not condition:
        failures.append(name)


for value, error in ((0, ValueError), (3601, ValueError), (float("nan"), ValueError), ("5", TypeError),
                     (True, TypeError)):
    for name in ("http_timeout", "http_read_timeout"):
        try:
            Context(**{name: value}).close()
            refused = False
        except error:
            refused = True
        check(f"{name}={value!r} is refused", refused, error.__name__)

class Server(ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        pass  # A request the client gave up on resets its connection.


server = Server(("127.0.0.1", 0), Slow)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
try:
    with Context(timeout=60, http_timeout=40) as ctx:
        reply, spent = fetch(ctx, base, "/stream?n=38&gap=0.6")
        check("a 22 s stream completes with http_timeout=40",
              reply == {"status": 200, "bytes": len(CHUNK) * 38} and spent > 21, f"{reply}, {spent:.1f} s")
    with Context(timeout=60, http_timeout=2) as ctx:
        reply, spent = fetch(ctx, base, "/stream?n=5&gap=1")
        check("http_timeout=2 ends a 4 s response", reply == {"error": "TypeError"} and 1.5 < spent < 3.5,
              f"{reply}, {spent:.1f} s")
    with Context(timeout=60, http_read_timeout=1) as ctx:
        reply, spent = fetch(ctx, base, "/stream?n=2&gap=2.5")
        check("http_read_timeout=1 ends a response that pauses 2.5 s",
              reply == {"error": "TypeError"} and 0.5 < spent < 2.3, f"{reply}, {spent:.1f} s")
    with Engine() as engine:
        with engine.context(timeout=60, http_timeout=2) as ctx:
            reply, spent = fetch(ctx, base, "/stream?n=5&gap=1")
            check("an engine session's http_timeout reaches its helpers",
                  reply == {"error": "TypeError"} and 1.5 < spent < 3.5, f"{reply}, {spent:.1f} s")
        with engine.context(timeout=60, http_timeout=1) as ctx:
            reply, spent = fetch(ctx, base, "/hop?i=0&n=10&delay=0.8")
            check("an engine session's redirect chain gets http_timeout + 5 s",
                  reply == {"error": "TypeError"} and 5 < spent < 7.5, f"{reply}, {spent:.1f} s")
        with engine.context(timeout=60) as ctx:
            reply, spent = fetch(ctx, base, "/hop?i=0&n=3&delay=0.2")
            check("a short redirect chain still completes", reply == {"status": 200, "bytes": 4},
                  f"{reply}, {spent:.1f} s")
finally:
    server.shutdown()
    server.server_close()

print("http timeout checks passed" if not failures else f"FAILED: {failures}")
sys.exit(1 if failures else 0)
