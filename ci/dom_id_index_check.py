"""getElementById returns the tree-order first element, including after id mutations.

The id index is a fast path for a single holder. These checks are the cases
that used to leave that holder stale: clone, setAttributeNS, toggleAttribute,
foreign insert, a repeated body start tag, an XML parse, and a duplicate id
that becomes unique again. querySelector is the tree walk and is only the oracle.
"""
from ruyijs import Context


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def load(ctx, html):
    ctx.load_page("https://offline.invalid/id-index/", "<!doctype html>" + html)


def test_id_index():
    with Context(timeout=60) as ctx:
        load(ctx, "<div id=wrap><div id=foo>original</div></div>")
        cloned = ctx.eval(
            """(() => {
              const original = document.getElementById('foo');
              const clone = original.cloneNode(true);
              clone.textContent = 'clone';
              original.parentNode.insertBefore(clone, original);
              const hit = document.getElementById('foo');
              const first = document.querySelector('#foo');
              clone.id = 'clone-only';
              return {
                hitIsClone: hit === clone,
                hitText: hit && hit.textContent,
                firstText: first && first.textContent,
                restored: document.getElementById('foo') === original,
                renamed: document.getElementById('clone-only') === clone,
              };
            })()"""
        )
        check(
            "getElementById after clone + insertBefore is the clone",
            cloned["hitIsClone"] is True and cloned["hitText"] == "clone" and cloned["firstText"] == "clone",
            cloned,
        )
        check("dropping the clone's id restores the original", cloned["restored"] is True and cloned["renamed"] is True, cloned)

        load(ctx, "<div id=wrap><div id=foo>original</div></div>")
        nested = ctx.eval(
            """(() => {
              const wrap = document.getElementById('wrap');
              const original = document.getElementById('foo');
              const clone = wrap.cloneNode(true);
              clone.querySelector('#foo').textContent = 'copy';
              wrap.parentNode.insertBefore(clone, wrap);
              const hit = document.getElementById('foo');
              return {hitText: hit && hit.textContent, inClone: hit && hit.parentNode === clone,
                      notOriginal: hit !== original};
            })()"""
        )
        check(
            "a cloned descendant id is the tree-order first holder",
            nested["hitText"] == "copy" and nested["inClone"] is True and nested["notOriginal"] is True,
            nested,
        )

        load(ctx, "<div id=ns>original</div>")
        namespaced = ctx.eval(
            """(() => {
              const original = document.getElementById('ns');
              const extra = document.createElement('div');
              extra.textContent = 'ns-clone';
              extra.setAttributeNS(null, 'id', 'ns');
              original.parentNode.insertBefore(extra, original);
              const first = document.getElementById('ns') === extra;
              extra.setAttributeNS(null, 'id', 'ns2');
              const back = document.getElementById('ns') === original;
              const moved = document.getElementById('ns2') === extra;
              extra.removeAttributeNS(null, 'id');
              return {first, back, moved, cleared: document.getElementById('ns2') === null,
                      still: document.getElementById('ns') === original};
            })()"""
        )
        check("setAttributeNS id inserted before the original wins", namespaced["first"] is True, namespaced)
        check("setAttributeNS rename restores the previous holder", namespaced["back"] is True and namespaced["moved"] is True, namespaced)
        check("removeAttributeNS drops that id", namespaced["cleared"] is True and namespaced["still"] is True, namespaced)

        load(ctx, "<i id=trip>a</i><b id=trip>b</b><em id=trip>c</em>")
        unique = ctx.eval(
            """(() => {
              const a = document.querySelector('i');
              const b = document.querySelector('b');
              const c = document.querySelector('em');
              a.removeAttribute('id');
              const afterA = document.getElementById('trip') === b;
              b.toggleAttribute('id');
              const afterB = document.getElementById('trip') === c && b.id === '';
              const early = document.createElement('span');
              early.textContent = 'early';
              early.setAttributeNS(null, 'id', 'trip');
              c.parentNode.insertBefore(early, c);
              const earlyWins = document.getElementById('trip') === early;
              early.removeAttributeNS(null, 'id');
              const restored = document.getElementById('trip') === c;
              c.toggleAttribute('id');
              return {afterA, afterB, earlyWins, restored, gone: document.getElementById('trip') === null};
            })()"""
        )
        check("removing the first duplicate leaves the tree-order next", unique["afterA"] is True, unique)
        check("toggleAttribute removes the next duplicate", unique["afterB"] is True, unique)
        check("a new earlier id wins over the restored unique holder", unique["earlyWins"] is True, unique)
        check("clearing that earlier id restores the survivor, then toggle clears it",
              unique["restored"] is True and unique["gone"] is True, unique)

        load(ctx, '<svg id=svgid></svg><math id=mathid></math><div id=svgid>svg-later</div><span id=mathid>math-later</span>')
        foreign = ctx.eval(
            """(() => {
              const svg = document.querySelector('svg');
              const math = document.querySelector('math');
              return {
                svgFirst: document.getElementById('svgid') === svg,
                mathFirst: document.getElementById('mathid') === math,
                svgNs: svg && svg.namespaceURI,
                mathNs: math && math.namespaceURI,
              };
            })()"""
        )
        check(
            "a foreign id parsed before a later duplicate wins",
            foreign["svgFirst"] is True and foreign["mathFirst"] is True
            and foreign["svgNs"] == "http://www.w3.org/2000/svg"
            and foreign["mathNs"] == "http://www.w3.org/1998/Math/MathML",
            foreign,
        )

        load(ctx, "<body><div id=merged>later</div><body id=merged>")
        merged = ctx.eval(
            """(() => ({
              bodyId: document.body && document.body.id,
              hitIsBody: document.getElementById('merged') === document.body,
              queryIsBody: document.querySelector('#merged') === document.body,
            }))()"""
        )
        check(
            "a repeated body start tag's id is visible ahead of a later element",
            merged["bodyId"] == "merged" and merged["hitIsBody"] is True and merged["queryIsBody"] is True,
            merged,
        )

        load(ctx, "<p id=keep>page</p>")
        xml = ctx.eval(
            """(() => {
              const doc = new DOMParser().parseFromString(
                '<root><a id="foo">first</a><b id="bar">second</b></root>', 'application/xml');
              const root = doc.documentElement;
              const first = root && root.firstElementChild;
              const second = first && first.nextElementSibling;
              if (!root || root.localName !== 'root' || !first || !second) {
                return {root: root && root.localName, first: first && first.localName};
              }
              const parsed = doc.getElementById('foo') === first;
              second.setAttribute('id', 'foo');
              const earlier = doc.getElementById('foo') === first;
              first.setAttribute('id', 'other');
              const survivor = doc.getElementById('foo') === second && doc.getElementById('other') === first;
              return {parsed, earlier, survivor};
            })()"""
        )
        check("XML getElementById finds the parsed id", xml.get("parsed") is True, xml)
        check("a later XML setAttribute does not hide the earlier id", xml.get("earlier") is True, xml)
        check("an XML id becomes unique again on the remaining element", xml.get("survivor") is True, xml)

        load(ctx, "<b id=aga>x<p>y</p></b>")
        adopted = ctx.eval(
            """(() => {
              const nodes = document.querySelectorAll('#aga');
              const first = document.querySelector('#aga');
              const same = document.getElementById('aga') === first;
              if (nodes.length < 2) return {same, count: nodes.length, split: false};
              first.id = 'moved';
              const next = document.querySelector('#aga');
              const tail = document.createElement('i');
              tail.id = 'aga';
              document.body.appendChild(tail);
              return {same, count: nodes.length, split: true,
                      nextWins: document.getElementById('aga') === next && next !== tail};
            })()"""
        )
        check("getElementById matches the tree-order formatting id", adopted["same"] is True, adopted)
        if adopted["split"]:
            check("an adopted duplicate stays ahead of a later id", adopted["nextWins"] is True, adopted)
        else:
            print("     adoption did not duplicate id=aga; split path not exercised", flush=True)

        load(ctx, "<div id=host></div>")
        detached = ctx.eval(
            """(() => {
              const el = document.createElement('div');
              el.id = 'gone';
              document.body.appendChild(el);
              const live = document.getElementById('gone') === el;
              el.remove();
              const after = document.getElementById('gone') === null && el.id === 'gone';
              document.body.appendChild(el);
              const back = document.getElementById('gone') === el;
              const host = document.getElementById('host');
              const root = host.attachShadow({mode: 'open'});
              const inner = document.createElement('span');
              inner.id = 'gone';
              root.appendChild(inner);
              const lightWins = document.getElementById('gone') === el;
              return {live, after, back, lightWins};
            })()"""
        )
        check("a detached id is not a hit, and reinsertion is", detached["live"] is True and detached["after"] is True and detached["back"] is True, detached)
        check("a shadow tree id does not hide the document element", detached["lightWins"] is True, detached)

    print("id index checks passed")


if __name__ == "__main__":
    test_id_index()
