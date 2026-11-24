"""Staggered connection attempts across a host's resolved addresses (RFC 8305 order).

One unreachable address used to cost the whole request budget: the candidates
were tried strictly one after another and each attempt inherited all of the
remaining time, so a host with two AAAA records on a network without a working
IPv6 route took 42 s to reach (a connect per dead address), for a page curl
fetches in 0.3 s. Attempts now start `stagger_delay()` apart and run together;
the first established connection wins and the others are cancelled, so a dead
address costs the stagger delay while a slow but working one still wins.

Installed as ruyijs_necko/connection_attempts.py on every platform:

- connect_any() serves ruyijs_necko/connection_control.open_socket (Windows,
  Linux), whose native socket factory honours an Attempt's cancellation. Each
  in-flight candidate holds its own transport reservation, as the HTTP/3-TCP
  race's two candidates already do.
- create_connection() replaces socket.create_connection for the macOS direct
  HTTPS client (necko/core_client.py): non-blocking sockets, one select loop.

RUYIJS_CONNECT_STAGGER (seconds, default 0.25, Firefox's fast fallback) and
RUYIJS_CONNECT_CONCURRENCY (default 4) tune both.
"""
import errno
import os
import select
import socket
import threading
import time


def _number(name, default, low, high, cast):
    try:
        value = cast(os.environ.get(name) or default)
    except (TypeError, ValueError):
        return default
    return value if low <= value <= high else default


def stagger_delay():
    """Seconds before the next address joins in."""
    return _number("RUYIJS_CONNECT_STAGGER", 0.25, 0.001, 60.0, float)


def concurrency():
    """How many address attempts may be in flight at once."""
    return _number("RUYIJS_CONNECT_CONCURRENCY", 4, 1, 16, int)


def ordered(rows):
    """Keep the resolver's order inside each family and alternate between families,
    so an unusable family cannot push every address of a usable one to the back."""
    families, order = {}, []
    for row in rows:
        if row[0] not in families:
            families[row[0]] = []
            order.append(row[0])
        families[row[0]].append(row)
    result = []
    while any(families[family] for family in order):
        for family in order:
            if families[family]:
                result.append(families[family].pop(0))
    return result


class _Candidate:
    __slots__ = ("attempt", "error", "address")

    def __init__(self, attempt, address):
        self.attempt = attempt
        self.error = None
        self.address = address


def connect_any(adapter, factory, rows, port, timeout, attempt, control, numeric_host):
    """The first connection one of `rows` establishes; the other attempts are cancelled."""
    from necko._connect import Attempt
    from .connection_budget import AdmissionError
    from .transport_resources import reserve

    candidates = ordered(rows)
    if not candidates:
        raise OSError("No usable TCP address")
    deadline = attempt.deadline if attempt is not None else None
    if control is not None and control.deadline is not None:
        deadline = control.deadline if deadline is None else min(deadline, control.deadline)
    delay, limit = stagger_delay(), concurrency()
    condition = threading.Condition()
    done, winner, admission = [], [], []
    stopped = False

    def run(record):
        connection, token = None, None
        try:
            if control is not None:
                control.remaining("tcp-connect")
            record.attempt.remaining()
            token = reserve(adapter, "tcp", control, record.attempt)
            try:
                options = {"budget_token": token} if token is not None else {}
                connection = factory(adapter.library, numeric_host(record.address), port,
                                     timeout, record.attempt, **options)
            finally:
                if token is not None:
                    token.abandon()
        except AdmissionError as caught:
            record.error = caught
            with condition:
                admission.append(caught)
        except Exception as caught:
            record.error = caught
        with condition:
            if connection is not None and not winner and not stopped:
                winner.append((record, connection))
                connection = None
            done.append(record)
            condition.notify_all()
        # A connection that arrived after another won is never handed out.
        if connection is not None:
            connection.close()

    threads, registrations, started = [], [], []
    try:
        with condition:
            index, next_start = 0, time.monotonic()
            while not winner and not admission:
                inflight = len(threads) - len(done)
                now = time.monotonic()
                if index < len(candidates) and inflight < limit and (now >= next_start or not inflight):
                    record = _Candidate(Attempt(deadline), candidates[index][4])
                    index += 1
                    started.append(record)
                    if control is not None:
                        registrations.append(control.register(record.attempt.cancel))
                    thread = threading.Thread(target=run, args=(record,),
                                              name="necko-connect-address", daemon=True)
                    threads.append(thread)
                    thread.start()
                    next_start = time.monotonic() + delay
                    continue
                if index >= len(candidates) and inflight == 0:
                    break
                wait = None if deadline is None else deadline - now
                if wait is not None and wait <= 0:
                    break
                if index < len(candidates) and inflight < limit:
                    until = max(0.0, next_start - now)
                    wait = until if wait is None else min(wait, until)
                condition.wait(wait)
            if admission:
                raise admission[0]
            if winner:
                record, connection = winner[0]
                # Downstream TLS, reads and writes stay bound to the request's own
                # Attempt, exactly as a single-address connect left them.
                if attempt is not None:
                    connection.attempt = attempt
                    attempt.set_interrupt(connection.interrupt)
                return connection
            error = next((candidate.error for candidate in reversed(started) if candidate.error), None)
            raise error or OSError("No usable TCP address")
    finally:
        with condition:
            stopped = True
            keep = winner[0][0] if winner else None
            condition.notify_all()
        for candidate in started:
            if candidate is not keep:
                candidate.attempt.cancel()
        for thread in threads:
            if thread.ident is not None:
                thread.join(timeout=1)
        for registration in registrations:
            registration.close()


