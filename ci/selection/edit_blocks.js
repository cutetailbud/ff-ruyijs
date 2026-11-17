(async () => {
  const engine = new URLSearchParams(location.search).get('engine');
  const rows = [];
  const put = (k, v) => rows.push([k, v]);
  const run = (k, f) => { try { put(k, 'ok ' + JSON.stringify(f())); } catch (e) { put(k, e.name); } };
  document.body.innerHTML = '<div id=h contenteditable></div>';
  const sel = getSelection(), h = document.getElementById('h');
  const name = (n) => n === null ? null : n.nodeName + (n.nodeType === 3 ? ':' + n.data : '');
  const go = (html, path, off, ops) => { h.innerHTML = html; let n = h; for (const i of path) n = n.childNodes[i]; sel.collapse(n, off);
    for (const op of ops) { if (op === '\n') document.execCommand('insertParagraph'); else if (op === '\b') document.execCommand('delete'); else if (op === '\r') document.execCommand('insertLineBreak'); else document.execCommand('insertText', false, op); }
    return [h.innerHTML, name(sel.anchorNode), sel.anchorOffset]; };
  run('enter.mid.text', () => go('abc', [0], 1, ['\n']));
  run('enter.mid.text.type', () => go('abc', [0], 1, ['\n', 'X']));
  run('enter.start.text', () => go('abc', [0], 0, ['\n']));
  run('enter.text.then.div', () => go('ab<div>cd</div>', [0], 2, ['\n']));
  run('enter.div.end.type', () => go('<div>ab</div>', [0, 0], 2, ['\n', 'X']));
  run('enter.inline.mid', () => go('a<b>bc</b>', [1, 0], 1, ['\n']));
  run('bs.br.only', () => go('<div><br></div><div>x</div>', [1, 0], 0, ['\b']));
  run('bs.div.empty', () => go('<div>ab</div><div><br></div>', [1], 0, ['\b']));
  run('bs.from.empty.div.to.text', () => go('ab<div><br></div>', [1], 0, ['\b']));
  run('bs.p.into.p', () => go('<p>ab</p><p>cd</p>', [1, 0], 0, ['\b']));
  run('bs.after.br.text', () => go('ab<br>cd', [2], 0, ['\b']));
  run('bs.trailing.br', () => go('ab<br><br>', [], 2, ['\b']));
  run('bs.at.host.start', () => go('ab', [0], 0, ['\b']));
  run('enter.then.bs', () => go('abc', [0], 3, ['\n', '\b']));
  run('enter.then.bs.type', () => go('abc', [0], 3, ['\n', '\b', 'X']));
  run('type.enter.type.lines', () => go('', [], 0, ['a', '\n', 'b', '\n', 'c']));
  run('bs.lines', () => go('', [], 0, ['a', '\n', 'b', '\b', '\b', '\b']));
  window.__result = JSON.stringify(rows);
  try { await fetch('/report?engine=' + engine, { method: 'POST', body: JSON.stringify(rows) }); } catch (e) {}
})();
