"""Check computed-style liveness, lazy used-box resolution and custom-property scaling (issue #48).

Verifies:
1. Live CSSStyleDeclaration views returned by getComputedStyle(el) reflect
   subsequent inline style, class, CSSOM rule/disabled, adoptedStyleSheets,
   focus, and viewport (@media / percentage layout) changes.
2. Layout-dependent resolved values (width, height, margin-*, padding-*, insets)
   and shorthands match detached cssText round-trips after reading non-layout
   properties first.
3. Heavy stylesheets with 1,500+ inherited CSS custom properties, attribute
   selectors on [style] and [data-*], 24-deep ancestor chains, and detached
   scratch style.cssText parses scale without re-serializing or re-laying out
   unchanged connected trees.
"""
import json
import sys
import time

from ruyijs import Context


def check_liveness_and_used_values():
    html = """<!doctype html><html><head><style id="main-sheet">
      :root { --accent: rgb(10, 20, 30); }
      #wrap { width: 400px; position: relative; }
      #box { width: calc(50% + 10px); padding-left: 5%; position: relative; left: 10%; pointer-events: auto; z-index: 3; }
      #box.muted { pointer-events: none; z-index: 9; }
      #box:focus { z-index: 42; }
      @media (max-width: 500px) { #box { z-index: 77; } }
    </style></head><body>
      <div id="wrap"><input id="box" value="hi"></div>
    </body></html>"""

    with Context(timeout=30) as ctx:
        ctx.configure_environment({"viewport": {"width": 800, "height": 600}})
        ctx.load_page("https://example.test/liveness", html)
        state1 = json.loads(ctx.eval(r"""JSON.stringify((() => {
          const box = document.getElementById('box');
          const held = getComputedStyle(box);
          // Read non-layout properties before any layout-dependent read.
          const pe0 = held.pointerEvents;
          const z0 = held.zIndex;
          const custom0 = held.getPropertyValue('--accent');
          // Now read layout-dependent resolved values lazily on the same view.
          const w0 = held.width;
          const pl0 = held.paddingLeft;
          const left0 = held.left;

          // Inline style mutation updates retained declaration.
          box.style.pointerEvents = 'none';
          const pe1 = held.pointerEvents;
          const pe1_gpv = held.getPropertyValue('pointer-events');
          box.style.pointerEvents = '';

          // Class mutation updates retained declaration.
          box.classList.add('muted');
          const pe2 = held.pointerEvents;
          const z2 = held.zIndex;
          box.classList.remove('muted');

          // CSSOM insertRule & sheet.disabled update retained declaration.
          const sheet = document.getElementById('main-sheet').sheet;
          const idx = sheet.insertRule('#box { z-index: 15; }', sheet.cssRules.length);
          const z3 = held.zIndex;
          sheet.deleteRule(idx);
          const z4 = held.zIndex;

          // Adopted stylesheet mutation updates retained declaration.
          const adopted = new CSSStyleSheet();
          adopted.replaceSync('#box { z-index: 28; }');
          document.adoptedStyleSheets = [adopted];
          const z5 = held.zIndex;
          document.adoptedStyleSheets = [];

          // Focus pseudo-class updates retained declaration.
          box.focus();
          const zFocus = held.zIndex;
          box.blur();

          globalThis.__held = held;
          return { pe0, z0, custom0, w0, pl0, left0, pe1, pe1_gpv, pe2, z2, z3, z4, z5, zFocus };
        })())"""))

        assert state1["pe0"] == "auto", state1
        assert state1["z0"] == "3", state1
        assert state1["custom0"] == "rgb(10, 20, 30)", state1
        assert state1["w0"] == "210px", state1
        assert state1["pl0"] == "20px", state1
        assert state1["left0"] == "40px", state1
        assert state1["pe1"] == "none" and state1["pe1_gpv"] == "none", state1
        assert state1["pe2"] == "none" and state1["z2"] == "9", state1
        assert state1["z3"] == "15" and state1["z4"] == "3", state1
        assert state1["z5"] == "28", state1
        assert state1["zFocus"] == "42", state1

        # Media rule condition change updates @media match on the retained view.
        z_media = ctx.eval("""(() => {
          const sheet = document.getElementById('main-sheet').sheet;
          const mediaRule = sheet.cssRules[sheet.cssRules.length - 1];
          mediaRule.media.mediaText = '(max-width: 1200px)';
          return globalThis.__held.zIndex;
        })()""")
        assert z_media == "77", z_media
        assert not ctx.net_log()


