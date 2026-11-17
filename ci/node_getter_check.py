"""Check narrow DOM node/element/character-data getters and O(1) sibling indexing (#35)."""
import time
from ruyijs import Context


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def test_narrow_getters_and_adoption():
    with Context(timeout=60) as ctx:
        ctx.load_page(
            "https://offline.invalid/dir/page.html",
            "<!doctype html><html><body>"
            "<div id=host data-a=1>lead<span id=s1>one</span><!--c--><em id=s2>two</em>tail</div>"
            "<div id=empty></div>"
            "</body></html>",
        )
        res = ctx.eval(
            """(() => {
              const host = document.getElementById('host');
              const empty = document.getElementById('empty');
              const s1 = document.getElementById('s1');
              const s2 = document.getElementById('s2');
              const lead = host.firstChild;
              const comm = s1.nextSibling;

              // Namespaced + prefixed element
              const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg:rect');

              // Shadow DOM getRootNode + isConnected
              const shadowHost = document.createElement('div');
              const sr = shadowHost.attachShadow({mode: 'open'});
              const inner = document.createElement('b');
              sr.appendChild(inner);
              const detachedRoots = [
                inner.isConnected,
                inner.getRootNode() === sr,
                inner.getRootNode({composed: true}) === shadowHost,
              ];
              document.body.appendChild(shadowHost);
              const connectedRoots = [
                inner.isConnected,
                inner.getRootNode() === sr,
                inner.getRootNode({composed: true}) === document,
              ];

              // Element-sibling & childElementCount before adoptNode removes s2
              const preAdoptLinks = [
                host.firstElementChild === s1,
                host.lastElementChild === s2,
                s1.nextElementSibling === s2,
                s2.previousElementSibling === s1,
                host.childElementCount === 2,
              ];

              // Cross-document adoptNode + getter evaluation order on getRootNode(options)
              const doc2 = document.implementation.createHTMLDocument('d2');
              let orderAdopted = false;
              const rootAfterAdopt = s2.getRootNode({
                get composed() {
                  doc2.adoptNode(s2);
                  orderAdopted = true;
                  return true;
                }
              });

              // CharacterData narrow getters & appendData
              lead.appendData('!');
              const cdInfo = [lead.nodeType, lead.nodeName, lead.data, lead.length, comm.nodeType, comm.nodeValue];

              // TreeWalker over host
              const tw = document.createTreeWalker(host, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_COMMENT);
              const walked = [];
              while (tw.nextNode()) walked.push(tw.currentNode.nodeName);

              // Brand errors preserved
              const errs = [];
              for (const fn of [
                () => Object.getOwnPropertyDescriptor(Node.prototype, 'nodeType').get.call({}),
                () => Object.getOwnPropertyDescriptor(Element.prototype, 'tagName').get.call(lead),
                () => Object.getOwnPropertyDescriptor(CharacterData.prototype, 'length').get.call(host),
              ]) {
                try { fn(); errs.push('no-throw'); }
                catch (e) { errs.push(e instanceof TypeError ? 'TypeError' : e.name); }
              }

              return {
                htmlTag: [host.nodeType, host.nodeName, host.tagName, host.localName, host.prefix, host.namespaceURI],
                svgTag: [svg.tagName, svg.localName, svg.prefix, svg.namespaceURI],
                links: [
                  ...preAdoptLinks,
                  document.ownerDocument === null,
                  s1.ownerDocument === document,
                  s1.parentNode === host,
                  s1.parentElement === host,
                  document.documentElement.parentNode === document,
                  document.documentElement.parentElement === null,
                  host.firstChild === lead,
                  host.lastChild.nodeValue === 'tail',
                  host.firstElementChild === s1,
                  host.lastElementChild === s1,
                  s1.previousSibling === lead,
                  s1.nextSibling === comm,
                  comm.previousElementSibling === s1,
                  s1.nextElementSibling === null,
                  host.childElementCount === 1,
                  host.hasChildNodes() === true,
                  empty.hasChildNodes() === false,
                  host.hasAttributes() === true,
                  document.createElement('div').hasAttributes() === false,
                ],
                detachedRoots,
                connectedRoots,
                adopt: [orderAdopted, rootAfterAdopt === s2, s2.ownerDocument === doc2, s2.isConnected === false],
                base: [document.baseURI, s1.baseURI],
                cdInfo,
                walked,
                errs,
              };
            })()"""
        )
        check(
            "HTML and prefixed SVG tag/namespace getters match",
            res["htmlTag"] == [1, "DIV", "DIV", "div", None, "http://www.w3.org/1999/xhtml"]
            and res["svgTag"] == ["svg:rect", "rect", "svg", "http://www.w3.org/2000/svg"],
            (res["htmlTag"], res["svgTag"]),
        )
        check("Parent, child, sibling, element-sibling, and attribute/child presence getters match", all(res["links"]), res["links"])
        check("ShadowRoot getRootNode and isConnected match before and after attach", res["detachedRoots"] == [False, True, True] and res["connectedRoots"] == [True, True, True], (res["detachedRoots"], res["connectedRoots"]))
        check("adoptNode inside getRootNode options preserves wrapper identity and ownerDocument", res["adopt"] == [True, True, True, True], res["adopt"])
        check("baseURI matches on Document and Element", res["base"] == ["https://offline.invalid/dir/page.html", "https://offline.invalid/dir/page.html"], res["base"])
        check("CharacterData data, length, and appendData match", res["cdInfo"] == [3, "#text", "lead!", 5, 8, "c"], res["cdInfo"])
        check("TreeWalker narrow traversal visits expected nodes", res["walked"] == ["SPAN", "#comment"], res["walked"])
        check("Invalid receivers still throw TypeError", res["errs"] == ["TypeError", "TypeError", "TypeError"], res["errs"])


