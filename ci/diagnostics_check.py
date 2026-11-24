"""Check the two diagnostics added for issue #53: Error console records and the document-start script.

usage: python ci/diagnostics_check.py      (RUYIJS_SHELL picks js.exe)

Issue #53's reporter saw an extra console record "[object Error]" with an
empty stack in a document a navigation had replaced, and could not reach that
document before its scripts ran. Two diagnostics answer that:

- A logged Error renders as "Name: message" and its record carries the stack
  of where the Error was constructed. Both are read from the engine's Error
  object, so logging still runs no page code: a replaced `name`, a `message`
  getter, an own or prototype `toString` and a Proxy are never called (the
  DataDome devtools probe depends on that).
- Context.set_document_start_script(source) runs `source` in every document
  the Context builds, ahead of that document's own first script: the
  load_page document, a document location.assign navigates to, a reload, and
  an iframe. Its own failure is recorded in the console log and the page loads
  normally; "" removes it.

Uses offline pages only (put_resource). Exit status is non-zero on failure.
"""
import sys

from ruyijs import Context

FAILURES = []


def check(name, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + name, detail)
    if not condition:
        FAILURES.append(name)


def records(context, method=None):
    rows = context.console_log()
    return [row for row in rows if method is None or row["method"] == method]


ERROR_PAGE = r"""<!doctype html><html><body><script>
function makeIt() { return new TypeError("bad thing"); }
window.__calls = 0;
const e = makeIt();
console.debug(e);
console.log("prefix", new RangeError("out of range"));
console.warn(new Error(""));
// Page code that must never run while logging.
const trap = new Error("trapped");
Object.defineProperty(trap, "name", {get() { window.__calls++; return "Spoofed"; }});
Object.defineProperty(trap, "message", {get() { window.__calls++; return "spoofed"; }});
trap.toString = function () { window.__calls++; return "spoofed"; };
console.error(trap);
class MyError extends Error {}
MyError.prototype.toString = function () { window.__calls++; return "proto"; };
console.info(new MyError("subclassed"));
console.log(new Proxy(new Error("proxied"), {get(t, k) { window.__calls++; return t[k]; }}));
console.log({toString() { window.__calls++; return "plain"; }});
</script></body></html>"""


def check_error_records():
    with Context() as context:
        context.load_page("https://diag.test/errors", ERROR_PAGE)
        rows = records(context)
        texts = [row["text"] for row in rows]
        check("an Error renders as name: message", texts[0] == "TypeError: bad thing", repr(texts[0]))
        check("its record carries where it was constructed",
              bool(rows[0]["stack"]) and rows[0]["stack"][0].startswith("makeIt@"), repr(rows[0]["stack"][:2]))
        check("an Error among other arguments", texts[1] == "prefix RangeError: out of range", repr(texts[1]))
        check("an empty message prints the name alone", texts[2] == "Error", repr(texts[2]))
        # Redefining `message` as an accessor clears the engine's own message
        # slot, so the record reads the bare type; the page's values never show.
        check("own name/message getters and toString are not used", texts[3] in ("Error", "Error: trapped")
              and "poof" not in texts[3], repr(texts[3]))
        check("a subclass keeps its engine type, prototype toString unread", texts[4] == "Error: subclassed",
              repr(texts[4]))
        check("a Proxy around an Error is not an Error", texts[5] == "[object Proxy]", repr(texts[5]))
        check("a plain object is still its class name", texts[6] == "[object Object]", repr(texts[6]))
        check("logging ran no page code", context.eval("window.__calls") == 0, context.eval("window.__calls"))
        check("a non-Error record carries no stack", rows[6]["stack"] == [], repr(rows[6]["stack"]))


START = r"""
(function () {
  const seen = (globalThis.__start = globalThis.__start || []);
  seen.push({url: location.href, scripts: document.scripts.length, ready: document.readyState});
  globalThis.__firstOwnScriptSawStart = false;
  const original = console.debug;
  globalThis.__debug = [];
  console.debug = function (...args) {
    try { __debug.push(args.map(a => a instanceof Error ? a.name + ":" + a.stack.split("\n")[0] : String(a))); }
    catch (_) {}
    return original.apply(this, args);
  };
})();
"""

