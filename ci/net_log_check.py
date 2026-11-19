"""Check reading and clearing the network log (issue #46).

A long-running host had no way to drop recorded requests: clear_resources()
clears stored responses only, and the log stops recording at 4096 entries.
net_log(clear=True) reads and drains in one command, clear_net_log() drains,
ids keep counting, and stored resources stay. Offline: requests are served by
put_resource. Exit status is non-zero on failure.
"""
import sys

from ruyijs import Context

BASE = "https://log.example.test"
failures = []


def check(name, condition, detail=""):
    print(f"{'ok  ' if condition else 'FAIL'} {name}  {detail}", flush=True)
    if not condition:
        failures.append(name)


def request(ctx, path):
    ctx.eval(f"fetch({BASE + path!r}).then(r => r.text()); void 0;")
    for _ in range(20):
        ctx.advance(25)


with Context(timeout=30) as ctx:
    ctx.put_resource(BASE + "/a", "a", headers=[("Content-Type", "text/plain")])
    ctx.put_resource(BASE + "/b", "b", headers=[("Content-Type", "text/plain")])
    ctx.load_page(BASE + "/", "<!doctype html><title>log</title>")
    request(ctx, "/a")
    request(ctx, "/b")
    first = ctx.net_log()
    urls = [entry["url"] for entry in first]
    check("net_log() lists the requests", urls[-2:] == [BASE + "/a", BASE + "/b"], urls)
    check("net_log() keeps them", ctx.net_log() == first)
    drained = ctx.net_log(clear=True)
    check("net_log(clear=True) returns the same entries", drained == first)
    check("and leaves the log empty", ctx.net_log() == [])
    request(ctx, "/a")
    after = ctx.net_log()
    check("ids keep counting after a clear", len(after) == 1 and after[0]["id"] > first[-1]["id"],
          [entry["id"] for entry in after])
    check("clear_resources() leaves the log", ctx.clear_resources()["resources"] == 0 and ctx.net_log() == after)
    ctx.clear_net_log()
    check("clear_net_log() empties it", ctx.net_log() == [])
    try:
        ctx.net_log(clear="yes")
        refused = False
    except TypeError:
        refused = True
    check("clear must be a boolean", refused)

print("net log checks passed" if not failures else f"FAILED: {failures}")
sys.exit(1 if failures else 0)
