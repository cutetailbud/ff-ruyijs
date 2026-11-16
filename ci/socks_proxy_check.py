"""Check SOCKS proxy URLs end to end with local servers only (issue #39).

A minimal SOCKS server on 127.0.0.1 speaks SOCKS5 with username/password
(RFC 1928/1929) and SOCKS4/4a, and relays to a local HTTP server. Each case
loads a page through ``Context(proxy=...)`` and checks the body, that the proxy
saw the expected handshake, and that a wrong password fails instead of
falling back to a direct connection. Not on macOS: its transport has no SOCKS
route. Exit status is non-zero on failure.
"""
import socket
import socketserver
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from ruyijs import Context

USER, PASSWORD = "user", "p@ss:wörd"
SEEN = []


class Page(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        body = b"<!doctype html><title>through socks</title><p>ok"
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def exact(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("client closed")
        data += chunk
    return data


def until_zero(sock):
    data = b""
    while not data.endswith(b"\x00"):
        data += exact(sock, 1)
    return data[:-1]


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


class Socks(socketserver.BaseRequestHandler):
    def handle(self):
        sock = self.request
        version = exact(sock, 1)[0]
        if version == 5:
            methods = exact(sock, exact(sock, 1)[0])
            if 2 not in methods:
                sock.sendall(b"\x05\xff")
                SEEN.append(("socks5", "no-auth-offered"))
                return
            sock.sendall(b"\x05\x02")
            exact(sock, 1)
            user = exact(sock, exact(sock, 1)[0]).decode("utf-8")
            password = exact(sock, exact(sock, 1)[0]).decode("utf-8")
            if (user, password) != (USER, PASSWORD):
                sock.sendall(b"\x01\x01")
                SEEN.append(("socks5", "bad-credentials"))
                return
            sock.sendall(b"\x01\x00")
            _, command, _, kind = exact(sock, 4)
            if kind == 1:
                host = socket.inet_ntoa(exact(sock, 4))
            elif kind == 3:
                host = exact(sock, exact(sock, 1)[0]).decode("idna")
            else:
                host = socket.inet_ntop(socket.AF_INET6, exact(sock, 16))
            port = struct.unpack(">H", exact(sock, 2))[0]
            SEEN.append(("socks5", "domain" if kind == 3 else "address", host, port))
            reply_ok = b"\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00"
            reply_fail = b"\x05\x05\x00\x01\x00\x00\x00\x00\x00\x00"
        elif version == 4:
            command, port = struct.unpack(">BH", exact(sock, 3))
            address = exact(sock, 4)
            user = until_zero(sock).decode("utf-8")
            if address[:3] == b"\x00\x00\x00" and address[3]:
                host = until_zero(sock).decode("idna")
                kind = "domain"
            else:
                host = socket.inet_ntoa(address)
                kind = "address"
            SEEN.append(("socks4", kind, host, port, user))
            reply_ok = b"\x00\x5a" + b"\x00" * 6
            reply_fail = b"\x00\x5b" + b"\x00" * 6
        else:
            return
        try:
            upstream = socket.create_connection(("127.0.0.1" if host == "localhost" else host, port), timeout=10)
        except OSError:
            sock.sendall(reply_fail)
            return
        sock.sendall(reply_ok)
        back = threading.Thread(target=relay, args=(upstream, sock), daemon=True)
        back.start()
        relay(sock, upstream)
        back.join(10)
        upstream.close()


class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True


def load(proxy, url):
    with Context(timeout=60, proxy=proxy) as ctx:
        ctx.configure_clock("system")
        try:
            ctx.navigate(url)
        except Exception as error:
            return f"error: {error}"
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            ctx.advance(100)
            if ctx.eval("document.readyState") == "complete":
                return ctx.eval("document.title")
        return "timeout"


def main():
    if sys.platform == "darwin":
        print("skip: the macOS transport has no SOCKS route")
        return 0
    from urllib.parse import quote
    page = ThreadingHTTPServer(("127.0.0.1", 0), Page)
    socks = Server(("127.0.0.1", 0), Socks)
    for server in (page, socks):
        threading.Thread(target=server.serve_forever, daemon=True).start()
    web, proxy = page.server_address[1], socks.server_address[1]
    credentials = f"{quote(USER)}:{quote(PASSWORD, safe='')}"
    cases = [
        # (proxy URL, page URL, expected handshake, page loads)
        (f"socks5://{credentials}@127.0.0.1:{proxy}", f"http://127.0.0.1:{web}/",
         ("socks5", "address", "127.0.0.1", web), True),
        (f"socks5h://{credentials}@127.0.0.1:{proxy}", f"http://localhost:{web}/",
         ("socks5", "domain", "localhost", web), True),
        (f"socks4a://someone@127.0.0.1:{proxy}", f"http://localhost:{web}/",
         ("socks4", "domain", "localhost", web, "someone"), True),
        (f"socks4://127.0.0.1:{proxy}", f"http://127.0.0.1:{web}/",
         ("socks4", "address", "127.0.0.1", web, ""), True),
        (f"socks5://{quote(USER)}:wrong@127.0.0.1:{proxy}", f"http://127.0.0.1:{web}/",
         ("socks5", "bad-credentials"), False),
        (f"socks5://127.0.0.1:{proxy}", f"http://127.0.0.1:{web}/",
         ("socks5", "no-auth-offered"), False),
    ]
    failed = False
    for proxy_url, url, handshake, loads in cases:
        SEEN.clear()
        title = load(proxy_url, url)
        shown = proxy_url.replace(quote(PASSWORD, safe=""), "***")
        ok = (title == "through socks") == loads and handshake in SEEN
        failed |= not ok
        print(f"{'ok' if ok else 'FAIL':4} {shown} -> {title!r}, proxy saw {SEEN[:2]}")
    for server in (page, socks):
        server.shutdown()
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
