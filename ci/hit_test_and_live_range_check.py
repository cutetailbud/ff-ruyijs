"""Verify CSS hit-testing (pointer-events inheritance, stacking contexts,
transforms, border-radius, ShadowRoot retargeting) and live DOM Range /
Selection boundary updates (#45).
"""

from __future__ import annotations

import json
import sys

import ruyijs


def check(label: str, cond: bool, detail: object = None) -> None:
    if not cond:
        raise AssertionError(f"FAIL {label}: {detail!r}")
    suffix = "" if detail is None else f"  ({detail!r})"
    print(f"ok   {label}{suffix}")


def test_hit_testing() -> None:
    # 1. pointer-events CSS inheritance + descendant pointer-events: auto override (#45 Case A)
    html_pe = """<!doctype html>
    <style>
      html, body { margin: 0; width: 300px; height: 200px; }
      #bg { position: fixed; inset: 0; }
      #wrap { position: fixed; left: 20px; top: 20px; width: 200px; height: 120px; pointer-events: none; }
      #inheritedBtn { display: block; width: 80px; height: 40px; }
      #overrideBtn { display: block; width: 80px; height: 40px; pointer-events: auto; }
    </style>
    <div id="bg"></div>
    <div id="wrap">
      <button id="inheritedBtn">skip</button>
      <button id="overrideBtn">hit</button>
    </div>"""
    with ruyijs.Context() as ctx:
        ctx.load_page("https://hit.test/pe", html_pe)
        res = json.loads(
            ctx.eval(
                """JSON.stringify({
                  wrapPE: getComputedStyle(document.getElementById('wrap')).pointerEvents,
                  inheritedPE: getComputedStyle(document.getElementById('inheritedBtn')).pointerEvents,
                  overridePE: getComputedStyle(document.getElementById('overrideBtn')).pointerEvents,
                  bodyPE: getComputedStyle(document.body).pointerEvents,
                  atInherited: document.elementFromPoint(40, 35)?.id ?? null,
                  allAtInherited: document.elementsFromPoint(40, 35).map(e => e.id || e.tagName),
                  atOverride: document.elementFromPoint(40, 75)?.id ?? null,
                  outNeg: document.elementFromPoint(-5, 10),
                  outFar: document.elementFromPoint(99999, 10),
                })"""
            )
        )
    check(
        "pointer-events inherits 'none' and honours descendant 'auto'",
        res["wrapPE"] == "none"
        and res["inheritedPE"] == "none"
        and res["overridePE"] == "auto"
        and res["bodyPE"] == "auto"
        and res["atInherited"] == "bg"
        and res["allAtInherited"] == ["bg", "BODY", "HTML"]
        and res["atOverride"] == "overrideBtn"
        and res["outNeg"] is None
        and res["outFar"] is None,
        res,
    )

    # 2. Nested stacking contexts, overflow clipping, 2D transform & border-radius (#45 Case B)
    html_sc = """<!doctype html>
    <style>
      html, body { margin: 0; width: 400px; height: 300px; }
      #low { position: fixed; left: 0; top: 0; width: 150px; height: 150px; z-index: 1; }
      #child { position: absolute; left: 10px; top: 10px; width: 100px; height: 100px; z-index: 100; }
      #upper { position: fixed; left: 0; top: 0; width: 150px; height: 150px; z-index: 2; }
      #clipBox { position: fixed; left: 200px; top: 20px; width: 60px; height: 60px; overflow: hidden; }
      #overflowed { width: 140px; height: 40px; }
      #circle { position: fixed; left: 20px; top: 180px; width: 100px; height: 100px; border-radius: 50%; }
      #diamond { position: fixed; left: 180px; top: 180px; width: 80px; height: 80px; transform: rotate(45deg); }
    </style>
    <div id="low"><button id="child">inside z=1</button></div>
    <div id="upper">overlay z=2</div>
    <div id="clipBox"><div id="overflowed">clipped</div></div>
    <div id="circle"></div>
    <div id="diamond"></div>"""
    with ruyijs.Context() as ctx:
        ctx.load_page("https://hit.test/sc", html_sc)
        res_sc = json.loads(
            ctx.eval(
                """JSON.stringify({
                  topAt60: document.elementFromPoint(60, 60)?.id,
                  stackAt60: document.elementsFromPoint(60, 60).map(e => e.id || e.tagName),
                  inClip: document.elementFromPoint(220, 35)?.id,
                  outClip: document.elementFromPoint(290, 35)?.id || document.elementFromPoint(290, 35)?.tagName,
                  circleCenter: document.elementFromPoint(70, 230)?.id,
                  circleCorner: document.elementFromPoint(23, 183)?.id || document.elementFromPoint(23, 183)?.tagName,
                  diamondCenter: document.elementFromPoint(220, 220)?.id,
                  diamondTip: document.elementFromPoint(220, 168)?.id,
                  diamondCornerMiss: document.elementFromPoint(182, 182)?.id || document.elementFromPoint(182, 182)?.tagName,
                })"""
            )
        )
    check(
        "nested stacking context keeps z=100 child below sibling z=2 overlay",
        res_sc["topAt60"] == "upper" and res_sc["stackAt60"] == ["upper", "child", "low", "BODY", "HTML"],
        res_sc["stackAt60"],
    )
    check(
        "overflow clip, border-radius ellipse, and 2D transform inverse hit-test accurately",
        res_sc["inClip"] == "overflowed"
        and res_sc["outClip"] == "BODY"
        and res_sc["circleCenter"] == "circle"
        and res_sc["circleCorner"] == "BODY"
        and res_sc["diamondCenter"] == "diamond"
        and res_sc["diamondTip"] == "diamond"
        and res_sc["diamondCornerMiss"] == "BODY",
        res_sc,
    )

    # 3. ShadowRoot.elementFromPoint / elementsFromPoint & Document light-DOM retargeting
    with ruyijs.Context() as ctx:
        ctx.load_page(
            "https://hit.test/shadow",
            "<!doctype html><style>html,body{margin:0}#host{width:120px;height:80px}</style><div id='host'></div>",
        )
        res_sr = json.loads(
            ctx.eval(
                """JSON.stringify((() => {
                  const host = document.getElementById('host');
                  const sr = host.attachShadow({ mode: 'open' });
                  sr.innerHTML = '<div id="inner" style="width:100px;height:60px">shadow</div>';
                  return {
                    docHit: document.elementFromPoint(30, 30)?.id,
                    srHit: sr.elementFromPoint(30, 30)?.id,
                    srAll: sr.elementsFromPoint(30, 30).map(e => e.id),
                  };
                })())"""
            )
        )
    check(
        "Document retargets shadow hits to host while ShadowRoot returns shadow element",
        res_sr == {"docHit": "host", "srHit": "inner", "srAll": ["inner"]},
        res_sr,
    )


