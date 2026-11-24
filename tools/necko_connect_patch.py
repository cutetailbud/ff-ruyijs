"""The staggered-connect edit to the pinned runtime's ruyijs_necko/connection_control.py.

ruyijs_necko/connection_control.py comes unchanged from the runtime runtime.json
pins. Its open_socket tried the resolved addresses one after another, each with
the whole remaining request budget; this replaces that loop with
connect_any() of src/resource_network/necko_connection_attempts.py, installed
beside it as ruyijs_necko/connection_attempts.py. ci/package/runtime.py
(Windows), tools/build_necko_linux.py (Linux) and tools/build.py (a local
helper) apply it. A changed upstream file fails the build; a file that already
carries the edit is left as it is.
"""
from pathlib import Path

TARGET = "ruyijs_necko/connection_control.py"
MODULE = "ruyijs_necko/connection_attempts.py"
SOURCE = "src/resource_network/necko_connection_attempts.py"
OLD = '''    error = None
    for _, _, _, _, address in rows:
        control.remaining("tcp-connect")
        if attempt:
            attempt.remaining()
        try:
            token = reserve(adapter, "tcp", control, attempt)
            try:
                options = {"budget_token": token} if token is not None else {}
                return factory(adapter.library, numeric_host(address), port, timeout, attempt, **options)
            finally:
                if token is not None:
                    token.abandon()
        except AdmissionError:
            raise
        except Exception as caught:
            error = caught
            control.remaining("tcp-connect")
    raise error or OSError("No usable TCP address")
'''
NEW = '''    # The resolved addresses are attempted together, staggered (RFC 8305), so
    # one unreachable address costs the stagger delay and not the whole request
    # budget (ruyijs_necko/connection_attempts.py).
    from .connection_attempts import connect_any
    try:
        return connect_any(adapter, factory, rows, port, timeout, attempt, control, numeric_host)
    except AdmissionError:
        raise
    except Exception:
        control.remaining("tcp-connect")
        raise
'''
PATCH = [(OLD, NEW)]


def apply(text):
    """`text` (LF line endings) with the edit made; unchanged when already made."""
    if NEW in text:
        return text
    if text.count(OLD) != 1:
        raise ValueError(f"{TARGET}: the open_socket loop the staggered connect replaces has changed")
    return text.replace(OLD, NEW)


def install(package, root):
    """Edit `package`/connection_control.py in place and install the module beside it.

    `package` is a ruyijs_necko directory; the file keeps its line endings.
    Returns the two paths written.
    """
    package, root = Path(package), Path(root)
    target = package / Path(TARGET).name
    raw = target.read_bytes()
    crlf = raw.count(b"\r\n") * 2 > raw.count(b"\n")
    text = apply(raw.decode("utf-8").replace("\r\n", "\n"))
    target.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("utf-8"))
    module = package / Path(MODULE).name
    module.write_bytes((root / SOURCE).read_bytes())
    return [target, module]
