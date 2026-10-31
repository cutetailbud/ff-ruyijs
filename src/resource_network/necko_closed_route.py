"""Closed route shared with the published 0.2.1 adapter.

Cleartext uses the necko_socket_* and necko_exchange_* exports. HTTP/3 uses
the necko_quic_* exports in front of Neqo 0.32.0. Python owns the UDP socket
and certificate check. This module does not start the donor socket process.
"""

from __future__ import annotations

import base64
import ctypes
import json
import math
import os
import re
import select
import socket
import struct
import sys
import time
from pathlib import Path
from urllib.parse import urlsplit

from necko.client import NeckoError, NeckoResponse

QUIC_VERSION = "neqo-0.32.0/nss-3.131"
_H3_ENTRY = re.compile(r'\s*h3="([^"\\]+)"\s*((?:;[^,]*)?)')
_ALT_SPLIT = re.compile(r",(?=\s*[a-zA-Z0-9-]+=)")
_MA = re.compile(r";\s*ma\s*=\s*(\d+)")
_HOP = {
    "connection",
    "keep-alive",
    "proxy-connection",
    "transfer-encoding",
    "upgrade",
    "te",
}


def _pairs(headers):
    for item in headers:
        if isinstance(item, (tuple, list)) and len(item) == 2:
            yield str(item[0]), str(item[1])
            continue
        name, value = item
        yield str(name), str(value)


def remember_alternative(value, origin_host):
    """Parse one Alt-Svc value the way the 0.2.1 transport remembers h3."""
    if str(value).strip() == "clear":
        return ("clear",)
    for entry in _ALT_SPLIT.split(str(value)):
        match = _H3_ENTRY.fullmatch(entry)
        if not match:
            continue
        authority, parameters = match.groups()
        try:
            text = origin_host + authority if authority.startswith(":") else authority
            endpoint = urlsplit("https://" + text)
            port = endpoint.port
            if not endpoint.hostname or not port or endpoint.username or endpoint.path:
                continue
            age_match = _MA.search(parameters or "")
            age = int(age_match.group(1)) if age_match else 86400
        except ValueError:
            continue
        return ("set", endpoint.hostname, port, min(age, 86400))
    return ("ignore",)


def _format_address(address):
    host, port = address[0], address[1]
    if len(address) == 4 and address[3]:
        host = host + "%" + str(address[3])
    if ":" in host:
        return f"[{host}]:{port}"
    return f"{host}:{port}"


def _quic_library_path():
    name = (
        "necko_quic.dll"
        if os.name == "nt"
        else "libnecko_quic.dylib"
        if sys.platform == "darwin"
        else "libnecko_quic.so"
    )
    override = os.environ.get("NECKO_QUIC_LIBRARY", "")
    if override:
        return Path(override)
    core = os.environ.get("NECKO_CORE_LIBRARY", "")
    if core:
        candidate = Path(core).with_name(name)
        if candidate.is_file():
            return candidate
    if os.name != "nt":
        for parent in Path(__file__).resolve().parents:
            for directory in (parent / "dist/necko_tls", parent / "dist/necko"):
                candidate = directory / name
                if candidate.is_file():
                    return candidate
    root = Path(__file__).resolve().parents[2]
    for directory in (root / "dist" / "necko_tls", root / "dist" / "necko"):
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def _ensure_necko_win():
    root = Path(__file__).resolve().parents[2]
    native = root / "native" / "necko_tls"
    if (native / "necko_win").is_dir():
        text = str(native)
        if text not in sys.path:
            sys.path.insert(0, text)


def _quic_profile():
    _ensure_necko_win()
    from necko_win._firefox import DEFAULT_FIREFOX, PROFILES

    version = os.environ.get("NECKO_FIREFOX") or DEFAULT_FIREFOX
    return PROFILES[str(version)]["quic"]


_Decoder = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_uint16,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.c_void_p,
    ctypes.c_size_t,
)
_QUIC = {"library": None, "decoder": None}


def _decode_certificate(_algorithm, data, length, output, capacity):
    try:
        raw = ctypes.string_at(data, length)
        if _algorithm == 1:
            import zlib

            decoded = zlib.decompress(raw)
        elif _algorithm == 2:
            import brotli

            decoded = brotli.decompress(raw)
        else:
            import zstandard

            decoded = zstandard.ZstdDecompressor().decompress(raw)
    except Exception:
        return -1
    if len(decoded) > capacity:
        return -1
    ctypes.memmove(output, decoded, len(decoded))
    return len(decoded)


