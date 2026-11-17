"""Check cascade resolution, getComputedStyle caching, and granular live collection sync."""
import statistics
from ruyijs import Context


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def test_cascade_and_computed_style():
    with Context(timeout=60) as ctx:
        ctx.configure_environment({"viewport": {"width": 1000, "height": 700, "devicePixelRatio": 1}})
        ctx.load_page(
            "https://offline.invalid/",
            "<!doctype html><html><head><style>"
            "@property --noninh { syntax: '<length>'; inherits: false; initial-value: 9px; }"
            ".hdr + .main .card > .txt { padding-left: 17px; }"
            ".sidebar .nav > li a { padding-right: 23px; }"
            ".list > .row { height: 20px; margin-top: 2px; --inh: 14px; }"
            ".list > .row:first-child { margin-top: 10px; }"
            ".list > .row:last-child { margin-bottom: 12px; }"
            ".list > .row[data-hit='1'] { height: 35px; }"
            ".zebra > span:nth-child(2n) { padding-top: 6px; }"
            ".haswrap > .box:has(> .badge) { padding-bottom: 11px; }"
            "fieldset input:disabled { opacity: 0.25; }"
            "@media (max-width: 600px) { #mq { width: 77px; } }"
            "</style></head><body>"
            "<div class=hdr></div>"
            "<div class=main><div class=card><p class=txt id=uncleTarget>hi</p></div></div>"
            "<div class=other><div class=card><p class=txt id=uncleMiss>no</p></div></div>"
            "<div class=sidebar><ul class=nav><li><a id=navLink href='#'>go</a></li></ul></div>"
            "<div class=list id=list>"
            "  <div class=row id=r0>0</div>"
            "  <div class=row id=r1>1</div>"
            "  <div class=row id=r2>2</div>"
            "  <div class=row id=r3 data-hit='1'>3</div>"
            "  <div class=row id=r4 style='height:41px; padding-left: var(--inh); padding-top: var(--noninh)'>4</div>"
            "  <div class=row id=r5>5</div>"
            "</div>"
            "<div class=zebra><span id=z1>a</span><span id=z2>b</span><span id=z3>c</span><span id=z4>d</span></div>"
            "<div class=haswrap><div class=box id=hb0><span>x</span></div><div class=box id=hb1><i class=badge></i></div></div>"
            "<fieldset disabled><legend><input id=leg1></legend><legend><input id=leg2></legend></fieldset>"
            "<div id=mq style='height:10px'></div>"
            "<div id=host></div>"
            "</body></html>",
        )
        res = ctx.eval(
            """(() => {
              const g = id => getComputedStyle(document.getElementById(id));
              // Shadow root + second Document isolation
              const sr = document.getElementById('host').attachShadow({mode: 'open'});
              sr.innerHTML = '<style>.t { width: 63px }</style><div class=t></div>';
              const sw = getComputedStyle(sr.querySelector('.t')).width;
              const doc2 = document.implementation.createHTMLDocument('d2');
              const d2el = doc2.createElement('div');
              doc2.body.appendChild(d2el);
              const d2d = getComputedStyle(d2el).display;
              return {
                uncleHit: g('uncleTarget').paddingLeft,
                uncleMiss: g('uncleMiss').paddingLeft,
                navLink: g('navLink').paddingRight,
                rows: ['r0', 'r1', 'r2', 'r3', 'r4', 'r5'].map(id => [
                  g(id).marginTop, g(id).marginBottom, g(id).height, g(id).paddingLeft, g(id).paddingTop
                ]),
                zebra: ['z1', 'z2', 'z3', 'z4'].map(id => g(id).paddingTop),
                has: [g('hb0').paddingBottom, g('hb1').paddingBottom],
                legends: [g('leg1').opacity, g('leg2').opacity],
                mqWide: g('mq').width,
                sw,
                d2d,
              };
            })()"""
        )
        check("Uncle + descendant selector matches without bloom false negative", (res["uncleHit"], res["uncleMiss"]) == ("17px", "0px"), (res["uncleHit"], res["uncleMiss"]))
        check("Ancestor chain selector matches", res["navLink"] == "23px", res["navLink"])
        check(
            "Sibling style sharing preserves :first-child, :last-child, attrs, inline vars, and @property",
            res["rows"] == [
                ["10px", "0px", "20px", "0px", "0px"],
                ["2px", "0px", "20px", "0px", "0px"],
                ["2px", "0px", "20px", "0px", "0px"],
                ["2px", "0px", "35px", "0px", "0px"],
                ["2px", "0px", "41px", "14px", "9px"],
                ["2px", "12px", "20px", "0px", "0px"],
            ],
            res["rows"],
        )
        check(":nth-child disables naive sharing and resolves exact parity", res["zebra"] == ["0px", "6px", "0px", "6px"], res["zebra"])
        check(":has() resolves exact descendant presence", res["has"] == ["0px", "11px"], res["has"])
        check("Disabled fieldset first legend vs second legend :disabled parity", res["legends"] == ["1", "0.25"], res["legends"])
        check("ShadowRoot and secondary Document getComputedStyle stay isolated", (res["sw"], res["d2d"]) == ("63px", "block"), (res["sw"], res["d2d"]))

        iframe_mq = ctx.eval(
            """(() => {
              const fr = document.createElement('iframe');
              fr.style.cssText = 'width:800px;height:200px;border:0';
              document.body.appendChild(fr);
              fr.contentDocument.open();
              fr.contentDocument.write('<!doctype html><style>#mq{width:120px}@media (max-width:600px){#mq{width:77px}}</style><div id=mq></div>');
              fr.contentDocument.close();
              const target = fr.contentDocument.getElementById('mq');
              const before = fr.contentWindow.getComputedStyle(target).width;
              fr.style.width = '500px';
              fr.offsetWidth;
              const after = fr.contentWindow.getComputedStyle(target).width;
              return [before, after];
            })()"""
        )
        check("@media iframe viewport resize invalidates getComputedStyle cache", iframe_mq == ["120px", "77px"], iframe_mq)

        dynamic_gcs = ctx.eval(
            """(() => {
              const sheet = new CSSStyleSheet();
              sheet.replaceSync('#focInput:focus { padding-left: 19px } #empBox:empty { padding-top: 13px }');
              document.adoptedStyleSheets = [sheet];
              const inp = document.createElement('input');
              inp.id = 'focInput';
              const emp = document.createElement('div');
              emp.id = 'empBox';
              emp.textContent = 'x';
              document.body.append(inp, emp);
              const f0 = getComputedStyle(inp).paddingLeft;
              inp.focus();
              const f1 = getComputedStyle(inp).paddingLeft;
              inp.blur();
              const f2 = getComputedStyle(inp).paddingLeft;
              const e0 = getComputedStyle(emp).paddingTop;
              emp.firstChild.data = '';
              const e1 = getComputedStyle(emp).paddingTop;
              sheet.insertRule('#empBox { padding-bottom: 21px }', sheet.cssRules.length);
              const e2 = getComputedStyle(emp).paddingBottom;
              return [f0, f1, f2, e0, e1, e2];
            })()"""
        )
        check(
            "focus/blur, CharacterData :empty, and CSSOM insertRule invalidate getComputedStyle cache",
            dynamic_gcs == ["0px", "19px", "0px", "0px", "13px", "21px"],
            dynamic_gcs,
        )