_IN_PROGRESS = {errno.EINPROGRESS, errno.EWOULDBLOCK, errno.EALREADY, 10035}  # 10035: WSAEWOULDBLOCK


def create_connection(address, timeout):
    """socket.create_connection((host, port), timeout) with the attempts staggered.

    The returned socket is connected and blocking with `timeout`, as
    socket.create_connection leaves it. `timeout` bounds the whole connect.
    """
    host, port = address
    deadline = time.monotonic() + timeout
    rows = ordered(socket.getaddrinfo(host, port, type=socket.SOCK_STREAM))
    if not rows:
        raise OSError(f"getaddrinfo returned no address for {host}")
    delay, limit = stagger_delay(), concurrency()
    pending, last, index, next_start = {}, None, 0, time.monotonic()
    try:
        while True:
            now = time.monotonic()
            if now >= deadline:
                raise last if isinstance(last, TimeoutError) else TimeoutError(
                    f"Connection to {host}:{port} timed out")
            if index < len(rows) and len(pending) < limit and (now >= next_start or not pending):
                family, kind, proto, _, sockaddr = rows[index]
                index += 1
                sock = socket.socket(family, kind, proto)
                try:
                    sock.setblocking(False)
                    code = sock.connect_ex(sockaddr)
                except OSError as error:
                    sock.close()
                    last, next_start = error, now
                    continue
                if code == 0:
                    sock.setblocking(True)
                    sock.settimeout(timeout)
                    return sock
                if code in _IN_PROGRESS:
                    pending[sock] = sockaddr
                    next_start = now + delay
                else:
                    sock.close()
                    last, next_start = OSError(code, os.strerror(code)), now
                continue
            if not pending:
                if index >= len(rows):
                    raise last or OSError(f"Connection to {host}:{port} failed")
                continue
            wait = deadline - now
            if index < len(rows) and len(pending) < limit:
                wait = min(wait, max(0.0, next_start - now))
            sockets = list(pending)
            _, writable, failed = select.select([], sockets, sockets, max(0.0, wait))
            for sock in dict.fromkeys(writable + failed):
                code = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                del pending[sock]
                if code == 0 and sock in writable:
                    sock.setblocking(True)
                    sock.settimeout(timeout)
                    return sock
                sock.close()
                last, next_start = OSError(code or errno.ECONNREFUSED, os.strerror(code or errno.ECONNREFUSED)), now
    finally:
        for sock in pending:
            sock.close()