def _load_quic():
    if _QUIC["library"] is not None:
        return _QUIC["library"]
    path = _quic_library_path()
    if path is None or not path.is_file():
        raise NeckoError(f"HTTP/3 unit {QUIC_VERSION} is not built")
    if os.name == "nt" and hasattr(os, "add_dll_directory"):
        os.add_dll_directory(str(path.parent))
    try:
        library = ctypes.CDLL(str(path))
    except OSError as error:
        raise NeckoError(f"HTTP/3 unit {QUIC_VERSION} is not built") from error
    library.necko_quic_init.argtypes = []
    library.necko_quic_init.restype = ctypes.c_int
    library.necko_quic_version.argtypes = []
    library.necko_quic_version.restype = ctypes.c_char_p
    library.necko_quic_set_decoder.argtypes = [_Decoder]
    library.necko_quic_set_decoder.restype = None
    library.necko_quic_call.argtypes = [ctypes.c_uint64, ctypes.c_char_p, ctypes.c_size_t]
    library.necko_quic_call.restype = ctypes.c_char_p
    if library.necko_quic_init() != 0:
        raise NeckoError(f"HTTP/3 unit {QUIC_VERSION} is not built")
    reported = library.necko_quic_version()
    if reported != QUIC_VERSION.encode("ascii"):
        raise NeckoError(f"HTTP/3 unit {QUIC_VERSION} is not built")
    decoder = _Decoder(_decode_certificate)
    library.necko_quic_set_decoder(decoder)
    _QUIC["decoder"] = decoder
    _QUIC["library"] = library
    return library


def _quic_call(library, handle, operation, **arguments):
    payload = json.dumps({"op": operation, **arguments}).encode("utf-8")
    raw = library.necko_quic_call(ctypes.c_uint64(handle), payload, len(payload))
    if not raw:
        raise NeckoError("HTTP/3 command returned nothing")
    result = json.loads(raw)
    if "error" in result:
        raise NeckoError(str(result["error"]))
    return result


def _authority(parts, headers):
    for name, value in _pairs(headers):
        if name.lower() == "host" and value:
            return value
    host = parts.hostname or ""
    port = parts.port
    if port and not ((parts.scheme == "https" and port == 443) or (parts.scheme == "http" and port == 80)):
        return f"{host}:{port}"
    return host


def _h3_fields(headers):
    fields = []
    priority = ""
    for name, value in _pairs(headers):
        lowered = name.lower()
        if lowered == "priority":
            priority = value
        if lowered in _HOP or lowered == "host":
            continue
        try:
            encoded = value.encode("latin-1")
        except UnicodeEncodeError as error:
            raise NeckoError("Invalid HTTP/3 header value") from error
        fields.append([lowered, base64.b64encode(encoded).decode("ascii")])
    urgency = 4
    incremental = False
    for item in priority.replace(" ", "").split(","):
        if item.startswith("u=") and item[2:].isdigit():
            urgency = min(7, int(item[2:]))
        elif item == "i":
            incremental = True
    return fields, urgency, incremental


def _apply_event(event, state):
    kind = event.get("type")
    if kind == "auth":
        certificates = [base64.b64decode(value) for value in event.get("certificates", [])]
        packed = b"".join(struct.pack("!I", len(item)) + item for item in certificates)
        try:
            state["context"].verify(state["hostname"], packed)
        except Exception as error:
            state["reject"] = True
            raise NeckoError("HTTP/3 certificate verification failed") from error
        state["accepted"] = True
        return
    if kind == "ech_retry":
        raise NeckoError("HTTP/3 ECH retry requires updated DNS configuration")
    if kind == "state" and event.get("state") == "connected":
        state["connected"] = True
        return
    if kind == "state" and event.get("state") == "going_away":
        state["goaway"] = event.get("last")
        return
    if kind == "state" and event.get("state") == "closed":
        raise NeckoError(event.get("reason") or "HTTP/3 connection closed")
    if kind == "headers" and not event.get("interim"):
        for name, encoded in event.get("headers", []):
            value = base64.b64decode(encoded).decode("latin-1")
            if name == ":status":
                state["status"] = int(value)
            elif not str(name).startswith(":"):
                state["listed"].append((str(name), value))
        state["headers_done"] = state["status"] is not None
        state["ended"] = state["ended"] or bool(event.get("fin"))
        return
    if kind == "readable":
        state["readable"] = True
        return
    if kind in ("reset", "stop_sending"):
        raise NeckoError(f"HTTP/3 {kind} {event.get('error')}")


