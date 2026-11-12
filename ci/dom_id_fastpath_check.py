"""Document querySelector('#id') and HTMLCollection.namedItem agree with a tree walk.

The id index may answer only when that answer is the one the walk would
return: a single connected holder for a pure document id selector, and a
namedItem whose id element is the first element in the collection. Duplicates,
compounds, element and fragment scopes, shadow trees, and an earlier name
attribute stay on the walk.
"""
from ruyijs import Context, JSError


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def load(ctx, html):
    ctx.load_page("https://offline.invalid/id-fast/", "<!doctype html>" + html)


def test_id_fast_path():
    with Context(timeout=60) as ctx:
        load(ctx, "<div id=wrap><div id=foo class=extra>original</div></div>")
        unique = ctx.eval(
            r"""(() => {
              const el = document.getElementById('foo');
              const one = document.querySelector('#foo');
              const all = document.querySelectorAll('#foo');
              const spaced = document.querySelector(' #foo ');
              const escaped = document.querySelector('#\\66 oo');
              const compound = document.querySelector('#foo.extra');
              const missingClass = document.querySelector('#foo.nope');
              const inner = document.getElementById('wrap').querySelector('#foo');
              return {
                one: one === el, allLen: all.length, all0: all[0] === el,
                spaced: spaced === el, escaped: escaped === el,
                compound: compound === el, missingClass: missingClass === null,
                inner: inner === el,
              };
            })()"""
        )
        check("document querySelector('#id') is the unique element", unique["one"] is True and unique["spaced"] is True, unique)
        check("querySelectorAll('#id') is that one element", unique["allLen"] == 1 and unique["all0"] is True, unique)
        check("a CSS id escape matches the same element", unique["escaped"] is True, unique)
        check("a compound id selector still matches only when every part matches",
              unique["compound"] is True and unique["missingClass"] is True, unique)
        check("an element scope still finds the id inside it", unique["inner"] is True, unique)

        try:
            ctx.eval("document.querySelector('#1')")
            check("an invalid id selector throws", False)
        except JSError:
            check("an invalid id selector throws", True)

        cloned = ctx.eval(
            """(() => {
              const original = document.getElementById('foo');
              const clone = original.cloneNode(true);
              clone.textContent = 'clone';
              original.parentNode.insertBefore(clone, original);
              const outside = document.createElement('section');
              const nested = document.createElement('span');
              nested.id = 'foo';
              outside.appendChild(nested);
              document.body.appendChild(outside);
              const all = document.querySelectorAll('#foo');
              return {
                first: all[0] === clone, second: all[1] === original,
                one: document.querySelector('#foo') === clone,
                len: all.length,
                section: outside.querySelector('#foo') === nested,
                wrapMiss: document.getElementById('wrap').querySelector('#foo') === clone,
              };
            })()"""
        )
        check("duplicate ids: querySelector is the tree-order first", cloned["one"] is True and cloned["len"] == 3, cloned)
        check("querySelectorAll lists the clone before the original", cloned["first"] is True and cloned["second"] is True, cloned)
        check("an element scope does not return an id outside that element",
              cloned["section"] is True and cloned["wrapMiss"] is True, cloned)

        scope = ctx.eval(
            """(() => {
              const frag = document.createDocumentFragment();
              const inner = document.createElement('div');
              inner.id = 'onlyfrag';
              frag.appendChild(inner);
              const host = document.createElement('div');
              document.body.appendChild(host);
              const root = host.attachShadow({mode: 'open'});
              const shadow = document.createElement('b');
              shadow.id = 'onlyshadow';
              root.appendChild(shadow);
              const light = document.createElement('i');
              light.id = 'onlylight';
              document.body.appendChild(light);
              return {
                frag: frag.querySelector('#onlyfrag') === inner,
                docFrag: document.querySelector('#onlyfrag') === null,
                docShadow: document.querySelector('#onlyshadow') === null,
                shadowId: root.getElementById('onlyshadow') === shadow,
                light: document.querySelector('#onlylight') === light,
              };
            })()"""
        )
        check("a fragment query stays inside the fragment", scope["frag"] is True and scope["docFrag"] is True, scope)
        check("a shadow id is not a document querySelector hit", scope["docShadow"] is True and scope["shadowId"] is True, scope)
        check("a unique light-dom id is still found", scope["light"] is True, scope)

        detached = ctx.eval(
            """(() => {
              const el = document.createElement('em');
              el.id = 'gone';
              document.body.appendChild(el);
              const live = document.querySelector('#gone') === el;
              el.remove();
              return {live, after: document.querySelector('#gone') === null && el.id === 'gone'};
            })()"""
        )
        check("a detached id is not a querySelector hit", detached["live"] is True and detached["after"] is True, detached)

        named = ctx.eval(
            """(() => {
              const root = document.createElement('div');
              document.body.appendChild(root);
              const only = document.createElement('span');
              only.id = 'solo';
              root.appendChild(only);
              const byTag = root.getElementsByTagName('span').namedItem('solo') === only;
              const byStar = root.getElementsByTagName('*').namedItem('solo') === only;
              const byChild = root.children.namedItem('solo') === only;
              const wrongTag = root.getElementsByTagName('div').namedItem('solo') === null;
              const namedFirst = document.createElement('b');
              namedFirst.setAttribute('name', 'solo');
              root.insertBefore(namedFirst, only);
              const star = root.getElementsByTagName('*').namedItem('solo') === namedFirst;
              const span = root.getElementsByTagName('span').namedItem('solo') === only;
              const a = document.createElement('i');
              const b = document.createElement('i');
              a.id = 'dup'; b.id = 'dup';
              root.appendChild(a); root.appendChild(b);
              const dup = root.getElementsByTagName('i').namedItem('dup') === a;
              const empty = root.getElementsByTagName('*').namedItem('') === null;
              return {byTag, byStar, byChild, wrongTag, star, span, dup, empty};
            })()"""
        )
        check("namedItem returns a unique id that is the first element in the collection",
              named["byTag"] is True and named["byStar"] is True and named["byChild"] is True, named)
        check("namedItem misses an id the collection does not contain", named["wrongTag"] is True, named)
        check("an earlier name attribute wins over a later id", named["star"] is True and named["span"] is True, named)
        check("duplicate ids: namedItem is the first element in the collection", named["dup"] is True, named)
        check("namedItem of the empty string is null", named["empty"] is True, named)

    print("id fast path checks passed")


if __name__ == "__main__":
    test_id_fast_path()
