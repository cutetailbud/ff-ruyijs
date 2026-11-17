(async () => {
  const engine = new URLSearchParams(location.search).get('engine');
  const rows = [];
  const put = (k, v) => rows.push([k, v]);
  const run = (k, f) => { try { put(k, 'ok ' + JSON.stringify(f())); } catch (e) { put(k, e.name); } };
  document.body.innerHTML = '<div id=h contenteditable></div>';
  const sel = getSelection(), h = document.getElementById('h');
  const S = (s) => s.replace(/ /g, '~');
  const seq = (html, ops) => { h.innerHTML = html; const last = h.lastChild; sel.collapse(last, last.textContent.length);
    let out = []; for (const op of ops) { if (op === '\n') document.execCommand('insertParagraph'); else if (op === '\b') document.execCommand('delete'); else document.execCommand('insertText', false, op); }
    return S(h.innerHTML); };
  for (let n = 1; n <= 6; n++) run('n' + n + '.end', () => seq('ab', Array(n).fill(' ')));
  for (let n = 1; n <= 6; n++) run('n' + n + '.then', () => seq('ab', [...Array(n).fill(' '), 'c']));
  run('mid.one', () => seq('abc', [' ', ]));
  run('start.one', () => { h.innerHTML = 'ab'; sel.collapse(h.firstChild, 0); document.execCommand('insertText', false, ' '); return S(h.innerHTML); });
  run('start.then', () => { h.innerHTML = 'ab'; sel.collapse(h.firstChild, 0); document.execCommand('insertText', false, ' '); document.execCommand('insertText', false, 'c'); return S(h.innerHTML); });
  run('mid.space.then', () => { h.innerHTML = 'ab'; sel.collapse(h.firstChild, 1); document.execCommand('insertText', false, ' '); document.execCommand('insertText', false, 'c'); return S(h.innerHTML); });
  run('del.after', () => { h.innerHTML = 'ab'; sel.collapse(h.firstChild, 2); document.execCommand('insertText', false, ' '); document.execCommand('delete'); return S(h.innerHTML); });
  run('cjk.space', () => { h.innerHTML = '你好'; sel.collapse(h.firstChild, 2); document.execCommand('insertText', false, ' '); document.execCommand('insertText', false, 'a'); return S(h.innerHTML); });
  window.__result = JSON.stringify(rows);
  try { await fetch('/report?engine=' + engine, { method: 'POST', body: JSON.stringify(rows) }); } catch (e) {}
})();
