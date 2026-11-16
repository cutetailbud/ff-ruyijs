"""Wildberries 卖家页（wbaas / Guardium antibot）：ruyijs+necko 原生环境走完挑战，
拿到 x_wbaas_token 并读出 risk_score，卖家页从 498 变为 200（issue #42）。

python wildberries.py
python wildberries.py --proxy http://127.0.0.1:7897 --output wildberries.json
python wildberries.py --seller 250197667 --direct

流程：navigate → 卖家页 498（挑战页）→ find-frontend-settings → create-token 498
（下发 challenge）→ create-token 200（提交 solution，写入 x_wbaas_token）→ 带
token 重新请求卖家页 200。token 第 4 段 base64 解码后的首字段就是 risk_score：
同一网络下真实 Firefox 160 为 15，ruyijs 0.4.1 起也是 15；0.4.0 恒为 100，
卖家页一直 498，三轮后挑战脚本报 "Get token tries exceeded"。

判定全部取自 net_log、cookie 与页面自身状态：脚本不注入、不替换任何 API。
risk 由站点按出口 IP 与环境综合打分，换网络后数值可能不同。
"""

import argparse
import base64
import json
import os
import socket
import sys
import time
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
if (ROOT / "dist" / ("js.exe" if os.name == "nt" else "js")).is_file() or os.environ.get("RUYIJS_SHELL"):
    sys.path.insert(0, str(ROOT / "python"))
from ruyijs import Context, JSError

SELLER = "250197667"
TOKEN_COOKIE = "x_wbaas_token"
API = "/__wbaas/challenges/antibot/api/v1/"


def auto_proxy():
    for name in ("HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy"):
        if os.environ.get(name):
            return os.environ[name]
    for port in (7897, 7890):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=.3):
                return f"http://127.0.0.1:{port}"
        except OSError:
            continue
    return None


def read_token(context):
    """x_wbaas_token 的 risk_score 与 UA（第 4 段 base64：risk|ip|ua|...）。IP 不外泄。"""
    cookies = context.export_cookies()
    for cookie in (cookies if isinstance(cookies, list) else cookies.get("cookies", [])):
        if cookie.get("name") != TOKEN_COOKIE:
            continue
        parts = cookie.get("value", "").split(".")
        if len(parts) < 4:
            return {"present": True}
        payload = parts[3].replace("-", "+").replace("_", "/")
        try:
            fields = base64.b64decode(payload + "=" * (-len(payload) % 4)).decode("utf-8", "replace").split("|")
        except ValueError:
            return {"present": True}
        risk = int(fields[0]) if fields and fields[0].isdigit() else None
        return {"present": True, "risk": risk, "userAgent": fields[2] if len(fields) > 2 else None}
    return {"present": False}


def classify(log, seller_url):
    """把 net_log 归约成挑战各阶段的状态序列。"""
    seller_path = urlsplit(seller_url).path
    flow = {"seller": [], "createToken": [], "frontendAnalytics": [], "report": []}
    for row in log:
        path = urlsplit(row.get("url", "")).path
        if path == seller_path:
            flow["seller"].append(row.get("status"))
        elif path.startswith("/__wbaas/") and path.endswith(API + "create-token"):
            flow["createToken"].append(row.get("status"))
        elif path.endswith(API + "frontend-analytics"):
            flow["frontendAnalytics"].append(row.get("status"))
        elif path.endswith(API + "report"):
            flow["report"].append(row.get("status"))
    return flow


def run(args, report):
    url = f"https://www.wildberries.ru/seller/{args.seller}"
    report["url"] = url
    deadline = time.monotonic() + args.timeout
    proxy = None if args.direct else (args.proxy or auto_proxy())
    report["proxy"] = proxy
    with Context(proxy=proxy or None, timeout=args.timeout) as context:
        def call(method, *values, **kwargs):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("等待页面超时")
            context.timeout = max(1, remaining)
            return method(*values, **kwargs)

        call(context.configure_clock, "system")
        started = time.monotonic()
        try:
            call(context.navigate, url)
        except JSError as error:
            report["scriptErrors"].append({"name": error.name, "message": error.message[:300]})
        while time.monotonic() < deadline:
            try:
                call(context.advance, 1500, max_tasks=2000)
            except JSError as error:
                entry = {"name": error.name, "message": error.message[:300]}
                if entry not in report["scriptErrors"] and len(report["scriptErrors"]) < 10:
                    report["scriptErrors"].append(entry)
            flow = classify(call(context.net_log), url)
            token = call(lambda: read_token(context))
            report["flow"], report["token"] = flow, token
            report["htmlLength"] = call(context.eval, "document.documentElement.outerHTML.length")
            report["seconds"] = round(time.monotonic() - started, 1)
            if 200 in flow["seller"] and report["htmlLength"] > 100000:
                report["passed"] = True
                report["stage"] = "seller_page_200"
                return
            if flow["report"] and flow["seller"].count(498) >= 3:
                # 挑战脚本三轮后上报 "Get token tries exceeded"，停在拦截页。
                report["stage"] = "token_rejected"
                return
        report["stage"] = "timeout"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seller", default=SELLER, help="卖家 ID（默认 issue #42 的 250197667）")
    parser.add_argument("--timeout", type=float, default=90, help="单次尝试期限，秒")
    parser.add_argument("--retries", type=int, default=1,
                        help="未通过时用新会话重试的次数（偶发的请求无响应、出口抖动）")
    parser.add_argument("--proxy", help="代理 URL（http/https/socks5；默认 env，再探测本地端口）")
    parser.add_argument("--direct", action="store_true", help="不使用代理")
    parser.add_argument("--output", type=Path, help="把 JSON 结果同时写入此文件")
    args = parser.parse_args(argv)
    report = {"passed": False, "stage": "not-run", "scriptErrors": []}
    for attempt in range(1, args.retries + 2):
        report["attempt"] = attempt
        try:
            run(args, report)
        except Exception as error:
            report.update(passed=False, error=type(error).__name__ + ": " + str(error))
        if report["passed"] or attempt > args.retries:
            break
        report.setdefault("attempts", []).append(
            {"attempt": attempt, "stage": report["stage"], "error": report.pop("error", None),
             "seller": report.get("flow", {}).get("seller")})
        report.update(stage="not-run", scriptErrors=[], flow={}, token={})
    text = json.dumps(report, ensure_ascii=False, indent=2)
    print(text)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
