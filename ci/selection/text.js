(async () => {
  const engine = new URLSearchParams(location.search).get('engine');
  const rows = [];
  const put = (k, v) => rows.push([k, v]);
  const run = (k, f) => { try { put(k, 'ok ' + JSON.stringify(f())); } catch (e) { put(k, e.name + ' ' + e.message); } };
  const box = document.createElement('div'); document.body.append(box);
  const sel = getSelection();
  const t = (html) => { box.innerHTML = html; sel.selectAllChildren(box); return sel.toString(); };
  for (const [k, h] of [['div2', '<div>a</div><div>b</div>'], ['p2', '<p>a</p><p>b</p>'], ['br', 'a<br>b'], ['brend', 'a<br>'],
      ['text.div', 'a<div>b</div>c'], ['li', '<ul><li>a</li><li>b</li></ul>'], ['h1p', '<h1>a</h1><p>b</p>'], ['span', 'a<span>b</span>c'],
      ['nested', '<div><div>a</div></div><div>b</div>'], ['ws', '  a \n  b  '], ['nbsp', 'a&nbsp;b'], ['pre', '<pre>a\n b</pre>'],
      ['table', '<table><tr><td>a</td><td>b</td></tr><tr><td>c</td></tr></table>'], ['hidden', 'a<span style="display:none">x</span>b'],
      ['script', 'a<script>x<\/script>b'], ['tab', 'a\tb'], ['img', 'a<img alt=x>b'], ['blockquote', '<blockquote>a</blockquote>b'],
      ['p.div', '<p>a</p><div>b</div>'], ['empty.p', '<p>a</p><p></p><p>b</p>'], ['input', 'a<input value=x>b'], ['textarea', 'a<textarea>x</textarea>b']])
    run(k, () => t(h));
  run('partial', () => { box.innerHTML = '<p>abc</p><p>def</p>'; sel.setBaseAndExtent(box.firstChild.firstChild, 1, box.lastChild.firstChild, 1); return sel.toString(); });
  run('partial.end.block', () => { box.innerHTML = '<p>abc</p><p>def</p>'; sel.setBaseAndExtent(box.firstChild.firstChild, 1, box.lastChild, 0); return sel.toString(); });
  run('ce.input', () => { box.innerHTML = '<input value=xyz>'; const i = box.firstChild; i.focus(); i.setSelectionRange(0, 2); return [sel.toString(), sel.rangeCount, sel.type]; });
  run('multi', () => { box.innerHTML = '<p>abc</p><p>def</p>'; sel.removeAllRanges(); for (const [n, a, b] of [[box.firstChild.firstChild, 0, 1], [box.lastChild.firstChild, 1, 2]]) { const r = document.createRange(); r.setStart(n, a); r.setEnd(n, b); sel.addRange(r); } return sel.toString(); });
  run('platform', () => navigator.platform);
  window.__result = JSON.stringify(rows);
  try { await fetch('/report?engine=' + engine, { method: 'POST', body: JSON.stringify(rows) }); } catch (e) {}
})();
