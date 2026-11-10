"""Check that page timers run while the load event waits on a slow image (issue #31).

The document completion task once waited for in-flight images and style sheets
inside one task, so a 50 ms setTimeout ran only after the image arrived and
`load` fired. The event loop has to keep turning while `load` is delayed:
the timer runs first, then the image, then `load`. Uses a local HTTP server
only. Exit status is non-zero on failure.
"""
import base64
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from ruyijs import Context

DELAY = 1.5
IMAGE = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jp1sAAAAASUVORK5CYII=")


class SlowImage(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        time.sleep(DELAY)
        self.send_response(200)
        self.send_header("Content-Type", "image/png")
        self.send_header("Content-Length", str(len(IMAGE)))
        self.end_headers()
        self.wfile.write(IMAGE)


PAGE = """<!doctype html><script>
window.begin = performance.now(); window.timerMs = null; window.order = [];
setTimeout(() => { timerMs = performance.now() - begin; order.push('timer'); }, 50);
addEventListener('load', () => order.push('load'));
</script><img src="/slow.png" onload="order.push('img')" onerror="order.push('imgerr')">"""

server = ThreadingHTTPServer(("127.0.0.1", 0), SlowImage)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with Context(timeout=30) as ctx:
        ctx.configure_clock("system")
        ctx.load_page(f"http://127.0.0.1:{server.server_port}/", PAGE)
        order = ctx.eval("order.join(',')")
        timer = ctx.eval("timerMs")
finally:
    server.shutdown()
    server.server_close()

ok = order == "timer,img,load" and timer is not None and timer < DELAY * 1000 / 2
print(f"{'ok  ' if ok else 'FAIL'} load timer: order {order}, 50 ms timer ran at {timer} ms")
sys.exit(0 if ok else 1)
