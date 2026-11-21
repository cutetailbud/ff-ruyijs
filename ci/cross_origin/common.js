function __fmt(v) {
  if (v === undefined) return 'undefined';
  if (v === null) return 'null';
  if (typeof v === 'object' || typeof v === 'function') return typeof v;
  return JSON.stringify(v);
}
function __run(rows, name, fn) {
  let v;
  try { v = __fmt(fn()); } catch (e) { v = 'THROW ' + (e && e.name) + ': ' + (e && e.message); }
  rows.push([name, v]);
}
function __desc(d) {
  if (!d) return 'none';
  return ['value' in d ? 'value:' + (d.value === undefined ? 'undefined' : typeof d.value) : '',
          'get' in d ? 'get:' + typeof d.get : '', 'set' in d ? 'set:' + typeof d.set : '',
          'e' + (d.enumerable ? 1 : 0), 'c' + (d.configurable ? 1 : 0),
          'writable' in d ? 'w' + (d.writable ? 1 : 0) : ''].filter(Boolean).join(' ');
}
function __xo(rows, prefix, w, names) {
  const R = (n, f) => __run(rows, prefix + n, f);
  R('document', () => w.document);
  R('secret', () => w.secret);
  R('length', () => w.length);
  R('[0]===frames[0]', () => w[0] === w.frames[0]);
  R('typeof [0]', () => typeof w[0]);
  R('["0"]===[0]', () => w['0'] === w[0]);
  R('[0].document', () => w[0].document);
  R('[1].leafVar', () => w[1].leafVar);
  R('[2]', () => w[2]);
  R('[-1]', () => w[-1]);
  R('["01"]', () => w['01']);
  R('[1.5]', () => w[1.5]);
  R('[4294967295]', () => w[4294967295]);
  for (const n of names) {
    R('.' + n, () => typeof w[n]);
    R('.' + n + '===[0]', () => w[n] === w[0]);
    R('.' + n + '===[1]', () => w[n] === w[1]);
    R('gOPD ' + n, () => __desc(Object.getOwnPropertyDescriptor(w, n)));
    R('has ' + n, () => n in w);
  }
  R('ownKeys', () => Reflect.ownKeys(w).map(String).join(','));
  R('getOwnPropertyNames', () => Object.getOwnPropertyNames(w).join(','));
  R('Object.keys', () => Object.keys(w).join(','));
  for (const k of ['0', '1', '2', 'postMessage', 'location', 'length', 'then', 'secret'])
    R('gOPD ' + k, () => __desc(Object.getOwnPropertyDescriptor(w, k)));
  R('gOPD @@toStringTag', () => __desc(Object.getOwnPropertyDescriptor(w, Symbol.toStringTag)));
  R('gOPD @@iterator', () => __desc(Object.getOwnPropertyDescriptor(w, Symbol.iterator)));
  for (const k of ['0', '1', '2', 'then', 'secret', 'postMessage'])
    R('has ' + k, () => k in w);
  R('has @@iterator', () => Symbol.iterator in w);
  R('has @@toStringTag', () => Symbol.toStringTag in w);
  R('@@iterator', () => w[Symbol.iterator]);
  R('@@toStringTag', () => w[Symbol.toStringTag]);
  R('@@toPrimitive', () => w[Symbol.toPrimitive]);
  R('@@for', () => w[Symbol.for('x')]);
  R('@@desc', () => w[Symbol('d')]);
  R('String(w)', () => String(w));
  R('toString.call', () => Object.prototype.toString.call(w));
  R('JSON.stringify', () => JSON.stringify(w));
  R('concat', () => [].concat(w).length);
  R('getPrototypeOf', () => Object.getPrototypeOf(w));
  R('isExtensible', () => Object.isExtensible(w));
  R('set [0]', () => { w[0] = 1; return 'no throw'; });
  R('set secret', () => { w.secret = 1; return 'no throw'; });
  R('define [0]', () => Object.defineProperty(w, '0', {value: 1}));
  R('delete [0]', () => delete w[0]);
  R('delete [5]', () => delete w[5]);
  R('top===top', () => w.top === top);
  R('frames===w', () => w.frames === w);
  R('self===w', () => w.self === w);
  R('typeof postMessage', () => typeof w.postMessage);
  R('then', () => w.then);
  R('closed', () => w.closed);
  R('opener', () => w.opener);
  R('location.href', () => w.location.href);
  R('typeof location.replace', () => typeof w.location.replace);
  R('loop postMessage', () => {
    let n = 0;
    for (let i = 0; i < w.frames.length; i++) { w.frames[i].postMessage('ping', '*'); n++; }
    return n;
  });
}

function __shape(rows, prefix, w) {
  const R = (n, f) => __run(rows, prefix + n, f);
  const d = (k) => Object.getOwnPropertyDescriptor(w, k);
  R('len get.name', () => d('length').get.name);
  R('len get.length', () => d('length').get.length);
  R('len get.call', () => d('length').get.call(w));
  R('len get===get', () => d('length').get === d('length').get);
  R('loc set.name', () => d('location').set.name);
  R('loc get===set', () => d('location').get === d('location').set);
  R('top get.call===top', () => d('top').get.call(w) === w.top);
  R('pm value===pm', () => d('postMessage').value === w.postMessage);
  R('pm===pm', () => w.postMessage === w.postMessage);
  R('pm.name', () => w.postMessage.name);
  R('pm.length', () => w.postMessage.length);
  R('focus.name', () => w.focus.name);
  R('close.length', () => w.close.length);
  R('window===w', () => w.window === w);
  R('location===location', () => w.location === w.location);
  R('loc ownKeys', () => Reflect.ownKeys(w.location).map(String).join(','));
  R('loc gOPD href', () => __desc(Object.getOwnPropertyDescriptor(w.location, 'href')));
  R('loc gOPD replace', () => __desc(Object.getOwnPropertyDescriptor(w.location, 'replace')));
  R('loc gOPD host', () => __desc(Object.getOwnPropertyDescriptor(w.location, 'host')));
  R('loc replace.name', () => w.location.replace.name);
  R('loc then', () => w.location.then);
  R('loc toString.call', () => Object.prototype.toString.call(w.location));
  R('loc String', () => String(w.location));
  R('loc host', () => w.location.host);
  R('set [5]', () => { w[5] = 1; return 'no throw'; });
  R('set length', () => { w.length = 1; return 'no throw'; });
  R('strict set length', () => { 'use strict'; w.length = 1; return 'no throw'; });
  R('set opener', () => { w.opener = 1; return 'no throw'; });
}
