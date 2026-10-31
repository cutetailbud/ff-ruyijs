"""Online smoke test for an installed ruyijs wheel: one real page load.

Navigates to https://example.com/ through the bundled Necko transport, lets
the event loop fetch and commit the document, and checks its title. Exit
status is non-zero when the page did not load.
"""
import sys
import time

from ruyijs import Context

URL = "https://example.com/"


def main():
    with Context(timeout=120) as ctx:
        ctx.configure_clock("system")
        ctx.navigate(URL)
        deadline = time.monotonic() + 90
        title = ""
        while time.monotonic() < deadline:
            ctx.advance(250)
            if ctx.eval("document.readyState") == "complete":
                title = ctx.eval("document.title")
                break
        requests = [(entry.get("method"), entry.get("status"), entry.get("url")) for entry in ctx.net_log()]
        print("requests:", requests[:5])
        print("title:", repr(title))
        if "Example Domain" not in title:
            print("FAIL: the page did not load")
            sys.exit(1)
    print("network smoke check passed")


if __name__ == "__main__":
    main()
