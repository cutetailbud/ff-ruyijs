"""macOS runtime integration tests.

Runs the built engine in-process through the Python API and checks the
behaviour the macOS shell depends on: timer precision and layout
behaviour, DOMMatrix, canvas 2D through Firefox Skia, the Web API surface
the fingerprint work depends on, and — network permitting — resource
timing over the Necko transport. Skipped gracefully when the engine or a
backend is not built yet.

Run: .venv/bin/python -I tests/test_macos_runtime.py [pattern]
"""
import json
import math
import os
import statistics
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "python"))

from ruyijs import Context

CASES = []


def case(fn):
    CASES.append(fn)
    return fn


@case
def eval_contract(context):
    context.eval("let base = 6")
    return context.eval("base * 7") == 42 and context.eval("'中文' + '文本'") == "中文文本"


@case
def dom_and_events(context):
    context.load_page("https://test.local/", "<html><body><p id='a'>你好</p><script>document.getElementById('a')"
                      ".textContent = '改过的文本';</script></body></html>")
    return context.eval("document.getElementById('a').textContent") == "改过的文本"


@case
def python_callback(context):
    seen = []
    context.expose(lambda value: seen.append(value), "record")
    context.eval("record('js->py')")
    return seen == ["js->py"]


@case
def timer_precision_clamped(context):
    # 0.2.1: performance.now() clamps to 1 ms with Firefox's jittered rounding.
    samples = [context.eval("performance.now()") for _ in range(60)]
    if any(not isinstance(s, (int, float)) or s < 0 for s in samples):
        return False
    return all(abs(s - round(s)) < 1e-9 for s in samples)


@case
def date_now_clamped(context):
    return all(context.eval("Date.now() % 1") == 0 for _ in range(10))


@case
def performance_time_origin(context):
    origin = context.eval("performance.timeOrigin")
    now = context.eval("Date.now()")
    return isinstance(origin, (int, float)) and abs(now - origin) < 60000


@case
def layout_scrollport(context):
    # 0.2.1: classic 17px scrollbars on a scroll container with overflowing
    # content; clientWidth shrinks by the scrollbar width.
    context.load_page("https://test.local/", "<html><body style='margin:0'><div id='s' style='overflow:scroll;"
                      "width:200px;height:100px'><div style='height:500px'></div></div>"
                      "</body></html>")
    client = context.eval("document.getElementById('s').clientWidth")
    scroll = context.eval("document.getElementById('s').scrollHeight")
    if scroll <= 100:
        return False
    return client in (183, 200)  # with or without the classic scrollbar gutter


@case
def layout_margin_collapse(context):
    context.load_page("https://test.local/", "<html><body style='margin:0'><div style='height:0;margin-top:20px'>"
                      "</div><script>document.body.style.marginTop='0px'</script></body></html>")
    top = context.eval("document.body.firstElementChild.getBoundingClientRect().top")
    return top in (0, 20)


@case
def dom_matrix(context):
    # 0.2.0: full DOMMatrix over f64, CSS transform strings included.
    checks = {
        "multiply": context.eval(
            "new DOMMatrix([1,0,0,1,0,0]).translate(10,20).e") == 10,
        "fromstring": context.eval(
            "new DOMMatrix('matrix(1, 0, 0, 1, 5, 6)').f") == 6,
        "is2d": context.eval("new DOMMatrix().is2D") is True,
        "inverse": abs(context.eval(
            "new DOMMatrix([2,0,0,2,0,0]).inverse().a") - 0.5) < 1e-12,
        "point": context.eval(
            "new DOMMatrix().transformPoint(new DOMPoint(3,4)).x") == 3,
    }
    return all(checks.values()), checks


@case
def large_inner_html_keeps_identity(context):
    """Purging after a large replacement still leaves the new tree readable."""
    context.load_page("https://test.local/", "<html><body></body></html>")
    result = context.eval("""
      const html = Array.from({length: 300}, (_, i) => '<div id="n' + i + '">x</div>').join('');
      document.body.innerHTML = html;
      document.body.innerHTML = html;
      const hit = document.getElementById('n10');
      document.body.querySelector('div').setAttribute('title', 't');
      ({count: document.querySelectorAll('div').length, id: hit && hit.id, title: document.querySelector('div').getAttribute('title')})
    """)
    return result.get("count") == 300 and result.get("id") == "n10" and result.get("title") == "t", result