def http3_fetch(method, url, headers, body, budget_s, endpoint):
    """One HTTP/3 exchange through necko_quic_* and the linked Neqo client."""
    parts = urlsplit(url)
    if parts.scheme != "https" or not parts.hostname or endpoint is None:
        raise NeckoError("HTTP/3 fetch is a cached direct alternative")
    host, port = endpoint
    payload = body or b""
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    library = _load_quic()
    _ensure_necko_win()
    from necko_win._nss import Context

    family = socket.AF_INET6 if ":" in str(host) else socket.AF_INET
    udp = socket.socket(family, socket.SOCK_DGRAM)
    handle = 0
    try:
        udp.connect((host, int(port)))
        udp.setblocking(False)
        created = _quic_call(
            library,
            0,
            "new",
            host=parts.hostname,
            local=_format_address(udp.getsockname()),
            remote=_format_address(udp.getpeername()),
            token="",
            profile=_quic_profile(),
            webtransport=False,
        )
        handle = int(created["handle"])
        _quic_call(library, handle, "attach", socket=int(udp.fileno()))
        fields, urgency, incremental = _h3_fields(headers)
        state = {
            "hostname": parts.hostname,
            "context": Context(True, None),
            "connected": False,
            "accepted": False,
            "reject": False,
            "headers_done": False,
            "readable": False,
            "ended": False,
            "status": None,
            "listed": [],
            "goaway": None,
            "stream": None,
            "sent": False,
        }
        deadline = time.monotonic() + float(budget_s)
        pieces = []
        state["offset"] = 0
        while time.monotonic() < deadline:
            delay = float(_quic_call(library, handle, "pump").get("delay") or 0)
            saw_event = False
            more = True
            while more:
                batch = _quic_call(library, handle, "events")
                more = bool(batch.get("more"))
                for event in batch.get("events", []):
                    saw_event = True
                    try:
                        _apply_event(event, state)
                    finally:
                        if state["reject"]:
                            try:
                                _quic_call(library, handle, "auth", accepted=False)
                            except NeckoError:
                                pass
                        elif state["accepted"]:
                            _quic_call(library, handle, "auth", accepted=True)
                            state["accepted"] = False
            if state["connected"] and state["stream"] is None:
                if state["goaway"] is not None:
                    raise NeckoError("HTTP/3 connection is draining")
                opened = _quic_call(
                    library,
                    handle,
                    "fetch",
                    method=method,
                    authority=_authority(parts, headers),
                    path=_request_target(parts),
                    headers=fields,
                    urgency=urgency,
                    incremental=incremental,
                )
                state["stream"] = int(opened["stream"])
                saw_event = True
            if state["stream"] is not None and not state["sent"]:
                view = memoryview(payload)
                offset = state["offset"]
                stalled = False
                while offset < len(view):
                    if time.monotonic() >= deadline:
                        raise NeckoError("HTTP/3 stream timed out")
                    chunk = bytes(view[offset : offset + 65536])
                    written = _quic_call(
                        library,
                        handle,
                        "write",
                        stream=state["stream"],
                        data=base64.b64encode(chunk).decode("ascii"),
                    )
                    count = int(written.get("count") or 0)
                    if count <= 0:
                        stalled = True
                        break
                    offset += count
                    saw_event = True
                state["offset"] = offset
                if not stalled and offset >= len(view):
                    _quic_call(library, handle, "finish", stream=state["stream"])
                    state["sent"] = True
                    saw_event = True
            if state["readable"] and state["stream"] is not None and not state["ended"]:
                result = _quic_call(
                    library, handle, "read", stream=state["stream"], size=65536
                )
                data = base64.b64decode(result.get("data") or "")
                if data:
                    pieces.append(data)
                    saw_event = True
                state["ended"] = state["ended"] or bool(result.get("fin"))
                if not data and not state["ended"]:
                    state["readable"] = False
            if state["headers_done"] and state["ended"] and state["sent"]:
                return NeckoResponse(state["status"], b"".join(pieces), 30, state["listed"])
            if saw_event:
                continue
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            select.select([udp], [], [], min(max(delay, 0.0), remaining))
        raise NeckoError("HTTP/3 stream timed out")
    finally:
        if handle:
            try:
                _quic_call(library, handle, "close")
            except NeckoError:
                pass
        udp.close()


