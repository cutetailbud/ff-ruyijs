"""Page scripts are parsed in full, as Firefox's ScriptLoader does, so inner functions get Firefox's names.

usage: python ci/script_full_parse_check.py      (RUYIJS_SHELL picks js.exe)

Firefox compiles every classic page script with dom.script_loader.delazification.strategy =
ParseEverythingEagerly (255) while the document's 10 MB budget lasts: external scripts of
any size (off the main thread or not), inline scripts, scripts inserted at run time. Only
eval and new Function stay lazily parsed. Parsing a function with its enclosing script is
also what names it: an anonymous callback inside `OD` inside module `639` reads
"639/OD/<" in Error.stack when parsed eagerly and only "OD/<" when delazified on its own.
DataDome's device check (issue #54) hashes such stacks. The expectations are Firefox 160 and 161's
own names for the same sources.
"""
import json
import sys

from ruyijs import Context

BODY = ("var M={639:function(A,w,D){var HA=function(f){return f()};var OD=function(){return HA(function()"
        "{return new Error().stack.split(String.fromCharCode(10))[0].split('@')[0]})};"
        "window.__r=window.__r||{};window.__r[%s]=OD()}};M[639]();")
PAD = "/*" + "x" * 6000 + "*/"
BASE = "http://127.0.0.1/"
FILES = {
    "ext_big.js": BODY % json.dumps("ext_big") + PAD,
    "ext_small.js": BODY % json.dumps("ext_small"),
    "ext_defer.js": BODY % json.dumps("ext_defer") + PAD,
    "dyn_small.js": BODY % json.dumps("dyn_small"),
    "dyn_big.js": BODY % json.dumps("dyn_big") + PAD,
}
PAGE = ("<!doctype html><html><head><meta charset=utf-8>"
        "<script src=ext_big.js></script><script src=ext_small.js></script>"
        "<script defer src=ext_defer.js></script>"
        "<script>" + BODY % json.dumps("inline_small") + "</script>"
        "<script>" + BODY % json.dumps("inline_big") + PAD + "</script>"
        "<script>eval(" + json.dumps(BODY % json.dumps("eval")) + ");new Function("
        + json.dumps(BODY % json.dumps("function")) + ")();"
        "for (const n of ['dyn_small.js', 'dyn_big.js']) { const s = document.createElement('script'); s.src = n;"
        " document.head.appendChild(s); }"
        "const i = document.createElement('script'); i.textContent = " + json.dumps(BODY % json.dumps("dom_inline"))
        + "; document.head.appendChild(i);"
        "</script></head><body><p>x</p></body></html>")
EAGER, LAZY = "639/OD/<", "OD/<"
WANT = {"ext_big": EAGER, "ext_small": EAGER, "ext_defer": EAGER, "inline_small": EAGER, "inline_big": EAGER,
        "dyn_small": EAGER, "dyn_big": EAGER, "dom_inline": EAGER, "eval": LAZY, "function": LAZY}


def main():
    failures = 0
    with Context(timeout=60) as context:
        for name, body in FILES.items():
            context.put_resource(BASE + name, body, headers=[("Content-Type", "text/javascript")])
        context.load_page(BASE + "page.html", PAGE)
        context.advance(500)
        got = context.eval("window.__r || {}")
    for name, want in WANT.items():
        ok = got.get(name) == want
        failures += not ok
        print(("ok  " if ok else "FAIL") + f"  {name:13} {got.get(name)!r}" + ("" if ok else f" (Firefox: {want!r})"))
    print("script full parse:", "all match Firefox" if not failures else f"{failures} rows differ")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
