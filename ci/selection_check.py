"""Check Selection, contenteditable editing and IndexedDB upgrade writes (issue #44).

Three parts, all offline:

1. ci/selection/*.js are probes whose rows were captured in Firefox 160
   (ci/selection/firefox160.json): the Selection API, Selection.toString(),
   multi-range selections, and the editing commands in a contenteditable host
   (typed text and spaces, Backspace, Delete, Enter, line breaks). Every row
   has to equal Firefox's, except the rows in KNOWN_GAPS, which are printed
   but do not fail the check.
2. Context.type() into an input, a textarea and a contenteditable element:
   the text arrives and each character fires beforeinput and input.
3. A put() on a store created in onupgradeneeded succeeds, is readable after
   the open succeeds, and an aborted upgrade leaves no database behind.

Exit status is non-zero on failure.
"""
import json
import sys
import uuid
from pathlib import Path

from ruyijs import Context

HERE = Path(__file__).resolve().parent / "selection"
# Rows that differ from Firefox 160 today, by probe.
KNOWN_GAPS = {
    # queryCommandEnabled/Supported for commands this runtime does not run
    # (bold, selectAll, undo).
    "edit_keys": {"queryCommand"},
    # A focused <input>'s own selection is not the window's Selection.
    "text": {"ce.input"},
    # extend() over several ranges: the serialized text of the result.
    "ranges": {"extend.multi"},
}
PAGE = "<!doctype html><html lang=en><meta charset=utf-8><body></body>"
failed = 0


def report(ok, text):
    global failed
    failed += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {text}")


def probe(name, expected):
    with Context(timeout=30) as ctx:
        ctx.load_page("https://example.test/?engine=ruyijs", PAGE)
        # The probes report over fetch when a server collects them; here the
        # rows are read from the page.
        ctx.eval("window.fetch = async () => ({}); 0")
        ctx.eval((HERE / f"{name}.js").read_text(encoding="utf-8") + ";0")
        for _ in range(50):
            if ctx.eval("!!window.__result"):
                break
            ctx.advance(100, max_tasks=500)
        rows = dict(json.loads(ctx.eval("window.__result || '[]'")))
    # The captures are from Windows, where Selection.toString() breaks lines
    # with CR LF; a macOS or Linux profile serializes LF. The rows are JSON
    # text, so the breaks are the escaped sequences.
    rows = {key: value.replace(r"\r\n", r"\n") for key, value in rows.items()}
    expected = [[key, value.replace(r"\r\n", r"\n")] for key, value in expected]
    gaps = KNOWN_GAPS.get(name, set())
    bad = [key for key, value in expected if rows.get(key) != value and key not in gaps]
    known = [key for key, value in expected if rows.get(key) != value and key in gaps]
    report(not bad, f"{name}: {len(expected) - len(bad) - len(known)}/{len(expected)} rows equal Firefox 160"
           + (f" ({len(known)} known gaps: {', '.join(known)})" if known else ""))
    for key in bad:
        print(f"       {key}\n         firefox: {dict(expected)[key]}\n         ruyijs:  {rows.get(key)}")


for name, expected in json.loads((HERE / "firefox160.json").read_text(encoding="utf-8")).items():
    probe(name, expected)

