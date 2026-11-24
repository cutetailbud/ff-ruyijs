"""Check that one unreachable address does not cost a request its whole budget.

usage: python ci/connect_fallback_check.py      (RUYIJS_SHELL picks js.exe)

The Necko helper tried a host's resolved addresses one after another, each
with all of the remaining request budget, so on a network without a working
IPv6 route a host with two AAAA records took 42 s to reach (one connect timeout
per dead address) for a page curl fetches in 0.3 s. The helper now starts the
attempts 250 ms apart and keeps the first connection that is established
(ruyijs_necko/connection_attempts.py).

This checks the module the installed runtime ships, with a loopback listener
and TEST-NET-1 (192.0.2.1, RFC 5737: routed nowhere, so a connect to it never
completes), so nothing depends on the host's own IPv6:

- create_connection (the macOS direct HTTPS client, where the module has it)
  connects in well under a second when a black-holed address comes first,
  raises promptly when every address refuses, and stays within its timeout
  when every address is dead;
- the family interleaving keeps the resolver's order inside each family;
- the helper uses it: connection_control.open_socket calls connect_any where
  the runtime has that module (Windows, Linux), necko/core_client.py calls
  create_connection where it has that client (macOS);
- connect_any, run in the helper's own Python with its own Attempt, wins with
  the live address, hands the winner the request's Attempt (so later TLS,
  reads and writes stay cancellable), raises when all refuse and stays within
  the request budget when every address is dead.

Exit status is non-zero on failure.
"""
import importlib.util
import json
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

from ruyijs import get_runtime_path

BLACKHOLE = "192.0.2.1"
FAILURES = []


def check(name, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + name, detail)
    if not condition:
        FAILURES.append(name)


def listener():
    server = socket.socket()
    server.bind(("127.0.0.1", 0))
    server.listen(16)

    def accept():
        while True:
            try:
                connection, _ = server.accept()
            except OSError:
                return
            connection.close()

    threading.Thread(target=accept, daemon=True).start()
    return server, server.getsockname()[1]


def installed(root):
    """The runtime's ruyijs_necko directories, and its helper Python."""
    vendor = root / "vendor"
    packages = sorted({p.parent for p in vendor.rglob("ruyijs_necko/connection_attempts.py")}) if vendor.is_dir() else []
    spec = importlib.util.find_spec("ruyijs_necko")  # the macOS wheel installs it beside ruyijs
    if spec and spec.submodule_search_locations:
        for location in spec.submodule_search_locations:
            if (Path(location) / "connection_attempts.py").is_file() and Path(location) not in packages:
                packages.append(Path(location))
    helper = next((p for p in (root / "vendor/necko-derived/python/python.exe",
                               root / "vendor/necko-derived/python/bin/python3") if p.is_file()), None)
    return packages, helper


