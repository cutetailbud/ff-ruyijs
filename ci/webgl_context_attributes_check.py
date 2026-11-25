"""WebGL getContextAttributes() follows the creating getContext options, as Firefox 161.

usage: python ci/webgl_context_attributes_check.py      (RUYIJS_SHELL picks js.exe)

The engine reported the profile's attributes whatever the page asked for, so
`getContext('webgl', {powerPreference: 'low-power'}).getContextAttributes().powerPreference`
was "default" where Firefox says "low-power" -- DataDome's device check (issue #54) does
exactly that from an OffscreenCanvas in a Worker. Every expectation is Firefox 161's own
answer to the same call.
"""
import json
import sys

from ruyijs import Context, JSError

DEFAULTS = {"alpha": True, "antialias": True, "depth": True, "failIfMajorPerformanceCaveat": False,
            "powerPreference": "default", "premultipliedAlpha": True, "preserveDrawingBuffer": False,
            "stencil": False}


def attrs(**changes):
    out = dict(DEFAULTS)
    out.update(changes)
    return json.dumps(out, separators=(",", ":"))


MIXED = ("{alpha: false, antialias: false, depth: false, stencil: true, premultipliedAlpha: false, "
         "preserveDrawingBuffer: true, failIfMajorPerformanceCaveat: true, powerPreference: 'low-power', "
         "xrCompatible: true}")
MIXED_WANT = attrs(alpha=False, antialias=False, depth=False, stencil=True, premultipliedAlpha=False,
                   preserveDrawingBuffer=True, powerPreference="low-power")
BOGUS = ("THROW TypeError: 'bogus' (value of 'powerPreference' member of WebGLContextAttributes) is not a "
         "valid value for enumeration WebGLPowerPreference.")

ROWS = []
for kind in ("webgl", "webgl2"):
    ROWS += [
        (f"{kind} none", f"document.createElement('canvas').getContext('{kind}')", attrs()),
        (f"{kind} low-power", f"document.createElement('canvas').getContext('{kind}', {{powerPreference: 'low-power'}})",
         attrs(powerPreference="low-power")),
        (f"{kind} high-performance",
         f"document.createElement('canvas').getContext('{kind}', {{powerPreference: 'high-performance'}})",
         attrs(powerPreference="high-performance")),
        (f"{kind} mixed", f"document.createElement('canvas').getContext('{kind}', {MIXED})", MIXED_WANT),
        (f"{kind} ToBoolean", f"document.createElement('canvas').getContext('{kind}', {{alpha: 0, depth: '', stencil: 'yes'}})",
         attrs(alpha=False, depth=False, stencil=True)),
        (f"{kind} bogus", f"document.createElement('canvas').getContext('{kind}', {{powerPreference: 'bogus'}})", BOGUS),
        (f"{kind} first call wins",
         f"(() => {{ const c = document.createElement('canvas'); c.getContext('{kind}', {{powerPreference: 'low-power'}}); "
         f"return c.getContext('{kind}', {{powerPreference: 'high-performance', alpha: false}}); }})()",
         attrs(powerPreference="low-power")),
    ]
ROWS.append(("offscreen low-power", "new OffscreenCanvas(1, 1).getContext('webgl', {powerPreference: 'low-power'})",
             attrs(powerPreference="low-power")))

WORKER = ("new Promise(r => { const w = new Worker(URL.createObjectURL(new Blob([\"postMessage(JSON.stringify("
          "new OffscreenCanvas(1,1).getContext('webgl',{powerPreference:'low-power'}).getContextAttributes()))\"], "
          "{type: 'text/javascript'}))); w.onmessage = e => r(e.data); w.onerror = e => r('ERR ' + e.message); })")


def main():
    failures = 0
    with Context(timeout=60) as context:
        context.load_page("http://127.0.0.1/g.html", "<!doctype html><title>webgl</title>")

        def report(name, got, want):
            nonlocal failures
            ok = got == want
            failures += not ok
            print(("ok  " if ok else "FAIL") + f"  {name}" + ("" if ok else f"\n      got  {got}\n      want {want}"))

        for name, expression, want in ROWS:
            source = (f"(() => {{ try {{ const gl = {expression}; return gl ? JSON.stringify(gl.getContextAttributes()) "
                      f": 'null'; }} catch (e) {{ return 'THROW ' + e.name + ': ' + e.message; }} }})()")
            try:
                got = context.eval(source)
            except JSError as error:
                got = f"ERROR {error}"
            report(name, got, want)
        context.eval(f"globalThis.__w = undefined; {WORKER}.then(v => globalThis.__w = v); undefined")
        for _ in range(100):
            context.advance(50)
            if context.eval("globalThis.__w !== undefined"):
                break
        report("worker offscreen low-power", context.eval("globalThis.__w"), attrs(powerPreference="low-power"))
    print("webgl context attributes:", "all match Firefox 161" if not failures else f"{failures} rows differ")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
