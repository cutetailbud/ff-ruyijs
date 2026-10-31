"""Independent aioquic WebTransport echo server, native Neqo/NSS client."""
import json
import os
import base64
from pathlib import Path
import sys
import time
import unittest
from aioquic.asyncio import QuicConnectionProtocol, serve
from aioquic.h3.connection import H3Connection
from aioquic.h3.events import HeadersReceived, DatagramReceived, WebTransportStreamDataReceived
from aioquic.quic.configuration import QuicConfiguration
from ruyijs_necko.webtransport_session import Session

sys.path.insert(0, str(Path(__file__).resolve().parent))
from http3_fixture import Fixture


class Protocol(QuicConnectionProtocol):
    def __init__(self, *args, owner, **kwargs):
        super().__init__(*args, **kwargs)
        self.http = H3Connection(self._quic, enable_webtransport=True)
        self.owner = owner
        self.streams = {}

    def quic_event_received(self, event):
        for e in self.http.handle_event(event):
            if isinstance(e, HeadersReceived):
                fields = dict(e.headers)
                self.owner.paths.append(fields[b':path'].decode())
                self.http.send_headers(e.stream_id, [(b':status', b'200'), (b'wt-protocol', b'"echo"')])
            elif isinstance(e, DatagramReceived):
                self.http.send_datagram(e.stream_id, e.data)
            elif isinstance(e, WebTransportStreamDataReceived):
                if e.stream_id & 2:
                    stream = self.streams.get(e.stream_id)
                    if stream is None:
                        stream = self.http.create_webtransport_stream(e.session_id, is_unidirectional=True)
                        self.streams[e.stream_id] = stream
                else:
                    stream = e.stream_id
                self._quic.send_stream_data(stream, e.data, end_stream=e.stream_ended)
        self.transmit()


class WTFixture(Fixture):
    async def start(self):
        configuration = QuicConfiguration(is_client=False, alpn_protocols=['h3'], max_datagram_frame_size=65536)
        configuration.load_cert_chain(self.directory / 'cert.pem', self.directory / 'key.pem')
        return await serve('127.0.0.1', 0, configuration=configuration,
            create_protocol=lambda *a, **kw: Protocol(*a, owner=self, **kw))


class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = WTFixture()

    @classmethod
    def tearDownClass(cls):
        cls.fixture.close()

    def wait(self, callback):
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            result = callback()
            if result:
                return result
            time.sleep(.01)
        self.fail('WebTransport echo timed out')

    def session(self):
        return Session(f'https://localhost:{self.fixture.port}/echo', 'https://client.test',
                       protocols=['echo'], verify=str(self.fixture.ca), timeout=4)

    def test_bidi_datagram_groups_stats_and_export(self):
        session = self.session()
        try:
            self.assertEqual(session.info['protocol'], 'echo')
            self.assertGreater(session.info['maxDatagramSize'], 0)
            self.assertIn('datagramsExpiredOutgoing', session.info)
            group = session.command('group', session=session.session)['group']
            stream = session.stream(True, group=group, order=3)
            payload = b'\x00webtransport\xff'
            self.assertEqual(session.write(stream, base64.b64encode(payload).decode()), len(payload))
            result = self.wait(lambda: (v if (v := session.read(stream))['data'] else None))
            self.assertEqual(base64.b64decode(result['data']), payload)
            stats = session.command('stream_stats', stream=stream, sending=True)
            self.assertEqual(stats['bytesWritten'], len(payload))
            session.command('order', stream=stream, order=-7)
            session.command('set_group', session=session.session, stream=stream, group=None)
            first = session.command('export', session=session.session, label='dGVzdA==', context='YQ==')['data']
            second = session.command('export', session=session.session, label='dGVzdA==', context='Yg==')['data']
            self.assertEqual(len(base64.b64decode(first)), 32)
            self.assertNotEqual(first, second)
            session.command('datagram', session=session.session, data=base64.b64encode(payload).decode())
            events = self.wait(lambda: [e for e in session.poll() if e['type'] == 'wt_datagram'])
            self.assertEqual(base64.b64decode(events[0]['data']), payload)
            session.finish(stream)
        finally:
            session.close(42, 'finished')
        self.assertFalse(session.connection._thread.is_alive())

    def test_context_javascript_stream_and_datagram(self):
        sys.path.insert(0, os.environ.get("RUYIJS_TEST_PACKAGE") or str(Path(__file__).resolve().parents[1] / "python"))
        from ruyijs import Context
        previous = os.environ.get("NECKO_EXTRA_CA")
        os.environ["NECKO_EXTRA_CA"] = str(self.fixture.ca)
        try:
            with Context(timeout=15) as ctx:
                ctx.load_page("https://client.test/", "<html><body>WebTransport</body></html>")
                url = json.dumps(f'https://localhost:{self.fixture.port}/echo')
                ctx.eval("""globalThis.wtResult=null;(async()=>{
                    const wt=new WebTransport("""+url+""",{protocols:['echo']});
                    await wt.ready;
                    const stream=await wt.createBidirectionalStream();
                    const writer=stream.writable.getWriter();
                    await writer.write(new Uint8Array([0,42,255]));
                    const read=await stream.readable.getReader().read();
                    const dw=wt.datagrams.writable.getWriter();
                    await dw.write(new Uint8Array([1,2,3]));
                    const datagram=await wt.datagrams.readable.getReader().read();
                    wtResult={stream:Array.from(read.value),datagram:Array.from(datagram.value)};
                    wt.close({closeCode:42,reason:'complete'});
                })().catch(e=>wtResult={error:String(e),stack:e.stack});""")
                result = None
                for _ in range(300):
                    ctx.advance(25)
                    result=ctx.eval("wtResult")
                    if result is not None: break
                self.assertEqual(result, {'stream':[0,42,255], 'datagram':[1,2,3]})
        finally:
            if previous is None: os.environ.pop("NECKO_EXTRA_CA", None)
            else: os.environ["NECKO_EXTRA_CA"] = previous

    def test_unidirectional_echo(self):
        session = self.session()
        try:
            stream = session.stream(False)
            session.write(stream, base64.b64encode(b'unidirectional').decode())
            session.finish(stream)
            events = self.wait(lambda: [e for e in session.poll() if e['type'] == 'wt_stream'])
            self.assertFalse(events[0]['bidirectional'])
            result = self.wait(lambda: (v if (v := session.read(events[0]['stream']))['data'] else None))
            self.assertEqual(base64.b64decode(result['data']), b'unidirectional')
        finally:
            session.close()


if __name__ == '__main__':
    unittest.main(verbosity=2)
