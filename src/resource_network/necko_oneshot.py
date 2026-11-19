"""HTTP requests through the existing Necko adapter. No browser or cookie state.

`-m ruyijs_necko.oneshot` serves one request. With `--session` the same process
serves requests in order over the same pipes until its input closes, keeping one
adapter so connections are reused per origin, as Firefox's connection manager
reuses them (docs/NECKO_TRANSPORT.md, ARCHITECTURE "Necko 会话传输").
"""
import os
import struct
import sys
from urllib.parse import urlsplit
from necko.adapters import HTTPAdapter
from requests import Request
from requests.structures import CaseInsensitiveDict
from ruyijs_necko import RequestControl, open_response
from ruyijs_necko.timing import snapshot

LIMIT = 8 * 1024 * 1024  # ResourceNetwork Limits::body_bytes, after decompression.


def seconds(name, default):
    """A positive number of seconds from the environment (at most an hour), else the default."""
    try:
        value = float(os.environ.get(name) or default)
    except ValueError:
        return default
    return value if 0 < value <= 3600 else default


# One request's whole budget. Firefox's connection timeout is 90 s
# (network.http.connection-timeout), so connecting gets no shorter limit of its
# own: TCP, the proxy CONNECT and TLS share this budget with the response, and a
# busy proxy queueing many tunnels no longer fails at an arbitrary 5 s.
# READ bounds the wait for each next byte. Context(http_timeout=...,
# http_read_timeout=...) sets both; the page side allows the redirect chain 5 s
# more (#45).
BUDGET = seconds("RUYIJS_HTTP_TIMEOUT", 15)
READ = seconds("RUYIJS_HTTP_READ_TIMEOUT", 10)
# Firefox network.http.request.max-attempts; one request's total deadline still applies.
ATTEMPTS = RequestControl.MAX_ATTEMPTS


class Framing(ValueError):
    """The request stream itself is unusable; the session ends."""


SOCKS_SCHEMES = ("socks4", "socks4a", "socks5", "socks5h")
SOCKS5_ERRORS = {1: "general failure", 2: "connection not allowed by ruleset", 3: "network unreachable",
                 4: "host unreachable", 5: "connection refused", 6: "TTL expired",
                 7: "command not supported", 8: "address type not supported"}


def transport_proxies():
    proxy = os.environ.get("RUYIJS_HTTP_PROXY", "")
    if not proxy:
        return {}
    address = urlsplit(proxy)
    if (len(proxy) > 65536 or address.scheme not in ("http", "https") + SOCKS_SCHEMES or
            not address.hostname or address.path not in ("", "/") or
            address.query or address.fragment or any(c in proxy for c in "\r\n\x00")):
        raise ValueError("Invalid HTTP proxy configuration")
    address.port  # Validate a supplied numeric port before opening any socket.
    return {"http": proxy, "https": proxy}


# SOCKS 4, 4a, 5 and 5h with a username and password (RFC 1928/1929), as
# Firefox's nsSOCKSIOLayer speaks them. The transport's own negotiator needs
# PySocks, which the runtime does not ship. socks5h and socks4a resolve the
# host at the proxy, socks5 and socks4 here (#39).
def _socks_exact(connection, size):
    data = b""
    while len(data) < size:
        chunk = connection.read(size - len(data))
        if not chunk:
            raise _proxy_error("SOCKS proxy closed the connection during negotiation")
        data += chunk
    return data


def _proxy_error(message):
    from requests.exceptions import ProxyError
    return ProxyError(message)


def _socks_field(value, what):
    from urllib.parse import unquote
    raw = unquote(value).encode("utf-8")
    if not 1 <= len(raw) <= 255:
        raise _proxy_error(f"SOCKS5 {what} must be 1 to 255 bytes")
    return raw


def _socks_resolve(host, port, family=0):
    import ipaddress
    import socket
    rows = socket.getaddrinfo(host, port, family, socket.SOCK_STREAM)
    if not rows:
        raise _proxy_error(f"SOCKS: cannot resolve {host}")
    return ipaddress.ip_address(rows[0][4][0])


def _socks_address(host, port, remote_dns, family=0):
    import ipaddress
    try:
        return ipaddress.ip_address(host.strip("[]"))
    except ValueError:
        return None if remote_dns else _socks_resolve(host, port, family)


