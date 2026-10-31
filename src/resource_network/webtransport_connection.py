"""Long-lived HTTP/3 session used only by the WebTransport helper.

Ordinary fetches stay on ``http3_fetch`` with ``webtransport=False``. This
object implements the ``necko._http3.Connection`` calls that
``webtransport_session.Session`` already makes. All ``necko_quic_call``
traffic stays on one thread because the library keeps sessions in
thread-local storage.
"""

import base64
import ctypes
import json
import os
import queue
import selectors
import socket
import struct
import sys
import threading
import time
from pathlib import Path

from urllib3.exceptions import ProtocolError


_Decoder = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_uint16,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.c_void_p,
    ctypes.c_size_t,
)


class _Call:
    def __init__(self, operation, exchange, timeout, arguments):
        self.operation = operation
        self.exchange = exchange
        self.arguments = arguments
        self.done = threading.Event()
        self.result = None
        self.error = None
        self.cancelled = False
        self.deadline = None if timeout is None else time.monotonic() + timeout


def _ensure_necko_win():
    candidate = Path(__file__).resolve().parents[2] / "native" / "necko_tls"
    if (candidate / "necko_win").is_dir():
        text = str(candidate)
        if text not in sys.path:
            sys.path.insert(0, text)


def _library_path():
    if os.name != "nt":
        from necko.closed_route import _quic_library_path
        return _quic_library_path()
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
    root = Path(__file__).resolve().parents[2]
    for directory in (root / "dist" / "necko_tls", root / "dist" / "necko"):
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return None


def _format_address(address):
    host, port = address[0], address[1]
    if len(address) == 4 and address[3]:
        host = host + "%" + str(address[3])
    if ":" in host:
        return "[%s]:%s" % (host, port)
    return "%s:%s" % (host, port)


def _decode(_algorithm, data, length, output, capacity):
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


