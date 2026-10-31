"""Independent local peers exercise the shared macOS/Linux network code."""

from http.server import ThreadingHTTPServer
import os
from pathlib import Path
import ssl
import sys
import threading
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tls_fixture import Certificates, H1, Handler, Server
from cryptography.x509.verification import VerificationError
from necko.adapters import RoutingClient
from necko.core_client import CoreClient, _hello_shape
from ruyijs_necko import decode_body


def body(response):
    return response.raw.read(8 * 1024 * 1024)


class TLSChecks(Certificates, unittest.TestCase):
    def setUp(self):
        self.server = Server(("127.0.0.1", 0), Handler)
        self.server.requests = self.server.connections = 0
        self.server.tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.server.tls.load_cert_chain(self.directory / "cert.pem", self.directory / "key.pem")
        self.server.tls.set_alpn_protocols(["h2", "http/1.1"])
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = f"https://localhost:{self.server.server_address[1]}"
        self.environment = patch.dict(os.environ, {"NECKO_EXTRA_CA": str(self.ca)})
        self.environment.start()
        self.client = CoreClient()
        self.assertTrue(self.client.available)

    def tearDown(self):
        self.client.close()
        self.environment.stop()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)

    def test_h1_chunked_compression_and_duplicate_cookies(self):
        self.server.tls.set_alpn_protocols(["http/1.1"])
        for path in ("/", "/chunked", "/gzip"):
            response = self.client.fetch("GET", self.url + path, [], b"", 4)
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.raw.version, 11)
            fields = response.raw.headers._list
            self.assertEqual(sum(name.lower() == "set-cookie" for name, _ in fields), 2)
            encoding = next((v for k, v in fields if k.lower() == "content-encoding"), "")
            self.assertEqual(decode_body(body(response), encoding), b"native-nss:")

    def test_h2_binary_post_and_connection_reuse(self):
        for _ in range(3):
            response = self.client.fetch("POST", self.url, [], b"\0native\xff", 4)
            self.assertEqual(response.raw.version, 20)
            self.assertEqual(body(response), b"native-nss:\0native\xff")
        self.assertEqual(self.server.connections, 1)
        self.assertEqual(self.server.requests, 3)

    def test_https_without_alpn_uses_http1(self):
        self.server.tls.set_alpn_protocols([])
        client = RoutingClient()
        try:
            response = client.fetch("GET", self.url, [], b"", 4)
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.raw.version, 11)
            self.assertEqual(body(response), b"native-nss:")
            self.assertIsNone(client._fallback)
        finally:
            client.close()

    def test_actual_client_hello_profiles(self):
        for version, expected in (("100", (17, 6, 2)), ("159", (15, 5, 3))):
            with patch.dict(os.environ, {"NECKO_FIREFOX": version}):
                response = self.client.fetch("GET", self.url, [], b"", 4)
                self.assertEqual(response.status_code, 200)
                shape = _hello_shape(self.client.last_hello)
                self.assertEqual(tuple(shape[key] for key in
                    ("cipher_count", "group_count", "key_share_count")), expected)

    def test_untrusted_certificate_and_hostname_are_rejected(self):
        with patch.dict(os.environ, {"NECKO_EXTRA_CA": ""}):
            untrusted = CoreClient()
            try:
                with self.assertRaises((ssl.SSLCertVerificationError, VerificationError)) as error:
                    untrusted.fetch("GET", self.url, [], b"", 4)
                self.assertRegex(str(error.exception), "[Cc]ertificate|validation|issuer")
            finally:
                untrusted.close()
        with self.assertRaises((ssl.SSLCertVerificationError, VerificationError)) as error:
            self.client.fetch("GET", self.url.replace("localhost", "127.0.0.1"), [], b"", 4)
        self.assertRegex(str(error.exception), "[Cc]ertificate|validation|subjectAltName")

    def test_shared_cleartext_exchange(self):
        server = ThreadingHTTPServer(("127.0.0.1", 0), H1)
        server.requests = 0
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        client = RoutingClient()
        try:
            response = client.fetch("POST", f"http://127.0.0.1:{server.server_port}/", [], b"linux", 4)
            self.assertEqual(response.status_code, 200)
            self.assertEqual(body(response), b"native-nss:linux")
        finally:
            client.close()
            server.shutdown()
            server.server_close()
            thread.join(2)


class HTTP3Checks(unittest.TestCase):
    def test_http3_rejects_an_untrusted_certificate(self):
        from http3_fixture import Fixture
        from necko.client import NeckoError
        fixture = Fixture()
        try:
            with patch.dict(os.environ, {"NECKO_EXTRA_CA": str(fixture.ca)}):
                client = RoutingClient()
                try:
                    first = client.fetch("GET", fixture.url + "/", [], b"", 4)
                    self.assertEqual(first.raw.version, 20)
                    with patch.dict(os.environ, {"NECKO_EXTRA_CA": ""}):
                        with self.assertRaises((ssl.SSLCertVerificationError, VerificationError, NeckoError)) as error:
                            client.fetch("GET", fixture.url + "/untrusted", [], b"", 4)
                        self.assertRegex(str(error.exception), "[Cc]ertificate|validation|issuer")
                finally:
                    client.close()
        finally:
            fixture.close()

    def test_alt_svc_upgrade_binary_post_and_large_body(self):
        from http3_fixture import Fixture
        fixture = Fixture()
        try:
            with patch.dict(os.environ, {"NECKO_EXTRA_CA": str(fixture.ca)}):
                client = RoutingClient()
                try:
                    first = client.fetch("GET", fixture.url + "/", [], b"", 4)
                    self.assertEqual(first.raw.version, 20)
                    second = client.fetch("POST", fixture.url + "/echo", [], b"\0h3\xff", 4)
                    self.assertEqual(second.raw.version, 30)
                    self.assertEqual(body(second), b"native-http3:\0h3\xff")
                    large = client.fetch("GET", fixture.url + "/large", [], b"", 4)
                    self.assertEqual(large.raw.version, 30)
                    self.assertEqual(body(large), bytes(range(256)) * 1024)
                finally:
                    client.close()
        finally:
            fixture.close()


if __name__ == "__main__":
    unittest.main()
