"""Check that geometry read after a mutation equals a fresh layout of the mutated page.

A geometry read (offsetHeight, getBoundingClientRect, scrollHeight, ...) reuses
the previous layout's computed styles for what a mutation cannot reach. Each
case loads a page, reads every element's geometry, mutates the page and reads
again; a fresh Context loads the same page, mutates it before any read and reads
once. Both must agree. Also prints how long a read takes after a style change
on a large page (issue #13). Exit status is non-zero on the first difference.
"""
import json
import time

from ruyijs import Context

READ = """JSON.stringify([...document.querySelectorAll('*')].filter(e => e.id).map(e => {
  const r = e.getBoundingClientRect();
  return [e.id, e.offsetTop, e.offsetLeft, e.offsetWidth, e.offsetHeight, r.width, r.height, e.scrollHeight];
}))"""

CASES = {
    "a padding change moves the children": (
        "<section id=s style='width:420px;padding:10px'><input id=i><div id=p>"
        + "".join(f"<div id=c{i}>A moderately long line of text {i}</div>" for i in range(30)) + "</div></section>",
        "document.getElementById('p').style.paddingTop = '17px'"),
    "display: flex blockifies the children": (
        "<div id=p style='width:300px'><span id=a>alpha</span><span id=b>beta</span></div>",
        "document.getElementById('p').style.display = 'flex'"),
    "an inherited font-size reaches the text": (
        "<div id=p style='width:300px'><div id=a>alpha beta gamma</div><span id=b>delta</span></div>",
        "document.getElementById('p').style.fontSize = '27px'"),
    "a custom property reaches var() in the children": (
        "<style>.c { width: var(--w, 50px); height: 10px }</style>"
        "<div id=p style='--w: 120px'><div id=a class=c></div><div id=b class=c></div></div>",
        "document.getElementById('p').style.setProperty('--w', '230px')"),
    "a child that inherits padding follows it": (
        "<style>#a { padding: inherit }</style><div id=p style='padding:4px;width:300px'><div id=a>x</div></div>",
        "document.getElementById('p').style.padding = '19px'"),
    "line-height 20px to 1.25 reaches a larger font": (
        "<div id=p style='font-size:16px;line-height:20px;width:300px'>"
        "<div id=a style='font-size:32px'>big text</div></div>",
        "document.getElementById('p').style.lineHeight = '1.25'"),
    "table cells inherit vertical-align": (
        "<table id=t><tr id=r style='height:80px'><td id=a>a</td><td id=b style='font-size:30px'>b</td></tr></table>",
        "document.getElementById('r').style.verticalAlign = 'bottom'"),
    "a border change keeps the wrapped text": (
        "<div id=p style='width:300px;border:1px solid'><p id=a>some text that wraps over the line maybe</p></div>",
        "document.getElementById('p').style.borderLeftWidth = '25px'"),
}


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def geometry(html, mutation, warm):
    with Context(timeout=60) as ctx:
        ctx.load_page("https://offline.invalid/", "<!doctype html><html><body>" + html + "</body></html>")
        if warm:
            ctx.eval(READ)
        ctx.eval(mutation)
        return json.loads(ctx.eval(READ))


def main():
    for name, (html, mutation) in CASES.items():
        incremental, fresh = geometry(html, mutation, True), geometry(html, mutation, False)
        differences = [(a, b) for a, b in zip(incremental, fresh) if a != b]
        check(name, incremental == fresh, differences[:2] if differences else "")
    with Context(timeout=60) as ctx:
        ctx.load_page("https://offline.invalid/", "<html><body></body></html>")
        ctx.eval("document.body.innerHTML = '<section style=\"width:420px;padding:10px\"><input id=editor>"
                 "<div id=preview></div></section>'")
        ctx.eval("""(() => { const p = document.getElementById('preview');
            for (let i = 0; i < 1500; i++) { const d = document.createElement('div');
                d.textContent = 'A moderately long line of text ' + i; p.appendChild(d); } })()""")
        ctx.eval("document.getElementById('editor').offsetHeight")
        times = []
        for i in range(5):
            ctx.eval(f"document.getElementById('preview').style.paddingTop = '{i + 1}px'")
            started = time.perf_counter()
            ctx.eval("document.getElementById('editor').offsetHeight")
            times.append((time.perf_counter() - started) * 1000)
    print(f"     offsetHeight after a style change, 1500 divs: {sorted(times)[2]:.1f} ms (median of 5)")
    print("layout checks passed")


if __name__ == "__main__":
    main()