def load(path):
    spec = importlib.util.spec_from_file_location("ruyijs_connection_attempts_check", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check_create_connection(module, port):
    real = socket.getaddrinfo

    def fake(*addresses):
        def resolve(host, port_, *args, **kwargs):
            return [(socket.AF_INET, socket.SOCK_STREAM, 6, "", (a, p)) for a, p in addresses]
        return resolve

    try:
        for label, addresses in (("dead address first", [(BLACKHOLE, port), ("127.0.0.1", port)]),
                                 ("live address first", [("127.0.0.1", port), (BLACKHOLE, port)])):
            socket.getaddrinfo = fake(*addresses)
            started = time.monotonic()
            sock = module.create_connection(("site.test", port), 20)
            elapsed = time.monotonic() - started
            check(f"create_connection, {label}: connects promptly",
                  sock.getpeername()[0] == "127.0.0.1" and elapsed < 1.5, f"{elapsed:.2f}s")
            check(f"create_connection, {label}: blocking with the timeout", sock.gettimeout() == 20,
                  sock.gettimeout())
            sock.close()
        closed = socket.socket()
        closed.bind(("127.0.0.1", 0))
        refused = closed.getsockname()[1]
        closed.close()
        socket.getaddrinfo = fake(("127.0.0.1", refused), ("127.0.0.1", refused))
        started = time.monotonic()
        try:
            module.create_connection(("site.test", refused), 20)
            check("create_connection: every address refusing raises", False, "connected")
        except OSError as error:
            check("create_connection: every address refusing raises",
                  time.monotonic() - started < 5, f"{type(error).__name__} {time.monotonic() - started:.2f}s")
        socket.getaddrinfo = fake((BLACKHOLE, 443), (BLACKHOLE, 443), (BLACKHOLE, 443))
        started = time.monotonic()
        try:
            module.create_connection(("site.test", 443), 1.5)
            check("create_connection: a dead host stays within the timeout", False, "connected")
        except OSError as error:
            elapsed = time.monotonic() - started
            check("create_connection: a dead host stays within the timeout", 1.4 <= elapsed < 3,
                  f"{type(error).__name__} {elapsed:.2f}s")
    finally:
        socket.getaddrinfo = real


def check_ordered(module):
    mixed = [(socket.AF_INET6, 1, 6, "", ("v6a", 443, 0, 0)), (socket.AF_INET6, 1, 6, "", ("v6b", 443, 0, 0)),
             (socket.AF_INET, 1, 6, "", ("v4a", 443)), (socket.AF_INET, 1, 6, "", ("v4b", 443))]
    names = [row[4][0] for row in module.ordered(mixed)]
    check("families interleave", names == ["v6a", "v4a", "v6b", "v4b"], str(names))
    check("an empty address list stays empty", module.ordered([]) == [])


# connect_any in the helper's own Python, with its Attempt and request control.
INSIDE = r"""
import json, select, socket, sys, time
from necko._connect import Attempt
from ruyijs_necko.connection_attempts import connect_any
from ruyijs_necko.request_control import RequestControl
port, blackhole = int(sys.argv[1]), sys.argv[2]
class Adapter: library = object()
class Connection:
    def __init__(self, sock, attempt): self.sock, self.attempt, self.interrupted = sock, attempt, False
    def interrupt(self): self.interrupted = True
    def close(self): self.sock.close()
def factory(library, host, port_, timeout, attempt, **options):
    # As the native factory: one non-blocking connect, polled until it is
    # established, fails, or the attempt is cancelled or out of time.
    sock = socket.socket(); sock.setblocking(False)
    code = sock.connect_ex((host, port_))
    while code in (10035, 115, 36, 150, 114, 37):  # in progress (Windows, Linux, macOS)
        if attempt is not None and attempt.cancelled.is_set():
            sock.close(); raise OSError("Connection attempt cancelled")
        if attempt is not None and attempt.deadline is not None and time.monotonic() > attempt.deadline:
            sock.close(); raise TimeoutError("Connection deadline elapsed")
        _, w, x = select.select([], [sock], [sock], 0.02)
        if w or x:
            code = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
            break
    if code:
        sock.close(); raise ConnectionError(f"refused {host} ({code})")
    sock.setblocking(True)
    return Connection(sock, attempt)
rows = lambda *a: [(socket.AF_INET, socket.SOCK_STREAM, 6, "", (h, p)) for h, p in a]
# The code the helper really runs: its bytecode, not the source text beside it.
import ruyijs_necko.connection_control as control
out = {"loaded": "connect_any" in control.open_socket.__code__.co_names}
for label, addresses in (("dead", rows((blackhole, port), ("127.0.0.1", port))),
                         ("live", rows(("127.0.0.1", port), (blackhole, port)))):
    started = time.monotonic()
    c = connect_any(Adapter(), factory, addresses, port, None, None, RequestControl(30), lambda a: a[0])
    out[label] = [c.sock.getpeername()[0], round(time.monotonic() - started, 3)]
    c.close()
request = Attempt(time.monotonic() + 30)
c = connect_any(Adapter(), factory, rows((blackhole, port), ("127.0.0.1", port)), port, None, request,
                RequestControl(30), lambda a: a[0])
out["attempt"] = c.attempt is request
request.cancel()
out["interrupted"] = c.interrupted
c.close()
closed = socket.socket(); closed.bind(("127.0.0.1", 0)); refused = closed.getsockname()[1]; closed.close()
started = time.monotonic()
try:
    connect_any(Adapter(), factory, rows(("127.0.0.1", refused), ("127.0.0.1", refused)), refused, None, None,
                RequestControl(30), lambda a: a[0]); out["refused"] = "connected"
except OSError as error:
    out["refused"] = [type(error).__name__, round(time.monotonic() - started, 3)]
started = time.monotonic()
try:
    connect_any(Adapter(), factory, rows((blackhole, 443), (blackhole, 443), (blackhole, 443)), 443, None, None,
                RequestControl(2), lambda a: a[0]); out["deadhost"] = "connected"
except Exception as error:
    out["deadhost"] = [type(error).__name__, round(time.monotonic() - started, 3)]
print(json.dumps(out))
"""


def check_connect_any(helper, port):
    result = subprocess.run([str(helper), "-I", "-B", "-c", INSIDE, str(port), BLACKHOLE],
                            capture_output=True, text=True, timeout=120)
    if result.returncode:
        check("connect_any runs in the helper's Python", False, result.stderr.strip()[-600:])
        return
    out = json.loads(result.stdout.strip().splitlines()[-1])
    check("connect_any: the helper's loaded open_socket calls it", out["loaded"] is True,
          "stale bytecode?" if not out["loaded"] else "")
    for label in ("dead", "live"):
        peer, elapsed = out[label]
        check(f"connect_any, {label} address first: connects promptly", peer == "127.0.0.1" and elapsed < 1.5,
              f"{elapsed}s")
    check("connect_any: the winner carries the request's Attempt", out["attempt"] is True)
    check("connect_any: cancelling the request interrupts the winner", out["interrupted"] is True)
    check("connect_any: every address refusing raises",
          isinstance(out["refused"], list) and out["refused"][1] < 5, out["refused"])
    check("connect_any: a dead host stays within the request budget",
          isinstance(out["deadhost"], list) and 1.9 <= out["deadhost"][1] < 6, out["deadhost"])


def main():
    root = Path(get_runtime_path()).resolve()  # the runtime root: dist/ and vendor/
    packages, helper = installed(root)
    check("the runtime ships ruyijs_necko/connection_attempts.py", bool(packages), str(root))
    if not packages:
        return 1
    server, port = listener()
    try:
        module = load(packages[0] / "connection_attempts.py")
        check_ordered(module)
        # The macOS direct HTTPS client's socket path; a Windows-only runtime
        # whose module serves open_socket alone has no create_connection.
        if hasattr(module, "create_connection"):
            check_create_connection(module, port)
        wired = False
        for package in packages:
            control = package / "connection_control.py"
            if control.is_file():
                text = control.read_text(encoding="utf-8")
                check(f"{control.parent.name}/connection_control.py: open_socket uses connect_any",
                      "connect_any(" in text and "for _, _, _, _, address in rows" not in text)
                wired = True
            core = package.parent / "necko/core_client.py"
            if core.is_file():
                text = core.read_text(encoding="utf-8")
                check("necko/core_client.py: direct HTTPS uses create_connection",
                      "create_connection(proxy or (host, port), budget_s)" in text
                      and "socket.create_connection(" not in text)
                wired = True
        check("the helper uses the staggered connect", wired)
        if any((p / "connection_control.py").is_file() for p in packages):
            if helper is None:
                check("the runtime has a helper Python", False, str(root))
            else:
                check_connect_any(helper, port)
    finally:
        server.close()
    print(("FAILED: " + ", ".join(FAILURES)) if FAILURES else "connect fallback checks passed")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    sys.exit(main())
