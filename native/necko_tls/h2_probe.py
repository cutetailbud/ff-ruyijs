"""GET https://example.com/ through libnecko_core. Prints status and ALPN only."""

from __future__ import annotations

import ctypes as ct
import json
import socket
import sys
import warnings
from pathlib import Path

from cryptography.utils import CryptographyDeprecationWarning

warnings.filterwarnings("ignore", category=CryptographyDeprecationWarning)

from h2.config import H2Configuration
from h2.connection import H2Connection
from h2.events import DataReceived, ResponseReceived, StreamEnded

sys.path.insert(0, str(Path(__file__).resolve().parent))

from necko_win._nss import Context, TLS, configure_library  # noqa: E402

HOST = "example.com"
PORT = 443


class SocketTransport:
    def __init__(self, sock: socket.socket) -> None:
        self._sock = sock
        self.timeout = sock.gettimeout()

    def read(self, size: int) -> bytes:
        data = self._sock.recv(size)
        if not data:
            raise ConnectionError("socket closed")
        return data

    def write(self, data: bytes) -> None:
        self._sock.sendall(data)

    def idle(self) -> bool:
        return False

    def close(self) -> None:
        self._sock.close()


def exchange(tls: TLS) -> tuple[int, int]:
    connection = H2Connection(H2Configuration(client_side=True, header_encoding="utf-8"))
    connection.initiate_connection()
    connection.send_headers(
        1,
        [
            (":method", "GET"),
            (":scheme", "https"),
            (":authority", HOST),
            (":path", "/"),
        ],
        end_stream=True,
    )
    tls.write(connection.data_to_send())
    status = None
    ended = False
    body = 0
    for _ in range(32):
        incoming = tls.read(65535)
        for event in connection.receive_data(incoming):
            if isinstance(event, ResponseReceived):
                for name, value in event.headers:
                    if name == ":status":
                        status = int(value)
            elif isinstance(event, DataReceived):
                body += len(event.data)
                connection.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
            elif isinstance(event, StreamEnded) and event.stream_id == 1:
                ended = True
        outgoing = connection.data_to_send()
        if outgoing:
            tls.write(outgoing)
        if status is not None and ended:
            return status, body
    raise TimeoutError("HTTP/2 response did not finish")


def fetch(library, version: str) -> dict:
    context = Context(True, None)
    sock = socket.create_connection((HOST, PORT), timeout=20)
    transport = SocketTransport(sock)
    tls = TLS(transport, context, HOST, library, f"{HOST}:{PORT}", True, version)
    try:
        status, body = exchange(tls)
        return {
            "alpn": tls.alpn,
            "body_bytes": body,
            "status": status,
        }
    finally:
        tls.close()


def main() -> int:
    library_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("dist/necko_tls/libnecko_core.dylib")
    library = ct.CDLL(str(library_path))
    configure_library(library)
    report = {"nss": library.necko_tls_nss_version().decode("ascii")}
    ok = report["nss"] == "3.132"
    for version in ("100", "159"):
        try:
            item = fetch(library, version)
        except Exception as exc:
            item = {"error": f"{type(exc).__name__}: {exc}"}
            ok = False
        else:
            if item["status"] != 200 or item["alpn"] != "h2":
                ok = False
        report[version] = item
    report["pass"] = ok
    print(json.dumps(report, sort_keys=True))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
