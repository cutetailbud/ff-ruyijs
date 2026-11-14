"""Akamai BM 3.x full-chain bypass using ruyijs (SpiderMonkey + Necko).

Architecture
============
ruyijs provides a real Firefox 160 environment (SpiderMonkey + Necko).
The Akamai JS runs natively — no deobfuscation, no algorithm reimplementation,
no environment patching needed.

Two doors:
  sensor_data → _abck (telemetry)
  sbsd        → bm_sc (sec-if-cpt challenge)

Usage:
  python akamai_bm3.py
  python akamai_bm3.py --proxy http://127.0.0.1:7890
  python akamai_bm3.py --direct --timeout 180
"""
import argparse
import json
import math
import os
import random
import socket
import sys
import time
from pathlib import Path
from time import monotonic
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
# Support running from source tree
RUYIJS_ROOT = ROOT.parents[1]
if (RUYIJS_ROOT / "dist" / "js.exe").is_file() or (RUYIJS_ROOT / "dist" / "js").is_file():
    sys.path.insert(0, str(RUYIJS_ROOT / "python"))
try:
    from ruyijs import Context, JSError
except ImportError:
    print("ruyijs not installed. Run: pip install ruyijs", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
DEFAULT_URL = "https://www.radissonhotels.com/zh-cn/"

# Firefox on Windows 10 — full validated profile (version follows --tls)
PROFILE = {
    "navigator": {
        "userAgent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:158.0) Gecko/20100101 Firefox/158.0",
        "platform": "Win32",
        "appCodeName": "Mozilla",
        "appName": "Netscape",
        "appVersion": "5.0 (Windows)",
        "product": "Gecko",
        "productSub": "20100101",
        "vendor": "",
        "vendorSub": "",
        "language": "zh-CN",
        "languages": ["zh-CN", "zh", "en-US", "en"],
        "hardwareConcurrency": 12,
        "maxTouchPoints": 0,
        "cookieEnabled": True,
        "webdriver": False,
        "oscpu": "Windows NT 10.0; Win64; x64",
        "buildID": "20181001000000",
        "doNotTrack": "unspecified",
        "pdfViewerEnabled": True,
        "onLine": True,
        "globalPrivacyControl": False,
    },
    "screen": {
        "width": 1920, "height": 1080,
        "availWidth": 1920, "availHeight": 1040,
        "availLeft": 0, "availTop": 0,
        "colorDepth": 24, "pixelDepth": 24,
    },
    "viewport": {
        "width": 1536, "height": 864,
        "devicePixelRatio": 1.25,
    },
    "locale": {
        "timezone": "Asia/Shanghai",
    },
    "webgl": {
        "vendor": "Mozilla",
        "renderer": "ANGLE (NVIDIA, NVIDIA GeForce RTX 3080 Laptop GPU Direct3D11 vs_5_0 ps_5_0), or similar",
        "version": "WebGL 1.0",
        "glslVersion": "WebGL GLSL ES 1.0",
        "unmaskedVendor": "Google Inc. (NVIDIA)",
        "unmaskedRenderer": "ANGLE (NVIDIA, NVIDIA GeForce RTX 3080 Laptop GPU Direct3D11 vs_5_0 ps_5_0), or similar",
        "version2": "WebGL 2.0",
        "glslVersion2": "WebGL GLSL ES 3.00",
        "attributes": {
            "alpha": True, "antialias": True, "depth": True,
            "failIfMajorPerformanceCaveat": False, "powerPreference": "default",
            "premultipliedAlpha": True, "preserveDrawingBuffer": False, "stencil": False,
        },
        "extensions": [
            "ANGLE_instanced_arrays", "EXT_blend_minmax", "EXT_color_buffer_half_float",
            "EXT_float_blend", "EXT_frag_depth", "EXT_shader_texture_lod", "EXT_sRGB",
            "EXT_texture_compression_bptc", "EXT_texture_compression_rgtc",
            "EXT_texture_filter_anisotropic", "OES_element_index_uint",
            "OES_standard_derivatives", "OES_texture_float", "OES_texture_float_linear",
            "OES_texture_half_float", "OES_texture_half_float_linear",
            "OES_vertex_array_object", "WEBGL_color_buffer_float",
            "WEBGL_compressed_texture_s3tc", "WEBGL_compressed_texture_s3tc_srgb",
            "WEBGL_debug_renderer_info", "WEBGL_debug_shaders",
            "WEBGL_depth_texture", "WEBGL_draw_buffers", "WEBGL_lose_context",
        ],
        "limits": {},
    },
    "speech": {
        "voices": [
            "Microsoft Huihui - Chinese (Simplified, PRC)",
            "Microsoft Kangkang - Chinese (Simplified, PRC)",
            "Microsoft Yaoyao - Chinese (Simplified, PRC)",
            "Microsoft Huihui Desktop - Chinese (Simplified)",
            "Microsoft Zira Desktop - English (United States)",
        ],
        "langs": ["zh-CN", "zh-CN", "zh-CN", "zh-CN", "en-US"],
        "default": "",
    },
}

# ---------------------------------------------------------------------------
# JS probes
# ---------------------------------------------------------------------------

JS_PAGE_STATE = """(function(){
    var h = document.documentElement;
    var size = h ? h.outerHTML.length : 0;
    return {
        size: size,
        title: document.title || '',
        href: location.href,
        ready: document.readyState,
        cpt: !!document.getElementById('sec-if-cpt-container'),
        real: size > 100000
    };
})()"""

JS_COOKIES = """(function(){
    var c = document.cookie;
    function g(n){ var m = c.match(new RegExp('(?:^|;\\\\s*)' + n + '=([^;]*)')); return m ? m[1] : null; }
    var a = g('_abck'), b = g('bm_sc');
    var av = null, bv = null;
    if(a){ var p = a.split('~'); if(p.length>=2) av = p[1]; }
    if(b){ var p = b.split('~'); bv = p[0]; }
    return { abck: av, bmsc: bv, bmsz: !!g('bm_sz') };
})()"""

JS_BMAK = """(function(){
    try {
        if(typeof window.bmak !== 'undefined')
            return { ok: true, startTs: window.bmak.startTs, ver: window.bmak.ver || null };
    } catch(e){}
    return { ok: false };
})()"""

# ---------------------------------------------------------------------------
# Mouse trajectory (60 Hz, random-walk, boundary reflection)
# ---------------------------------------------------------------------------

def gen_trajectory(vw=1536, vh=864, n=30):
    pts = []
    x, y = random.uniform(vw*0.1, vw*0.9), random.uniform(vh*0.1, vh*0.9)
    d, s = random.uniform(0, 6.28), random.uniform(5, 20)
    fclk = 0.0
    pauses = {random.randint(4, max(5, n-3)) for _ in range(random.randint(0, 2))} if n > 8 else set()
    for i in range(n):
        if i == 0:
            dt = 0
        elif i in pauses:
            dt = random.uniform(80, 450)
        elif random.random() < 0.10:
            dt = random.uniform(5, 14)
        else:
            fclk += 1000.0/60.0
            dt = max(14, round(fclk) - sum(p[2] for p in pts) if pts else 17)
        d += random.uniform(-0.22, 0.22)
        s = max(2, min(s * random.uniform(0.85, 1.18), 100))
        x += math.cos(d) * s; y += math.sin(d) * s
        if x < 0:    x, d = -x, math.pi - d
        if x > vw:   x, d = 2*vw - x, math.pi - d
        if y < 0:    y, d = -y, -d
        if y > vh:   y, d = 2*vh - y, -d
        pts.append((int(max(0, min(x, vw))), int(max(0, min(y, vh))), int(dt)))
    return pts


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def auto_proxy():
    for n in ("HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy"):
        if os.environ.get(n): return os.environ[n]
    try:
        with socket.create_connection(("127.0.0.1", 7890), timeout=0.3): return "http://127.0.0.1:7890"
    except OSError:
        return None


def log(msg, **kw):
    ts = time.strftime("%H:%M:%S")
    extra = " ".join(f"{k}={v}" for k, v in kw.items()) if kw else ""
    print(f"[{ts}] {msg}" + (f"  ({extra})" if extra else ""), flush=True)


# ---------------------------------------------------------------------------
# Core
# ---------------------------------------------------------------------------

def run(args, report):
    deadline = monotonic() + args.timeout
    proxy = report["proxy"] if report["proxy"] != "direct" else None

    with Context(proxy=proxy, timeout=30, tls=args.tls) as ctx:
        script_errors = report["scriptErrors"]

        def tl():
            r = deadline - monotonic()
            if r <= 0: raise TimeoutError("overall timeout")
            ctx.timeout = min(r, 20)
            return r

        def adv(ms=250, mt=400):
            tl()
            try:
                ctx.advance(ms, max_tasks=mt)
            except JSError as e:
                entry = {"name": e.name, "msg": str(e.message)[:200]}
                if entry not in script_errors and len(script_errors) < 30:
                    script_errors.append(entry)
            except Exception as e:
                report.setdefault("advErr", [])
                if len(report["advErr"]) < 10:
                    report["advErr"].append(str(e)[:150])

        def pump(secs, check=None, label=""):
            end = min(monotonic() + secs, deadline)
            n = 0
            while monotonic() < end:
                adv(200)
                n += 1
                if check:
                    r = check()
                    if r: return r
            return None

        def ck():
            tl()
            try: return ctx.eval(JS_COOKIES)
            except: return {"abck": None, "bmsc": None}

        def ps():
            tl()
            try: return ctx.eval(JS_PAGE_STATE)
            except: return {"size": 0, "real": False, "cpt": False, "ready": ""}

        def bm():
            tl()
            try: return ctx.eval(JS_BMAK)
            except: return {"ok": False}

        def net_summary():
            try:
                rows = ctx.net_log()
                return [{"m": r["method"], "u": r["url"][:160], "s": r.get("status")}
                        for r in rows]
            except: return []

        # ==============================================================
        # 1. Configure
        # ==============================================================
        log(f"Configuring Firefox {args.tls} environment (UA + TLS)...")
        ctx.configure_clock("system")
        profile = json.loads(json.dumps(PROFILE))
        # The User-Agent agrees with the TLS ClientHello Necko sends.
        profile["navigator"]["userAgent"] = (
            f"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:{args.tls}.0) "
            f"Gecko/20100101 Firefox/{args.tls}.0")
        try:
            ctx.configure_fingerprint(profile)
            log("  Profile applied", gpu="RTX 3080", lang="zh-CN", screen="1920x1080")
        except Exception as e:
            log(f"  Fingerprint warning: {e}")

        # ==============================================================
        # 2. Navigate
        # ==============================================================
        log(f"Navigating to {args.url}...")
        ctx.navigate(args.url)

        # Wait for page load
        pump(15, lambda: ps() if ps().get("ready") == "complete" else None, "load")

        s = ps()
        c = ck()
        report["init"] = {"size": s["size"], "cpt": s["cpt"], "real": s["real"],
                          "abck": c["abck"], "bmsc": c["bmsc"]}
        log(f"Page loaded", size=s["size"], cpt=s["cpt"], real=s["real"],
            abck=c["abck"], bmsc=c["bmsc"])

        # Already real page?
        if s["real"]:
            log("Real page directly — TLS trusted!")
            report.update(method="direct", passed=True, finalSize=s["size"])
            return

        if not s["cpt"] and s["size"] < 20000 and s["size"] > 100:
            log("403 blocked. Try --proxy or retry later.")
            report.update(method="blocked", passed=False)
            return

        # ==============================================================
        # 3. Wait for bmak init + automatic POSTs
        # ==============================================================
        log("Waiting for sensor_data engine...")
        pump(8, lambda: bm() if bm().get("ok") else None, "bmak")

        b = bm()
        if b.get("ok"):
            log(f"  bmak loaded (startTs={b.get('startTs')})")
        else:
            log("  bmak not detected — scripts may still be loading")

        # Let scripts settle — the first 2 sensor_data POSTs + sbsd POST
        # happen automatically within a few seconds
        log("Letting scripts settle (auto POSTs)...")

        def early_success():
            """Check if challenges resolved automatically."""
            c = ck()
            s = ps()
            if s.get("real"):
                return {"stage": "real-page", "c": c, "s": s}
            if c.get("bmsc") in ("2", "4"):
                return {"stage": "bmsc-passed", "c": c, "s": s}
            return None

        early = pump(8, early_success, "auto-settle")

        c = ck()
        log(f"After settle: abck={c['abck']}, bmsc={c['bmsc']}")

        # Log network
        net = net_summary()
        report["netAfterInit"] = net[-15:] if net else []
        posts = [r for r in net if r["m"] == "POST"]
        log(f"  Network: {len(net)} total, {len(posts)} POSTs")
        for p in posts:
            log(f"    POST {p['u'][:80]} -> {p['s']}")

        # ==============================================================
        # 4. Check if already resolved (sbsd often passes immediately)
        # ==============================================================
        if early and early["stage"] == "real-page":
            log("Already got real page! (both doors passed automatically)")
            report.update(method="auto", passed=True, finalSize=early["s"]["size"])
            report["final"] = {"size": early["s"]["size"], "abck": early["c"]["abck"],
                               "bmsc": early["c"]["bmsc"], "real": True}
            report["fullNet"] = net_summary()
            return

        if early and early["stage"] == "bmsc-passed":
            log(f"DOOR 2 already PASSED (bm_sc={early['c']['bmsc']})")
            log("Waiting for page reload to real page...")
            pump(20, lambda: ps() if ps().get("real") else None, "reload")
            s = ps()
            if s.get("real"):
                log(f"Real page obtained ({s['size']} bytes)")
                c = ck()
                report.update(method="auto-sbsd", passed=True, finalSize=s["size"])
                report["final"] = {"size": s["size"], "abck": c.get("abck"),
                                   "bmsc": c.get("bmsc"), "real": True}
                report["fullNet"] = net_summary()
                return
            else:
                log(f"Page reload didn't produce real page (size={s.get('size')})")

        # ==============================================================
        # 5. Mouse events (if sensor_data still needs them)
        # ==============================================================
        c = ck()
        if c.get("abck") != "0":
            log("Dispatching trusted mouse events (30 points)...")
            vw = int(PROFILE["viewport"]["width"])
            vh = int(PROFILE["viewport"]["height"])
            traj = gen_trajectory(vw, vh, 30)

            for i, (px, py, dt) in enumerate(traj):
                try:
                    ctx.dispatch_input("mousemove", target="document.documentElement",
                                       clientX=px, clientY=py,
                                       screenX=px, screenY=py + 80,
                                       button=0, buttons=0)
                except Exception:
                    pass
                if dt > 5:
                    adv(min(dt, 60))

            # mousedown + mouseup
            lx, ly = traj[-1][0], traj[-1][1]
            try:
                ctx.dispatch_input("mousedown", target="document.documentElement",
                                   clientX=lx, clientY=ly, button=0, buttons=1)
                adv(80)
                ctx.dispatch_input("mouseup", target="document.documentElement",
                                   clientX=lx, clientY=ly, button=0, buttons=0)
            except Exception:
                pass

            log("  Waiting for sensor upload...")

            def abck_ok():
                c = ck()
                return c if c.get("abck") == "0" else None

            pump(20, abck_ok, "abck-flip")

            c = ck()
            report["afterMouse"] = {"abck": c.get("abck"), "bmsc": c.get("bmsc")}
            log(f"After mouse: abck={c.get('abck')}, bmsc={c.get('bmsc')}")

            if c.get("abck") == "0":
                log("DOOR 1 PASSED: _abck -> ~0~")
        else:
            log("DOOR 1 already passed (abck=0)")

        # ==============================================================
        # 6. Wait for sbsd if not yet resolved
        # ==============================================================
        c = ck()
        if c.get("bmsc") not in ("2", "4"):
            log("Waiting for sbsd (bm_sc -> 2/4)...")

            def resolved():
                c = ck()
                s = ps()
                if s.get("real"): return {"stage": "real", "c": c, "s": s}
                if c.get("bmsc") in ("2", "4"): return {"stage": "ok", "c": c}
                if c.get("bmsc") == "3": return {"stage": "fail", "c": c}
                return None

            r = pump(30, resolved, "sbsd")

            if r and r["stage"] in ("ok", "real"):
                log(f"DOOR 2 PASSED: bm_sc={r['c'].get('bmsc')}")
                if r["stage"] != "real":
                    pump(15, lambda: ps() if ps().get("real") else None, "reload")
            elif r and r["stage"] == "fail":
                log("DOOR 2 FAILED: bm_sc=3")
                report["sbsdFailed"] = True
        else:
            log(f"DOOR 2 already passed (bmsc={c['bmsc']})")

        # ==============================================================
        # 7. Final
        # ==============================================================
        s = ps()
        c = ck()
        report["final"] = {"size": s.get("size", 0), "abck": c.get("abck"),
                           "bmsc": c.get("bmsc"), "href": s.get("href", ""),
                           "real": s.get("real", False)}
        log(f"FINAL: size={s.get('size')}, abck={c.get('abck')}, bmsc={c.get('bmsc')}, real={s.get('real')}")

        if s.get("real"):
            log(f"SUCCESS ({s.get('size')} bytes)")
            report.update(method="challenge-solved", passed=True, finalSize=s.get("size"))
        else:
            report.update(passed=False)
            log(f"Did not reach real page (size={s.get('size')})")

        # Full network log
        report["fullNet"] = net_summary()

        # Cookie jar
        try:
            report["cookies"] = ctx.export_cookies()
        except Exception:
            pass

        # Console log for debugging
        try:
            logs = ctx.console_log()
            report["consoleLog"] = [{"level": l.get("method",""), "text": l.get("text","")[:200]}
                                    for l in logs[-30:]]
        except Exception:
            pass


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default=DEFAULT_URL, help="Target URL")
    parser.add_argument("--timeout", type=float, default=180, help="Overall timeout (s)")
    parser.add_argument("--output", "-o", type=Path, help="JSON report file")
    parser.add_argument("--test-api", action="store_true", help="Test business API")
    parser.add_argument("--tls", default="160",
                        help="Firefox version (100-160) for the TLS ClientHello and the User-Agent")
    px = parser.add_mutually_exclusive_group()
    px.add_argument("--proxy", help="HTTP(S) proxy")
    px.add_argument("--direct", action="store_true", help="Direct (no proxy)")
    args = parser.parse_args(argv)

    proxy = None if args.direct else (args.proxy or auto_proxy())
    report = {
        "status": "error", "target": args.url,
        "transport": f"ruyijs-spidermonkey160+necko(tls={args.tls})",
        "proxy": proxy or "direct",
        "passed": False, "scriptErrors": [], "method": None,
    }

    t0 = monotonic()
    log("=" * 60)
    log(f"Akamai BM 3.x — ruyijs (SpiderMonkey 160 + Necko, Firefox {args.tls} TLS/UA)")
    log(f"Target: {args.url}")
    log(f"Proxy:  {report['proxy']}")
    log("=" * 60)

    try:
        run(args, report)
        report["status"] = "ok" if report["passed"] else "failed"
    except Exception as e:
        report["error"] = f"{type(e).__name__}: {str(e)[:400]}"
        log(f"ERROR: {report['error']}")

    report["elapsed"] = round(monotonic() - t0, 3)

    print("\n" + "=" * 60)
    if report["passed"]:
        log(f"PASSED in {report['elapsed']}s  method={report['method']}")
    else:
        log(f"FAILED after {report['elapsed']}s")
    print("=" * 60 + "\n")

    txt = json.dumps(report, ensure_ascii=False, indent=2, default=str)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(txt + "\n", "utf-8")
        log(f"Report -> {args.output}")
    print(txt[:5000] if len(txt) > 5000 else txt)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