def _socks5(connection, proxy, host, port, remote_dns):
    credentials = proxy.username is not None
    connection.write(b"\x05\x02\x00\x02" if credentials else b"\x05\x01\x00")
    version, method = _socks_exact(connection, 2)
    if version != 5:
        raise _proxy_error("SOCKS5 proxy sent an invalid greeting reply")
    if method == 2:
        if not credentials:
            raise _proxy_error("SOCKS5 proxy requires a username and password")
        user, password = _socks_field(proxy.username, "username"), _socks_field(proxy.password or "", "password")
        connection.write(b"\x01" + bytes([len(user)]) + user + bytes([len(password)]) + password)
        if _socks_exact(connection, 2)[1] != 0:
            raise _proxy_error("SOCKS5 proxy rejected the username or password")
    elif method != 0:
        raise _proxy_error("SOCKS5 proxy accepts no offered authentication method")
    address = _socks_address(host, port, remote_dns)
    if address is None:
        name = host.encode("idna")
        if len(name) > 255:
            raise _proxy_error("SOCKS5 host name is too long")
        target = b"\x03" + bytes([len(name)]) + name
    else:
        target = (b"\x01" if address.version == 4 else b"\x04") + address.packed
    connection.write(b"\x05\x01\x00" + target + port.to_bytes(2, "big"))
    version, reply, _, kind = _socks_exact(connection, 4)
    if version != 5:
        raise _proxy_error("SOCKS5 proxy sent an invalid reply")
    if reply != 0:
        raise _proxy_error(f"SOCKS5 proxy: {SOCKS5_ERRORS.get(reply, f'error {reply}')}")
    if kind == 1:
        _socks_exact(connection, 4)
    elif kind == 4:
        _socks_exact(connection, 16)
    elif kind == 3:
        _socks_exact(connection, _socks_exact(connection, 1)[0])
    else:
        raise _proxy_error("SOCKS5 proxy sent an unknown bound address type")
    _socks_exact(connection, 2)


def _socks4(connection, proxy, host, port, remote_dns):
    import socket
    from urllib.parse import unquote
    user = unquote(proxy.username).encode("utf-8") if proxy.username else b""
    address = _socks_address(host, port, remote_dns, socket.AF_INET)
    if address is not None and address.version != 4:
        raise _proxy_error("SOCKS4 reaches IPv4 addresses only")
    if address is None:  # SOCKS4a: 0.0.0.x, the name follows the user id
        request = (b"\x04\x01" + port.to_bytes(2, "big") + b"\x00\x00\x00\x01" + user + b"\x00" +
                   host.encode("idna") + b"\x00")
    else:
        request = b"\x04\x01" + port.to_bytes(2, "big") + address.packed + user + b"\x00"
    connection.write(request)
    reply = _socks_exact(connection, 8)
    if reply[0] != 0 or reply[1] != 0x5A:
        raise _proxy_error(f"SOCKS4 proxy rejected the request (code {reply[1]})")


def socks_negotiate(connection, proxy, target):
    """Opens the tunnel to `target` (a split URL) through the SOCKS `proxy`."""
    if proxy.scheme not in SOCKS_SCHEMES:
        from requests.exceptions import InvalidSchema
        raise InvalidSchema(f"Unsupported proxy scheme: {proxy.scheme}")
    port = target.port or (443 if target.scheme == "https" else 80)
    remote_dns = proxy.scheme in ("socks5h", "socks4a")
    (_socks5 if proxy.scheme.startswith("socks5") else _socks4)(connection, proxy, target.hostname, port, remote_dns)


def install_socks():
    """Routes the transport's SOCKS negotiation here. The macOS adapter has no
    SOCKS route and keeps rejecting these URLs."""
    try:
        from necko import _socks
    except ImportError:
        return
    _socks.negotiate = socks_negotiate


def read_exact(size):
    data = sys.stdin.buffer.read(size)
    if len(data) != size:
        raise Framing("Truncated oneshot request")
    return data


def read_text(head=None):
    size = struct.unpack("<I", head or read_exact(4))[0]
    if size > 65536:
        raise Framing("Oneshot field is too large")
    return read_exact(size).decode("utf-8")


