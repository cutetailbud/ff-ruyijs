# Wildberries 卖家页（wbaas / Guardium antibot）

ruyijs+necko 原生环境走完 Wildberries 的 wbaas 挑战，拿到 `x_wbaas_token` 并读出
token 里的 risk_score，卖家页从 498 变为 200。对应 issue
[#42](https://github.com/cutetailbud/ff-ruyijs/issues/42)。

```bash
python -m ruyijs.examples wildberries
python wildberries.py --proxy http://127.0.0.1:7897 --output wildberries.json
python wildberries.py --seller 250197667 --direct
```

## 流程与判定

```
navigate → /seller/<id> 498（挑战页）→ find-frontend-settings 200
→ create-token 498（下发 challenge）→ create-token 200（提交 solution，写入 x_wbaas_token）
→ /seller/<id> 200（带 token）→ seller_page_200
```

`x_wbaas_token` 的第 4 段 base64 解码后形如 `risk|ip|ua|...`，首字段就是
risk_score。输出里的 `token.risk` 即这个值（IP 不写进结果）。

| 版本 | risk | 卖家页 |
|---|---:|---|
| 真实 Firefox 160（同一网络） | 15 | 200 |
| ruyijs 0.4.1 起 | 15 | 498 → 200 |
| ruyijs 0.4.0 及以前 | 100 | 一直 498，三轮后 `Get token tries exceeded` |

判定全部来自 net_log、cookie 与页面自身状态，脚本不注入任何 API。

## 0.4.0 为什么是 100

挑战脚本在加密前测量页面布局并上报。和真实 Firefox 160 逐项对比，差异全在布局：
`getComputedStyle` 对已渲染元素返回 `calc()`/百分比而不是像素、`white-space: pre`
合并空格、窄容器里文本不换行、float 和表格撑满容器、`aspect-ratio` 不生效、
`<progress>` 的属性抛错。0.4.1 修复后，指纹明文除窗口尺寸和计时外与 Firefox 一致。
与 IP、TLS、canvas、音频、字体无关；`configure_fingerprint` 的窗口尺寸也不影响分数。

## 已知的环境因素

risk 由站点按出口 IP 与环境综合打分，换网络后数值可能不同。`stage` 为
`token_rejected` 时先换 `--proxy` 出口再排查环境。
