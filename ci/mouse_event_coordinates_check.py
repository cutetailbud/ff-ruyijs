"""MouseEvent pageX/offsetX/layerX of script-made events, as Firefox computes them.

usage: python ci/mouse_event_coordinates_check.py      (RUYIJS_SHELL picks js.exe)

A script-made event carries its window's pres context (Event::Init), so pageX adds
the document's scroll even before dispatch; offsetX of an event without a target is
its page point (Event::GetOffsetCoords); layerX of a script-made event is the stored
zero (UIEvent::GetLayerPoint, internal event); and an event whose target is not a
node (the Window, a plain EventTarget) answers offsetX 0 instead of throwing. The
expected rows are Firefox 160's own for this page.
"""
import json
import sys

from ruyijs import Context

PAGE = """<!doctype html><html><body style="margin:0"><div style="height:3000px"></div>
<div id=box style="position:absolute;left:30px;top:200px;width:100px;height:50px"></div></body></html>"""
PROBE = """(() => {
  scrollTo(0, 100);
  const box = document.getElementById('box');
  const read = e => { try { return [e.pageX, e.pageY, e.offsetX, e.offsetY, e.layerX, e.layerY]; }
                      catch (x) { return 'ERR ' + x.name; } };
  const fresh = () => new MouseEvent('mousemove', {clientX: 40, clientY: 120, bubbles: true});
  const r = {undispatched: read(fresh())};
  let seen;
  window.addEventListener('mousemove', e => { seen = read(e); }, {once: true});
  window.dispatchEvent(fresh()); r.onWindow = seen;
  const t = new EventTarget(); t.addEventListener('mousemove', e => { seen = read(e); });
  t.dispatchEvent(fresh()); r.onEventTarget = seen;
  box.addEventListener('mousemove', e => { seen = read(e); }, {once: true});
  box.dispatchEvent(fresh()); r.onElement = seen;
  const kept = fresh(); box.dispatchEvent(kept); r.afterDispatch = read(kept);
  return JSON.stringify(r);
})()"""
FIREFOX = {
    "undispatched": [40, 220, 40, 220, 0, 0],
    "onWindow": [40, 220, 0, 0, 0, 0],
    "onEventTarget": [40, 220, 0, 0, 0, 0],
    "onElement": [40, 220, 10, 20, 0, 0],
    "afterDispatch": [40, 220, 10, 20, 0, 0],
}

with Context() as ctx:
    ctx.load_page("https://mouse.invalid/", PAGE)
    got = json.loads(ctx.eval(PROBE))
failures = []
for name, want in FIREFOX.items():
    ok = got.get(name) == want
    print(f"{'ok  ' if ok else 'FAIL'} {name:14} {got.get(name)!r}" + ("" if ok else f" (Firefox {want!r})"))
    if not ok:
        failures.append(name)
if failures:
    print("mouse_event_coordinates_check FAILED:", ", ".join(failures))
    sys.exit(1)
print(f"mouse_event_coordinates_check passed ({len(FIREFOX)} rows)")
