"""Locate the runtime for Python 3.10+ clients."""

import os
from pathlib import Path
import platform
import sys


def macos_arm64():
    return sys.platform == "darwin" and platform.machine().lower() in ("arm64", "aarch64")


def linux_x64():
    return sys.platform == "linux" and platform.machine().lower() in ("amd64", "x86_64")


def shell_filename():
    """Basename of the SpiderMonkey shell this host launches."""
    return "js" if macos_arm64() or linux_x64() else "js.exe"


def glue_filename():
    """Donor glue library copied beside the shell."""
    if linux_x64():
        return "libmozglue.so"
    return "libmozglue.dylib" if macos_arm64() else "mozglue.dll"


def get_runtime_path():
    """Return the runtime root containing ``dist`` and ``vendor``.

    ``RUYIJS_SHELL`` selects an existing executable and its enclosing runtime
    root. Otherwise prefer a source checkout's shell to the runtime bundled in
    the installed package. This function never downloads files.
    """
    if not (macos_arm64() or linux_x64()) and (os.name != "nt" or platform.machine().lower() not in ("amd64", "x86_64")):
        raise OSError("ruyijs requires Windows x64, macOS arm64 or Linux x64")

    override = os.environ.get("RUYIJS_SHELL")
    if override:
        shell = Path(override).resolve()
        if not shell.is_file():
            raise FileNotFoundError(f"RUYIJS_SHELL does not exist: {shell}")
        return shell.parent.parent

    package = Path(__file__).resolve().parent
    candidates = [package / "_runtime"]
    if package.parent.name == "python":
        candidates.insert(0, package.parent.parent)
    shell_name = shell_filename()
    for root in candidates:
        if (root / "dist" / shell_name).is_file():
            return root

    searched = ", ".join(str(root / "dist" / shell_name) for root in candidates)
    raise FileNotFoundError(
        "ruyijs runtime is missing. Install the platform wheel, build the "
        "source runtime from the Firefox donor, or set RUYIJS_SHELL to an "
        f"existing executable. Searched: {searched}"
    )
