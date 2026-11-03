"""Check that aborting an in-flight fetch rejects with AbortError (issue #30).

The abort callback once wrote its return value before reading its own callee
(both live in vp[0]) and then dereferenced `undefined` as an object, so js.exe
died with 0xC0000005 whenever a page aborted a fetch that had not completed.
Runs offline. Exit status is non-zero on failure.
"""
import sys

from ruyijs import Context

COUNT = 50
PAGE = """<script>
window.done = 0; window.names = [];
for (let i = 0; i < %d; i++) {
  const c = new AbortController();
  fetch('https://example.test/slow?' + i, {signal: c.signal})
    .then(() => { names.push('resolved'); }, e => { names.push(e.name); })
    .finally(() => { done++; });
  c.abort();
}
</script>""" % COUNT

with Context(timeout=30) as ctx:
    for i in range(COUNT):
        ctx.put_resource(f"https://example.test/slow?{i}", "x")
    ctx.load_page("https://example.test/", PAGE)
    for _ in range(40):
        ctx.advance(50)
        if ctx.eval("done") == COUNT:
            break
    done = ctx.eval("done")
    names = sorted(set(ctx.eval("names.join(',')").split(",")))
    alive = ctx.eval("1 + 1") == 2

ok = done == COUNT and names == ["AbortError"] and alive
print(f"{'ok  ' if ok else 'FAIL'} fetch abort: {done}/{COUNT} settled, {names}")
sys.exit(0 if ok else 1)
