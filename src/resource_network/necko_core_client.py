"""HTTPS through libnecko_core for the Mac oneshot.

The donor xpcshell stays in place when this library is missing, reports a
different NSS build, or the request is not direct HTTPS. Cookies and
redirects are still not stored here. One session reuses an HTTP/2 connection
per origin and Firefox version. HTTP/1.1 is one exchange, then the socket
closes. Certificate verification failures stay on this path.
"""

from __future__ import annotations

import json
import os
import socket
import ssl
import sys
import warnings
from pathlib import Path
from urllib.parse import urlsplit

from cryptography.utils import CryptographyDeprecationWarning
from cryptography.x509.verification import VerificationError

warnings.filterwarnings("ignore", category=CryptographyDeprecationWarning)

import ctypes as ct

from h2.events import ConnectionTerminated, DataReceived, ResponseReceived, StreamEnded
from h2.exceptions import H2Error

from necko.client import NeckoError, NeckoResponse
from necko_win._nss import Context, TLS, configure_library
from necko_win._wire import (
    format_authority,
    merge_headers,
    open_connection,
    request_fields,
    send_request,
)


class Unwired(NeckoError):
    """The library cannot represent this request. The xpcshell path may."""


def failure_kind(error):
    """Classify a library failure. ``verify`` must not fall back."""
    if isinstance(error, Unwired):
        return "unwired"
    if isinstance(error, (ssl.SSLCertVerificationError, VerificationError)):
        return "verify"
    text = str(error)
    if "client certificate is not wired" in text:
        return "unwired"
    if "did not negotiate a supported protocol" in text:
        return "unwired"
    if "peer requires HTTP/1.1" in text:
        return "unwired"
    if "TLS cipher profile" in text or "Certificate" in text:
        return "verify"
    return "error"


def library_path() -> Path | None:
    override = os.environ.get("NECKO_CORE_LIBRARY", "")
    if override:
        path = Path(override)
        return path if path.is_file() else None
    here = Path(__file__).resolve()
    name = "libnecko_core.so" if sys.platform == "linux" else "libnecko_core.dylib"
    for parent in here.parents:
        preferred = parent / "dist" / "necko_tls" / name
        staged = parent / "dist" / "necko" / name
        if preferred.is_file():
            return preferred
        if staged.is_file():
            return staged
    sibling = here.parent / name
    if sibling.is_file():
        return sibling
    return None


def _hello_shape(data: bytes) -> dict | None:
    """Count ClientHello fields. The raw suites stay in the record."""
    if len(data) < 5 or data[0] != 22:
        return None
    record_len = int.from_bytes(data[3:5], "big")
    record = data[5 : 5 + record_len]
    if len(record) < 4 or record[0] != 1 or len(record) < record_len:
        return None
    body = record[4:]
    if len(body) < 35:
        return None
    index = 34
    session_len = body[index]
    index += 1 + session_len
    if index + 2 > len(body):
        return None
    cipher_bytes = int.from_bytes(body[index : index + 2], "big")
    index += 2
    cipher_count = 0
    for offset in range(0, cipher_bytes, 2):
        value = int.from_bytes(body[index + offset : index + offset + 2], "big")
        if (value & 0x0F0F) != 0x0A0A:
            cipher_count += 1
    index += cipher_bytes
    if index >= len(body):
        return None
    index += 1 + body[index]
    if index + 2 > len(body):
        return None
    extension_bytes = int.from_bytes(body[index : index + 2], "big")
    index += 2
    end = index + extension_bytes
    group_count = 0
    key_share_count = 0
    signed_cert_timestamps = False
    ech = False
    compression_algorithms = 0
    while index + 4 <= end and index + 4 <= len(body):
        extension_type = int.from_bytes(body[index : index + 2], "big")
        extension_len = int.from_bytes(body[index + 2 : index + 4], "big")
        index += 4
        payload = body[index : index + extension_len]
        index += extension_len
        if extension_type == 10 and len(payload) >= 2:
            group_bytes = int.from_bytes(payload[:2], "big")
            for offset in range(2, 2 + group_bytes, 2):
                value = int.from_bytes(payload[offset : offset + 2], "big")
                if (value & 0x0F0F) != 0x0A0A:
                    group_count += 1
        elif extension_type == 51 and len(payload) >= 2:
            share_bytes = int.from_bytes(payload[:2], "big")
            cursor = 2
            stop = 2 + share_bytes
            while cursor + 4 <= stop and cursor + 4 <= len(payload):
                key_len = int.from_bytes(payload[cursor + 2 : cursor + 4], "big")
                key_share_count += 1
                cursor += 4 + key_len
        elif extension_type == 18:
            signed_cert_timestamps = True
        elif extension_type == 0xFE0D:
            ech = True
        elif extension_type == 27 and payload:
            compression_algorithms = payload[0] // 2
    return {
        "cipher_count": cipher_count,
        "compression_algorithms": compression_algorithms,
        "ech": ech,
        "group_count": group_count,
        "key_share_count": key_share_count,
        "nbytes": 5 + record_len,
        "signed_cert_timestamps": signed_cert_timestamps,
    }


