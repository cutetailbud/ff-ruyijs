"""Check that an installed ruyijs wheel sends Firefox 160's TLS ClientHello.

Listens on localhost, opens https://localhost:PORT/ with a Context, records
the first ClientHello, answers it with a fatal alert, and compares it with
ci/firefox160-hello.json (captured from Firefox 160.0): legacy session id
length, cipher suites, extensions in order, supported groups, key shares,
signature algorithms, ALPN and supported versions. Exit status is non-zero on
any difference unless --report-only is given.
"""
import json
import selectors
import socket
import struct
import sys
import threading
import time
from pathlib import Path

from ruyijs import Context

REFERENCE = Path(__file__).with_name("firefox160-hello.json")
FIELDS = ("session_id_len", "cipher_suites", "extensions", "groups", "key_shares", "sig_algs", "alpn", "versions")
GREASE = {0x0A0A + 0x1010 * i for i in range(16)}


def fold(values):
    return ["GREASE" if v in GREASE else v for v in values]


def u16s(data):
    return fold(struct.unpack(f"!{len(data) // 2}H", data))


def parse(body):
    p = 4 + 2 + 32
    sid = body[p]; p += 1 + sid
    n = struct.unpack("!H", body[p:p + 2])[0]; p += 2
    hello = {"session_id_len": sid, "cipher_suites": u16s(body[p:p + n]), "extensions": [], "groups": [],
             "key_shares": [], "sig_algs": [], "alpn": [], "versions": []}
    p += n
    p += 1 + body[p]
    end = p + 2 + struct.unpack("!H", body[p:p + 2])[0]; p += 2
    while p < end:
        kind, size = struct.unpack("!HH", body[p:p + 4]); p += 4
        data = body[p:p + size]; p += size
        hello["extensions"] += fold([kind])
        if kind == 10:
            hello["groups"] = u16s(data[2:])
        elif kind == 13:
            hello["sig_algs"] = u16s(data[2:])
        elif kind == 43:
            hello["versions"] = u16s(data[1:])
        elif kind == 51:
            q = 2
            while q < len(data):
                group, length = struct.unpack("!HH", data[q:q + 4]); q += 4 + length
                hello["key_shares"] += fold([group])
        elif kind == 16:
            q = 2
            while q < len(data):
                hello["alpn"].append(data[q + 1:q + 1 + data[q]].decode("latin-1")); q += 1 + data[q]
    return hello


def read(conn, size):
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            raise ConnectionError("closed")
        data += chunk
    return data


def serve(listeners, hellos, stop):
    selector = selectors.DefaultSelector()
    for listener in listeners:
        selector.register(listener, selectors.EVENT_READ)
    while not stop.is_set():
        for key, _ in selector.select(0.2):
            conn, _ = key.fileobj.accept()
            try:
                conn.settimeout(10)
                body = b""
                while True:
                    header = read(conn, 5)
                    if header[0] != 0x16:
                        break
                    body += read(conn, struct.unpack("!H", header[3:5])[0])
                    if len(body) >= 4 and len(body) >= 4 + int.from_bytes(body[1:4], "big"):
                        break
                if body[:1] == b"\x01":
                    hellos.append(parse(body))
                conn.sendall(b"\x15\x03\x03\x00\x02\x02\x28")
            except (OSError, ConnectionError, struct.error, IndexError):
                pass
            finally:
                conn.close()


def main():
    report_only = "--report-only" in sys.argv[1:]
    listeners = []
    v4 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    v4.bind(("127.0.0.1", 0))
    port = v4.getsockname()[1]
    listeners.append(v4)
    try:  # localhost may resolve to ::1 first
        v6 = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        v6.bind(("::1", port))
        listeners.append(v6)
    except OSError:
        pass
    for listener in listeners:
        listener.listen(16)
    hellos, stop = [], threading.Event()
    thread = threading.Thread(target=serve, args=(listeners, hellos, stop), daemon=True)
    thread.start()
    with Context(timeout=60) as ctx:
        ctx.configure_clock("system")
        ctx.navigate(f"https://localhost:{port}/")
        deadline = time.monotonic() + 30
        while not hellos and time.monotonic() < deadline:
            ctx.advance(250)
    stop.set()
    thread.join(5)
    if not hellos:
        print(f"{'REPORT' if report_only else 'FAIL'}: no ClientHello arrived")
        sys.exit(0 if report_only else 1)
    reference = json.loads(REFERENCE.read_text(encoding="utf-8"))
    hello = hellos[0]
    differences = [field for field in FIELDS if hello[field] != reference[field]]
    for field in FIELDS:
        mark = "DIFF" if field in differences else "ok  "
        print(f"{mark} {field}: {hello[field]}")
        if field in differences:
            print(f"     Firefox 160: {reference[field]}")
    if differences:
        print(f"{'REPORT' if report_only else 'FAIL'}: the ClientHello differs from Firefox 160 in {', '.join(differences)}")
        sys.exit(0 if report_only else 1)
    print("ClientHello matches Firefox 160")


if __name__ == "__main__":
    main()
