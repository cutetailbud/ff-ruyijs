"""A persistent SpiderMonkey session per Python Context: its own process, or a
session of a shared Engine process (see _engine.py)."""

import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from collections import deque

from ._pipe import MAX_FRAME, Pipe
from ._callbacks import Callbacks
from ._environment import EnvironmentControl
from ._scheduling import SchedulingControl
from ._random import RandomControl
from ._resources import ResourceControl
from ._console import ConsoleControl
from ._inspector import InspectorControl
from ._input import InputControl
from ._context_eval import EvaluationControl
from ._runtime_paths import get_runtime_path, shell_filename
from ._values import JSError, decode, encode



def macos_core_library(runtime_root):
    """Prefer the freshly built dylib, then the staged runtime copy."""
    root = Path(runtime_root)
    for name in ("necko_tls", "necko"):
        candidate = root / "dist" / name / "libnecko_core.dylib"
        if candidate.is_file():
            return candidate
    return None


def darwin_adapter_python(runtime_root):
    """The packaged adapter, then the build-tree interpreter."""
    root = Path(runtime_root)
    for relative in (
        ("dist", "necko-adapter", "python", "bin", "python3"),
        ("build", "necko-darwin", "python", "bin", "python3"),
    ):
        candidate = root.joinpath(*relative)
        if candidate.is_file():
            return candidate
    return None


def adapter_core_library(python_path):
    """The Windows adapter's libnecko_core.dylib beside a virtualenv interpreter.

    The venv ``python3`` link points at ``python3.14``, and that file points at
    the framework interpreter. Following every link would leave the venv.
    """
    if not python_path:
        return None
    executable = Path(python_path)
    if not executable.is_absolute():
        executable = Path.cwd() / executable
    name = executable.name
    if executable.is_symlink():
        target = Path(os.readlink(executable))
        if not target.is_absolute() and target.name.startswith("python"):
            name = target.name
    lib = executable.parent.parent / "lib"
    if not lib.is_dir():
        return None
    suffix = name.removeprefix("python")
    if suffix[:1].isdigit() and suffix.count(".") == 1:
        candidate = lib / f"python{suffix}" / "site-packages" / "necko" / "libnecko_core.dylib"
        if candidate.is_file():
            return candidate
    matches = [item for item in lib.glob("python*/site-packages/necko/libnecko_core.dylib") if item.is_file()]
    return matches[-1] if matches else None


def _running_under_wine():
    try:
        import ctypes
        return hasattr(ctypes.WinDLL("ntdll"), "wine_get_version")
    except (OSError, AttributeError):
        return False


# js.exe is compiled against the MSVC STL whose msvcp140.dll ships beside it.
# Wine loads its own builtin msvcp140 instead, whose condition_variable waits
# do not match that STL: background transport completions then never wake the
# owning thread, so every load that waits on one (external scripts,
# stylesheets, preloads) hangs until the RPC timeout (issue #1). Wine takes
# the load order from its per-program registry overrides (WINEDLLOVERRIDES in
# a Windows child environment is ignored), so prefer the bundled DLL there.
# A value the user already set for that DLL is kept.
_BUNDLED_CRT = ("msvcp140",)


def _prefer_bundled_crt(shell):
    if not _running_under_wine():
        return
    try:
        import winreg
        path = rf"Software\Wine\AppDefaults\{shell.name}\DllOverrides"
        with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, path, 0,
                                winreg.KEY_QUERY_VALUE | winreg.KEY_SET_VALUE) as key:
            for name in _BUNDLED_CRT:
                try:
                    winreg.QueryValueEx(key, name)
                except FileNotFoundError:
                    winreg.SetValueEx(key, name, 0, winreg.REG_SZ, "native,builtin")
    except OSError:
        pass


TLS_VERSIONS = tuple(str(v) for v in range(100, 161))


def resolve_shell(shell):
    """The shell executable: the argument, RUYIJS_SHELL, or the bundled runtime."""
    selected = shell or os.environ.get("RUYIJS_SHELL")
    path = Path(selected).resolve() if selected else get_runtime_path() / "dist" / shell_filename()
    if not path.is_file():
        raise FileNotFoundError(f"ruyijs shell does not exist: {path}")
    return path


