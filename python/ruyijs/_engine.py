"""One ruyijs process shared by several independent Contexts (docs/engine.md)."""

import contextlib
import json
import math
import tempfile
import threading
import time
import weakref

from ._pipe import MAX_FRAME, Pipe
from ._values import JSError, decode


class Engine:
    """Run several independent Contexts as sessions of one ruyijs process.

    A plain ``Context`` starts its own shell process and Necko helpers. Each
    context an Engine makes (``engine.context(...)``, ``Context(engine=engine)``
    or any ``Context()`` inside ``use_engine(engine)``) is a session on its own
    thread of the engine's one process instead: its own JavaScript heap, page,
    clock, cookies, storage, host functions, proxy and TLS profile, served over
    its own private pipe exactly like a process of its own. Sessions run in
    parallel; a timeout or ``close()`` ends only that session.

    ``timeout`` bounds startup and each control request. ``max_helpers`` caps
    the Necko helper processes of all sessions together (0: no cap): when it is
    reached, the least recently used idle helper of another session makes way.
    ``prewarm`` (0-2) is how many helpers each new session starts in the
    background. A plain Context on Windows starts 2; on Linux and macOS the
    helpers start on the first request. More start on demand either way.
    A crash of the engine process ends all of its sessions.
    """

    def __init__(self, shell=None, timeout=10.0, *, max_helpers=0, prewarm=1):
        from ._context import exit_detail, pipe_broken, process_environment, resolve_shell, start_shell
        if not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or timeout <= 0:
            raise ValueError("timeout must be a finite positive number")
        if isinstance(max_helpers, bool) or not isinstance(max_helpers, int) or max_helpers < 0:
            raise ValueError("max_helpers must be a nonnegative int")
        _check_prewarm(prewarm)
        self.timeout = float(timeout)
        self.prewarm = prewarm
        self._lock, self._next_id = threading.Lock(), 0
        self._contexts = weakref.WeakSet()
        self._closed, self._pipe, self._process, self._stderr = True, None, None, None
        self.shell = resolve_shell(shell)
        child_environment = process_environment(self.shell)
        # Sessions name their own proxy; the engine itself makes no requests.
        child_environment["RUYIJS_HTTP_PROXY"] = ""
        self.default_tls = child_environment.get("NECKO_FIREFOX") or "160"
        arguments = []
        try:
            self._pipe = Pipe()
            arguments = ["--ruyijs-host", self._pipe.name]
            if max_helpers:
                arguments += ["--ruyijs-max-helpers", str(max_helpers)]
            self._stderr = tempfile.TemporaryFile()
            self._process = start_shell(self.shell, arguments, child_environment, self._stderr)
            self._pipe.connect(time.monotonic() + self.timeout)
            self._closed = False
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
    def contexts(self):
        """The open contexts of this engine."""
        return [context for context in list(self._contexts) if not context.closed]

    def context(self, **options):
        """A new independent Context in this engine (same arguments as Context)."""
        from ._context import Context
        return Context(engine=self, **options)

    def status(self):
        """[{"session": id, "running": bool, "network": name}, ...] of live sessions."""
        return self._control("status")

    def _open(self, context, pipe_name, mode, proxy, tls, network, prewarm):
        if network is not None and (not isinstance(network, str) or not network or "\0" in network):
            raise ValueError("network must be a nonempty string without NUL, or None")
        prewarm = self.prewarm if prewarm is None else prewarm
        _check_prewarm(prewarm)
        reply = self._control("open", pipe=pipe_name, mode=mode, proxy=proxy, tls=tls,
                              network=network or "", prewarm=prewarm)
        self._contexts.add(context)
        return int(reply["session"])

    def _interrupt(self, session):
        """Stop a session's running script; best effort, never raises."""
        if self._closed:
            return
        try:
            self._control("interrupt", session=session)
        except Exception:
            pass

    def _forget(self, context):
        self._contexts.discard(context)

    def _control(self, operation, **fields):
        with self._lock:
            if self._closed:
                raise RuntimeError("ruyijs Engine is closed")
            self._next_id += 1
            request = {"id": self._next_id, "op": operation, **fields}
            payload = json.dumps(request, ensure_ascii=True, allow_nan=False).encode("utf-8")
            if len(payload) > MAX_FRAME:
                raise ValueError("ruyijs request exceeds 16 MiB")
            deadline = time.monotonic() + self.timeout
            try:
                self._pipe.send(payload, deadline)
                response = json.loads(self._pipe.receive(deadline).decode("utf-8"))
            except BaseException:
                self._dispose()
                raise
            if (not isinstance(response, dict) or response.get("id") != self._next_id or
                    type(response.get("ok")) is not bool):
                self._dispose()
                raise ValueError("mismatched ruyijs engine response")
            if response["ok"]:
                return decode(response.get("value"))
            error = response.get("error") or {}
            raise JSError(error.get("name", "Error"), error.get("message", ""), error.get("stack", ""))

    def close(self):
        """Close every session, then the engine process; repeated calls do nothing."""
        if self._closed:
            return
        for context in self.contexts:
            try:
                context.close()
            except Exception:
                pass
        try:
            # The host waits for its session threads before it replies.
            self._control("close")
            self._process.wait(timeout=self.timeout)
        except Exception:
            pass
        finally:
            self._dispose()

    def _dispose(self):
        self._closed = True
        for context in list(self._contexts):
            try:
                context._closed = True
                if context._pipe is not None:
                    context._pipe.close()
            except Exception:
                pass
        self._contexts = weakref.WeakSet()
        if self._process is not None and self._process.poll() is None:
            self._process.kill()
            self._process.wait(timeout=5)
        if self._pipe is not None:
            self._pipe.close()
        if self._stderr is not None:
            self._stderr.close()
            self._stderr = None

    def __enter__(self):
        if self._closed:
            raise RuntimeError("ruyijs Engine is closed")
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        try:
            self._dispose()
        except Exception:
            pass


def _check_prewarm(prewarm):
    if isinstance(prewarm, bool) or not isinstance(prewarm, int) or not 0 <= prewarm <= 2:
        raise ValueError("prewarm must be 0, 1 or 2")


_default = None
_default_lock = threading.Lock()


def default_engine():
    """The engine use_engine() made the default, or None."""
    engine = _default
    return engine if engine is not None and not engine.closed else None


@contextlib.contextmanager
def use_engine(engine):
    """Make every Context() created inside the block, from any thread, a
    session of ``engine`` unless it names another one."""
    global _default
    if not isinstance(engine, Engine):
        raise TypeError("use_engine needs an Engine")
    with _default_lock:
        previous, _default = _default, engine
    try:
        yield engine
    finally:
        with _default_lock:
            _default = previous
