"""An independent aioquic HTTP/3 server and HTTP/2 Alt-Svc bootstrap."""
import asyncio
from pathlib import Path
import ssl
import sys
import threading
from aioquic.asyncio import QuicConnectionProtocol, serve
from aioquic.h3.connection import H3Connection
from aioquic import tls
from aioquic.buffer import Buffer
from aioquic.h3.events import HeadersReceived, DataReceived
from aioquic.quic.configuration import QuicConfiguration

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tls_fixture import Handler, Server, Certificates


class Protocol(QuicConnectionProtocol):
    def __init__(self, *args, owner, **kwargs):
        super().__init__(*args, **kwargs)
        self.http = H3Connection(self._quic)
        self.requests = {}
        self.owner = owner
        self.owner.connections += 1

    def quic_event_received(self, event):
        for e in self.http.handle_event(event):
            if isinstance(e, HeadersReceived):
                self.requests[e.stream_id] = [dict(e.headers), bytearray()]
            elif isinstance(e, DataReceived):
                self.requests[e.stream_id][1].extend(e.data)
            else:
                continue
            if e.stream_ended:
                headers, data = self.requests.pop(e.stream_id)
                path = headers[b":path"]
                self.owner.paths.append(path.decode())
                if path == b"/slow":
                    continue
                body = b"native-http3:" + data
                if path == b"/large":
                    body = bytes(range(256)) * 1024
                fields = [(b":status", b"200"), (b"content-length", str(len(body)).encode()),
                    (b"set-cookie", b"h3one=1; Path=/; Secure"), (b"set-cookie", b"h3two=2; Path=/; Secure")]
                self.http.send_headers(e.stream_id, fields)
                self.http.send_data(e.stream_id, body, end_stream=True)
                self.transmit()


class Fixture:
    def __init__(self):
        Certificates.setUpClass()
        self.directory, self.ca = Certificates.directory, Certificates.ca
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self.loop.run_forever, daemon=True)
        self.thread.start()
        self.paths = []
        self.connections = 0
        self.parameters = []
        self._pull_hello = tls.pull_client_hello
        def pull_hello(buf):
            hello = self._pull_hello(buf)
            for kind, value in hello.other_extensions:
                if kind == 0x39:
                    wire = Buffer(data=value)
                    ids = []
                    while not wire.eof():
                        ids.append(wire.pull_uint_var())
                        wire.pull_bytes(wire.pull_uint_var())
                    self.parameters.append(ids)
            return hello
        tls.pull_client_hello = pull_hello
        self.server = asyncio.run_coroutine_threadsafe(self.start(), self.loop).result(5)
        self.port = self.server._transport.get_extra_info("sockname")[1]
        self.tcp = Server(("127.0.0.1", 0), Handler)
        self.tcp.requests = 0
        self.tcp.alt_svc = f'h3="127.0.0.1:{self.port}"; ma=3600'
        self.tcp.tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.tcp.tls.load_cert_chain(self.directory / "cert.pem", self.directory / "key.pem")
        self.tcp.tls.set_alpn_protocols(["h2", "http/1.1"])
        self.tcp_thread = threading.Thread(target=self.tcp.serve_forever, daemon=True)
        self.tcp_thread.start()
        self.url = f"https://localhost:{self.tcp.server_address[1]}"

    async def start(self):
        configuration = QuicConfiguration(is_client=False, alpn_protocols=["h3"])
        configuration.load_cert_chain(self.directory / "cert.pem", self.directory / "key.pem")
        return await serve("127.0.0.1", 0, configuration=configuration,
            create_protocol=lambda *a, **kw: Protocol(*a, owner=self, **kw))

    def close(self):
        async def stop():
            self.server.close()
            await asyncio.sleep(0)
        asyncio.run_coroutine_threadsafe(stop(), self.loop).result(5)
        self.loop.call_soon_threadsafe(self.loop.stop)
        self.thread.join(5)
        self.loop.close()
        self.tcp.shutdown()
        self.tcp.server_close()
        self.tcp_thread.join(2)
        tls.pull_client_hello = self._pull_hello
        Certificates.tearDownClass()