def write_text(value):
    data = value.encode("utf-8")
    sys.stdout.buffer.write(struct.pack("<I", len(data)) + data)


def write_error(message):
    sys.stdout.buffer.write(struct.pack("<I", 0))
    write_text(message[:1024])
    sys.stdout.buffer.flush()


def read_request(head=None):
    method, url = read_text(head), read_text()
    count = struct.unpack("<I", read_exact(4))[0]
    if count > 256:
        raise Framing("Too many request headers")
    headers = [(read_text(), read_text()) for _ in range(count)]
    length = struct.unpack("<I", read_exact(4))[0]
    if length > LIMIT:
        raise Framing("Request body exceeds 8 MiB")
    return method, url, headers, read_exact(length)


def serve(adapter, method, url, headers, body):
    if sum(len(k.encode("utf-8")) + len(v.encode("utf-8")) for k, v in headers) > 65536:
        raise ValueError("Request headers exceed 64 KiB")
    fields = CaseInsensitiveDict()
    for name, value in headers:
        fields[name] = fields[name] + ", " + value if name in fields else value
    prepared = Request(method, url, headers=fields, data=body or None).prepare()
    # The page side frames the body exactly as Firefox does; requests' own
    # "Content-Length: 0" for a bodiless non-GET (a CORS preflight) is not sent.
    if "Content-Length" not in fields and not body:
        prepared.headers.pop("Content-Length", None)
    control = RequestControl(BUDGET, max_attempts=ATTEMPTS)
    reader = open_response(adapter, prepared, control,
                           timeout=(BUDGET, READ), verify=True, proxies=transport_proxies())
    chunks, total = [], 0
    try:
        for chunk in reader:
            total += len(chunk)
            if total > LIMIT:
                raise ValueError("Response body exceeds 8 MiB")
            chunks.append(chunk)
    finally:
        reader.close()
    payload = b"".join(chunks)
    if len(reader.headers) > 256 or sum(
            len(k.encode("utf-8")) + len(v.encode("utf-8")) for k, v in reader.headers) > 65536:
        raise ValueError("Response headers exceed the transport limit")
    sys.stdout.buffer.write(struct.pack("<I", reader.status))
    sys.stdout.buffer.write(struct.pack("<I", len(reader.headers)))
    for name, value in reader.headers:
        write_text(name)
        write_text(value)
    sys.stdout.buffer.write(struct.pack("<I", len(payload)))
    sys.stdout.buffer.write(payload)
    # A versioned observation trailer accompanies the existing response.
    # Counts come from the one strict decoder, including compressed bodies.
    measured = '--timing-v2' in sys.argv or '--timing-v3' in sys.argv
    establish = '--timing-v3' in sys.argv
    version = 0x33545252 if establish else 0x32545252 if measured else 0x31545252
    sys.stdout.buffer.write(struct.pack('<IQQ', version,
                                       reader.decoder.encoded, reader.decoder.decoded))
    write_text({10: 'http/1.0', 11: 'http/1.1', 20: 'h2', 30: 'h3'}.get(reader.version, ''))
    if establish:
        from ruyijs_necko.timing import FIELDS
        sys.stdout.buffer.write(struct.pack('<dddddddd', *snapshot(control, FIELDS)))
    elif measured:
        sys.stdout.buffer.write(struct.pack('<ddd', *snapshot(control)))
    sys.stdout.buffer.flush()


def main():
    request = read_request()
    adapter = HTTPAdapter()
    try:
        serve(adapter, *request)
    finally:
        adapter.close()


def session():
    adapter = HTTPAdapter()
    try:
        while True:
            head = sys.stdin.buffer.read(4)
            if not head:
                return
            if len(head) != 4:
                raise Framing("Truncated session request")
            request = read_request(head)
            # A failed request leaves the stream framed: report it and go on.
            try:
                serve(adapter, *request)
            except Exception as error:
                write_error(str(error))
    finally:
        adapter.close()


if __name__ == "__main__":
    install_socks()
    try:
        if "--webtransport" in sys.argv[1:]:
            from ruyijs_necko.webtransport_worker import main as webtransport
            webtransport()
        else:
            session() if "--session" in sys.argv[1:] else main()
    except Exception as error:
        write_error(str(error))
        raise SystemExit(1)
