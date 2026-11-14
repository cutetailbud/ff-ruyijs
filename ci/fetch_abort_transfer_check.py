"""Check that aborting an in-flight request stops its transfer (issue #43).

AbortController.abort() rejected the fetch, but the Necko helper carrying the
request kept downloading the response to the end; XMLHttpRequest.abort() did
the same. A local server streams a slow response; once its first chunk is out
the page aborts the request and retries without aborting. The server has to
see the first connection close before the response ends, and the retry still
has to succeed. Uses a local HTTP server only. Exit status is non-zero on
failure.
"""
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from ruyijs import Context

CHUNK = b"x" * 8192
CHUNKS = 32
TOTAL = len(CHUNK) * CHUNKS
INTERVAL = 0.1
lock = threading.Lock()
rows = {}


class Slow(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, ConnectionAbortedError):
            pass

    def do_GET(self):
        with lock:
            row = {"sent": 0, "closed": False, "done": False}
            rows.setdefault(self.path, []).append(row)
            first = len(rows[self.path]) == 1
        payload, count = (CHUNK, CHUNKS) if first else (b"ok", 1)
        try:
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(payload) * count))
            self.end_headers()
            for _ in range(count):
                self.wfile.write(payload)
                self.wfile.flush()
                with lock:
                    row["sent"] += len(payload)
                if first:
                    time.sleep(INTERVAL)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            with lock:
                row["closed"] = True
        finally:
            with lock:
                row["done"] = True


FETCH = """<script>
const controller = new AbortController(); window.result = {};
window.stop = () => controller.abort();
fetch('/fetch', {signal: controller.signal}).then(r => r.arrayBuffer())
  .then(b => { result.first = 'resolved ' + b.byteLength; },
        e => { result.first = e.name;
               return fetch('/fetch').then(r => r.arrayBuffer())
                 .then(b => { result.retry = b.byteLength; }, e => { result.retry = e.name; }); });
</script>"""
XHR = """<script>
window.result = {};
const xhr = new XMLHttpRequest();
window.stop = () => xhr.abort();
xhr.onload = () => { result.first = 'loaded ' + xhr.response.byteLength; };
xhr.onabort = () => {
  result.first = 'abort';
  const again = new XMLHttpRequest();
  again.responseType = 'arraybuffer';
  again.onload = () => { result.retry = again.response.byteLength; };
  again.onerror = () => { result.retry = 'error'; };
  again.open('GET', '/xhr'); again.send();
};
xhr.responseType = 'arraybuffer';
xhr.open('GET', '/xhr'); xhr.send();
</script>"""


def run(base, path, page, aborted):
    result = {}
    with Context(timeout=30) as ctx:
        ctx.configure_clock("system")
        ctx.load_page(base + "/", page)
        end = time.monotonic() + 10
        while time.monotonic() < end:
            with lock:
                if rows.get(path) and rows[path][0]["sent"]:
                    break
            ctx.advance(20, max_tasks=100)
            time.sleep(0.01)
        ctx.eval("stop()")
        # The Context stays open until the first handler ends, so closing it
        # cannot pass for cancellation.
        end = time.monotonic() + 15
        while time.monotonic() < end:
            ctx.advance(20, max_tasks=100)
            result = ctx.eval("result")
            with lock:
                finished = bool(rows.get(path)) and rows[path][0]["done"]
            if finished and "retry" in result:
                break
            time.sleep(0.02)
    with lock:
        first = dict(rows[path][0]) if rows.get(path) else {}
    ok = (result.get("first") == aborted and result.get("retry") == 2 and
          first.get("closed") is True and first.get("sent", TOTAL) < TOTAL)
    print(f"{'ok  ' if ok else 'FAIL'} {path[1:]} abort stops the transfer: page {result}, "
          f"aborted response sent {first.get('sent')} of {TOTAL} bytes, closed {first.get('closed')}")
    return ok


server = ThreadingHTTPServer(("127.0.0.1", 0), Slow)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    base = f"http://127.0.0.1:{server.server_port}"
    results = [run(base, "/fetch", FETCH, "AbortError"), run(base, "/xhr", XHR, "abort")]
finally:
    server.shutdown()
    server.server_close()
sys.exit(0 if all(results) else 1)
