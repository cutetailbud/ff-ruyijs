(async () => {
  const engine = new URLSearchParams(location.search).get('engine');
  const rows = [];
  const put = (k, v) => rows.push([k, v]);
  const run = (k, f) => { try { put(k, 'ok ' + JSON.stringify(f())); } catch (e) { put(k, (e.constructor && e.constructor.name) + '|' + e.name + '|' + e.message); } };
  document.body.innerHTML = '<p id=p1>abc</p><p>def</p>';
  const sel = getSelection(), t = document.getElementById('p1').firstChild;
  run('bidi.none', () => sel.caretBidiLevel);
  run('composed.none', () => sel.getComposedRanges());
  sel.setBaseAndExtent(t, 2, t, 1);
  run('bidi', () => sel.caretBidiLevel);
  run('bidi.set', () => { sel.caretBidiLevel = 1; return sel.caretBidiLevel; });
  run('composed', () => sel.getComposedRanges().map((r) => [r.constructor.name, r.startContainer === t, r.startOffset, r.endOffset, r === sel.getRangeAt(0)]));
  run('composed.opts', () => sel.getComposedRanges({ shadowRoots: [] }).length);
  run('composed.legacy', () => sel.getComposedRanges(document.body).length);
  run('lengths', () => ['getRangeAt','addRange','removeRange','removeAllRanges','empty','getComposedRanges','collapse','setPosition','collapseToStart','collapseToEnd','extend','setBaseAndExtent','selectAllChildren','modify','deleteFromDocument','containsNode','toString'].map((m) => m + ':' + Selection.prototype[m].length).join(','));
  run('desc.direction', () => { const d = Object.getOwnPropertyDescriptor(Selection.prototype, 'direction'); return [typeof d.get, typeof d.set, d.enumerable]; });
  run('desc.bidi', () => { const d = Object.getOwnPropertyDescriptor(Selection.prototype, 'caretBidiLevel'); return [typeof d.get, typeof d.set, d.enumerable]; });
  run('collapse.noargs', () => sel.collapse());
  run('collapse.undef', () => { sel.collapse(undefined); return sel.rangeCount; });
  run('addRange.bad', () => sel.addRange({}));
  run('containsNode.null', () => sel.containsNode(null));
  run('extend.foreign', () => { sel.collapse(t, 1); const d = document.implementation.createHTMLDocument(''); sel.extend(d.body, 0); return sel.rangeCount; });
  run('modify.args', () => { sel.collapse(t, 0); sel.modify('move', 'forward', 'character'); return [sel.anchorOffset]; });
  run('modify.bad', () => { sel.collapse(t, 0); sel.modify('bogus', 'forward', 'character'); return [sel.anchorOffset]; });
  window.__result = JSON.stringify(rows);
  try { await fetch('/report?engine=' + engine, { method: 'POST', body: JSON.stringify(rows) }); } catch (e) {}
})();