def test_live_collections():
    with Context(timeout=60) as ctx:
        ctx.load_page(
            "https://offline.invalid/",
            "<!doctype html><html><body>"
            "<div id=wrap>"
            "  <div class='item a' id=first>one</div>"
            "  <div class='item b'>two</div>"
            "  <a id=lnk>anchor</a>"
            "</div>"
            "<table id=tbl><tr><td>1</td></tr></table>"
            "<select id=sel><option>x</option></select>"
            "</body></html>",
        )
        res = ctx.eval(
            """(() => {
              const wrap = document.getElementById('wrap');
              const first = document.getElementById('first');
              const lnk = document.getElementById('lnk');
              const divs = wrap.getElementsByTagName('div');
              const items = wrap.getElementsByClassName('item');
              const kids = wrap.children;
              const nodes = wrap.childNodes;
              const rows = document.getElementById('tbl').rows;
              const opts = document.getElementById('sel').options;
              const all = document.all;
              const links = document.links;
              const cl = first.classList;
              const pl = first.part;
              const attrs = first.attributes;

              // 1. Non-class attribute edits: divs/items/kids/cl stay valid; namedItem, attrs, all, links, part update!
              first.setAttribute('data-step', '42');
              first.setAttribute('part', 'badge pill');
              first.id = 'renamed';
              lnk.setAttribute('href', 'https://offline.invalid/p');
              first.style.paddingTop = '5px';
              const styleCreated = attrs.style && attrs.style.value.includes('5px');
              first.style.paddingTop = '9px';
              const styleUpdated = attrs.style && attrs.style.value.includes('9px');
              first.removeAttribute('style');
              const styleRemoved = attrs.getNamedItem('style') === null && attrs.style === undefined;
              const afterAttr = {
                divsLen: divs.length,
                itemsLen: items.length,
                namedNew: divs.namedItem('renamed') === first,
                namedOld: divs.namedItem('first') === null,
                allNew: all.renamed === first,
                linksLen: links.length,
                plTokens: Array.from(pl),
                attrData: attrs.getNamedItem('data-step').value,
                clTokens: Array.from(cl),
                styleAttrsOk: Boolean(styleCreated && styleUpdated && styleRemoved),
              };

              // 2. Class edits via className, classList, toggleAttribute, setAttributeNS
              // Materialize index 0 before removing 'item' from `first` so shifted
              // index slots must re-wrap the new element rather than a stale JSObject.
              const initialItem0 = items[0] === first && divs[0] === first;
              first.classList.remove('item');
              const lenAfterRemove = items.length;
              const shiftedItem0 = items[0] !== first && items[0].classList.contains('b');
              first.setAttributeNS(null, 'class', 'item c');
              const lenAfterNS = items.length;
              const clAfterNS = Array.from(cl);
              first.toggleAttribute('class', false);
              const lenAfterToggleOff = items.length;
              first.className = 'item restored';
              const lenAfterRestore = items.length;

              // 3. Tree edits (including prepend) update TagName, children, childNodes, rows, options
              const extra = document.createElement('div');
              extra.className = 'item d';
              wrap.insertBefore(extra, wrap.firstChild);
              const prependedFirst = divs[0] === extra && items[0] === extra && kids[0] === extra;
              document.getElementById('tbl').rows[0].parentNode.appendChild(document.createElement('tr'));
              document.getElementById('sel').appendChild(document.createElement('option'));

              return {
                afterAttr,
                classSteps: [lenAfterRemove, lenAfterNS, lenAfterToggleOff, lenAfterRestore],
                clAfterNS,
                indexShiftOk: initialItem0 && shiftedItem0 && prependedFirst,
                afterTree: [divs.length, items.length, kids.length, rows.length, opts.length],
              };
            })()"""
        )
        check(
            "Non-class attribute mutations keep TagName/ClassNames valid while updating namedItem, document.all, links, part, and NamedNodeMap",
            res["afterAttr"] == {
                "divsLen": 2,
                "itemsLen": 2,
                "namedNew": True,
                "namedOld": True,
                "allNew": True,
                "linksLen": 1,
                "plTokens": ["badge", "pill"],
                "attrData": "42",
                "clTokens": ["item", "a"],
                "styleAttrsOk": True,
            },
            res["afterAttr"],
        )
        check(
            "Class mutations (classList, setAttributeNS, toggleAttribute, className) sync ClassNames and DOMTokenList",
            res["classSteps"] == [1, 2, 1, 2] and res["clAfterNS"] == ["item", "c"],
            (res["classSteps"], res["clAfterNS"]),
        )
        check(
            "Tree mutations and front insertions sync live index slots without stale wrapper reuse",
            res["afterTree"] == [3, 3, 4, 2, 2] and res["indexShiftOk"] is True,
            (res["afterTree"], res["indexShiftOk"]),
        )


