"""MessagePorts transferred between a dedicated Worker and its page, as Firefox 160.

usage: python ci/worker_port_check.py      (RUYIJS_SHELL picks js.exe)

reCAPTCHA's anchor frame starts webworker.js, whose recaptcha__en.js runs
`aj = new MessageChannel; postMessage(tq, [aj.port2])` and the page waits five
seconds for that port. A Worker runs in its own ruyijs process, and its
postMessage could not transfer a MessagePort (DataCloneError, reported as
"uncaught exception: Object"), so the anchor always waited the full five
seconds, failed with "Error: Timed out." and only then built its challenge
frame. Ports now cross the worker pipe both ways: the transferred port's
endpoint stays entangled with its peer and relays over the pipe, the receiving
side gets a MessagePort for the same id.

The two port channels and the worker's own messages are separate task sources,
so rows are compared as a set (Firefox orders them either way). Expected values
were measured in Firefox 160 from page script.
"""
import json

from ruyijs import Context

WORKER = "const ch = new MessageChannel();\nch.port1.onmessage = e => { postMessage('worker port1 got: ' + e.data); ch.port1.postMessage('pong from worker'); };\npostMessage('setup', [ch.port2]);\nonmessage = e => {\n  if (e.ports.length) { const p = e.ports[0]; p.onmessage = ev => p.postMessage('worker echo ' + ev.data); postMessage('worker got port from parent'); }\n  else postMessage('worker got ' + e.data);\n};\n"
PROBE = "(async () => {\n  const log = [];\n  const w = new Worker('/wport_worker.js');\n  w.onerror = e => log.push('error ' + e.message);\n  let detached;\n  w.onmessage = e => {\n    log.push('main got ' + e.data + ' ports=' + e.ports.length + ' isPort=' + (e.ports[0] instanceof MessagePort));\n    if (e.ports.length) {\n      const p = e.ports[0];\n      p.onmessage = ev => log.push('main port got ' + ev.data);\n      p.postMessage('ping from main');\n      const c = new MessageChannel();\n      c.port1.onmessage = ev => log.push('main c.port1 got ' + ev.data);\n      w.postMessage('here', [c.port2]);\n      detached = c.port2;\n      setTimeout(() => c.port1.postMessage('hello worker'), 200);\n    }\n  };\n  await new Promise(r => setTimeout(r, 2000));\n  try { detached.postMessage('after transfer'); log.push('detached post: no throw'); } catch (e) { log.push('detached post: ' + e.name); }\n  try { w.postMessage('x', [detached]); log.push('retransfer: no throw'); } catch (e) { log.push('retransfer: ' + e.name); }\n  await new Promise(r => setTimeout(r, 300));\n  const rows = log.map((v, i) => [String(i).padStart(2, '0'), v]);\n  window.__result = JSON.stringify(rows);\n})();\n"
FIREFOX = [
 "main got setup ports=1 isPort=true",
 "main got worker port1 got: ping from main ports=0 isPort=false",
 "main port got pong from worker",
 "main got worker got port from parent ports=0 isPort=false",
 "main c.port1 got worker echo hello worker",
 "detached post: no throw",
 "retransfer: DataCloneError"
]


def main():
    base = "http://127.0.0.1:28490"
    with Context(timeout=30) as context:
        context.put_resource(base + "/wport_worker.js", WORKER, headers=[("Content-Type", "text/javascript")])
        context.load_page(base + "/page.html", "<!doctype html><html lang=en><meta charset=utf-8><body><script>" +
                          PROBE.replace("</script>", "<\\/script>") + "</script>")
        for _ in range(100):
            context.advance(50)
            if context.eval("!!window.__result"):
                break
        got = [value for _, value in json.loads(context.eval("window.__result || '[]'"))]
    failures = 0
    for want in FIREFOX:
        ok = want in got
        failures += not ok
        print(("ok  " if ok else "FAIL"), want)
    for extra in got:
        if extra not in FIREFOX:
            failures += 1
            print("FAIL unexpected", extra)
    print("worker port checks", "passed" if not failures else f"FAILED ({failures})")
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