FIRST = r"""<!doctype html><html><head><script>
window.__firstOwnScriptSawStart = Array.isArray(window.__start);
console.debug(new Error("first-doc"));
</script></head><body><iframe src="https://diag.test/frame"></iframe></body></html>"""
SECOND = r"""<!doctype html><html><head><script>
window.__firstOwnScriptSawStart = Array.isArray(window.__start);
function nested() { return new SyntaxError("second-doc"); }
console.debug(nested());
</script></head><body>second</body></html>"""
FRAME = r"""<!doctype html><html><head><script>
window.__frameSawStart = Array.isArray(window.__start);
</script></head><body>frame</body></html>"""


def check_document_start():
    with Context() as context:
        context.put_resource("https://diag.test/second", SECOND, headers=[("Content-Type", "text/html")])
        context.put_resource("https://diag.test/frame", FRAME, headers=[("Content-Type", "text/html")])
        installed = context.set_document_start_script(START)
        check("installing reports the source length", installed == {"length": len(START.encode())}, installed)
        context.load_page("https://diag.test/first", FIRST)
        context.advance(100)
        check("load_page document: runs before the first own script",
              context.eval("window.__firstOwnScriptSawStart") is True)
        start = context.eval("window.__start")
        check("load_page document: no own script has run yet",
              start and start[0]["scripts"] == 0 and start[0]["url"] == "https://diag.test/first", start)
        debug = context.eval("window.__debug")
        check("the hook sees the first document's console.debug(Error)",
              bool(debug) and debug[0][0].startswith("Error:"), debug)
        frame = context.eval("document.querySelector('iframe').contentWindow.__frameSawStart")
        check("an iframe document runs it first too", frame is True, frame)

        context.eval("location.assign('https://diag.test/second')")
        for _ in range(20):
            context.advance(50)
            if context.eval("location.href") == "https://diag.test/second":
                break
        check("a navigated-to document runs it before its own script",
              context.eval("window.__firstOwnScriptSawStart") is True)
        debug = context.eval("window.__debug")
        check("and reaches that document's console.debug(Error)",
              bool(debug) and debug[0][0].startswith("SyntaxError:"), debug)
        logged = records(context, "debug")
        last = logged[-1] if logged else {}
        check("the native record names the Error and where it came from",
              last.get("text") == "SyntaxError: second-doc" and bool(last.get("stack"))
              and last["stack"][0].startswith("nested@"), last)

        context.eval("location.reload()")
        for _ in range(20):
            context.advance(50)
            if context.eval("Array.isArray(window.__start) && document.readyState === 'complete'"):
                break
        check("a reload runs it again", context.eval("window.__firstOwnScriptSawStart") is True)

        # load_page replaces the document in the same Window global, so the
        # earlier runs' __start stays; what must not happen is another run.
        runs = context.eval("window.__start.length")
        context.set_document_start_script("")
        context.load_page("https://diag.test/third", "<script>window.__after = window.__start.length</script>")
        check("an empty source removes it", context.eval("window.__after") == runs,
              f"{runs} runs before, {context.eval('window.__after')} after")

    with Context() as context:
        context.set_document_start_script("throw new Error('hook failed')")
        context.load_page("https://diag.test/x", "<script>window.__ran = true</script>")
        check("a failing hook does not stop the page", context.eval("window.__ran") is True)
        failed = records(context, "documentStart")
        check("its failure is recorded in the console log", len(failed) == 1 and failed[0]["level"] == "error",
              failed)
        check("and is not reported to the page", context.eval("typeof window.onerror") == "object")

    with Context() as context:
        try:
            context.set_document_start_script(7)
            check("a non-string is refused", False)
        except TypeError:
            check("a non-string is refused", True)


def main():
    check_error_records()
    check_document_start()
    print(("FAILED: " + ", ".join(FAILURES)) if FAILURES else "diagnostics checks passed")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    sys.exit(main())
