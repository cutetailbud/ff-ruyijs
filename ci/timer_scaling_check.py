"""Check timer queue ordering, cancellation semantics, and scaling (issue #34).

Verifies that indexed macrotask and callback tracking preserves exact timer,
interval, animation-frame, idle-callback, scheduler-task, and realm-retirement
behavior while keeping registration, cancellation, and dispatch near-linear up
to 8,000 queued timers. Exit status is non-zero on failure.
"""
import statistics
import sys
import time

from ruyijs import Context


def check_semantics() -> None:
    with Context(timeout=30) as ctx:
        ctx.configure_clock("logical", epoch_ms=1700000000000)
        ctx.load_page("https://timers.invalid/", "<!doctype html><body><div id='root'></div></body>")

        # 1. Deadline ordering + FIFO tie-breaking for equal deadlines.
        ctx.eval("""
            globalThis.seq = [];
            setTimeout(() => seq.push("d50a"), 50);
            setTimeout(() => seq.push("d10a"), 10);
            setTimeout(() => seq.push("d30"), 30);
            setTimeout(() => seq.push("d10b"), 10);
            setTimeout(() => seq.push("d0a"), 0);
            setTimeout(() => seq.push("d50b"), 50);
            setTimeout(() => seq.push("d0b"), 0);
            void 0;
        """)
        ctx.advance(60, step_ms=60, max_tasks=100)
        seq = ctx.eval("seq")
        assert seq == ["d0a", "d0b", "d10a", "d10b", "d30", "d50a", "d50b"], f"bad order: {seq}"

        # 2. Self-cancellation, sibling cancellation, and handle-domain separation.
        ctx.eval("""
            globalThis.events = [];
            let ticks = 0;
            const iv = setInterval(() => {
                ticks++;
                events.push("iv:" + ticks);
                if (ticks === 3) clearInterval(iv);
            }, 10);
            let selfId = 0;
            selfId = setTimeout(() => {
                clearTimeout(selfId);
                clearTimeout(laterId);
                events.push("self");
            }, 15);
            const laterId = setTimeout(() => events.push("later-cancelled"), 25);
            const rafKeep = requestAnimationFrame(() => events.push("raf-kept"));
            clearTimeout(rafKeep);
            const timerKeep = setTimeout(() => events.push("timer-kept"), 20);
            cancelAnimationFrame(timerKeep);
            void 0;
        """)
        ctx.advance(50, step_ms=16, max_tasks=100)
        events = ctx.eval("events")
        assert events == ["raf-kept", "iv:1", "self", "iv:2", "timer-kept", "iv:3"], f"bad cancel events: {events}"

        # 3. HTML 5-level nesting clamp (>= 4 ms starting at depth 5).
        ctx.eval("""
            globalThis.stamps = [];
            const start = performance.now();
            function step(depth) {
                stamps.push(Math.round(performance.now() - start));
                if (depth < 7) setTimeout(() => step(depth + 1), 0);
            }
            setTimeout(() => step(1), 0);
            void 0;
        """)
        ctx.advance(40, step_ms=1, max_tasks=100)
        stamps = ctx.eval("stamps")
        assert stamps[:4] == [0, 0, 0, 0] and stamps[4] >= 4 and stamps[6] >= 12, f"bad clamp: {stamps}"

        # 4. requestAnimationFrame batching + cancelAnimationFrame.
        ctx.eval("""
            globalThis.rafOrder = [];
            const r1 = requestAnimationFrame(() => {
                rafOrder.push("f1a");
                requestAnimationFrame(() => rafOrder.push("f2"));
            });
            const r2 = requestAnimationFrame(() => rafOrder.push("f1b-cancelled"));
            const r3 = requestAnimationFrame(() => rafOrder.push("f1c"));
            cancelAnimationFrame(r2);
            void r1; void r3;
        """)
        ctx.advance(16, step_ms=16, max_tasks=50)
        assert ctx.eval("rafOrder") == ["f1a", "f1c"], f"bad first frame: {ctx.eval('rafOrder')}"
        ctx.advance(16, step_ms=16, max_tasks=50)
        assert ctx.eval("rafOrder") == ["f1a", "f1c", "f2"], f"bad second frame: {ctx.eval('rafOrder')}"

        # 5. requestIdleCallback + cancelIdleCallback + timeout expiration.
        ctx.eval("""
            globalThis.idleLog = [];
            const icCancel = requestIdleCallback(() => idleLog.push("cancelled"));
            cancelIdleCallback(icCancel);
            requestIdleCallback(d => idleLog.push("zero:" + d.didTimeout), { timeout: 0 });
            requestIdleCallback(d => idleLog.push("spare:" + d.didTimeout + ":" + (d.timeRemaining() > 0)), { timeout: 50 });
            setTimeout(() => idleLog.push("t20"), 20);
            void 0;
        """)
        ctx.advance(10, step_ms=10, max_tasks=50)
        ctx.advance(20, step_ms=20, max_tasks=50)
        idle_log = ctx.eval("idleLog")
        assert idle_log == ["zero:true", "spare:false:true", "t20"], f"bad idle log: {idle_log}"

        # 6. scheduler.postTask dynamic priority promotion + AbortController cancellation.
        ctx.eval("""
            globalThis.schedLog = [];
            const ctrlPromote = new TaskController({ priority: "background" });
            const ctrlAbort = new AbortController();
            scheduler.postTask(() => schedLog.push("bg"), { priority: "background" });
            scheduler.postTask(() => schedLog.push("promoted"), { signal: ctrlPromote.signal });
            scheduler.postTask(() => schedLog.push("visible"), { priority: "user-visible" });
            scheduler.postTask(() => schedLog.push("aborted"), { signal: ctrlAbort.signal })
                .catch(err => schedLog.push("caught:" + err.name));
            ctrlPromote.setPriority("user-blocking");
            ctrlAbort.abort();
            void 0;
        """)
        ctx.advance(5, step_ms=5, max_tasks=50)
        sched_log = ctx.eval("schedLog")
        assert sched_log == ["caught:AbortError", "promoted", "visible", "bg"], f"bad sched log: {sched_log}"

        # 7. Iframe realm handle isolation and retirement on removal.
        ctx.eval("""
            globalThis.realmLog = [];
            const frame = document.createElement("iframe");
            document.body.appendChild(frame);
            const topId = setTimeout(() => realmLog.push("top"), 10);
            const subKeep = frame.contentWindow.setTimeout(() => realmLog.push("sub-keep"), 10);
            const subDrop = frame.contentWindow.setTimeout(() => realmLog.push("sub-retired"), 30);
            // Clearing handle 999999 or cross-realm handle must not cancel the other realm.
            frame.contentWindow.clearTimeout(topId + 50000);
            void subKeep; void subDrop;
        """)
        ctx.advance(15, step_ms=15, max_tasks=50)
        assert ctx.eval("realmLog") == ["top", "sub-keep"], f"bad realm run: {ctx.eval('realmLog')}"
        ctx.eval("document.querySelector('iframe').remove(); void 0;")
        ctx.advance(30, step_ms=30, max_tasks=50)
        assert ctx.eval("realmLog") == ["top", "sub-keep"], f"retired iframe ran: {ctx.eval('realmLog')}"


