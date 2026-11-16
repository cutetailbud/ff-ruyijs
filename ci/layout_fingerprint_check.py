"""Check the layout and DOM values an antibot fingerprint reads (issue #42).

A wbaas (Guardium) challenge measures the page and reports a risk score. Every
case below was compared with Firefox 160 on the same page; the expected values
are what Firefox returned. The text cases depend on the font `monospace`
resolves to, which differs between machines, so their expectations are derived
from the measured advance of one character as Firefox lays monospace text out:
a preserved run is one advance per character, and a line breaks greedily at
spaces. Covers preserved spaces, line breaking, float and
table shrink-to-fit, aspect-ratio, the progress element, and the resolved
values getComputedStyle returns for a rendered element. No network. Exit status
is non-zero on failure.
"""
import json
import sys

from ruyijs import Context

R = ("position: absolute; left: 0px; top: 0px; visibility: hidden; display: block; "
     "box-sizing: content-box; margin: 0px; padding: 0px; border: 0px; height: auto; "
     "min-width: 0px; min-height: 0px; max-width: none; max-height: none; float: none; "
     "transform: none; ")
T = ("font: 16px / 20px monospace; letter-spacing: 0px; word-spacing: 0px; "
     "text-transform: none; text-indent: 0px; overflow-wrap: normal; word-break: normal; ")
PAGE = """<!doctype html><body><script>
const RESET = __R__, TEXT = __T__;
const out = {};
function div(style, text) {
  const d = document.createElement("div");
  d.setAttribute("style", RESET + TEXT + style);
  d.textContent = text || "";
  document.body.appendChild(d);
  return d;
}
out.advance = div("width: auto; white-space: pre;", "aaaaaaaaaa").getBoundingClientRect().width / 10;
out.pre2 = div("width: auto; white-space: pre;", "sogxg  gbnvcp").offsetWidth;
out.pre4 = div("width: auto; white-space: pre;", "zhhszo    gxx").offsetWidth;
out.wrap1px = div("width: 1px;", "zxokka apz").offsetHeight;
out.wrap40px = div("width: 40px;", "aa bb cc dd ee").offsetHeight;
out.wrap60px = div("width: 60px;", "aa bb cc dd ee").offsetHeight;
out.wrap120px = div("width: 120px;", "aa bb cc dd ee").offsetHeight;
const host = div("width: 2000px;");
host.style.visibility = "visible";
host.innerHTML = '<div style="display: block; float: left; padding: 2px;">' +
  '<div style="display: block; width: 39px; height: 1px;"></div></div>';
out.float = host.firstChild.offsetWidth;
host.innerHTML = '<table><tbody><tr><td><div style="display: block; width: 30px; height: 1px;"></div></td>' +
  '<td><div style="display: block; width: 40px; height: 1px;"></div></td></tr></tbody></table>';
out.table = host.firstChild.offsetWidth;
host.innerHTML = '<div style="display: block; width: 160px; aspect-ratio: 16 / 9;"></div>';
out.aspect = host.firstChild.offsetHeight;
host.innerHTML = '<div id="used" style="display: block; width: calc(33.3% + 0.7px); height: 1px; ' +
  'padding: 5%; margin: 10%; position: relative; left: 10%;"></div>';
const used = getComputedStyle(document.getElementById("used"));
out.gcs = [used.width, used.height, used.paddingLeft, used.marginLeft, used.left];
const progress = document.createElement("progress");
out.progress = [progress.max, progress.value, progress.position];
progress.setAttribute("max", "0");
progress.setAttribute("value", "3");
out.progressMax0 = [progress.max, progress.value, progress.position];
progress.setAttribute("max", "5");
out.progress5 = [progress.max, progress.value, progress.position];
window.__check = out;
</script>""".replace("__R__", json.dumps(R)).replace("__T__", json.dumps(T))

LINE = 20  # font: 16px / 20px


def lines(text, width, advance):
    """Greedy line count for monospace text breaking at single spaces."""
    count, line = 1, 0.0
    for word in text.split(" "):
        size = len(word) * advance
        if line > 0 and line + advance + size > width + 1e-6:
            count, line = count + 1, size
        else:
            line += (advance if line > 0 else 0) + size
    return count


def text_expectations(advance):
    return {
        "pre2": round(13 * advance), "pre4": round(13 * advance),
        "wrap1px": LINE * lines("zxokka apz", 1, advance),
        "wrap40px": LINE * lines("aa bb cc dd ee", 40, advance),
        "wrap60px": LINE * lines("aa bb cc dd ee", 60, advance),
        "wrap120px": LINE * lines("aa bb cc dd ee", 120, advance),
    }


EXPECTED = {
    "float": 43, "table": 80, "aspect": 90,
    "gcs": ["666.7px", "1px", "100px", "200px", "200px"],
    "progress": [1, 0, -1], "progressMax0": [1, 1, 1], "progress5": [5, 3, 0.6],
}


def main():
    with Context(timeout=60) as context:
        context.load_page("http://127.0.0.1/layout", PAGE)
        values = context.eval("window.__check")
    advance = values.get("advance") or 0
    print(f"     monospace advance: {advance:.4f}px")
    failed = not advance > 0
    expected_values = dict(text_expectations(advance), **EXPECTED)
    for name, expected in expected_values.items():
        actual = values.get(name)
        same = actual == expected or (isinstance(expected, list) and
                                      [str(v) for v in actual] == [str(v) for v in expected])
        failed |= not same
        print(f"{'ok' if same else 'FAIL':4} {name}: {actual!r}" + ("" if same else f", expected {expected!r}"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