def test_speed():
    with Context(timeout=60) as ctx:
        ctx.load_page(
            "https://offline.invalid/",
            "<!doctype html><html><head><style>"
            + "\n".join(f".s{i} .nav .item {{ color: rgb({i}, 10, 20); }}" for i in range(120))
            + "\n.row > .cell { height: 18px; font-size: 14px; }"
            + "</style></head><body><div id=wrap>"
            + "".join(f'<div class="row"><span class="cell k{i % 4}">{i}</span></div>' for i in range(1500))
            + "</div></body></html>",
        )
        ctx.eval(
            """(() => {
              window.__divs = document.getElementsByTagName('div');
              window.__cells = document.getElementsByClassName('cell');
              window.__staticCells = Array.from(document.querySelectorAll('.cell')).slice(0, 500);
            })()"""
        )
        gcs_samples = []
        col_samples = []
        for step in range(5):
            gcs_samples.append(
                ctx.eval(
                    f"""(() => {{
                      document.body.setAttribute('data-s', '{step}');
                      const cells = window.__staticCells;
                      const t0 = performance.now();
                      let n = 0;
                      for (let i = 0; i < cells.length; i++) n += getComputedStyle(cells[i]).fontSize.length;
                      return performance.now() - t0 + n * 0;
                    }})()"""
                )
            )
            col_samples.append(
                ctx.eval(
                    """(() => {
                      const el = window.__cells[0];
                      const divs = window.__divs;
                      const cells = window.__cells;
                      const t0 = performance.now();
                      let n = 0;
                      for (let i = 0; i < 200; i++) {
                        el.setAttribute('data-k', String(i));
                        n += divs.length + cells.length + (cells[0] === el ? 1 : 0);
                      }
                      return performance.now() - t0 + n * 0;
                    })()"""
                )
            )
        gcs_med = round(statistics.median(gcs_samples), 2)
        col_med = round(statistics.median(col_samples), 2)
        print(f"     getComputedStyle x500 (cold revision): {gcs_med} ms (median of 5)")
        print(f"     live HTMLCollections + setAttribute x200 (1500 rows): {col_med} ms (median of 5)")
        check("getComputedStyle x500 stays fast", gcs_med < 80, f"{gcs_med} ms")
        check("live HTMLCollection reads across 200 unrelated setAttributes stay sub-15ms", col_med < 15, f"{col_med} ms")


def main():
    test_cascade_and_computed_style()
    test_live_collections()
    test_speed()
    print("cascade and collection checks passed")


if __name__ == "__main__":
    main()