class _Socket:
    def __init__(self, sock: socket.socket) -> None:
        self._sock = sock
        self.timeout = sock.gettimeout()
        self.hello = None

    def read(self, size: int) -> bytes:
        data = self._sock.recv(size)
        if not data:
            raise ConnectionError("socket closed")
        return data

    def write(self, data: bytes) -> None:
        if self.hello is None and data[:1] == b"\x16":
            self.hello = bytes(data)
        self._sock.sendall(data)

    def idle(self) -> bool:
        return False

    def close(self) -> None:
        self._sock.close()


class _Lane:
    def __init__(self, tls, connection, version: str) -> None:
        self.tls = tls
        self.connection = connection
        self.version = version
        self.alpn = tls.alpn


def _text(value) -> str:
    if isinstance(value, bytes):
        return value.decode("latin-1")
    return str(value)


def _reraise(error):
    if isinstance(error, (Unwired, NeckoError, ssl.SSLCertVerificationError, VerificationError)):
        raise error
    kind = failure_kind(error)
    if kind == "verify":
        raise error
    if kind == "unwired":
        raise Unwired(str(error)) from error
    if isinstance(error, (TimeoutError, socket.timeout, ConnectionError, OSError)):
        raise NeckoError(str(error)) from error
    raise NeckoError(str(error)) from error


def _request_path(parts) -> str:
    path = parts.path or "/"
    if parts.query:
        path = path + "?" + parts.query
    return path


def _read_h1(read):
    buffer = bytearray()

    def fill_headers():
        while b"\r\n\r\n" not in buffer:
            chunk = read(65535)
            if not chunk:
                raise NeckoError("HTTP/1.1 response ended during headers")
            buffer.extend(chunk)
            if len(buffer) > 1024 * 1024:
                raise NeckoError("HTTP/1.1 headers are too large")

    while True:
        fill_headers()
        head, _, rest = bytes(buffer).partition(b"\r\n\r\n")
        lines = head.decode("iso-8859-1").split("\r\n")
        parts = lines[0].split(" ", 2)
        if len(parts) < 2 or not parts[1].isdigit():
            raise NeckoError("HTTP/1.1 status line was not understood")
        status = int(parts[1])
        listed = []
        for line in lines[1:]:
            if ":" not in line:
                continue
            name, value = line.split(":", 1)
            listed.append((name.strip(), value.strip()))
        del buffer[:]
        buffer.extend(rest)
        if 100 <= status < 200:
            continue
        return status, listed, buffer


def _body_h1(read, listed, buffer: bytearray) -> bytes:
    headers = {name.lower(): value for name, value in listed}
    if "content-length" in headers:
        need = int(headers["content-length"])
        while len(buffer) < need:
            buffer.extend(read(65535))
        return bytes(buffer[:need])
    if "chunked" in headers.get("transfer-encoding", "").lower():
        payload = bytearray()
        while True:
            while b"\r\n" not in buffer:
                buffer.extend(read(65535))
            line, _, buffer_rest = bytes(buffer).partition(b"\r\n")
            del buffer[:]
            buffer.extend(buffer_rest)
            size = int(line.split(b";", 1)[0], 16)
            if size == 0:
                return bytes(payload)
            while len(buffer) < size + 2:
                buffer.extend(read(65535))
            payload.extend(buffer[:size])
            del buffer[: size + 2]
    while True:
        try:
            incoming = read(65535)
        except (ConnectionError, OSError):
            break
        if not incoming:
            break
        buffer.extend(incoming)
    return bytes(buffer)


