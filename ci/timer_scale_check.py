"""Check that timer registration, cancellation and dispatch grow about linearly (issue #34).

Every timer insertion once re-sorted the whole macrotask vector, and lookup,
cancellation and completion scanned it and the callback list, so 4x the timers
cost about 20x the time. On the logical clock (no real waiting) this registers,
clears, re-registers and dispatches 2,000 and 8,000 timers, takes the best of
three runs per size, and fails when 4x the timers costs more than 10x the time.
It also checks the callbacks: none after clearTimeout, every timer once in
registration order at the same deadline. Exit status is non-zero on failure.
"""
import sys
import time

from ruyijs import Context

SIZES = (2000, 8000)
LIMIT = 10.0
SETUP = """
globalThis.hits = []; globalThis.ids = [];
globalThis.register = n => {
  ids = [];
  for (let i = 0; i < n; i++) ids.push(setTimeout(() => { hits.push(i); }, 1000));
  return ids.length;
};
globalThis.cancel = () => { for (const id of ids) clearTimeout(id); ids = []; return hits.length; };
globalThis.ordered = n => hits.length === n && hits.every((value, index) => value === index);
void 0;
"""


def timed(operation):
    began = time.perf_counter()
    value = operation()
    return value, time.perf_counter() - began


def measure(n):
    with Context(timeout=60) as context:
        context.configure_clock("logical", epoch_ms=1700000000000)
        context.eval(SETUP)
        for _ in range(3):
            assert context.eval("register(64); cancel()") == 0
        value, register = timed(lambda: context.eval(f"register({n})"))
        assert value == n, value
        value, cancel = timed(lambda: context.eval("cancel()"))
        assert value == 0, f"{value} callbacks ran after clearTimeout"
        context.advance(0, max_tasks=1000)
        assert context.eval(f"register({n})") == n
        _, dispatch = timed(lambda: context.advance(1000, step_ms=1000, max_tasks=100000))
        assert context.eval(f"ordered({n})"), "timers did not each run once in registration order"
        return {"register": register, "cancel": cancel, "dispatch": dispatch}


def main():
    best = {}
    for _ in range(3):
        for n in SIZES:
            for phase, seconds in measure(n).items():
                best[n, phase] = min(best.get((n, phase), seconds), seconds)
    failed = False
    small, large = SIZES
    for phase in ("register", "cancel", "dispatch"):
        # A floor keeps sub-millisecond timings from turning noise into a ratio.
        ratio = best[large, phase] / max(best[small, phase], 0.002)
        ok = ratio <= LIMIT
        failed |= not ok
        print(f"{'ok' if ok else 'FAIL':4} {phase}: {small} timers {best[small, phase] * 1000:.1f} ms, "
              f"{large} timers {best[large, phase] * 1000:.1f} ms, ratio {ratio:.1f} (limit {LIMIT:g})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
