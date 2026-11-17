"""A Blob Worker posts a message back to the page.

The parent poll used to burn a logical advance() in non-blocking pipe reads
before the child process could answer, so onmessage never ran. advance() now
waits a few wall-clock milliseconds between those reads while a reply may be
on its way (the child just started or was posted to, nothing back yet). An idle
worker that already answered, or a silent one past its patience, must not hold a
logical advance() in wall time.
"""
import time

from ruyijs import Context


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def test_worker_blob_message():
    html = """<!doctype html><body><script>
      window.__msgEvents = [];
      window.__gotMsg = '';
      window.__workerErr = '';
      window.__workerOk = false;
      try {
        const code = 'self.onmessage = function(e) { self.postMessage("pong:" + e.data); };';
        const url = URL.createObjectURL(new Blob([code], {type: 'text/javascript'}));
        window.__w = new Worker(url);
        window.__w.onmessage = function(e) {
          window.__gotMsg = String(e.data);
          window.__msgEvents.push('onmessage');
        };
        window.__w.onerror = function(e) {
          window.__msgEvents.push('onerror:' + String(e && e.message || ''));
        };
        window.__w.postMessage('ping');
        window.__workerOk = true;
      } catch (e) {
        window.__workerErr = String(e && e.message || e);
      }
    </script></body>"""
    with Context(timeout=60) as ctx:
        ctx.configure_clock("logical", epoch_ms=1_700_000_000_000)
        ctx.load_page("https://offline.invalid/worker-blob/", html)
        ctx.advance(3000)
        result = ctx.eval(
            """({
              workerOk: Boolean(window.__workerOk),
              workerErr: String(window.__workerErr || ''),
              gotMsg: String(window.__gotMsg || ''),
              events: (window.__msgEvents || []).join(','),
              worker: typeof Worker,
              blob: typeof Blob,
              url: typeof URL.createObjectURL
            })"""
        )
        check("Worker, Blob, and createObjectURL exist",
              result["worker"] == "function" and result["blob"] == "function" and result["url"] == "function",
              result)
        check("Worker was constructed", result["workerOk"] is True, result.get("workerErr"))
        check("the page received pong:ping", result["gotMsg"] == "pong:ping", result)
        # The worker answered and is idle now: a logical advance stays logical.
        start = time.perf_counter()
        ctx.advance(10000)
        idle = time.perf_counter() - start
        check("an idle worker does not hold a logical advance", idle < 1.0, f"{idle:.2f} s")
    silent = """<!doctype html><body><script>
      window.__w = new Worker(URL.createObjectURL(new Blob(['onmessage = () => {};'])));
    </script></body>"""
    with Context(timeout=60) as ctx:
        ctx.configure_clock("logical", epoch_ms=1_700_000_000_000)
        ctx.load_page("https://offline.invalid/worker-silent/", silent)
        start = time.perf_counter()
        for _ in range(10):
            ctx.eval("window.__w.postMessage(1)")
            ctx.advance(1000)
        spent = time.perf_counter() - start
        # One full patience (5 s) for the first wait, then a short one per post.
        check("posts to a silent worker wait a bounded time", spent < 12.0, f"{spent:.2f} s")
    print("worker blob message check passed")


if __name__ == "__main__":
    test_worker_blob_message()