def process_environment(shell):
    """The environment a shell process starts with, without os.environ changes."""
    child_environment = dict(os.environ)
    if sys.platform == "darwin":
        runtime_root = shell.parent.parent
        explicit_python = child_environment.get("RUYIJS_NECKO_PYTHON")
        if not explicit_python:
            built = darwin_adapter_python(runtime_root)
            if built is not None:
                child_environment["RUYIJS_NECKO_PYTHON"] = str(built)
                explicit_python = child_environment["RUYIJS_NECKO_PYTHON"]
            else:
                bundled = runtime_root / "vendor" / "necko-derived" / "python" / "bin" / "python3"
                plain = runtime_root / "vendor" / "necko-derived" / "python" / "python3"
                if not bundled.is_file() and not plain.is_file():
                    child_environment["RUYIJS_NECKO_PYTHON"] = sys.executable
        if not child_environment.get("NECKO_CORE_LIBRARY"):
            # An explicit interpreter keeps the libnecko_core.dylib installed
            # beside it, including that copy's firefox-roots.pem.
            core = adapter_core_library(explicit_python) if explicit_python else None
            if core is None:
                core = macos_core_library(runtime_root)
            if core is not None:
                child_environment["NECKO_CORE_LIBRARY"] = str(core)
    return child_environment


def start_shell(shell, arguments, child_environment, stderr):
    """Start a shell process with the private pipe as its only channel."""
    _prefer_bundled_crt(shell)
    popen_kwargs = {
        "stdin": subprocess.DEVNULL,
        "stdout": subprocess.DEVNULL,
        "stderr": stderr,
        "close_fds": True,
        "env": child_environment,
    }
    if os.name == "nt":
        # No console: a CREATE_NO_WINDOW child gets a hidden console and a
        # conhost.exe of its own (about 6 MiB); its stdio is redirected anyway.
        popen_kwargs["creationflags"] = subprocess.DETACHED_PROCESS
    return subprocess.Popen([str(shell), *arguments], **popen_kwargs)


def exit_detail(process, stderr):
    """'ruyijs process exited (code N): <stderr tail>' for a failed start."""
    if process is not None and process.poll() is None:
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            pass
    exit_code = process.returncode if process is not None else None
    stderr_tail = ""
    if stderr is not None:
        try:
            stderr.seek(0)
            stderr_tail = stderr.read(8192).decode("utf-8", "replace")
        except OSError:
            pass
    detail = f"ruyijs process exited (code {exit_code})"
    if stderr_tail:
        detail += ": " + stderr_tail[-2000:]
    return detail


def pipe_broken(error):
    return isinstance(error, EOFError) or (
        os.name == "nt" and isinstance(error, OSError) and error.winerror == 109)


def _tls_version(value):
    """Normalize a TLS profile choice ("120", 120, "120.0", "firefox-120")."""
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (str, int)):
        raise TypeError("tls must be a Firefox version string or int, or None")
    version = str(value).strip().lower()
    version = version.removeprefix("firefox-").removeprefix("firefox").strip()
    version = version.removesuffix(".0")
    if version not in TLS_VERSIONS:
        raise ValueError(f"unsupported tls {value!r}; choose {TLS_VERSIONS[0]}-{TLS_VERSIONS[-1]}")
    return version