def check_scaling() -> dict:
    samples = {2000: {"register": [], "cancel": [], "dispatch": []},
               8000: {"register": [], "cancel": [], "dispatch": []}}
    for repeat in range(3):
        order = (2000, 8000) if repeat % 2 == 0 else (8000, 2000)
        for n in order:
            with Context(timeout=35) as ctx:
                ctx.configure_clock("logical", epoch_ms=1700000000000)
                ctx.eval("""
                    globalThis.hits = 0;
                    globalThis.ids = [];
                    globalThis.register = n => {
                        ids = [];
                        for (let i = 0; i < n; i++)
                            ids.push(setTimeout(() => { hits++; }, 1000));
                        return ids.length;
                    };
                    globalThis.cancel = reverse => {
                        if (reverse) {
                            for (let i = ids.length - 1; i >= 0; i--) clearTimeout(ids[i]);
                        } else {
                            for (const id of ids) clearTimeout(id);
                        }
                        ids = [];
                        return hits;
                    };
                    void 0;
                """)
                assert ctx.eval("register(64); cancel(false)") == 0

                t0 = time.perf_counter_ns()
                assert ctx.eval(f"register({n})") == n
                samples[n]["register"].append((time.perf_counter_ns() - t0) / 1e6)

                t0 = time.perf_counter_ns()
                assert ctx.eval(f"cancel({'true' if repeat == 1 else 'false'})") == 0
                samples[n]["cancel"].append((time.perf_counter_ns() - t0) / 1e6)

                ctx.advance(0, max_tasks=1000)
                assert ctx.eval(f"register({n})") == n
                t0 = time.perf_counter_ns()
                ctx.advance(1000, step_ms=1000, max_tasks=100000)
                samples[n]["dispatch"].append((time.perf_counter_ns() - t0) / 1e6)
                assert ctx.eval("hits") == n
    return {
        n: {phase: statistics.median(vals) for phase, vals in phases.items()}
        for n, phases in samples.items()
    }


def main() -> int:
    check_semantics()
    medians = check_scaling()
    ratios = {
        phase: medians[8000][phase] / max(0.5, medians[2000][phase])
        for phase in ("register", "cancel", "dispatch")
    }
    ok = all(r < 8.0 for r in ratios.values()) and all(
        medians[8000][phase] < 150.0 for phase in ("register", "cancel", "dispatch")
    )
    print(
        f"{'ok  ' if ok else 'FAIL'} timer scaling: "
        f"2k={medians[2000]} ms, 8k={medians[8000]} ms, 8k/2k={ratios}"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
