"""DOMTokenList attributes as Firefox 160: [PutForwards=value], [SameObject], supports().

usage: python ci/token_list_check.py      (RUYIJS_SHELL picks js.exe)

Element.classList, Element.part and HTMLIFrameElement.sandbox are
[PutForwards=value]: assigning the attribute sets the list's value (a DOMString,
so null gives "null"). ruyijs had getters only, so the assignment was dropped.
reCAPTCHA (recaptcha__en.js) creates its anchor and bframe iframes through
Closure's setProperties, i.e. iframe.sandbox = "allow-forms allow-popups
allow-same-origin allow-scripts allow-top-navigation", then sandbox.add()s
allow-modals, allow-popups-to-escape-sandbox and
allow-storage-access-by-user-activation when supports() says so. Losing the
first five left both frames without allow-scripts: no script ran in them and
the page's recaptcha code failed on the bframe window ("m is undefined").
sandbox is also [SameObject], supports() checks Firefox's sandbox keyword list
(ASCII case-insensitive), and supports() on class or part throws Firefox's
TypeError (its message has the element and attribute names swapped). Expected
values were measured in Firefox 160 from a page script on http://127.0.0.1:28490.
"""
import json

from ruyijs import Context

PROBE = "(async () => {\n  const rows = [];\n  const put = (k, v) => rows.push([k, v]);\n  const run = (k, f) => {\n    try { put(k, 'ok ' + JSON.stringify(f())); }\n    catch (e) { put(k, `${e.constructor.name}|${e.name}|${e.message}`); }\n  };\n  const desc = (p, n) => { const d = Object.getOwnPropertyDescriptor(p, n); return d ? [typeof d.get, d.get && d.get.name, typeof d.set, d.set && d.set.name, d.set && d.set.length, d.enumerable, d.configurable] : null; };\n  run('desc.classList', () => desc(Element.prototype, 'classList'));\n  run('desc.part', () => desc(Element.prototype, 'part'));\n  run('desc.sandbox', () => desc(HTMLIFrameElement.prototype, 'sandbox'));\n  run('sandbox.prop', () => { const f = document.createElement('iframe'); f.sandbox = 'allow-forms allow-scripts'; const r = [f.getAttribute('sandbox'), String(f.sandbox), f.sandbox.length]; f.sandbox.add('allow-modals'); r.push(f.getAttribute('sandbox')); return r; });\n  run('sandbox.same', () => { const f = document.createElement('iframe'); const s = f.sandbox; f.sandbox = 'x'; return [s === f.sandbox, s.value]; });\n  run('sandbox.null', () => { const f = document.createElement('iframe'); f.sandbox = null; return f.getAttribute('sandbox'); });\n  run('sandbox.undef', () => { const f = document.createElement('iframe'); f.sandbox = undefined; return f.getAttribute('sandbox'); });\n  run('sandbox.obj', () => { const f = document.createElement('iframe'); f.sandbox = { toString() { return 'allow-popups'; } }; return f.getAttribute('sandbox'); });\n  run('sandbox.supports', () => { const s = document.createElement('iframe').sandbox; return ['allow-scripts', 'ALLOW-SCRIPTS', 'allow-same-origin', 'allow-modals', 'allow-downloads', 'allow-storage-access-by-user-activation', 'allow-top-navigation-to-custom-protocols', 'bogus', '', 'allow-scripts '].map(t => s.supports(t)); });\n  run('classList.prop', () => { const d = document.createElement('div'); const c = d.classList; d.classList = 'x  y'; const r = [d.className, c === d.classList, c.length]; d.classList.add('z'); r.push(d.className); return r; });\n  run('classList.null', () => { const d = document.createElement('div'); d.classList = null; return d.className; });\n  run('classList.supports', () => document.createElement('div').classList.supports('x'));\n  run('part.prop', () => { const d = document.createElement('div'); d.part = 'p q'; return [d.getAttribute('part'), d.part.length]; });\n  run('part.supports', () => document.createElement('div').part.supports('x'));\n  run('setter.receiver', () => { const s = Object.getOwnPropertyDescriptor(HTMLIFrameElement.prototype, 'sandbox').set; try { s.call(document.createElement('div'), 'x'); return 'no throw'; } catch (e) { return e.constructor.name + ': ' + e.message; } });\n  const f = document.createElement('iframe'); f.sandbox = 'allow-scripts allow-same-origin'; f.srcdoc = '<script>parent.__fr = 1<\\/script>'; document.body.appendChild(f);\n  await new Promise(r => setTimeout(r, 800));\n  put('frame.prop.sandbox.ran', String(window.__fr));\n  window.__result = JSON.stringify(rows);\n})();\n"
FIREFOX = json.loads("{\"desc.classList\": \"ok [\\\"function\\\",\\\"get classList\\\",\\\"function\\\",\\\"set classList\\\",1,true,true]\", \"desc.part\": \"ok [\\\"function\\\",\\\"get part\\\",\\\"function\\\",\\\"set part\\\",1,true,true]\", \"desc.sandbox\": \"ok [\\\"function\\\",\\\"get sandbox\\\",\\\"function\\\",\\\"set sandbox\\\",1,true,true]\", \"sandbox.prop\": \"ok [\\\"allow-forms allow-scripts\\\",\\\"allow-forms allow-scripts\\\",2,\\\"allow-forms allow-scripts allow-modals\\\"]\", \"sandbox.same\": \"ok [true,\\\"x\\\"]\", \"sandbox.null\": \"ok \\\"null\\\"\", \"sandbox.undef\": \"ok \\\"undefined\\\"\", \"sandbox.obj\": \"ok \\\"allow-popups\\\"\", \"sandbox.supports\": \"ok [true,true,true,true,true,true,true,false,false,false]\", \"classList.prop\": \"ok [\\\"x  y\\\",true,2,\\\"x y z\\\"]\", \"classList.null\": \"ok \\\"null\\\"\", \"classList.supports\": \"TypeError|TypeError|DOMTokenList.supports: div attribute of <class> does not define any supported tokens\", \"part.prop\": \"ok [\\\"p q\\\",2]\", \"part.supports\": \"TypeError|TypeError|DOMTokenList.supports: div attribute of <part> does not define any supported tokens\", \"setter.receiver\": \"ok \\\"TypeError: 'set sandbox' called on an object that does not implement interface HTMLIFrameElement.\\\"\", \"frame.prop.sandbox.ran\": \"1\"}")


def main():
    page = "<!doctype html><html lang=en><meta charset=utf-8><body><script>" + PROBE.replace("</script>", "<\\/script>") + "</script>"
    with Context(timeout=30) as context:
        context.load_page("http://127.0.0.1:28490/page.html", page)
        for _ in range(60):
            context.advance(50)
            if context.eval("!!window.__result"):
                break
        got = dict(json.loads(context.eval("window.__result || '[]'")))
    failures = 0
    for key, want in FIREFOX.items():
        ok = got.get(key) == want
        failures += not ok
        print(("ok  " if ok else "FAIL"), key, "" if ok else f"got {got.get(key)!r} want {want!r}")
    print("token list checks", "passed" if not failures else f"FAILED ({failures})")
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