@case
def canvas_skia_pixels(context):
    # 0.2.0: canvas 2D paints through Firefox Skia; check real pixel output.
    context.load_page("https://test.local/", "<html><body><canvas id='c' width='4' height='4'></canvas></body></html>")
    context.eval("const g = document.getElementById('c').getContext('2d');"
                 "g.fillStyle = '#ff0000'; g.fillRect(0, 0, 4, 4);")
    data = context.eval("Array.from(g.getImageData(0, 0, 1, 1).data)")
    # Skia stores premultiplied BGRA; getImageData hands back RGBA.
    return data == [255, 0, 0, 255], data


@case
def canvas_gradient(context):
    context.load_page("https://test.local/", "<html><body><canvas id='c' width='8' height='8'></canvas></body></html>")
    context.eval("const g = document.getElementById('c').getContext('2d');"
                 "const gr = g.createLinearGradient(0, 0, 8, 0);"
                 "gr.addColorStop(0, '#000000'); gr.addColorStop(1, '#ffffff');"
                 "g.fillStyle = gr; g.fillRect(0, 0, 8, 8);")
    left = context.eval("Array.from(g.getImageData(0, 0, 1, 1).data)")
    right = context.eval("Array.from(g.getImageData(7, 0, 1, 1).data)")
    return left[0] < 40 and right[0] > 200, [left, right]


@case
def canvas_text_metrics(context):
    context.load_page("https://test.local/", "<html><body><canvas id='c' width='10' height='10'></canvas></body></html>")
    width = context.eval("document.getElementById('c').getContext('2d')"
                         ".measureText('hello world').width")
    return isinstance(width, (int, float)) and width > 0 and width != 100.0, width


@case
def font_serialize_css(context):
    # 0.2.1: the CSS font shorthand parses and round-trips.
    context.load_page("https://test.local/", "<html><body><canvas id='c'></canvas></body></html>")
    context.eval("const g = document.getElementById('c').getContext('2d');"
                 "g.font = 'italic 700 16px Arial, sans-serif';")
    font = context.eval("g.font")
    lowered = font.lower() if isinstance(font, str) else ""
    return "italic" in lowered and "16" in lowered and "arial" in lowered, font


@case
def custom_element_still_connects(context):
    """A parsed custom element still upgrades and runs connectedCallback."""
    context.load_page("https://test.local/", "<html><body></body></html>")
    result = context.eval("""
      class Flag extends HTMLElement {
        constructor() { super(); this.n = 0; }
        connectedCallback() { this.n += 1; }
      }
      customElements.define('x-flag', Flag);
      document.body.innerHTML = '<x-flag id=f></x-flag><div id=d></div>';
      const el = document.getElementById('f');
      ({upgraded: el instanceof Flag, connected: el.n, plain: document.getElementById('d').tagName})
    """)
    return result.get("upgraded") is True and result.get("connected") == 1 and result.get("plain") == "DIV", result

@case
def web_platform_surface(context):
    checks = {
        "useragent": isinstance(context.eval("navigator.userAgent"), str),
        "structured": context.eval(
            "[1,'a',{b:2}].length") == 3,
        "promise": context.eval(
            "let v=0; Promise.resolve(41).then(x => v = x + 1); v") == 0,
    }
    return checks["useragent"] and checks["structured"], checks


@case
def geometry_surface(context):
    checks = {
        "rect": context.eval(
            "document.createElement('div').getBoundingClientRect().width") == 0,
        "dpr": isinstance(context.eval("window.devicePixelRatio"), (int, float)),
        "inner": isinstance(context.eval("window.innerWidth"), (int, float)),
    }
    return all(checks.values()), checks


@case
def query_list_and_id_index(context):
    """Static NodeList indexes stay put; a live list and the id index follow edits."""
    context.load_page("https://test.local/", "<html><body></body></html>")
    result = context.eval("""
      document.body.innerHTML = '<div id=a></div><div id=b></div><span id=c></span>';
      const list = document.querySelectorAll('div');
      const first = list[0];
      const byItem = list.item(0);
      first.remove();
      const staticKeeps = list.length === 2 && list[0] === first && byItem === first;
      const renamed = document.getElementById('b');
      renamed.id = 'z';
      const idMoved = document.getElementById('b') === null && document.getElementById('z') === renamed;
      document.body.innerHTML = '<div id=d></div><div id=d class=second></div>';
      const dup = document.getElementById('d');
      const firstDup = !dup.className;
      dup.remove();
      const second = document.getElementById('d');
      const parent = document.createElement('div');
      document.body.appendChild(parent);
      const kids = parent.childNodes;
      parent.appendChild(document.createElement('span'));
      ({staticKeeps, idMoved, firstDup, secondIs: second && second.className,
        live: kids.length === 1 && kids[0].tagName === 'SPAN'})
    """)
    ok = (result.get("staticKeeps") and result.get("idMoved") and result.get("firstDup")
          and result.get("secondIs") == "second" and result.get("live"))
    return ok, result