def test_live_range_and_selection() -> None:
    probe = r"""(() => {
      const host = document.createElement('div');
      host.id = 'host';
      document.body.appendChild(host);
      const nm = (n) => {
        if (!n) return 'null';
        if (n.nodeType === 3) return '#text(' + JSON.stringify(n.data) + ')';
        if (n.nodeType === 8) return '#comment(' + JSON.stringify(n.data) + ')';
        if (n.nodeType === 11) return '#frag';
        return n.tagName + (n.id ? '#' + n.id : '');
      };
      const desc = (r) => [nm(r.startContainer), r.startOffset, nm(r.endContainer), r.endOffset, r.collapsed, r.toString()];
      const out = {};

      // #45 exact reproduction: 'abc' [1,3] ('bc') + insertData(0, 'X') -> [2,4] ('bc')
      host.innerHTML = 'abc';
      {
        const t = host.firstChild;
        const r = document.createRange();
        r.setStart(t, 1); r.setEnd(t, 3);
        const sel = getSelection();
        sel.removeAllRanges();
        sel.addRange(r);
        t.insertData(0, 'X');
        out.issue45 = {
          start: r.startOffset, end: r.endOffset, text: r.toString(),
          selText: sel.toString(), anchorOff: sel.anchorOffset, focusOff: sel.focusOffset,
        };
        sel.removeAllRanges();
      }

      // CharacterData deleteData straddling start & .data setter collapse
      host.innerHTML = 'abcdef';
      {
        const t = host.firstChild;
        const r = document.createRange(); r.setStart(t, 2); r.setEnd(t, 5);
        t.deleteData(1, 2);
        const afterDel = desc(r);
        t.data = 'z';
        out.cdata = { afterDel, afterSet: desc(r) };
      }

      // Text.splitText (attached & detached)
      host.innerHTML = 'abcdef';
      {
        const t = host.firstChild;
        const rIn = document.createRange(); rIn.setStart(t, 1); rIn.setEnd(t, 5);
        const rAfter = document.createRange(); rAfter.setStart(host, 1); rAfter.setEnd(host, 1);
        t.splitText(3);
        const dt = document.createTextNode('abcdef');
        const rDet = document.createRange(); rDet.setStart(dt, 4); rDet.setEnd(dt, 6);
        dt.splitText(2);
        out.split = { rIn: desc(rIn), rAfter: desc(rAfter), rDet: desc(rDet) };
      }

      // Tree insertBefore / appendChild reorder / removeChild / replaceChild / ChildNode.replaceWith / replaceChildren
      host.innerHTML = '<span id=a>A</span><span id=b>B</span><span id=c>C</span>';
      {
        const r = document.createRange(); r.setStart(host, 1); r.setEnd(host, 3);
        const x = document.createElement('em'); x.id = 'x'; x.textContent = 'X';
        host.insertBefore(x, host.children[1]);
        const afterIns = desc(r);
        host.firstChild.before(host.lastChild);
        const afterMove = desc(r);
        host.children[2].remove();
        const afterRem = desc(r);
        host.replaceChildren(document.createTextNode('only'));
        out.tree = { afterIns, afterMove, afterRem, afterReplaceChildren: desc(r) };
      }

      // Node.normalize(): merges adjacent Texts, preserves Comments, shifts Ranges,
      // AND keeps detached merged Text identities valid for MutationObserver.removedNodes!
      {
        host.innerHTML = '';
        const t1 = host.appendChild(document.createTextNode('ab'));
        const t2 = host.appendChild(document.createTextNode(''));
        const t3 = host.appendChild(document.createTextNode('cd'));
        const cm = host.appendChild(document.createComment('keep'));
        const t4 = host.appendChild(document.createTextNode('ef'));
        const r = document.createRange(); r.setStart(t1, 1); r.setEnd(t3, 1);
        let removedInfo = null;
        const mo = new MutationObserver((records) => {
          removedInfo = records.flatMap(rec => Array.from(rec.removedNodes).map(n => [n.nodeType, n.data, n.parentNode === null]));
        });
        mo.observe(host, { childList: true });
        host.normalize();
        out.normBeforeMicrotask = { r: desc(r), childCount: host.childNodes.length, commentKept: host.childNodes[1] === cm };
        Promise.resolve().then(() => {
          out.normRemovedNodes = removedInfo;
          mo.disconnect();
        });
      }

      // Range.insertNode / deleteContents / extractContents / surroundContents
      {
        const box = document.createElement('div');
        document.body.appendChild(box);
        box.innerHTML = '<span id=s1>ab</span><i id=mid>X</i><span id=s2>cd</span>';
        const r1 = document.createRange();
        r1.setStart(box.firstChild.firstChild, 1);
        r1.setEnd(box.lastChild.firstChild, 1);
        const r2 = document.createRange();
        r2.setStart(box, 2); r2.setEnd(box, 3);
        r1.deleteContents();
        out.rangeDel = { html: box.innerHTML, r1: desc(r1), r2: desc(r2) };

        box.innerHTML = 'hello world';
        const t = box.firstChild;
        const rs = document.createRange(); rs.setStart(t, 6); rs.setEnd(t, 11);
        const mark = document.createElement('mark'); mark.id = 'm';
        rs.surroundContents(mark);
        out.surround = { html: box.innerHTML, rs: desc(rs) };
      }

      globalThis.__rangeOut = out;
    })()"""

    with ruyijs.Context() as ctx:
        ctx.load_page("https://range.test/", "<!doctype html><body></body>")
        ctx.eval(probe)
        ctx.advance(10, max_tasks=100)
        res = json.loads(ctx.eval("JSON.stringify(globalThis.__rangeOut)"))

    check(
        "#45 CharacterData.insertData shifts live Range & Selection from [1,3] to [2,4] ('bc')",
        res["issue45"]
        == {"start": 2, "end": 4, "text": "bc", "selText": "bc", "anchorOff": 2, "focusOff": 4},
        res["issue45"],
    )
    check(
        "CharacterData.deleteData & .data replacement update live Range offsets",
        res["cdata"]["afterDel"] == ['#text("adef")', 1, '#text("adef")', 3, False, "de"]
        and res["cdata"]["afterSet"] == ['#text("z")', 0, '#text("z")', 0, True, ""],
        res["cdata"],
    )
    check(
        "Text.splitText updates spanning, parent-boundary, and detached Ranges",
        res["split"]["rIn"] == ['#text("abc")', 1, '#text("def")', 2, False, "bcde"]
        and res["split"]["rAfter"] == ["DIV#host", 2, "DIV#host", 2, True, ""]
        and res["split"]["rDet"] == ['#text("ab")', 2, '#text("ab")', 2, True, ""],
        res["split"],
    )
    check(
        "DOM tree insertions, sibling reorders, removals, and replaceChildren update live Ranges",
        res["tree"]["afterIns"] == ["DIV#host", 1, "DIV#host", 4, False, "XBC"]
        and res["tree"]["afterMove"] == ["DIV#host", 2, "DIV#host", 4, False, "XB"]
        and res["tree"]["afterRem"] == ["DIV#host", 2, "DIV#host", 3, False, "B"]
        and res["tree"]["afterReplaceChildren"] == ["DIV#host", 0, "DIV#host", 0, True, ""],
        res["tree"],
    )
    check(
        "Node.normalize() merges Texts, preserves Comments, shifts Ranges, and keeps removedNodes alive for MutationObserver",
        res["normBeforeMicrotask"]
        == {
            "r": ['#text("abcd")', 1, '#text("abcd")', 3, False, "bc"],
            "childCount": 3,
            "commentKept": True,
        }
        and res["normRemovedNodes"] == [[3, "", True], [3, "cd", True]],
        (res["normBeforeMicrotask"], res["normRemovedNodes"]),
    )
    check(
        "Range.deleteContents and surroundContents match WHATWG live Range rules",
        res["rangeDel"]
        == {
            "html": '<span id="s1">a</span><span id="s2">d</span>',
            "r1": ["DIV", 1, "DIV", 1, True, ""],
            "r2": ["DIV", 1, "DIV", 2, False, "d"],
        }
        and res["surround"]
        == {
            "html": 'hello <mark id="m">world</mark>',
            "rs": ["DIV", 1, "DIV", 2, False, "world"],
        },
        (res["rangeDel"], res["surround"]),
    )


def main() -> int:
    test_hit_testing()
    test_live_range_and_selection()
    print("hit_test_and_live_range_check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