class CoreClient:
    def __init__(self) -> None:
        self.available = False
        self.last_hello = b""
        self._library = None
        self._context = None
        self._lanes = {}
        path = library_path()
        if path is None:
            return
        library = ct.CDLL(str(path))
        configure_library(library)
        self._library = library
        self._context = Context(True, None)
        self.available = True

    def _record_hello(self, version: str) -> None:
        target = os.environ.get("NECKO_CORE_HELLO_LOG", "")
        shape = _hello_shape(self.last_hello)
        if not target or shape is None:
            return
        line = json.dumps({"tls": version, **shape}, sort_keys=True)
        with open(target, "a", encoding="utf-8") as handle:
            handle.write(line + "\n")

    def _discard(self, key) -> None:
        lane = self._lanes.pop(key, None)
        if lane is None:
            return
        try:
            lane.tls.close()
        except Exception:
            pass

    def close(self) -> None:
        for key in list(self._lanes):
            self._discard(key)

    def _connect(self, host, port, version, budget_s):
        sock = None
        try:
            sock = socket.create_connection((host, port), timeout=budget_s)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            transport = _Socket(sock)
            tls = TLS(
                transport,
                self._context,
                host,
                self._library,
                f"{host}:{port}",
                True,
                version,
            )
        except Exception as error:
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
            _reraise(error)
        self.last_hello = transport.hello or b""
        self._record_hello(version)
        if tls.alpn == "h2":
            try:
                connection = open_connection(version)
                preface = connection.data_to_send()
                if preface:
                    tls.write(preface)
            except Exception as error:
                try:
                    tls.close()
                except Exception:
                    pass
                _reraise(error)
            return _Lane(tls, connection, version)
        # Servers predating ALPN still speak HTTP/1.1 over the verified TLS
        # connection. An empty ALPN does not require the xpcshell fallback.
        if tls.alpn in ("http/1.1", ""):
            return _Lane(tls, None, version)
        try:
            tls.close()
        except Exception:
            pass
        raise Unwired("necko core did not negotiate a supported protocol")

    def fetch(self, method: str, url: str, headers, body: bytes, budget_s: float, cert=None):
        if cert:
            raise Unwired("client certificate is not wired")
        if not self.available:
            raise NeckoError("necko core library is not available")
        parts = urlsplit(url)
        if parts.scheme != "https" or not parts.hostname:
            raise NeckoError("necko core fetch is direct HTTPS only")
        if os.environ.get("RUYIJS_HTTP_PROXY"):
            raise NeckoError("necko core fetch does not own the proxy path")
        version = os.environ.get("NECKO_FIREFOX") or "159"
        host = parts.hostname
        port = parts.port or 443
        key = (host, port, version)
        lane = self._lanes.get(key)
        if lane is None:
            lane = self._connect(host, port, version, budget_s)
            if lane.alpn == "h2":
                self._lanes[key] = lane
        try:
            if lane.alpn == "h2":
                return self._exchange_h2(lane, method, parts, headers, body or b"")
            return self._exchange_h1(lane, method, parts, headers, body or b"")
        except Exception as error:
            self._discard(key)
            if lane.alpn != "h2":
                try:
                    lane.tls.close()
                except Exception:
                    pass
            _reraise(error)

    def _exchange_h2(self, lane, method, parts, headers, body: bytes):
        authority = format_authority(parts.hostname, parts.port or 443, "https")
        wired = merge_headers(
            method, parts.geturl(), headers, lane.version, True, authority
        )
        fields, document = request_fields(
            method, _request_path(parts), wired, wired.get("Host", authority)
        )
        stream_id = send_request(lane.connection, lane.version, fields, not body, document)
        if body:
            lane.connection.send_data(stream_id, body, end_stream=True)
        outgoing = lane.connection.data_to_send()
        if outgoing:
            lane.tls.write(outgoing)
        status = None
        listed = []
        payload = bytearray()
        ended = False
        try:
            for _ in range(64):
                incoming = lane.tls.read(65535)
                events = lane.connection.receive_data(incoming)
                for event in events:
                    if isinstance(event, ConnectionTerminated):
                        if event.error_code == 13:
                            raise Unwired("HTTP/2 peer requires HTTP/1.1")
                        raise NeckoError("HTTP/2 connection closed")
                    if isinstance(event, ResponseReceived) and event.stream_id == stream_id:
                        for name, value in event.headers:
                            name = _text(name)
                            if name == ":status":
                                status = int(_text(value))
                            elif not name.startswith(":"):
                                listed.append((name, _text(value)))
                    elif isinstance(event, DataReceived) and event.stream_id == stream_id:
                        payload.extend(event.data)
                        lane.connection.acknowledge_received_data(
                            event.flow_controlled_length, event.stream_id
                        )
                    elif isinstance(event, StreamEnded) and event.stream_id == stream_id:
                        ended = True
                outgoing = lane.connection.data_to_send()
                if outgoing:
                    lane.tls.write(outgoing)
                if status is not None and ended:
                    return NeckoResponse(status, bytes(payload), 20, listed)
        except H2Error as exc:
            raise NeckoError(f"HTTP/2 failed: {exc}") from exc
        raise NeckoError("HTTP/2 response did not finish")

    def _exchange_h1(self, lane, method, parts, headers, body: bytes):
        authority = format_authority(parts.hostname, parts.port or 443, "https")
        wired = merge_headers(
            method, parts.geturl(), headers, lane.version, False, authority
        )
        lines = [f"{method} {_request_path(parts)} HTTP/1.1"]
        for name, value in wired.items():
            lines.append(f"{name}: {value}")
        if body and "Content-Length" not in wired:
            lines.append(f"Content-Length: {len(body)}")
        blob = ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1")
        if body:
            blob += body
        try:
            lane.tls.write(blob)
            status, listed, leftover = _read_h1(lane.tls.read)
            payload = _body_h1(lane.tls.read, listed, leftover)
            return NeckoResponse(status, payload, 11, listed)
        finally:
            try:
                lane.tls.close()
            except Exception:
                pass