class Context(EnvironmentControl, SchedulingControl, RandomControl, ResourceControl, ConsoleControl,
              InspectorControl, InputControl, EvaluationControl):
    """Evaluate JavaScript from Python 3.10+ with bounded transport waits.

    ``timeout`` applies to startup and each request after it acquires the context
    lock. A timeout or broken protocol closes the context and kills its process.
    ``shell`` overrides RUYIJS_SHELL; otherwise select the source or bundled runtime.
    ``proxy`` selects an explicit HTTP(S) proxy URL for this context's network
    requests. None or an empty string uses direct connections. It does not
    alter the host environment or other contexts.
    ``tls`` selects the Firefox release (100-160, e.g. ``"120"`` or ``120``)
    whose TLS ClientHello, HTTP/2 settings and HTTP/3 priorities this context's
    Necko transport sends. None keeps Firefox 160 (or NECKO_FIREFOX if set).
    It changes the network fingerprint only; pair it with a matching
    ``configure_fingerprint`` user agent when the page should agree.

    ``engine`` runs this context as a session of a shared ``Engine`` process
    instead of starting a process of its own (``Engine.context`` does the
    same; ``use_engine`` makes an engine the default). The session is as
    independent as a process: its own JavaScript heap, page, clock, cookies,
    storage, host functions, proxy and TLS profile. Closing it or a timeout in
    it ends only this session. ``network`` and ``prewarm`` apply to engine
    sessions only: contexts that pass the same ``network`` name share Necko
    helper processes and connections (a server can then link them), and
    ``prewarm`` (0-2, default the engine's) helpers start in the background.
    """

    def __init__(self, shell=None, timeout=10.0, environment=None, mode="default", proxy=None,
                 tls=None, *, engine=None, network=None, prewarm=None):
        if not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or timeout <= 0:
            raise ValueError("timeout must be a finite positive number")
        self.timeout = float(timeout)
        if mode not in ("default", "debug"):
            raise ValueError("mode must be 'default' or 'debug'")
        if proxy is not None and not isinstance(proxy, str):
            raise TypeError("proxy must be a URL string or None")
        if proxy and any(c in proxy for c in "\x00\r\n"):
            raise ValueError("proxy URL contains invalid characters")
        tls_version = _tls_version(tls)
        if engine is None:
            from ._engine import default_engine
            engine = default_engine()
        if engine is None and (network is not None or prewarm is not None):
            raise ValueError("network and prewarm apply to Engine sessions only")
        self.mode = mode
        self._lock, self._next_id = threading.Lock(), 0
        self._callbacks = Callbacks()
        self._host_releases = deque()
        self._host_callback_sequence = 0
        self._closed, self._pipe, self._process, self._stderr = True, None, None, None
        self._engine, self._session = engine, None
        if engine is not None:
            if shell is not None:
                raise ValueError("an Engine session uses the engine's shell")
            self.shell = engine.shell
            self.tls = tls_version or engine.default_tls
            try:
                self._pipe = Pipe()
                self._session = engine._open(self, self._pipe.name, mode, proxy or "",
                                             tls_version or "", network, prewarm)
                self._pipe.connect(time.monotonic() + self.timeout)
                self._closed = False
                if environment is not None:
                    self.configure_environment(environment)
            except BaseException:
                self._dispose()
                raise
            return
        self.shell = resolve_shell(shell)
        # Capture transport settings for this child without changing os.environ.
        child_environment = process_environment(self.shell)
        child_environment["RUYIJS_HTTP_PROXY"] = proxy or ""
        if tls_version is not None:
            # The Necko child inherits this and builds its TLS profile from it.
            child_environment["NECKO_FIREFOX"] = tls_version
        self.tls = tls_version or child_environment.get("NECKO_FIREFOX") or "160"
        try:
            self._pipe = Pipe()
            self._stderr = tempfile.TemporaryFile()
            self._process = start_shell(
                self.shell, ["--ruyijs-rpc", self._pipe.name, "--ruyijs-mode", mode],
                child_environment, self._stderr)
            self._pipe.connect(time.monotonic() + self.timeout)
            self._closed = False
            if environment is not None:
                self.configure_environment(environment)
        except BaseException as error:
            if pipe_broken(error):
                detail = exit_detail(self._process, self._stderr)
                self._dispose()
                raise BrokenPipeError(detail) from error
            self._dispose()
            raise

    @property
    def closed(self):
        return self._closed

    @property
    def engine(self):
        """The shared Engine this context runs in, or None for its own process."""
        return getattr(self, "_engine", None)

    def _dispose(self):
        was_open = not self._closed
        self._closed = True
        if hasattr(self, "_host_releases"):
            self._host_releases.clear()
        if hasattr(self, "_callbacks"):
            self._callbacks.functions.clear()
        engine = getattr(self, "_engine", None)
        if engine is not None:
            # Only this session ends: a script still running in it is
            # interrupted, and its thread exits once the pipe is closed.
            if was_open and self._session is not None:
                engine._interrupt(self._session)
            if self._pipe is not None:
                self._pipe.close()
            engine._forget(self)
            return
        if self._process is not None and self._process.poll() is None:
            self._process.kill()
            self._process.wait(timeout=5)
        if self._pipe is not None:
            self._pipe.close()
        if self._stderr is not None:
            self._stderr.close()
            self._stderr = None

    def _request(self, operation, **fields):
        if self._closed:
            raise RuntimeError("ruyijs Context is closed")
        self._next_id += 1
        request = {"id": self._next_id, "op": operation, **fields}
        payload = json.dumps(request, ensure_ascii=True, allow_nan=False).encode("utf-8")
        if len(payload) > MAX_FRAME:
            raise ValueError("ruyijs request exceeds 16 MiB")
        deadline = time.monotonic() + self.timeout
        try:
            self._pipe.send(payload, deadline)
            response = json.loads(self._pipe.receive(deadline).decode("utf-8"))
            while isinstance(response, dict) and response.get("event") == "callback":
                result = self._callbacks.dispatch(response, deadline)
                payload = json.dumps(result, ensure_ascii=True, allow_nan=False).encode("utf-8")
                self._pipe.send(payload, deadline)
                response = json.loads(self._pipe.receive(deadline).decode("utf-8"))
            if not isinstance(response, dict) or type(response.get("id")) is not int:
                raise ValueError("invalid ruyijs response identity")
            if response["id"] != self._next_id or type(response.get("ok")) is not bool:
                raise ValueError("mismatched ruyijs response")
            if response["ok"]:
                return decode(response.get("value"))
            error = response.get("error")
            if not isinstance(error, dict) or any(
                not isinstance(error.get(key), str) for key in ("name", "message", "stack")
            ):
                raise ValueError("invalid ruyijs error response")
            raise JSError(error["name"], error["message"], error["stack"])
        except JSError:
            raise
        except BaseException:
            self._dispose()
            raise

    def load_page(self, url, html, *, replay_async_scripts=None):
        """Navigate the page to url and load html into the main document.

        Parser-inserted classic scripts run in order with document.currentScript
        set; src scripts are read through the resource directory or network.
        readyState then becomes interactive (DOMContentLoaded) and complete
        (load). The first uncaught script or listener error is raised after the
        load finishes. Returns {"url", "scripts"}.

        replay_async_scripts optionally lists recorded async classic URLs to
        run at the interactive boundary, draining due tasks between entries.
        This explicit replay phase does not reproduce a complete task schedule.
        """
        if not isinstance(url, str) or not isinstance(html, str):
            raise TypeError("url and html must be strings")
        self._callbacks.guard()
        with self._lock:
            fields = {} if replay_async_scripts is None else {"replay_async_scripts": replay_async_scripts}
            return self._request("page", action="load", url=url, html=html, **fields)

    def navigate(self, url):
        """Start a top-level navigation the browser itself makes (address bar).

        The request has no Referer, Sec-Fetch-Site "none" and Sec-Fetch-User
        "?1"; a later location.reload() of that document is made the same way.
        The navigation runs on the event loop: advance() lets it fetch and
        commit. Returns {"url"}.
        """
        if not isinstance(url, str):
            raise TypeError("url must be a string")
        self._callbacks.guard()
        with self._lock:
            return self._request("page", action="navigate", url=url)

    def checkpoint(self):
        """Drain pending engine jobs through the shared runtime coordinator."""
        self._callbacks.guard()
        with self._lock:
            return self._request("checkpoint")

    def collect_garbage(self):
        """Run a full engine collection and return the major GC counter."""
        self._callbacks.guard()
        with self._lock:
            self._flush_host_releases()
            return self._request("collect")

    def expose(self, function, name=None):
        """Expose a global host function, or copy data into __ruyijs__.data[name].

        Data needs an explicit name and adds no Window property. The lexical
        __ruyijs__ namespace remains accessible to scripts that know its name.
        """
        if not callable(function):
            if not isinstance(name, str) or not name or "\0" in name:
                raise ValueError("exposed data requires a nonempty name without NUL")
            self._callbacks.guard()
            with self._lock:
                self._request("expose_data", name=name, value=encode(function))
            return
        name = name if name is not None else getattr(function, "__name__", None)
        if not isinstance(name, str) or not name or "\0" in name:
            raise ValueError("host function requires a nonempty name without NUL")
        self._callbacks.guard()
        with self._lock:
            self._request("expose", name=name)
            self._callbacks.functions[name] = function

    def set(self, name, value):
        """Copy a Python value to a property of this context's page global."""
        if not isinstance(name, str) or not name or "\0" in name:
            raise ValueError("global property requires a nonempty name without NUL")
        self._callbacks.guard()
        with self._lock:
            self._request("set", name=name, value=encode(value))

    def native(self, name, source, *, constructor=False):
        """Bind a native host wrapper, optionally callable with new."""
        if not isinstance(name, str) or "\0" in name:
            raise ValueError("native function requires a string name without NUL")
        if not isinstance(source, str):
            raise TypeError("native source must be a string")
        if not isinstance(constructor, bool):
            raise TypeError("native constructor must be a boolean")
        self._callbacks.guard()
        with self._lock:
            self._request("native", name=name, code=source, constructor=constructor)

    def close(self):
        """Close the shell process, or this Engine session; repeated calls do nothing."""
        if not self._closed:
            self._callbacks.guard()
        with self._lock:
            if self._closed:
                return
            try:
                self._request("close")
                if self._process is not None:
                    self._process.wait(timeout=self.timeout)
            finally:
                self._closed = True
                self._dispose()

    def __enter__(self):
        if self._closed:
            raise RuntimeError("ruyijs Context is closed")
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        try:
            self._dispose()
        except Exception:
            pass
