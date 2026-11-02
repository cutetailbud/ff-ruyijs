"""Check the shared Engine of an installed ruyijs wheel.

Several Contexts run as sessions of one engine process. Each must stay as
independent as a Context with a process of its own (globals, page, cookies,
storage, host functions, recursion depth), sessions must run in parallel, and
closing or timing out one session must leave the others working. Runs offline;
``--network`` also loads https://example.com/ in two sessions, one through a
local CONNECT proxy, and checks that only that session used the proxy.
Exit status is non-zero on the first failed check.
"""
import os
import socket
import socketserver
import sys
import threading
import time

from ruyijs import Context, Engine, JSError, use_engine

DEPTH = "(() => { let n = 0; const f = () => { with ({}) {} n++; f(); }; try { f(); } catch (e) {} return n; })()"
WORK = "(() => { let x = 0; for (let i = 0; i < 400000000; i++) x = (x + i) | 0; return x !== 1; })()"


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def steady_depth(context):
    # Ion compiles a hot function off-thread and switches it over in the middle
    # of a deep recursion, at a moment that varies from call to call. `with`
    # keeps DEPTH's function out of Ion: after a first call that warms Baseline
    # up, every call reaches the same depth.
    context.eval(DEPTH)
    return min(context.eval(DEPTH) for _ in range(3))


