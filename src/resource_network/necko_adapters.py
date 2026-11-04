"""Necko/NSS session for the worker frames.

Direct HTTPS uses libnecko_core when that library reports NSS 3.131.
Direct cleartext HTTP uses the recovered socket and HTTP/1 exchange.
A response Alt-Svc h3 entry is remembered for that origin. The next
direct request uses necko_quic and the linked Neqo client. A proxy uses
the donor xpcshell when that binary is present, the same Necko prefs as
Windows. Without xpcshell, HTTPS uses CONNECT and then libnecko_core, and
cleartext HTTP uses an absolute-form request on the recovered exchange.
Any other non-HTTPS URL keeps the donor xpcshell. A request the library
cannot represent falls back for that request only. Certificate
verification failures stay on the library path. This process does not
store cookies or turn the socket process on. The library route returns a
3xx response; the xpcshell fallback follows up to 20 redirects.
"""

import time
from urllib.parse import urlsplit

from necko.client import NeckoClient


def _should_fallback(error):
    try:
        from necko.core_client import failure_kind
    except ImportError:
        return False
    return failure_kind(error) == "unwired"


class RoutingClient:
    def __init__(self):
        self.last_hello = b""
        self._core = None
        self._core_failed = False
        self._fallback = None
        self._alternatives = {}

    def _origin_key(self, parts):
        return (parts.scheme, parts.hostname, parts.port)

    def _alternative(self, parts):
        key = self._origin_key(parts)
        cached = self._alternatives.get(key)
        if cached is None:
            return None
        host, port, expiry = cached
        if time.monotonic() >= expiry:
            self._alternatives.pop(key, None)
            return None
        return host, port

    def _remember(self, parts, response):
        raw = getattr(response, "raw", None)
        header_list = getattr(getattr(raw, "headers", None), "_list", None)
        if not header_list:
            return
        value = ""
        for name, item in header_list:
            if str(name).lower() == "alt-svc":
                value = str(item)
        if not value:
            return
        from necko.closed_route import remember_alternative

        outcome = remember_alternative(value, parts.hostname)
        key = self._origin_key(parts)
        if outcome[0] == "clear":
            self._alternatives.pop(key, None)
        elif outcome[0] == "set":
            _kind, host, port, age = outcome
            self._alternatives[key] = (host, port, time.monotonic() + age)

    def _xpcshell(self):
        if self._fallback is None:
            self._fallback = NeckoClient()
        return self._fallback

    def _library(self):
        if self._core_failed:
            return None
        if self._core is not None:
            return self._core
        try:
            from necko.closed_route import _ensure_necko_win

            _ensure_necko_win()
            from necko.core_client import CoreClient

            core = CoreClient()
        except (ImportError, OSError, RuntimeError):
            self._core_failed = True
            return None
        if not core.available:
            core.close()
            self._core_failed = True
            return None
        self._core = core
        return core

    def fetch(self, method, url, headers, body, budget_s):
        parts = urlsplit(url)
        from necko.client import proxy_endpoint, xpcshell_available

        proxied = proxy_endpoint() is not None
        if proxied and xpcshell_available():
            client = self._xpcshell()
            response = client.fetch(method, url, headers, body, budget_s)
            self.last_hello = getattr(client, "last_hello", b"") or b""
            return response
        if parts.scheme == "http" and parts.hostname:
            from necko.closed_route import cleartext_fetch

            return cleartext_fetch(method, url, headers, body, budget_s)
        if not proxied and parts.scheme == "https" and parts.hostname:
            from necko.closed_route import http3_fetch

            endpoint = self._alternative(parts)
            if endpoint is not None:
                return http3_fetch(method, url, headers, body, budget_s, endpoint)
        use_core = parts.scheme == "https" and bool(parts.hostname)
        client = self._library() if use_core else None
        if client is None:
            client = self._xpcshell()
            use_core = False
        served = client
        try:
            response = client.fetch(method, url, headers, body, budget_s)
        except Exception as error:
            if not use_core or not _should_fallback(error):
                raise
            served = self._xpcshell()
            response = served.fetch(method, url, headers, body, budget_s)
        self.last_hello = getattr(served, "last_hello", b"") or b""
        if use_core and served is client and parts.scheme == "https":
            self._remember(parts, response)
        return response

    def close(self):
        if self._core is not None:
            self._core.close()
        if self._fallback is not None:
            self._fallback.close()


class HTTPAdapter:
    def __init__(self):
        self.client = RoutingClient()

    def close(self):
        self.client.close()