def test_large_sibling_scaling():
    with Context(timeout=60) as ctx:
        ctx.load_page("https://offline.invalid/", "<!doctype html><body><div id=host></div></body>")
        res = ctx.eval(
            """(() => {
              const host = document.getElementById('host');
              const N = 16384;
              host.innerHTML = '<i></i>'.repeat(N);
              const target = host.firstChild;

              // Walk all 16,384 siblings forward via nextSibling (would be O(N^2) without child_index_)
              let fwd = 0;
              for (let cur = host.firstChild; cur; cur = cur.nextSibling) fwd++;

              // Mutate front, middle, and back after child_index_ is populated, then walk backward
              const extraFront = document.createElement('b');
              const extraMid = document.createElement('u');
              host.insertBefore(extraFront, host.firstChild);
              host.insertBefore(extraMid, host.children[500]);
              host.removeChild(host.lastChild);
              host.removeChild(extraMid);
              host.replaceChild(document.createElement('em'), extraFront);

              let bwd = 0;
              for (let cur = host.lastChild; cur; cur = cur.previousSibling) bwd++;

              host.insertBefore(target, host.firstChild);
              const run = fn => { const t0 = performance.now(); const s = fn(); return [s, performance.now() - t0]; };
              const readType = () => { let s = 0; for (let i = 0; i < 100000; i++) s += target.nodeType; return s; };
              const readTag = () => { let s = 0; for (let i = 0; i < 100000; i++) s += target.tagName.length; return s; };

              return {fwd, bwd, count: host.childElementCount};
            })()"""
        )
        check("Forward and post-mutation backward nextSibling/previousSibling walks over 16,384 children stay exact", (res["fwd"], res["bwd"], res["count"]) == (16384, 16384, 16384), res)

        ctx.eval(
            """
            globalThis.host = document.getElementById('host');
            globalThis.target = host.firstChild;
            globalThis.readType = () => { let s = 0; for (let i = 0; i < 100000; i++) s += target.nodeType; return s; };
            globalThis.readTag = () => { let s = 0; for (let i = 0; i < 100000; i++) s += target.tagName.length; return s; };
            void 0;
            """
        )
        ctx.eval("host.insertBefore(target, host.firstChild); void 0;")
        t0 = time.perf_counter()
        assert ctx.eval("readType()") == 100000
        first_type_ms = (time.perf_counter() - t0) * 1000

        ctx.eval("host.appendChild(target); void 0;")
        t0 = time.perf_counter()
        assert ctx.eval("readType()") == 100000
        last_type_ms = (time.perf_counter() - t0) * 1000

        t0 = time.perf_counter()
        assert ctx.eval("readTag()") == 100000
        last_tag_ms = (time.perf_counter() - t0) * 1000

        print(f"     100k nodeType reads @16k siblings: first={first_type_ms:.1f} ms, last={last_type_ms:.1f} ms; tagName last={last_tag_ms:.1f} ms")
        check("Last-sibling nodeType/tagName reads do not scale with sibling position", last_type_ms < max(250.0, first_type_ms * 1.8) and last_tag_ms < max(250.0, first_type_ms * 1.8), (first_type_ms, last_type_ms, last_tag_ms))


if __name__ == "__main__":
    test_narrow_getters_and_adoption()
    test_large_sibling_scaling()
    print("node getter checks passed")
