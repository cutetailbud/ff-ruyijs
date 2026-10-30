"""Drive one donor xpcshell Necko process. No cookie jar and no Python TLS."""
from __future__ import annotations

import os
import struct
import subprocess
from pathlib import Path
from urllib.parse import urlsplit

READY = b"RDY1"
TEXT_LIMIT = 65536
BODY_LIMIT = 8 * 1024 * 1024


def proxy_endpoint():
    """Return ``(host, port)`` for ``RUYIJS_HTTP_PROXY``, or None when unset.

    An ``https`` proxy URL only selects the default port. It does not open TLS
    to the proxy. The same checks as ``configureProxy()`` in ``necko_fetch.js``.
    ``ruyijs_necko/oneshot.py`` keeps its own copy of them: it also runs in the
    Windows and Linux helpers, which have no ``necko.client``.
    """
    proxy = os.environ.get("RUYIJS_HTTP_PROXY", "")
    if not proxy:
        return None
    address = urlsplit(proxy)
    if (len(proxy) > 65536 or address.scheme not in ("http", "https") or
            not address.hostname or address.path not in ("", "/") or
            address.query or address.fragment or any(c in proxy for c in "\r\n\x00")):
        raise ValueError("Invalid HTTP proxy configuration")
    port = address.port
    if port is None:
        port = 443 if address.scheme == "https" else 80
    return address.hostname, port


def xpcshell_available() -> bool:
    """True when the donor or packaged xpcshell binary and script exist."""
    try:
        binary, script = _donor_paths()
    except NeckoError:
        return False
    return binary.is_file() and script.is_file()


def _packaged_xpcshell():
    try:
        from ruyijs._runtime_paths import get_runtime_path
    except Exception:
        return None
    candidate = get_runtime_path() / "dist" / "necko" / "xpcshell"
    return candidate if candidate.is_file() else None


def _donor_paths():
    script_env = os.environ.get("RUYIJS_NECKO_SCRIPT")
    binary_env = os.environ.get("RUYIJS_XPCSHELL")
    repo = None
    marker = Path("src") / "resource_network" / "necko_fetch.js"
    here = Path(__file__).resolve().parent
    for parent in here.parents:
        if (parent / marker).is_file():
            repo = parent
            break
    sibling = here / "necko_fetch.js"
    if script_env:
        script = Path(script_env)
    elif repo is not None:
        script = repo / marker
    elif sibling.is_file():
        script = sibling
    else:
        raise NeckoError("Necko donor script was not found")
    if binary_env:
        binary = Path(binary_env)
    else:
        packaged = _packaged_xpcshell()
        if packaged is not None:
            binary = packaged
        elif repo is not None:
            candidates = [
                repo.parent / "firefox-src" / "firefox-159.0" / "obj-necko" / "dist" / "bin" / "xpcshell",
                repo.parent / "firefox-159" / "obj-necko" / "dist" / "bin" / "xpcshell",
                repo.parent / "firefox-159.0" / "obj-necko" / "dist" / "bin" / "xpcshell",
            ]
            for candidate in candidates:
                if candidate.is_file():
                    binary = candidate
                    break
            else:
                binary = repo.parent / "firefox-159" / "obj-necko" / "dist" / "bin" / "xpcshell"
        else:
            raise NeckoError("Necko donor xpcshell was not found")
    return binary, script


class NeckoError(RuntimeError):
    pass


class _Raw:
    def __init__(self, payload: bytes, version: int, headers: list[tuple[str, str]]):
        self._payload = payload
        self._at = 0
        self.version = version
        self.headers = type("HeaderList", (), {"_list": headers})()

    def read(self, size: int, decode_content: bool = False) -> bytes:
        if self._at >= len(self._payload):
            return b""
        chunk = self._payload[self._at : self._at + size]
        self._at += len(chunk)
        return chunk