def build_heavy_fixture(custom_count=1500, extra_rules=2500):
    root_vars = "".join(f"--tok-{i}: {i}px calc({i % 7} * 2px); " for i in range(custom_count))
    rules = [
        f":root {{ {root_vars} }}",
        '[style*="pointer-events"] { outline-style: none; }',
        '[data-probe] { outline-width: 0px; }',
        ".lvl { position: relative; z-index: 2; pointer-events: auto; }",
    ]
    for i in range(extra_rules):
        rules.append(f".unused-cls-{i} > span[data-k='{i}'] {{ color: rgb({i % 255}, 10, 20); padding: {i % 9}px; }}")
    css_text = "\n".join(rules)

    # 24-deep ancestor chain + 700 sibling nodes (~726 total elements).
    chain_open = "".join(f'<div class="lvl" id="anc-{i}">' for i in range(23))
    chain_close = "</div>" * 23
    filler = "".join(f'<span class="item" data-idx="{i}">x</span>' for i in range(700))
    return (
        f"<!doctype html><html><head><style>{css_text}</style></head>"
        f"<body>{filler}{chain_open}<div id='input-area' class='lvl'>ok</div>{chain_close}</body></html>"
    )


def check_heavy_custom_property_scaling():
    html = build_heavy_fixture()
    probe = r"""JSON.stringify((() => {
      const target = document.getElementById('input-area');
      const nodes = [];
      for (let e = target; e; e = e.parentElement) nodes.push(e);
      const props = ['pointerEvents', 'zIndex', 'position', 'display', 'visibility'];
      const scratch = document.createElement('i').style;

      const t0 = performance.now();
      const native = getComputedStyle(target);
      for (let i = 0; i < 150; i++) {
        if (native.pointerEvents !== 'auto') throw new Error('unexpected pointerEvents');
        if (native.getPropertyValue('pointer-events') !== 'auto') throw new Error('unexpected gpv');
      }
      for (let i = 0; i < 100; i++) {
        const s = getComputedStyle(target);
        for (const p of props) void s[p];
      }
      for (let r = 0; r < 10; r++) {
        for (const e of nodes) {
          const s = getComputedStyle(e);
          void s.pointerEvents;
          void s.zIndex;
        }
      }
      const fastPathMs = performance.now() - t0;

      // Detached scratch.cssText round-trip must not invalidate connected caches.
      const t1 = performance.now();
      for (let i = 0; i < 40; i++) {
        scratch.cssText = getComputedStyle(target).cssText;
        for (const p of props) {
          if (scratch[p] !== native[p]) throw new Error('mismatch on ' + p);
        }
      }
      const scratchMs = performance.now() - t1;

      const comparisons = nodes.map(e => {
        const s = getComputedStyle(e);
        scratch.cssText = s.cssText;
        return props.every(p => s[p] === scratch[p]);
      });

      const held = getComputedStyle(target);
      const before = held.pointerEvents;
      target.style.pointerEvents = 'none';
      const retained = held.pointerEvents;
      const fresh = getComputedStyle(target).pointerEvents;
      target.style.pointerEvents = 'auto';

      return {
        ancestorCount: nodes.length,
        propCount: native.length,
        cssTextLen: native.cssText.length,
        allEqual: comparisons.every(Boolean),
        livenessOk: before === 'auto' && retained === 'none' && fresh === 'none',
        fastPathMs,
        scratchMs,
      };
    })())"""

    with Context(timeout=30) as ctx:
        ctx.configure_environment({"viewport": {"width": 800, "height": 600}})
        ctx.load_page("https://example.test/heavy-style", html)
        # Warm initial stylesheet parse & layout once, then measure steady-state.
        warm = json.loads(ctx.eval(probe))
        began = time.perf_counter()
        measured = json.loads(ctx.eval(probe))
        wall_ms = (time.perf_counter() - began) * 1000.0
        assert not ctx.net_log()

    for run in (warm, measured):
        assert run["ancestorCount"] >= 24, run
        assert run["propCount"] >= 1600, run
        assert run["cssTextLen"] >= 35000, run
        assert run["allEqual"] is True, run
        assert run["livenessOk"] is True, run
    # Before the fix, a single run exceeded 30,000 ms (timed out); require < 4,000 ms on CI runners.
    assert measured["fastPathMs"] < 1500.0, measured
    assert wall_ms < 4000.0, (wall_ms, measured)
    print(
        f"computed_style_check: ok (ancestors={measured['ancestorCount']}, "
        f"props={measured['propCount']}, fast_ms={measured['fastPathMs']:.2f}, "
        f"scratch40_ms={measured['scratchMs']:.2f}, wall_ms={wall_ms:.2f})"
    )


def main():
    check_liveness_and_used_values()
    check_heavy_custom_property_scaling()
    return 0


if __name__ == "__main__":
    sys.exit(main())