def _request_target(parts) -> str:
    path = parts.path or "/"
    if parts.query:
        path = path + "?" + parts.query
    return path


def _wire_headers(parts, headers, body: bytes):
    fields = []
    seen_host = False
    seen_length = False
    for name, value in _pairs(headers):
        if "\x00" in name or "\x00" in value or "\r" in name or "\n" in name:
            raise NeckoError("Invalid native request framing")
        lowered = name.lower()
        if lowered == "host":
            seen_host = True
        if lowered == "content-length":
            seen_length = True
        fields.append(f"{name}: {value}")
    if not seen_host:
        host = parts.hostname
        port = parts.port
        if port and port != 80:
            host = f"{host}:{port}"
        fields.append(f"Host: {host}")
    if body and not seen_length:
        fields.append(f"Content-Length: {len(body)}")
    return fields


_Pointer = ctypes.c_void_p
_Bytes = ctypes.POINTER(ctypes.c_uint8)
_Read = ctypes.CFUNCTYPE(
    ctypes.c_int, _Pointer, _Bytes, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)
)
_Write = ctypes.CFUNCTYPE(ctypes.c_int, _Pointer, _Bytes, ctypes.c_uint32)


def _milliseconds(value):
    if value is None:
        return 0xFFFFFFFF
    if not math.isfinite(value) or value <= 0 or value * 1000 >= 0xFFFFFFFF:
        raise NeckoError("timeout must be positive and finite")
    return max(1, math.ceil(value * 1000))


def _core_library_path():
    if os.name != "nt":
        from necko.core_client import library_path
        return library_path()
    override = os.environ.get("NECKO_CORE_LIBRARY", "")
    if override:
        return Path(override)
    root = Path(__file__).resolve().parents[2]
    name = "necko_core.dll" if os.name == "nt" else "libnecko_core.dylib"
    for directory in (root / "dist" / "necko_tls", root / "dist" / "necko"):
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def _load_core():
    path = _core_library_path()
    if path is None or not path.is_file():
        raise NeckoError("native necko exchange library is unavailable")
    if os.name == "nt" and hasattr(os, "add_dll_directory"):
        os.add_dll_directory(str(path.parent))
    library = ctypes.CDLL(str(path))
    library.necko_socket_open.argtypes = [
        ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint32, ctypes.POINTER(_Pointer)
    ]
    library.necko_socket_open.restype = ctypes.c_int
    library.necko_socket_close.argtypes = [_Pointer]
    library.necko_socket_close.restype = None
    library.necko_socket_read.argtypes = [
        _Pointer, _Bytes, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)
    ]
    library.necko_socket_read.restype = ctypes.c_int
    library.necko_socket_write.argtypes = [_Pointer, _Bytes, ctypes.c_uint32, ctypes.c_uint32]
    library.necko_socket_write.restype = ctypes.c_int
    library.necko_exchange_open.argtypes = [
        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, _Read, _Write, _Pointer,
        ctypes.POINTER(_Pointer),
    ]
    library.necko_exchange_open.restype = ctypes.c_int
    library.necko_exchange_write.argtypes = [_Pointer, _Bytes, ctypes.c_uint32]
    library.necko_exchange_write.restype = ctypes.c_int
    library.necko_exchange_receive.argtypes = [_Pointer]
    library.necko_exchange_receive.restype = ctypes.c_int
    library.necko_exchange_headers.argtypes = [_Pointer]
    library.necko_exchange_headers.restype = ctypes.c_char_p
    library.necko_exchange_read.argtypes = [
        _Pointer, _Bytes, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)
    ]
    library.necko_exchange_read.restype = ctypes.c_int
    library.necko_exchange_reusable.argtypes = [_Pointer]
    library.necko_exchange_reusable.restype = ctypes.c_int
    library.necko_exchange_close.argtypes = [_Pointer]
    library.necko_exchange_close.restype = None
    return library


