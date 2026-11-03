"""Speed and memory baseline for an installed ruyijs wheel on Linux.

The checkout's own ``dist/js`` wins over an installed wheel, so run this with
the wheel's interpreter from outside the source tree, the same way
``ci/linux/build.sh test`` does::

    cd /tmp
    /path/to/venv/bin/python /path/to/ci/linux_baseline.py
    /path/to/venv/bin/python /path/to/ci/linux_baseline.py --network

``--hold NAME`` repeats one workload until ``--seconds`` elapse and prints the
shell pid, so a sampler can attach. It does not change the numbers above.
"""

import argparse
import json
import os
import statistics
import threading
import time

from ruyijs import Context, Engine

COMPUTE = "(() => { let x = 0; for (let i = 0; i < 400000000; i++) x = (x + i) | 0; return x; })()"
JSON_WORK = """(() => {
  const a = [];
  for (let i = 0; i < 2000; i++) a.push({i: i, t: 'item-' + i});
  return JSON.parse(JSON.stringify(a)).length;
})()"""
PAGE = (
    "<!doctype html><title>base</title><body>"
    + "".join(f"<div id=d{i}>line {i}</div>" for i in range(100))
    + "<script>window.n = document.querySelectorAll('div').length;</script></body>"
)
GEOMETRY_SETUP = """(() => { const p = document.getElementById('preview');
    for (let i = 0; i < 1500; i++) { const d = document.createElement('div');
        d.textContent = 'A moderately long line of text ' + i; p.appendChild(d); } })()"""
READ_HEIGHT = "document.getElementById('editor').offsetHeight"


def percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        raise ValueError("empty sample")
    index = (len(ordered) - 1) * fraction
    low = int(index)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (index - low)


def sample(values, unit="ms"):
    return {
        "n": len(values),
        "unit": unit,
        "min": min(values),
        "median": statistics.median(values),
        "p95": percentile(values, 0.95),
        "max": max(values),
    }


def ms(started):
    return (time.perf_counter() - started) * 1000


def descendants(root):
    """Name and RSS of every process under ``root``, including ``root``."""
    children = {}
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        pid = int(entry)
        try:
            raw = open(f"/proc/{pid}/stat", encoding="ascii", errors="replace").read()
        except OSError:
            continue
        parent = int(raw[raw.rfind(")") + 2:].split()[1])
        children.setdefault(parent, []).append(pid)
    found = []
    pending = [root]
    while pending:
        pid = pending.pop()
        try:
            status = open(f"/proc/{pid}/status", encoding="ascii", errors="replace").read()
        except OSError:
            continue
        name, rss = None, None
        for line in status.splitlines():
            if line.startswith("Name:"):
                name = line.split()[1]
            elif line.startswith("VmRSS:"):
                rss = int(line.split()[1])
        if name is not None:
            found.append({"pid": pid, "name": name, "rss_kib": rss})
        pending.extend(children.get(pid, []))
    return found


def tree_summary(processes):
    by_name = {}
    for process in processes:
        slot = by_name.setdefault(process["name"], {"count": 0, "rss_kib": 0})
        slot["count"] += 1
        slot["rss_kib"] += process["rss_kib"] or 0
    return {
        "processes": by_name,
        "rss_kib": sum(process["rss_kib"] or 0 for process in processes),
    }


def shell_pid(context):
    process = getattr(context, "_process", None)
    if process is not None and process.pid:
        return process.pid
    engine = context.engine
    process = getattr(engine, "_process", None) if engine is not None else None
    if process is not None and process.pid:
        return process.pid
    raise RuntimeError("the context has no shell process")


def settle_tree(root):
    """Wait briefly so background Necko helpers are in the process tree."""
    deadline = time.monotonic() + 2
    tree = descendants(root)
    while time.monotonic() < deadline:
        if any(item["name"].startswith("python") for item in tree):
            break
        time.sleep(0.05)
        tree = descendants(root)
    time.sleep(0.2)
    return tree_summary(descendants(root))


def wait_gone(pid):
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline and os.path.exists(f"/proc/{pid}"):
        time.sleep(0.01)


def startup_samples(count):
    samples = []
    for _ in range(count):
        started = time.perf_counter()
        with Context(timeout=60) as ctx:
            pid = shell_pid(ctx)
            if ctx.eval("1 + 1") != 2:
                raise RuntimeError("startup eval failed")
            samples.append(ms(started))
        # A context that is still exiting holds two helpers. The next sample
        # waits until that shell is gone so the samples do not pile up.
        wait_gone(pid)
    with Context(timeout=60) as ctx:
        ctx.eval("1 + 1")
        memory = settle_tree(shell_pid(ctx))
    return samples, memory


def eval_samples(count):
    with Context(timeout=60) as ctx:
        for _ in range(20):
            ctx.eval("1 + 1")
        samples = []
        for _ in range(count):
            started = time.perf_counter()
            ctx.eval("1 + 1")
            samples.append((time.perf_counter() - started) * 1e6)
        return samples


def repeated(code, count):
    samples = []
    with Context(timeout=120) as ctx:
        ctx.eval(code)
        for _ in range(count):
            started = time.perf_counter()
            ctx.eval(code)
            samples.append(ms(started))
    return samples


def page_samples(count):
    samples = []
    for _ in range(count):
        with Context(timeout=60) as ctx:
            started = time.perf_counter()
            ctx.load_page("https://offline.invalid/", PAGE)
            samples.append(ms(started))
            if ctx.eval("window.n") != 100:
                raise RuntimeError("page did not build 100 divs")
    return samples


