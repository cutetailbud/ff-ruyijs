"""Resource DTO conversion; js.exe owns matching, storage and quotas."""
import base64


class ResourceControl:
    def export_cookies(self):
        """Snapshot all live cookies in this Context, including HttpOnly.

        Records retain domain/path, hostOnly, secure, httpOnly, sameSite,
        session, expires, created and order. Times are Unix seconds; session
        expiry is None. Strings map jar bytes to Latin-1 without losing bytes.
        This host RPC installs no page properties or lexical bindings.
        """
        self._callbacks.guard()
        with self._lock:
            return self._request("cookies", action="export")

    def cookie_header(self, url, *, source_url=None, navigation=False, method="GET"):
        """Select the Cookie header for an external HTTP(S) request, including HttpOnly.

        Uses the runtime's domain/path/Secure/expiry/SameSite rules and preserves
        duplicate names. source_url is the initiating site's URL; None or ""
        means no initiating site. navigation marks a top-level navigation.
        Strings preserve bytes as Latin-1. Recompute for each redirect target.
        """
        self._callbacks.guard()
        if not isinstance(url, str):
            raise TypeError("url must be a string")
        if source_url is not None and not isinstance(source_url, str):
            raise TypeError("source_url must be a string or None")
        if type(navigation) is not bool:
            raise TypeError("navigation must be a boolean")
        if not isinstance(method, str):
            raise TypeError("method must be a string")
        with self._lock:
            return self._request("cookies", action="header", url=url,
                                 source_url=source_url or "", navigation=navigation, method=method)

    def _resource(self, action, **fields):
        self._callbacks.guard()
        with self._lock:
            return self._request("resource", action=action, **fields)

    def put_resource(self, url, body, status=200, headers=None):
        """Atomically store a text or bytes response under an explicit resource key.

        Headers are ordered name/value pairs. This operation does not perform a
        network request; the runtime validates status, fields and storage limits.
        """
        self._callbacks.guard()
        if not isinstance(url, str):
            raise TypeError("resource url must be a string")
        url.encode("utf-8", errors="strict")
        if isinstance(body, str):
            kind, data = "text", body.encode("utf-8", errors="strict")
        elif isinstance(body, (bytes, bytearray, memoryview)):
            kind, data = "bytes", bytes(body)
        else:
            raise TypeError("resource body must be text or bytes")
        return self._resource("put", url=url,
            body={"kind": kind, "base64": base64.b64encode(data).decode("ascii")},
            status=status, headers=[] if headers is None else headers)

    def get_resource(self, url):
        """Return an independent snapshot with a str/bytes body, or None."""
        self._callbacks.guard()
        if not isinstance(url, str):
            raise TypeError("resource url must be a string")
        url.encode("utf-8", errors="strict")
        result = self._resource("get", url=url)
        if result is None:
            return None
        body = result["body"]
        data = base64.b64decode(body["base64"], validate=True)
        if base64.b64encode(data).decode("ascii") != body["base64"]:
            raise ValueError("invalid resource body encoding")
        if body["kind"] not in ("text", "bytes"):
            raise ValueError("invalid resource body kind")
        result["body"] = data.decode("utf-8", errors="strict") if body["kind"] == "text" else data
        return result

    def resource_status(self):
        """Read the shared resource count and charged storage bytes."""
        return self._resource("status")

    def net_log(self, clear=False):
        """Return the ordered page requests recorded by the resource owner.

        Each entry has id, method, url, requestBody, status and matched. A request
        carrying cookies also includes cookieNames (names only, never values). The
        log is appended by the same aggregate call that matches a request, so
        an unmatched request is listed with matched False and status 0.

        The log keeps at most 4096 entries (16 MiB) and then stops recording.
        With clear=True the same command also drops them, the atomic
        read-and-drain a long-running host uses; ids keep counting.
        """
        if not isinstance(clear, bool):
            raise TypeError("clear must be a boolean")
        return self._resource("log", clear=clear)["entries"]

    def clear_net_log(self):
        """Drop the recorded requests. Stored resources and cookies stay."""
        self._resource("log", clear=True)

    def clear_resources(self):
        """Clear the runtime resource directory and return its empty status.

        The net log is separate: clear it with clear_net_log().
        """
        return self._resource("clear")