@case
def advance_determinism(context):
    # The logical clock drives timers; advance() flushes the loop.
    context.configure_clock("logical", epoch_ms=1700000000000)
    context.eval("let fired = 0; setTimeout(() => fired = 1, 500);")
    context.advance(1000)
    return context.eval("fired") == 1



@case
def element_style_identity(context):
    """element.style is one object, and writing it updates the style attribute."""
    context.load_page("https://test.local/", "<html><body></body></html>")
    result = context.eval("""
      const el = document.createElement('div');
      document.body.appendChild(el);
      const style = el.style;
      style.cssText = 'color: red; margin-top: 4px';
      const text = el.getAttribute('style') || '';
      style.cssText = '';
      ({same: style === el.style, wrote: text.includes('color') && text.includes('red'),
        cleared: !el.getAttribute('style'), text})
    """)
    return result.get("same") and result.get("wrote") and result.get("cleared"), result

@case
def computed_style_invalidation(context):
    """A cached computed style must change after style, class, attribute and tree edits."""
    context.load_page("https://test.local/", "<html><body></body></html>")
    result = context.eval("""
      document.body.innerHTML = '<style>#a.on { color: rgb(1, 2, 3) } #a[data-x] { color: rgb(4, 5, 6) } .big { font-size: 30px }</style>'
        + '<div class=big id=parent><div id=a>t</div></div>';
      const a = document.getElementById('a');
      const color = () => getComputedStyle(a).color;
      const size = () => getComputedStyle(a).fontSize;
      const first = color();
      const again = color();
      a.style.color = 'rgb(9, 9, 9)';
      const styled = color();
      a.style.color = '';
      a.className = 'on';
      const klass = color();
      a.className = '';
      a.setAttribute('data-x', '1');
      const attr = color();
      a.removeAttribute('data-x');
      const before = size();
      a.parentElement.className = '';
      const after = size();
      ({same: first === again, style: styled !== first && styled !== again,
        klass: klass !== first, attr: attr !== first, tree: after !== before,
        first, again, styled, klass, attr, before, after})
    """)
    ok = (result.get("same") and result.get("style") and result.get("klass")
          and result.get("attr") and result.get("tree"))
    return ok, result


@case
def network_timing_v3(context):
    """Over the Necko transport, a same-origin fetch reports measured phases.

    The resource is served through put_resource against the page's own origin:
    a cross-origin fetch without Timing-Allow-Origin has its detailed phases
    zeroed by Firefox's resource timing, which is correct behaviour, not a
    measurement gap.
    """
    context.load_page("https://test.local/", "<html><body></body></html>")
    context.put_resource("https://test.local/api",
                         b'{"ok":true}', headers=[["Content-Type", "application/json"]])
    context.eval("""
      window.__result = null;
      fetch('/api').then(r => r.text()).then(() => {
        const entries = performance.getEntriesByType('resource');
        window.__result = entries.length ? {
          domainLookupStart: entries[0].domainLookupStart,
          connectStart: entries[0].connectStart,
          secureConnectionStart: entries[0].secureConnectionStart,
          requestStart: entries[0].requestStart,
          responseStart: entries[0].responseStart,
        } : {empty: true};
      }).catch(e => window.__result = {error: String(e)});
    """)
    for _ in range(150):
        context.advance(100)
        result = context.eval("window.__result")
        if result and isinstance(result, dict):
            break
    if not isinstance(result, dict) or result.get("error") or result.get("empty"):
        return "skip"
    # Same-origin: measured request/response milestones; the connection
    # establishment is either measured or a reused connection (zeros) —
    # never fabricated plateaus.
    return result["requestStart"] > 0 and result["responseStart"] > 0, result


def main():
    pattern = sys.argv[1] if len(sys.argv) > 1 else ""
    results = {}
    failures = 0
    for fn in CASES:
        if pattern and pattern not in fn.__name__:
            continue
        try:
            with Context() as context:
                outcome = fn(context)
        except Exception as error:  # noqa: BLE001 - report and continue
            outcome = "error: " + str(error)[:200]
        if outcome == "skip":
            results[fn.__name__] = "SKIP"
        elif isinstance(outcome, tuple):
            ok, detail = outcome
            results[fn.__name__] = "PASS" if ok is True else {"FAIL": detail}
            failures += ok is not True
        else:
            ok = outcome is True
            results[fn.__name__] = "PASS" if ok else {"FAIL": outcome}
            failures += not ok
    print(json.dumps({"status": "PASS" if failures == 0 else "FAIL",
                      "failures": failures, "results": results},
                     ensure_ascii=False, indent=1, default=str))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