def descendants():
    """Names of this process's descendants (Windows), from a Toolhelp snapshot."""
    import ctypes
    import ctypes.wintypes as wt

    class Entry(ctypes.Structure):  # PROCESSENTRY32W
        _fields_ = [("size", wt.DWORD), ("usage", wt.DWORD), ("pid", wt.DWORD),
                    ("heap", ctypes.c_void_p), ("module", wt.DWORD), ("threads", wt.DWORD),
                    ("parent", wt.DWORD), ("priority", ctypes.c_long), ("flags", wt.DWORD),
                    ("exe", ctypes.c_wchar * 260)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
    snapshot = kernel32.CreateToolhelp32Snapshot(0x2, 0)  # TH32CS_SNAPPROCESS
    entry = Entry()
    entry.size = ctypes.sizeof(entry)
    children = {}
    more = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
    while more:
        children.setdefault(entry.parent, []).append((entry.pid, entry.exe.lower()))
        more = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    kernel32.CloseHandle(snapshot)
    names, todo, seen = [], [os.getpid()], {os.getpid()}
    while todo:
        for pid, name in children.get(todo.pop(), []):
            if pid not in seen:
                seen.add(pid)
                names.append(name)
                todo.append(pid)
    return names


def check_no_console_hosts(label):
    """A CREATE_NO_WINDOW child would bring a conhost.exe of its own (Windows)."""
    if os.name != "nt":
        return
    deadline = time.monotonic() + 20
    while "python.exe" not in descendants() and time.monotonic() < deadline:
        time.sleep(0.2)  # wait for a prewarmed Necko helper
    names = descendants()
    counts = {name: names.count(name) for name in sorted(set(names))}
    check(label, "conhost.exe" not in counts and "python.exe" in counts, counts)


def page(context, url, marker):
    context.load_page(url, f"<!doctype html><title>{marker}</title><p id=p>{marker}</p>")


def offline():
    with Context(timeout=60) as alone:
        alone_depth = steady_depth(alone)
        check_no_console_hosts("a Context and its helpers start without a console")
    with Engine(timeout=60) as engine:
        a, b, c = (engine.context(timeout=60) for _ in range(3))
        check("three sessions in one engine", len(engine.contexts) == 3 and a.engine is engine)
        check_no_console_hosts("an engine, its sessions and helpers start without a console")
        page(a, "https://a.example/", "A")
        page(b, "https://b.example/", "B")
        page(c, "https://c.example/", "C")
        a.eval("var shared = 'a'; document.cookie = 'id=a'; localStorage.setItem('k', 'a')")
        b.eval("var shared = 'b'")
        check("each session has its own document",
              [x.eval("document.title + location.host") for x in (a, b, c)] ==
              ["Aa.example", "Bb.example", "Cc.example"])
        check("globals do not leak between sessions",
              [x.eval("typeof shared === 'undefined' ? null : shared") for x in (a, b, c)] == ["a", "b", None])
        page(b, "https://a.example/other", "B2")
        check("cookies and storage stay per session",
              (a.eval("document.cookie"), b.eval("document.cookie"), b.eval("localStorage.getItem('k')")) ==
              ("id=a", "", None))
        a.expose(lambda value: f"a:{value}", "hostEcho")
        check("host functions stay per session",
              (a.eval("hostEcho(1)"), b.eval("typeof hostEcho")) == ("a:1", "undefined"))
        depths = [steady_depth(x) for x in (a, b, c)]
        check("all sessions recurse equally deep", len(set(depths)) == 1, depths)
        # A Context and the engine are two processes, and a process's stack
        # starts where its system puts it: Windows ASLR starts a main thread's
        # stack up to 2 KiB lower at random (13 frames of DEPTH across 60
        # processes), macOS also takes the command line and environment out of it.
        slack = {"win32": 16, "darwin": 1}.get(sys.platform, 0)
        check("recursion depth matches a Context of its own", abs(depths[0] - alone_depth) <= slack,
              f"{depths[0]} vs {alone_depth}")

        def timed(run):
            started = time.monotonic()
            run()
            return time.monotonic() - started

        def three():
            workers = [threading.Thread(target=lambda x=x: results.append(x.eval(WORK))) for x in (a, b, c)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join()

        # Three sessions one after another take three times as long as one;
        # in parallel they take about as long. A CI runner shares its few cores,
        # so each side is the best of three runs, and the line sits between.
        single = min(timed(lambda: a.eval(WORK)) for _ in range(3))
        results = []
        elapsed = min(timed(three) for _ in range(3))
        check("sessions run in parallel", results == [True] * 9 and elapsed < 2.5 * single,
              f"3 sessions {elapsed:.2f}s, one alone {single:.2f}s")

        slow = engine.context(timeout=2)
        try:
            slow.eval("while (true) {}")
            check("a runaway script times out", False)
        except TimeoutError:
            check("a runaway script times out", slow.closed)
        check("other sessions keep working after a timeout", a.eval("1 + 1") == 2)
        c.close()
        check("closing a session leaves the others", c.closed and b.eval("document.title") == "B2")
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline and len(engine.status()) > 2:
            time.sleep(0.2)
        check("ended sessions leave the engine", len(engine.status()) == 2, engine.status())
        try:
            c.eval("1")
            check("a closed session refuses requests", False)
        except RuntimeError:
            check("a closed session refuses requests", True)
        with use_engine(engine):
            d = Context(timeout=60)
        check("use_engine makes Context() a session", d.engine is engine and d.eval("6 * 7") == 42)
        try:
            d.eval("throw new TypeError('boom')")
        except JSError as error:
            check("errors come back as JSError", error.name == "TypeError" and error.message == "boom")
    check("closing the engine closes its sessions", a.closed and b.closed and d.closed)


SEEN = []


def relay(source, target):
    try:
        while True:
            data = source.recv(65536)
            if not data:
                break
            target.sendall(data)
    except OSError:
        pass
    finally:
        for sock in (source, target):
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


class Tunnel(socketserver.BaseRequestHandler):
    def handle(self):
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = self.request.recv(4096)
            if not chunk or len(head) > 65536:
                return
            head += chunk
        line = head.partition(b"\r\n")[0].decode("latin-1")
        method, target, _ = line.split(" ", 2)
        SEEN.append(f"{method} {target}")
        if method != "CONNECT":
            self.request.sendall(b"HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n")
            return
        host, _, port = target.rpartition(":")
        try:
            upstream = socket.create_connection((host.strip("[]"), int(port)), timeout=30)
        except OSError:
            self.request.sendall(b"HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n")
            return
        self.request.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
        back = threading.Thread(target=relay, args=(upstream, self.request), daemon=True)
        back.start()
        relay(self.request, upstream)
        back.join(30)
        upstream.close()


class Proxy(socketserver.ThreadingTCPServer):
    daemon_threads = True


def title_of(context, url):
    context.configure_clock("system")
    context.navigate(url)
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        context.advance(200)
        title = context.eval("document.readyState === 'complete' ? document.title : ''")
        if title:
            return title
    return ""


def network():
    with Proxy(("127.0.0.1", 0), Tunnel) as proxy:
        threading.Thread(target=proxy.serve_forever, daemon=True).start()
        address = f"http://127.0.0.1:{proxy.server_address[1]}"
        with Engine(timeout=120) as engine:
            direct = engine.context(timeout=120)
            proxied = engine.context(timeout=120, proxy=address)
            check("a direct session loads a page", "Example" in title_of(direct, "https://example.com/"))
            check("the direct session did not use the proxy", SEEN == [], SEEN)
            check("a proxied session loads a page", "Example" in title_of(proxied, "https://example.com/"))
            check("the proxied session used its own proxy", SEEN and set(SEEN) == {"CONNECT example.com:443"}, SEEN)


def main():
    offline()
    if "--network" in sys.argv[1:]:
        network()
    print("engine checks passed")


if __name__ == "__main__":
    main()