class Connection:
    def __init__(
        self,
        host,
        port,
        context,
        library_path,
        timeout,
        endpoint=None,
        token="",
        token_callback=None,
        attempt=None,
        firefox=None,
        resolved=None,
        budget_token=None,
        webtransport=None,
    ):
        del library_path, token, token_callback, attempt, budget_token
        self.host = host
        self.port = int(port)
        self.context = context
        self.timeout = timeout
        self.endpoint = endpoint or (host, self.port)
        self.resolved = resolved
        self.firefox = firefox
        self._webtransport = webtransport
        self.streams = {}
        self.error = None
        self.closed = False
        self.info = None
        self.ready = threading.Event()
        self._stopping = threading.Event()
        self._commands = queue.Queue()
        self._handle = 0
        self._library = None
        self._decoder = None
        self._wake_read, self._wake_write = socket.socketpair()
        self._wake_read.setblocking(False)
        self._wake_write.setblocking(False)
        self._thread = threading.Thread(target=self._run, name="necko-webtransport", daemon=True)
        self._thread.start()
        if not self.ready.wait(timeout):
            self.close()
            raise TimeoutError("HTTP/3 handshake timed out")
        if self.error:
            error = self.error
            self.close()
            raise error

    def _wake(self):
        try:
            self._wake_write.send(b"x")
        except (BlockingIOError, OSError):
            pass

    def _call(self, operation, **arguments):
        payload = json.dumps({"op": operation, **arguments}).encode()
        raw = self._library.necko_quic_call(self._handle, payload, len(payload))
        if not raw:
            raise ProtocolError("HTTP/3 command returned nothing")
        result = json.loads(raw)
        if "error" in result:
            raise ProtocolError(result["error"])
        if operation == "new":
            self._handle = int(result["handle"])
        return result

    def submit(self, operation, exchange, timeout, **arguments):
        call = _Call(operation, exchange, timeout, arguments)
        self._commands.put(call)
        self._wake()
        if not call.done.wait(timeout):
            call.cancelled = True
            raise TimeoutError("HTTP/3 stream timed out")
        if call.error:
            raise call.error
        return call.result

    def close(self):
        self._stopping.set()
        self._wake()
        if threading.current_thread() is not self._thread and self._thread.is_alive():
            self._thread.join(self.timeout)

    def _execute(self, call):
        operation = call.operation
        if self._webtransport is not None:
            self._webtransport.check_operation(call.exchange, operation)
        if operation == "webtransport":
            closing = call.arguments.get("action") == "close"
            if closing:
                # Neqo removes the session during close. Read its final live
                # snapshot while the stream ID still belongs to that session.
                snapshot = self._call("webtransport", action="info", session=call.arguments["session"])
            call.result = self._call("webtransport", **call.arguments)
            if call.arguments.get("action") == "stream":
                self._webtransport.add_endpoint(self, call.result["stream"])
            if closing:
                self._webtransport.info.update((key, value) for key, value in snapshot.items() if value is not None)
                self._webtransport.info.update(call.result)
                self._call("pump")
            return True
        exchange = call.exchange
        if operation == "wt_read":
            if exchange.ended:
                call.result = {"data": "", "fin": True, "pending": False}
                return True
            result = self._call("read", stream=exchange.stream_id, **call.arguments)
            exchange.ended = exchange.ended or bool(result.get("fin"))
            exchange.readable = bool(result.get("data")) and not exchange.ended
            call.result = dict(result, pending=not result.get("data") and not result.get("fin"))
            return True
        if operation == "write":
            if exchange.send_stopped:
                call.result = len(base64.b64decode(call.arguments.get("data") or ""))
                return True
            result = self._call("write", stream=exchange.stream_id, **call.arguments)
            count = int(result.get("count") or 0)
            if not count:
                return False
            call.result = count
            return True
        if operation == "finish":
            if not exchange.send_stopped:
                self._call("finish", stream=exchange.stream_id)
            return True
        raise ProtocolError("Unsupported WebTransport connection operation")

    def _events(self):
        batch = self._call("events")
        for event in batch.get("events", []):
            kind = event.get("type")
            if isinstance(kind, str) and kind.startswith("wt_") and self._webtransport is not None:
                self._webtransport.event(self, event)
                continue
            if kind in ("reset", "stop_sending") and self._webtransport is not None:
                if self._webtransport.stream_event(self, event):
                    continue
            if kind == "auth":
                certificates = [base64.b64decode(value) for value in event.get("certificates", [])]
                packed = b"".join(struct.pack("!I", len(item)) + item for item in certificates)
                try:
                    self.context.verify(self.host, packed)
                except Exception as error:
                    self._call("auth", accepted=False)
                    raise ProtocolError("HTTP/3 certificate verification failed") from error
                self._call("auth", accepted=True)
            elif kind == "state" and event.get("state") == "connected":
                self.ready.set()
            elif kind == "state" and event.get("state") == "closed":
                raise ProtocolError(event.get("reason") or "HTTP/3 connection closed")
            elif kind == "ech_retry":
                raise ProtocolError("HTTP/3 ECH retry requires updated DNS configuration")
        return bool(batch.get("more"))

    def _open_library(self):
        _ensure_necko_win()
        path = _library_path()
        if path is None or not path.is_file():
            raise ProtocolError("HTTP/3 unit is not built")
        if os.name == "nt" and hasattr(os, "add_dll_directory"):
            os.add_dll_directory(str(path.parent))
        library = ctypes.CDLL(str(path))
        library.necko_quic_init.argtypes = []
        library.necko_quic_init.restype = ctypes.c_int
        library.necko_quic_version.argtypes = []
        library.necko_quic_version.restype = ctypes.c_char_p
        library.necko_quic_set_decoder.argtypes = [_Decoder]
        library.necko_quic_set_decoder.restype = None
        library.necko_quic_call.argtypes = [ctypes.c_uint64, ctypes.c_char_p, ctypes.c_size_t]
        library.necko_quic_call.restype = ctypes.c_char_p
        if library.necko_quic_init() != 0:
            raise ProtocolError("HTTP/3 unit is not built")
        self._decoder = _Decoder(_decode)
        library.necko_quic_set_decoder(self._decoder)
        self._library = library

    def _run(self):
        udp = None
        selector = None
        try:
            self._open_library()
            from necko_win._firefox import DEFAULT_FIREFOX, PROFILES

            version = os.environ.get("NECKO_FIREFOX") or self.firefox or DEFAULT_FIREFOX
            profile = PROFILES[str(version)]["quic"]
            family = socket.AF_INET6 if ":" in str(self.endpoint[0]) else socket.AF_INET
            udp = socket.socket(family, socket.SOCK_DGRAM)
            udp.connect((self.endpoint[0], int(self.endpoint[1])))
            udp.setblocking(False)
            selector = selectors.DefaultSelector()
            selector.register(udp, selectors.EVENT_READ)
            selector.register(self._wake_read, selectors.EVENT_READ)
            created = self._call(
                "new",
                host=self.host,
                local=_format_address(udp.getsockname()),
                remote=_format_address(udp.getpeername()),
                token="",
                profile=profile,
                webtransport=self._webtransport is not None,
            )
            del created
            self._call("attach", socket=int(udp.fileno()))
            pending = []
            while not self._stopping.is_set():
                self._events()
                while True:
                    try:
                        pending.append(self._commands.get_nowait())
                    except queue.Empty:
                        break
                waiting = []
                for call in pending:
                    if call.cancelled or (call.deadline is not None and time.monotonic() >= call.deadline):
                        call.error = TimeoutError("HTTP/3 stream timed out")
                        call.done.set()
                        continue
                    try:
                        if not self._execute(call):
                            waiting.append(call)
                            continue
                    except Exception as error:
                        call.error = error
                    call.done.set()
                pending = waiting
                delay = min(0.1, float(self._call("pump").get("delay") or 0))
                if self._events():
                    delay = 0
                readable = {key.fileobj for key, _mask in selector.select(delay)}
                if self._wake_read in readable:
                    try:
                        while self._wake_read.recv(4096):
                            pass
                    except BlockingIOError:
                        pass
                if udp in readable:
                    self._call("pump")
        except Exception as error:
            self.error = error
            self.ready.set()
            if self._webtransport is not None:
                self._webtransport.connection_closed(error)
        finally:
            self.closed = True
            self.ready.set()
            error = self.error or ProtocolError("HTTP/3 connection closed")
            if self._webtransport is not None:
                self._webtransport.connection_closed(error)
            while True:
                try:
                    call = self._commands.get_nowait()
                except queue.Empty:
                    break
                call.error = call.exchange.error if getattr(call.exchange, "error", None) else error
                call.done.set()
            if self._library is not None and self._handle:
                try:
                    self._call("close")
                except Exception:
                    pass
            if selector is not None:
                selector.close()
            if udp is not None:
                udp.close()
            self._wake_read.close()
            self._wake_write.close()