with Context(timeout=30) as ctx:
    ctx.load_page("https://example.test/", '<!doctype html><input id="plain"><textarea id="area"></textarea>'
                  '<div id="rich" contenteditable="true"><p><br></p></div>'
                  '<div id="edit" contenteditable="true"><p>abc</p></div>')
    ctx.eval("""globalThis.events = [];
      for (const type of ['beforeinput', 'input'])
        document.addEventListener(type, e => events.push([e.target.id, e.type, e.isTrusted, e.inputType]));0""")
    for target in ["plain", "area", "rich"]:
        ctx.type("你好abc", target=f"document.getElementById({json.dumps(target)})", interval_ms=5)
        ctx.advance(50, max_tasks=200)
        ctx.set("targetId", target)
        text, before, after, trusted = json.loads(ctx.eval("""JSON.stringify((() => {
          const e = document.getElementById(targetId), mine = events.filter(x => x[0] === targetId);
          return ['value' in e ? e.value : e.textContent, mine.filter(x => x[1] === 'beforeinput').length,
                  mine.filter(x => x[1] === 'input').length, mine.every(x => x[2] && x[3] === 'insertText')];
        })())"""))
        report(text == "你好abc" and before == 5 and after == 5 and trusted,
               f"type into #{target}: {text!r}, {before} beforeinput, {after} input")
    # Typing replaces the selected text; a canceled beforeinput inserts nothing.
    ctx.eval("""(() => { const t = document.querySelector('#edit p').firstChild;
      getSelection().setBaseAndExtent(t, 0, t, 2); })()""")
    ctx.type("X", target="document.getElementById('edit')", interval_ms=5)
    replaced = ctx.eval("document.getElementById('edit').innerHTML")
    ctx.eval("document.getElementById('edit').addEventListener('beforeinput', e => e.preventDefault()); 0")
    ctx.type("Y", target="document.getElementById('edit')", interval_ms=5)
    canceled = ctx.eval("document.getElementById('edit').innerHTML")
    report(replaced == "<p>Xc</p>" and canceled == replaced,
           f"typing replaces the selection ({replaced}); a canceled beforeinput inserts nothing ({canceled})")
    # A live NodeList follows a rebuilt subtree: it never hands out the node
    # that used to be at an index (the probes rebuild their host per row).
    live = ctx.eval("""(() => { const box = document.createElement('div'); document.body.append(box);
      box.innerHTML = 'ab'; const list = box.childNodes, old = list[0]; box.innerHTML = 'cd<i></i>';
      const grown = list.length === 2 && list[0] === box.firstChild && list[0].data === 'cd' && !old.isConnected;
      box.innerHTML = ''; return grown && list.length === 0 && list[0] === undefined; })()""")
    report(live is True, "childNodes follows an innerHTML rebuild")
    same = ctx.eval("document.getSelection() === window.getSelection() && "
                    "new DOMParser().parseFromString('<p>', 'text/html').getSelection() === null")
    report(same is True, "document.getSelection() is the window's Selection; a windowless document has none")

with Context(timeout=30) as ctx:
    ctx.load_page("https://example.test/", PAGE)
    ctx.set("databaseName", "selection-check-" + uuid.uuid4().hex)
    ctx.eval("""(() => {
      window.steps = []; window.done = false;
      const request = indexedDB.open(databaseName, 1);
      request.onupgradeneeded = () => {
        const store = request.result.createObjectStore('items');
        store.createIndex('byKind', 'kind');
        steps.push('upgrade:' + request.transaction.mode);
        store.put({kind: 'a', n: 1}, 'k1').onsuccess = () => steps.push('put1');
        store.add({kind: 'b', n: 2}, 'k2').onsuccess = () => {
          steps.push('put2');
          store.get('k1').onsuccess = e => steps.push('get:' + e.target.result.n);
          store.index('byKind').count('b').onsuccess = e => steps.push('count:' + e.target.result);
        };
        request.transaction.oncomplete = () => steps.push('complete');
      };
      request.onerror = () => { steps.push('error:' + request.error.name); done = true; };
      request.onsuccess = () => {
        steps.push('success');
        const db = request.result;
        db.transaction('items').objectStore('items').getAllKeys().onsuccess = e => {
          steps.push('keys:' + e.target.result.join('+'));
          db.close();
          // An upgrade that aborts after a put leaves nothing behind.
          const second = indexedDB.open(databaseName + '-aborted', 1);
          second.onupgradeneeded = () => {
            second.result.createObjectStore('items').put('v', 'k').onsuccess = () => {
              steps.push('aborted-put'); second.transaction.abort(); };
          };
          second.onsuccess = () => { steps.push('aborted-success'); done = true; };
          second.onerror = e => {
            e.preventDefault();
            steps.push('aborted:' + second.error.name);
            indexedDB.databases().then(list => {
              steps.push('left:' + list.filter(d => d.name === databaseName + '-aborted').length); done = true; });
          };
        };
      };
    })()""")
    for _ in range(100):
        if ctx.eval("done"):
            break
        ctx.advance(50, max_tasks=200)
    steps = ctx.eval("steps.join(' ')")
    want = ("upgrade:versionchange put1 put2 get:1 count:1 complete success keys:k1+k2 "
            "aborted-put aborted:AbortError left:0")
    report(steps == want, f"IndexedDB upgrade writes: {steps}")

sys.exit(1 if failed else 0)
