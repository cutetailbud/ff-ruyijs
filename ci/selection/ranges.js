(async () => {
  const engine = new URLSearchParams(location.search).get('engine');
  const rows = [];
  const put = (k, v) => rows.push([k, v]);
  const run = (k, f) => { try { put(k, 'ok ' + JSON.stringify(f())); } catch (e) { put(k, e.name); } };
  document.body.innerHTML = '<p id=p1>abc</p><p id=p2>def</p><p id=p3>ghi</p>';
  const t = [1, 2, 3].map((i) => document.getElementById('p' + i).firstChild);
  const sel = getSelection();
  const name = (n) => n === null ? null : n.nodeName + (n.nodeType === 3 ? ':' + n.data : '');
  const st = () => [sel.rangeCount, name(sel.anchorNode), sel.anchorOffset, name(sel.focusNode), sel.focusOffset, sel.toString()];
  const R = (n, a, b) => { const r = document.createRange(); r.setStart(n, a); r.setEnd(n, b); return r; };
  run('order', () => { sel.removeAllRanges(); const a = R(t[2], 0, 1), b = R(t[0], 0, 1); sel.addRange(a); sel.addRange(b);
    return [st(), sel.getRangeAt(0) === b, sel.getRangeAt(1) === a]; });
  run('overlap', () => { sel.removeAllRanges(); const a = R(t[0], 0, 2), b = R(t[0], 1, 3); sel.addRange(a); sel.addRange(b);
    return [st(), sel.getRangeAt(0) === a]; });
  run('dup.collapsed', () => { sel.removeAllRanges(); const a = R(t[0], 1, 1), b = R(t[0], 1, 1); sel.addRange(a); sel.addRange(b); return st(); });
  run('collapse.multi', () => { sel.removeAllRanges(); sel.addRange(R(t[0], 0, 1)); sel.addRange(R(t[2], 0, 1)); sel.collapseToStart(); return st(); });
  run('collapseToEnd.multi', () => { sel.removeAllRanges(); sel.addRange(R(t[2], 0, 1)); sel.addRange(R(t[0], 0, 1)); sel.collapseToEnd(); return st(); });
  run('extend.multi', () => { sel.removeAllRanges(); sel.addRange(R(t[0], 0, 1)); sel.addRange(R(t[2], 0, 1)); sel.extend(t[1], 2); return st(); });
  run('remove.first', () => { sel.removeAllRanges(); const a = R(t[0], 0, 1), b = R(t[2], 0, 1); sel.addRange(a); sel.addRange(b); sel.removeRange(a); return st(); });
  run('remove.primary', () => { sel.removeAllRanges(); const a = R(t[0], 0, 1), b = R(t[2], 0, 1); sel.addRange(a); sel.addRange(b); sel.removeRange(b); return st(); });
  run('delete.multi', () => { document.body.innerHTML = '<p>abc</p><p>def</p>'; const u = [...document.querySelectorAll('p')].map((p) => p.firstChild);
    sel.removeAllRanges(); sel.addRange(R(u[0], 0, 1)); sel.addRange(R(u[1], 1, 2)); sel.deleteFromDocument(); return [st(), document.body.innerHTML]; });
  run('backward.then.add', () => { document.body.innerHTML = '<p>abc</p>'; const u = document.querySelector('p').firstChild; sel.setBaseAndExtent(u, 2, u, 0);
    const before = [sel.anchorOffset, sel.direction]; sel.addRange(R(u, 2, 3)); return [before, st(), sel.direction]; });
  window.__result = JSON.stringify(rows);
  try { await fetch('/report?engine=' + engine, { method: 'POST', body: JSON.stringify(rows) }); } catch (e) {}
})();