class NeckoResponse:
    def __init__(self, status: int, payload: bytes, version: int, headers: list[tuple[str, str]]):
        self.status_code = status
        self.headers = {k: v for k, v in headers}
        self.raw = _Raw(payload, version, headers)

    def close(self) -> None:
        return None


def _u32(value: int) -> bytes:
    return struct.pack("<I", value)


def _text(value: str) -> bytes:
    data = value.encode("utf-8")
    if len(data) > TEXT_LIMIT:
        raise NeckoError("Necko field is too large")
    return _u32(len(data)) + data


class NeckoClient:
    def __init__(self) -> None:
        binary, script = _donor_paths()
        if not binary.is_file() or not script.is_file():
            raise NeckoError("Necko donor xpcshell is not built")
        env = os.environ.copy()
        for key in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "http_proxy", "https_proxy", "all_proxy"):
            env.pop(key, None)
        env["MOZ_HEADLESS"] = "1"
        env["XPCOM_DEBUG_BREAK"] = "warn"
        env["MOZ_DISABLE_SOCKET_PROCESS"] = "1"
        self._stderr = open("/tmp/ruyijs-necko-xpcshell.log", "ab")
        self._proc = subprocess.Popen(
            [str(binary), str(script)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self._stderr,
            cwd=str(binary.parent),
            env=env,
            start_new_session=True,
        )
        assert self._proc.stdout is not None
        ready = self._read(4)
        if ready != READY:
            self.close()
            raise NeckoError("Necko donor did not start")

    def _read(self, size: int) -> bytes:
        assert self._proc.stdout is not None
        data = b""
        while len(data) < size:
            chunk = self._proc.stdout.read(size - len(data))
            if not chunk:
                raise NeckoError("Necko donor closed the pipe")
            data += chunk
        return data

    def fetch(self, method: str, url: str, headers: list[tuple[str, str]], body: bytes, budget_s: float):
        if self._proc.poll() is not None:
            raise NeckoError("Necko donor is not running")
        payload = b"".join(
            [
                _text(method),
                _text(url),
                _u32(len(headers)),
                b"".join(_text(name) + _text(value) for name, value in headers),
                _u32(len(body)),
                body,
                _u32(max(1, int(budget_s * 1000))),
            ]
        )
        assert self._proc.stdin is not None
        self._proc.stdin.write(payload)
        self._proc.stdin.flush()
        kind = struct.unpack("<I", self._read(4))[0]
        if kind == 0:
            length = struct.unpack("<I", self._read(4))[0]
            if length > TEXT_LIMIT:
                raise NeckoError("Necko error is too large")
            message = self._read(length).decode("utf-8", "replace")
            raise NeckoError(message[:300])
        if kind != 1:
            raise NeckoError("Necko donor sent an unknown frame")
        status = struct.unpack("<I", self._read(4))[0]
        count = struct.unpack("<I", self._read(4))[0]
        if count > 256:
            raise NeckoError("Necko response has too many headers")
        listed = []
        for _ in range(count):
            name_n = struct.unpack("<I", self._read(4))[0]
            name = self._read(name_n).decode("utf-8", "replace")
            value_n = struct.unpack("<I", self._read(4))[0]
            value = self._read(value_n).decode("utf-8", "replace")
            listed.append((name, value))
        body_n = struct.unpack("<I", self._read(4))[0]
        if body_n > BODY_LIMIT:
            raise NeckoError("Necko response body exceeds 8 MiB")
        payload = self._read(body_n)
        version = struct.unpack("<I", self._read(4))[0]
        if version not in (10, 11, 20, 30):
            version = 11
        return NeckoResponse(status, payload, version, listed)

    def close(self) -> None:
        proc = getattr(self, "_proc", None)
        if proc is not None and proc.poll() is None:
            if proc.stdin is not None:
                try:
                    proc.stdin.close()
                except OSError:
                    pass
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, 15)
                except OSError:
                    proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(proc.pid, 9)
                    except OSError:
                        proc.kill()
        stderr = getattr(self, "_stderr", None)
        if stderr is not None:
            stderr.close()
