"""Check cross-origin WindowProxy / Location access against Firefox 160 (issue #52).

Three parts, all offline (documents come from put_resource; the proxy is a
closed port, so nothing reaches the network):

1. ci/cross_origin/top.html (origin 127.0.0.1) holds a cross-origin iframe
   (localhost) with two named children (one per origin) and a same-origin
   iframe. Page scripts in all three levels probe each other: indexed and
   named child access (w[0], w.name), [[GetOwnProperty]] / [[OwnPropertyKeys]]
   / has / set / define / delete, the accessor shape of the cross-origin
   attributes, the SecurityError texts, contentDocument, window.name of a
   frame and its renames. Every row has to equal the row Firefox 160 produced
   for the same documents (ci/cross_origin/firefox160.json).
2. The reCAPTCHA v2 handshake shape: a frame walks parent.frames of a
   cross-origin parent, posts a MessagePort to each, and its sibling answers
   on the port; a named sibling is found through parent.frames[name].
3. Context.eval runs with host privilege: it reads a cross-origin frame's
   contentDocument, while the page's own script gets null (part 1).

Exit status is non-zero on failure.
"""
import json
import sys
from pathlib import Path

from ruyijs import Context

sys.stdout.reconfigure(errors="backslashreplace")
HERE = Path(__file__).resolve().parent / "cross_origin"
PORT = 28491
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript"}
failures = []


def check(name, condition, detail=""):
    print(f"{'ok  ' if condition else 'FAIL'} {name}  {detail}", flush=True)
    if not condition:
        failures.append(name)


def pump(ctx, done, rounds=400):
    for _ in range(rounds):
        ctx.advance(25, max_tasks=1000)
        if ctx.eval(done):
            return True
    return False


# 1. The Firefox 160 rows.
expected = [tuple(row) for row in json.loads((HERE / "firefox160.json").read_text(encoding="utf-8"))]
with Context(timeout=60, proxy="http://127.0.0.1:9") as ctx:
    for origin in (f"http://127.0.0.1:{PORT}", f"http://localhost:{PORT}"):
        for path in HERE.iterdir():
            if path.suffix in TYPES:
                ctx.put_resource(f"{origin}/{path.name}", path.read_text(encoding="utf-8"),
                                 headers=[("Content-Type", TYPES[path.suffix])])
    ctx.navigate(f"http://127.0.0.1:{PORT}/top.html?engine=ruyi")
    finished = pump(ctx, "!!window.__result")
    check("probe documents finished", finished)
    rows = [tuple(row) for row in json.loads(ctx.eval("window.__result || '[]'"))]
    # Part 3 on the same documents: the host sees through, the page does not.
    host = ctx.eval("document.getElementById('A').contentDocument !== null")
    check("Context.eval reads a cross-origin contentDocument (host privilege)", host is True, host)
ours = dict(rows)
mismatched = [(name, value, ours.get(name)) for name, value in expected if ours.get(name) != value]
for name, firefox, value in mismatched:
    print(f"     {name}\n       Firefox: {firefox}\n       ruyijs : {value}")
check(f"{len(expected)} rows equal Firefox 160", not mismatched and len(rows) == len(expected),
      f"{len(expected) - len(mismatched)}/{len(expected)} equal, {len(rows)} rows")

# 2. The handshake shape of reCAPTCHA v2 (anchor and bframe of one origin in a
# cross-origin parent).
ANCHOR = """<!doctype html><script>
const channels = [];
const report = (data) => parent.postMessage(data, '*');
try {
  const named = parent.frames['c-frame'];
  report({named: named === parent.frames[1], namedHref: named.location.href});
  for (let i = 0; i < parent.frames.length; i++) {
    const channel = new MessageChannel();
    channels.push(channel);
    channel.port1.onmessage = (e) => report({reply: e.data, index: i});
    parent.frames[i].postMessage({type: 'recaptcha-setup'}, 'https://google.invalid', [channel.port2]);
  }
  report({posted: parent.frames.length});
} catch (e) { report({error: e.name + ': ' + e.message}); }
</script>"""
BFRAME = """<!doctype html><script>
addEventListener('message', (e) => {
  if (e.data && e.data.type === 'recaptcha-setup' && e.source !== window)
    e.ports[0].postMessage('bframe-ready');
});
</script>"""
with Context(timeout=60, proxy="http://127.0.0.1:9") as ctx:
    ctx.put_resource("https://google.invalid/anchor", ANCHOR, headers=[("Content-Type", "text/html")])
    ctx.put_resource("https://google.invalid/bframe", BFRAME, headers=[("Content-Type", "text/html")])
    ctx.load_page("https://site.invalid/", """<!doctype html>
<script>window.log = []; addEventListener('message', (e) => log.push(e.data));</script>
<iframe name="c-frame" src="https://google.invalid/bframe"></iframe>
<script>
document.querySelector('iframe').onload = () => {
  const anchor = document.createElement('iframe');
  anchor.name = 'a-frame';
  anchor.src = 'https://google.invalid/anchor';
  document.body.insertBefore(anchor, document.body.firstChild);
};
</script>""")
    pump(ctx, "log.some((m) => m && m.reply)", rounds=200)
    log = ctx.eval("log")
check("the anchor walks the cross-origin parent's frames", {"posted": 2} in log, log)
check("parent.frames[name] finds the same-origin sibling",
      {"named": True, "namedHref": "https://google.invalid/bframe"} in log, log)
check("the sibling answers on the transferred port", {"reply": "bframe-ready", "index": 1} in log, log)
check("no SecurityError on the way", not any("error" in m for m in log if isinstance(m, dict)), log)

if failures:
    print("cross_origin_window_check FAILED:", ", ".join(failures))
    sys.exit(1)
print("cross_origin_window_check passed")
