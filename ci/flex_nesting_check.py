"""Check nested flex layout against Firefox 160 (issue #50).

Nested flex containers re-place an item when its final size differs from the
one it was laid out at, and a layout stops re-placing past a budget of
placements per node. A row container placed every stretched item at its
content height first and again at the stretched height, so each nesting level
doubled the work: a 240px frame whose flex-grow child should get 180px got 0px
when no whitespace text nodes padded the node count (markup without newlines
between tags), and two more nesting levels broke it even with them. A
single-line container with a definite cross size now places a stretched item
once, at its line's cross size (CSS Flexbox §9.4 step 8). The expectations are
Firefox 160's getBoundingClientRect() (x, y, width, height). Offline; exit
status is non-zero on failure.
"""
import sys

from ruyijs import Context

CSS = """body { margin: 0; }
.f { display: flex; } .c { display: flex; flex-direction: column; } .w { width: 320px; }
#frame { height: 240px; width: 320px; } #frame, .row, #column, #panel, #stack, #grow { display: flex; }
.row, #column, #panel, #stack { width: 320px; } #column, #stack { flex-direction: column; }
#header { height: 40px; flex: none; } #panel { height: 100%; min-height: 0; }
#grow { flex-grow: 1; } #footer { height: 20px; flex: none; }"""
ISSUE = ('<div id="frame"><div class="row"><div class="row"><div id="column">'
         '<div id="header"></div><div id="panel"><div id="stack">'
         '<div id="grow"><div></div><div></div><div></div></div>'
         '<div id="footer"></div></div></div></div></div></div></div>')
DEEPER = (ISSUE.replace('<div id="frame">', '<div id="frame"><div class="row"><div class="row">')
          .removesuffix("</div>" * 7) + "</div>" * 9)
NEST = ('<div id="leaf" style="flex-grow:1"><div style="height:10px"></div></div>'
        '<div id="tail" style="height:15px;flex:none"></div>')
for level in range(12):
    NEST = f'<div class="{"f" if level % 2 else "c"}" style="flex:1 1 auto;min-height:0">{NEST}</div>'
ISSUE_BOXES = "stack=0,40,320,200 grow=0,40,320,180 footer=0,220,320,20"
CASES = {
    "compact markup (#50)": (ISSUE, ISSUE_BOXES),
    "newlines between tags": (ISSUE.replace("><", ">\n<"), ISSUE_BOXES),
    "an unrelated element after it": (ISSUE + "<i></i>", ISSUE_BOXES),
    "two more nesting levels": (DEEPER, ISSUE_BOXES),
    "twelve nested flex levels": (f'<div class="c w" style="height:300px">{NEST}</div>',
                                  "leaf=0,0,320,285 tail=0,285,320,15"),
    "stretch clamped by max/min-height": (
        '<div class="f w" style="height:240px"><div id="a" style="max-height:100px;width:50px"></div>'
        '<div id="b" style="min-height:260px;width:50px"></div></div>', "a=0,0,50,100 b=50,0,50,260"),
    "a percentage height inside a stretched item": (
        '<div class="f w" style="height:200px"><div id="a" style="width:50px">'
        '<div id="p" style="height:50%"></div></div></div>', "a=0,0,50,200 p=0,0,50,100"),
    "cross margins, auto margins": (
        '<div class="f w" style="height:200px"><div id="a" style="width:50px;margin:10px 0 30px"></div>'
        '<div id="b" style="width:50px;margin-top:auto;height:20px"></div>'
        '<div id="c" style="width:50px;margin-top:auto"><div style="height:7px"></div></div></div>',
        "a=0,10,50,160 b=50,180,50,20 c=100,193,50,7"),
    "padding and border": (
        '<div class="f w" style="height:200px;padding:5px;box-sizing:border-box">'
        '<div id="a" style="width:50px;padding:3px;border:2px solid"></div></div>', "a=5,5,60,190"),
    "wrapping lines": (
        '<div class="f w" style="height:200px;flex-wrap:wrap"><div id="a" style="width:200px">'
        '<div style="height:30px"></div></div><div id="b" style="width:200px"><div style="height:50px"></div>'
        '</div></div>', "a=0,0,200,90 b=0,90,200,110"),
    "an indefinite height": (
        '<div class="f w"><div id="a" style="width:50px"><div style="height:30px"></div></div>'
        '<div id="b" style="width:50px"><div style="height:70px"></div></div></div>', "a=0,0,50,70 b=50,0,50,70"),
    "a column inside a stretched item": (
        '<div class="f w" style="height:200px"><div id="col" class="c" style="width:100px">'
        '<div id="g" style="flex-grow:1"></div><div id="t" style="height:20px"></div></div></div>',
        "col=0,0,100,200 g=0,0,100,180 t=0,180,100,20"),
    "align-self: flex-end": (
        '<div class="f w" style="height:200px"><div id="a" style="width:50px;align-self:flex-end">'
        '<div style="height:30px"></div></div></div>', "a=0,170,50,30"),
    "a grid item": (
        '<div class="f w" style="height:150px"><div id="gr" style="display:grid;grid-template-rows:1fr 30px;'
        'width:100px"><div id="r1"></div><div id="r2"></div></div></div>',
        "gr=0,0,100,150 r1=0,0,100,120 r2=0,120,100,30"),
}
MEASURE = """[...document.querySelectorAll('[id]')].filter(e => %s.includes(e.id)).map(e => {
  const r = e.getBoundingClientRect();
  return e.id + '=' + [r.x, r.y, r.width, r.height].map(Math.round).join(',');
}).sort((a, b) => %s.indexOf(a.split('=')[0]) - %s.indexOf(b.split('=')[0])).join(' ')"""

failures = []
for name, (body, expected) in CASES.items():
    ids = repr([item.split("=")[0] for item in expected.split()])
    with Context(timeout=30) as ctx:
        ctx.configure_environment({"viewport": {"width": 1280, "height": 843}})
        ctx.load_page("https://flex.example.test/",
                      f"<!doctype html><html><head><style>{CSS}</style></head><body>{body}</body></html>")
        actual = ctx.eval(MEASURE % (ids, ids, ids))
    ok = actual == expected
    print(f"{'ok  ' if ok else 'FAIL'} {name}: {actual}" + ("" if ok else f"  (Firefox 160: {expected})"))
    if not ok:
        failures.append(name)

print("flex nesting checks passed" if not failures else f"FAILED: {failures}")
sys.exit(1 if failures else 0)