class _NativeCleartext:
    """Call necko_socket_* and necko_exchange_* the way the 0.2.1 transport does."""

    def __init__(self, library, host, port, timeout_s):
        self.library = library
        self.timeout_ms = _milliseconds(timeout_s)
        self.socket = _Pointer()
        self.exchange = _Pointer()
        self.error = None
        self._read_callback = _Read(self._read)
        self._write_callback = _Write(self._write)
        result = library.necko_socket_open(
            host.encode("idna"), int(port), self.timeout_ms, ctypes.byref(self.socket)
        )
        if result == 5:
            raise NeckoError("Connection timed out")
        if result:
            raise NeckoError(f"Native connection error {result}")

    def _read(self, _closure, buffer, capacity, length):
        try:
            count = ctypes.c_uint32()
            result = self.library.necko_socket_read(
                self.socket, buffer, capacity, self.timeout_ms, ctypes.byref(count)
            )
            if result:
                self.error = NeckoError(
                    "Read timed out" if result == 5 else f"Native socket read error {result}"
                )
                return 6
            length[0] = count.value
            return 0
        except Exception as error:
            self.error = error
            return 6

    def _write(self, _closure, buffer, length):
        try:
            data = ctypes.string_at(buffer, length)
            view = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
            result = self.library.necko_socket_write(
                self.socket, view, len(data), self.timeout_ms
            )
            if result:
                self.error = NeckoError(
                    "Write timed out" if result == 5 else f"Native socket write error {result}"
                )
                return 6
            return 0
        except Exception as error:
            self.error = error
            return 6

    def _check(self, result):
        if self.error is not None:
            error, self.error = self.error, None
            raise error
        if result == 1:
            raise NeckoError("Invalid native request framing or Content-Length")
        if result:
            raise NeckoError(f"Native HTTP framing error {result}")

    def open_exchange(self, method, target, header_block):
        self._check(
            self.library.necko_exchange_open(
                method.encode("ascii"),
                target.encode("ascii"),
                header_block.encode("latin-1"),
                self._read_callback,
                self._write_callback,
                None,
                ctypes.byref(self.exchange),
            )
        )

    def write_body(self, payload: bytes):
        view = memoryview(payload)
        for start in range(0, len(view), 65536):
            part = view[start : start + 65536]
            buffer = (ctypes.c_uint8 * len(part)).from_buffer_copy(part)
            self._check(self.library.necko_exchange_write(self.exchange, buffer, len(part)))

    def receive(self):
        self._check(self.library.necko_exchange_receive(self.exchange))
        wire = self.library.necko_exchange_headers(self.exchange).decode("latin-1")
        status, _, fields = wire.partition("\r\n")
        version_token, code, *_rest = status.split(" ", 2)
        version = 11 if version_token == "HTTP/1.1" else 10
        listed = []
        for line in fields.split("\r\n"):
            if ":" not in line:
                continue
            name, value = line.split(":", 1)
            listed.append((name.strip(), value.strip()))
        return version, int(code), listed

    def read_body(self):
        pieces = []
        while True:
            buffer = (ctypes.c_uint8 * 65536)()
            length = ctypes.c_uint32()
            self._check(
                self.library.necko_exchange_read(
                    self.exchange, buffer, 65536, ctypes.byref(length)
                )
            )
            if length.value == 0:
                break
            pieces.append(bytes(buffer[: length.value]))
            if self.library.necko_exchange_reusable(self.exchange):
                break
        return b"".join(pieces)

    def close(self):
        if self.exchange:
            self.library.necko_exchange_close(self.exchange)
            self.exchange = _Pointer()
        if self.socket:
            self.library.necko_socket_close(self.socket)
            self.socket = _Pointer()
        self._read_callback = None
        self._write_callback = None


def cleartext_fetch(method, url, headers, body, budget_s):
    """One HTTP/1 exchange through necko_socket_* and necko_exchange_*."""
    parts = urlsplit(url)
    if parts.scheme != "http" or not parts.hostname:
        raise NeckoError("cleartext fetch is direct HTTP only")
    target = _request_target(parts)
    if "\x00" in method or "\x00" in target:
        raise NeckoError("Invalid native request framing")
    payload = body or b""
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    header_block = "\r\n".join(_wire_headers(parts, headers, payload)) + "\r\n"
    native = _NativeCleartext(
        _load_core(), parts.hostname, parts.port or 80, budget_s
    )
    try:
        native.open_exchange(method, target, header_block)
        if payload:
            native.write_body(payload)
        version, status, listed = native.receive()
        return NeckoResponse(status, native.read_body(), version, listed)
    finally:
        native.close()