def geometry():
    """The 1,500-div case from ci/layout_check.py, timed in pieces."""
    with Context(timeout=120) as ctx:
        ctx.load_page("https://offline.invalid/", "<html><body></body></html>")
        started = time.perf_counter()
        ctx.eval("document.body.innerHTML = '<section style=\"width:420px;padding:10px\"><input id=editor>"
                 "<div id=preview></div></section>'")
        ctx.eval(GEOMETRY_SETUP)
        build_ms = ms(started)
        ctx.eval(READ_HEIGHT)
        reads = []
        for index in range(5):
            ctx.eval(f"document.getElementById('preview').style.paddingTop = '{index + 1}px'")
            started = time.perf_counter()
            ctx.eval(READ_HEIGHT)
            reads.append(ms(started))
        memory = settle_tree(shell_pid(ctx))
    return build_ms, reads, memory


def engine_case():
    started = time.perf_counter()
    with Engine(timeout=120) as engine:
        contexts = [engine.context(timeout=120) for _ in range(4)]
        for context in contexts:
            context.eval("1 + 1")
        startup = ms(started)
        memory = settle_tree(shell_pid(contexts[0]))
        alone_started = time.perf_counter()
        contexts[0].eval(COMPUTE)
        alone = ms(alone_started)
        barrier = threading.Barrier(4)
        elapsed = []

        def run(context):
            barrier.wait()
            started_at = time.perf_counter()
            context.eval(COMPUTE)
            elapsed.append(ms(started_at))

        threads = [threading.Thread(target=run, args=(context,)) for context in contexts]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        wall = max(elapsed) if elapsed else None
    return {
        "startup_ms": startup,
        "one_session_compute_ms": alone,
        "four_sessions_wall_ms": wall,
        "four_sessions_each_ms": elapsed,
        "memory": memory,
    }


def local_http():
    """First and second navigation to a page served on localhost."""
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
    from threading import Thread

    body = b"<!doctype html><title>local</title><p>ok</p>"

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            return

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_address[1]}/"

    def once(ctx):
        started = time.perf_counter()
        ctx.navigate(url)
        deadline = time.monotonic() + 30
        title = ""
        while time.monotonic() < deadline:
            ctx.advance(20)
            if ctx.eval("document.readyState") == "complete" and ctx.eval("document.title"):
                title = ctx.eval("document.title")
                break
        return ms(started), title

    runs = []
    try:
        for _ in range(3):
            with Context(timeout=60) as ctx:
                ctx.configure_clock("system")
                first, title = once(ctx)
                second, title2 = once(ctx)
            runs.append({"first_ms": first, "second_ms": second,
                         "ok": title == "local" and title2 == "local"})
    finally:
        server.shutdown()
    return {"url_host": "127.0.0.1", "runs": runs}


def network_once():
    started = time.perf_counter()
    with Context(timeout=120) as ctx:
        ctx.configure_clock("system")
        ctx.navigate("https://example.com/")
        title = ""
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            ctx.advance(100)
            if ctx.eval("document.readyState") == "complete":
                title = ctx.eval("document.title")
                break
        return {
            "ms": ms(started),
            "title": title,
            "ok": "Example Domain" in title,
        }


def hold(name, seconds):
    with Context(timeout=max(seconds + 30, 60)) as ctx:
        print(f"shell_pid {shell_pid(ctx)}", flush=True)
        deadline = time.monotonic() + seconds
        if name == "eval":
            while time.monotonic() < deadline:
                ctx.eval("1 + 1")
        elif name == "compute":
            while time.monotonic() < deadline:
                ctx.eval(COMPUTE)
        elif name == "geometry":
            ctx.load_page("https://offline.invalid/", "<html><body></body></html>")
            ctx.eval("document.body.innerHTML = '<section style=\"width:420px;padding:10px\"><input id=editor>"
                     "<div id=preview></div></section>'")
            ctx.eval(GEOMETRY_SETUP)
            ctx.eval(READ_HEIGHT)
            index = 0
            while time.monotonic() < deadline:
                index += 1
                ctx.eval(f"document.getElementById('preview').style.paddingTop = '{index}px'")
                ctx.eval(READ_HEIGHT)
        else:
            raise SystemExit(f"unknown hold workload: {name}")
        print("hold done", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", action="store_true",
                        help="also time one https://example.com/ navigation")
    parser.add_argument("--hold", choices=("eval", "compute", "geometry"),
                        help="repeat one workload so a sampler can attach")
    parser.add_argument("--seconds", type=float, default=20)
    args = parser.parse_args()
    if args.hold:
        hold(args.hold, args.seconds)
        return

    startup, idle_memory = startup_samples(8)
    # The first context pays for loading the shell into the page cache.
    build_ms, reads, geometry_memory = geometry()
    report = {
        "context_startup": sample(startup[1:]),
        "context_startup_first_ms": startup[0],
        "context_startup_ms": [round(value, 3) for value in startup],
        "eval_latency_us": sample(eval_samples(400), unit="us"),
        "compute_400m": sample(repeated(COMPUTE, 3)),
        "json_2000": sample(repeated(JSON_WORK, 5)),
        "offline_page_100_divs": sample(page_samples(5)),
        "geometry_1500_build_ms": build_ms,
        "geometry_1500_read": sample(reads),
        "engine": engine_case(),
        "memory_after_idle_context": idle_memory,
        "memory_after_1500_divs": geometry_memory,
        "local_http": local_http(),
    }
    if args.network:
        report["example_com"] = network_once()
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
