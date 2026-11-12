"""document.getElementsByTagNameNS returns the same live collection elements use."""
from ruyijs import Context


def check(label, condition, detail=""):
    print(("ok   " if condition else "FAIL ") + label + (f"  ({detail})" if detail != "" else ""))
    if not condition:
        raise SystemExit(1)


def test_document_get_elements_by_tag_name_ns():
    xhtml = "http://www.w3.org/1999/xhtml"
    svg = "http://www.w3.org/2000/svg"
    mathml = "http://www.w3.org/1998/Math/MathML"
    with Context(timeout=60) as ctx:
        ctx.load_page(
            "https://offline.invalid/tag-ns/",
            "<!doctype html><html><body>"
            "<div id=a></div><div id=b></div>"
            '<svg xmlns="http://www.w3.org/2000/svg"><circle id=c></circle></svg>'
            "<math><mi>x</mi></math>"
            "</body></html>",
        )
        found = ctx.eval(
            f"""(() => {{
              const xhtml = {xhtml!r};
              const svgNs = {svg!r};
              const math = {mathml!r};
              const divs = document.getElementsByTagNameNS(xhtml, 'div');
              const circles = document.getElementsByTagNameNS(svgNs, 'circle');
              return {{
                docLen: divs.length,
                elLen: document.body.getElementsByTagNameNS(xhtml, 'div').length,
                svgLen: circles.length,
                mathLen: document.getElementsByTagNameNS(math, 'mi').length,
                anyLen: document.getElementsByTagNameNS('*', 'div').length,
                emptyLen: document.getElementsByTagNameNS('', 'div').length,
                docIsCollection: typeof divs.item === 'function',
                sameObject: divs === document.getElementsByTagNameNS(xhtml, 'div'),
              }};
            }})()"""
        )
        check("Document.getElementsByTagNameNS finds the two divs", found["docLen"] == 2, found)
        check("Element.getElementsByTagNameNS finds the two divs", found["elLen"] == 2, found)
        check("Document.getElementsByTagNameNS finds the SVG circle", found["svgLen"] == 1, found)
        check("Document.getElementsByTagNameNS finds the MathML mi", found["mathLen"] == 1, found)
        check("a wildcard namespace matches the divs", found["anyLen"] == 2, found)
        check("an empty namespace does not match XHTML divs", found["emptyLen"] == 0, found)
        check("the document method returns a collection", found["docIsCollection"] is True and found["sameObject"] is True, found)

        live = ctx.eval(
            f"""(() => {{
              const xhtml = {xhtml!r};
              const svgNs = {svg!r};
              const divs = document.getElementsByTagNameNS(xhtml, 'div');
              const circles = document.getElementsByTagNameNS(svgNs, 'circle');
              const fromElement = document.body.getElementsByTagNameNS(xhtml, 'div');
              const before = {{divs: divs.length, circles: circles.length, element: fromElement.length}};
              document.body.appendChild(document.createElement('div'));
              document.querySelector('svg').appendChild(document.createElementNS(svgNs, 'circle'));
              const after = {{divs: divs.length, circles: circles.length, element: fromElement.length}};
              document.body.lastElementChild.remove();
              return {{before, after, removed: divs.length}};
            }})()"""
        )
        check(
            "Document and Element div collections grow when a div is appended",
            live["before"]["divs"] == 2 and live["after"]["divs"] == 3 and live["after"]["element"] == 3,
            live,
        )
        check("the SVG collection grows when a circle is appended", live["before"]["circles"] == 1 and live["after"]["circles"] == 2, live)
        check("removing the extra div shrinks the document collection", live["removed"] == 2, live)
    print("tag name NS checks passed")


if __name__ == "__main__":
    test_document_get_elements_by_tag_name_ns()
