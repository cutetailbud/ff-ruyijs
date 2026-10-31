<div align="center">

# 🦊 ruyiJS

### 在 Python 里跑任意站点的 JavaScript —— 用真正的 Firefox 引擎。

**新一代、专为 AI 驱动打造的极简浏览器。**<br>
原生的 Firefox JavaScript 引擎与网络栈，没有窗口、没有渲染器 ——<br>
专为网页抓取、JS 逆向、数据采集与绕过人机验证而生。

[![PyPI](https://img.shields.io/pypi/v/ruyijs?label=pypi&color=111111&logo=pypi&logoColor=white)](https://pypi.org/project/ruyijs/) [![Python](https://img.shields.io/badge/Python-%E2%89%A53.10-111111?logo=python&logoColor=white)](https://pypi.org/project/ruyijs/) [![License](https://img.shields.io/badge/license-AGPL--3.0%20%2B%20商业授权-111111)](LICENSE) [![Windows](https://img.shields.io/badge/Windows%20x64-0078D4?logo=windows&logoColor=white)](https://pypi.org/project/ruyijs/) [![LinuxDO](https://img.shields.io/badge/LinuxDO-社区-FFB003?logo=discourse&logoColor=white)](https://linux.do/t/topic/3078486)

<sub>赞助商</sub>

<a href="https://www.fastaitoken.com"><img src="assets/sponsor-fastaitoken.png" height="42" alt="FastAIToken"></a>

<sub>贡献者</sub>

<a href="https://github.com/Facetomyself" title="Facetomyself · #4 macOS arm64 外壳、#6 QUIC 配置"><img src="assets/contributors/Facetomyself.png" width="48" height="48" alt="Facetomyself"></a>&nbsp;&nbsp;<a href="https://github.com/Starktomy" title="Starktomy · #5 Linux 运行时"><img src="assets/contributors/Starktomy.png" width="48" height="48" alt="Starktomy"></a>

**[安装](#-安装)** &nbsp;·&nbsp; **[功能](#-功能)** &nbsp;·&nbsp; **[攻克的验证系统](#-攻克的验证系统)** &nbsp;·&nbsp; **[示例](#-运行示例)** &nbsp;·&nbsp; **[FAQ](#-常见问题)** &nbsp;·&nbsp; **[联系](#-联系方式)** &nbsp;·&nbsp; **[LinuxDO](https://linux.do/t/topic/3078486)** &nbsp;·&nbsp; [English](README.md) · **简体中文**

> **⚠️ 声明：** 本项目用于研究新一代 AI 浏览器，拒绝任何形式的暴力逆向破解。源码按 AGPL-3.0 开源；闭源或私有化使用须取得商业授权。

<br>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/architecture-dark.svg">
  <img src="assets/architecture.svg" width="880" alt="ruyiJS 架构：你的 Python 代码驱动一个 ruyiJS Context —— 独立进程，内含 SpiderMonkey JS 引擎、38 个浏览器上下文与 Necko 网络栈 —— 再与目标站点通信。">
</picture>

</div>

<br>

## ruyiJS 适合你吗？

Headless Chrome 和 Playwright 给你的是一整个浏览器：一个负责 GPU 渲染、管理窗口、吃内存的进程，而反爬厂商已经花了多年时间学习如何识别它。ruyiJS 走的是相反的路 —— 只嵌入与自动化真正相关的 Firefox 内核，其余一概不要：

- **原生引擎，而非模仿。** 货真价实的 SpiderMonkey（Firefox 159）与 Necko 网络栈，会像 Firefox 一样原样执行站点的 JavaScript、challenge、签名与指纹。
- **真正无头。** 没有窗口、没有布局、没有渲染器。纯 JavaScript + Web API 环境，启动快、占用低。
- **隔离且可复现。** 每个 `Context` 都是独立进程，拥有持续保存的 JavaScript 世界与自己的代理，会话之间互不干扰。
- **需要时可离线。** 自己提供脚本与响应，无网络也能复跑整条已捕获的流程。

如果你做网页抓取、逆向 Web JavaScript，或采集 challenge 背后的数据，它就适合你。如果普通 HTTP 客户端已经能拿到页面，那就不需要它。

Windows x64 以 wheel 发布，需要 **Python 3.10+**（已在 Python 3.14 下验证）。macOS arm64 可以源码构建，fork Release 上也有 wheel；donor 布局和仍然留空的表面写在 [docs/macos.zh-CN.md](docs/macos.zh-CN.md)。

## 📦 安装

```sh
python -m pip install ruyijs
```

**系统要求：** Windows x64（64 位）+ Python 3.10+，已在 Python 3.10 – 3.14 下验证。

Windows 的 wheel 内置 SHA256 锁定的 SpiderMonkey + Necko 运行时（约 42 MB），安装后即可离线运行，无需下载浏览器，无外部依赖。

**macOS arm64** 不使用这个 PyPI wheel。它用 Python 3.12 从源码构建，并依赖放在仓库外面的两份官方 Firefox donor，也可以安装维护者 fork Release `v0.2.1` 上的 `macosx_26_0_arm64` wheel。这个 wheel 不在 PyPI。这一轮查过的页面路径是直连 HTTPS 上的 HTTP/2。ALPN 为 `http/1.1` 时会写 HTTP/1.1，这一段做过单元测试，没有对活的源打开过。步骤和未完成的表面见 [docs/macos.zh-CN.md](docs/macos.zh-CN.md)。

升级：
```sh
python -m pip install --upgrade ruyijs
```

验证安装：
```python
import ruyijs
print(ruyijs.__version__)   # 0.2.1
```

## 🚀 快速开始

**1 · 打开上下文，执行 JavaScript。** 同一上下文内，变量和函数在多次 `eval` 之间保留。

```python
from ruyijs import Context

with Context() as context:
    context.eval("let count = 40")
    print(context.eval("count += 2"))        # 42
    print(context.eval("navigator.userAgent"))
```

**3 · 对准真实目标。** 配置环境、加载页面，让站点自身的 JavaScript 去生成它所需的 token。

```python
with Context(proxy="http://127.0.0.1:7890") as context:
    context.configure_fingerprint({
        "navigator": {"language": "zh-CN", "hardwareConcurrency": 8},
        "locale": {"timezone": "Asia/Shanghai"},
    })
    context.load_page("https://target.example/", html)
    token = context.eval("window.__sign(payload)")   # 调用页面自己的签名函数
    print(token)
```

## 🏗️ 整体结构

```
Python  ──►  ruyiJS Context（独立进程）  ──►  目标站点
                 ├─ SpiderMonkey —— Firefox 159 JS 引擎
                 ├─ 38 个浏览器上下文 —— DOM · Canvas · Worker · iframe · …
                 ├─ Skia —— Firefox 159 2D 图形，Canvas 的绘制后端
                 └─ Necko —— HTTP/1.1 · 2 · 3，按上下文独立代理
```

你的 Python 代码驱动一个 `Context`。在其中，原生 SpiderMonkey 引擎针对真实 DOM 与 38 个受限浏览器子系统运行页面脚本，页面内的 `fetch`/XHR 走 Necko 传输。实现代码位于 [`src/`](src)（领域层、应用层与各浏览器子系统）与 [`python/ruyijs/`](python/ruyijs)（Python 绑定）。

## ✨ 功能

### 运行站点的 JavaScript

在持续保存的上下文中执行脚本，加载含 iframe 与经典脚本的真实 HTML，并查询生成的 DOM。对存活 JS 对象的引用会作为可从 Python 调用的 `JSObject` / `JSFunction` 返回。

| 功能 | 用法 |
| :-- | :-- |
| 执行 JS 并保留状态 | `context.eval(src)` —— 变量和函数持续保留 |
| 取回可调用对象 | `context.eval(src, to_py=False)` → `JSFunction` |
| 加载页面 | `context.load_page(url, html)` —— 相对地址按 `url` 解析 |
| 顶层导航 | `context.navigate(url)` —— 无 Referer，`Sec-Fetch-Site: none` |
| 查询 DOM | `context.eval("document.querySelector('#x').textContent")` |

```python
with Context() as context:
    context.set("config", {"offset": 2})
    add = context.eval("(value) => value + config.offset", to_py=False)
    print(add(40))  # 42
```

### 配置浏览器环境

通过一套带校验的 profile 接口设置 Navigator、Screen、视口、时区与区域。请在首个页面脚本运行前提交。

```python
with Context() as context:
    context.configure_fingerprint({
        "navigator": {"language": "zh-CN", "hardwareConcurrency": 8},
        "screen": {"width": 1920, "height": 1080},
        "viewport": {"width": 1280, "height": 720},
        "locale": {"timezone": "Asia/Shanghai"},
    })
    print(context.eval("[navigator.language, screen.width]"))  # ['zh-CN', 1920]
```

### 基于 Necko 的原生网络

页面内 `fetch` 与 XHR 走真实 HTTP/1.1、2、3，并通过按上下文隔离的代理。用 `put_resource()` 自行提供响应即可离线复跑整条已捕获的流程。用 `net_log()` 查看每一次请求。macOS 源码构建这一轮查过的页面路径是直连 HTTPS 上的 HTTP/2，界限见 `docs/macos.zh-CN.md`。

### 原生 TLS 指纹 —— 自由切换 Firefox 100 – 159

网络栈使用 Firefox 自身的 **Necko + NSS**（与真实 Firefox 完全一致的 TLS 库）。TLS ClientHello 中的密码套件、扩展、ALPN、密钥共享及其排列顺序均由真实 NSS 库生成，而非模拟，基于 TLS 指纹检测的反爬系统无法区分 ruyiJS 与真实 Firefox 的流量。

Necko 传输层可**自由切换 Firefox 100 – 159 的 TLS 配置**，每个版本均精确复现该版本的 ClientHello，在线上呈现不同的 JA3 / JA4 指纹——可用于轮换浏览器身份、规避按版本封禁，或测试站点对不同 Firefox 版本的反应。

每个 Context 用 `tls=` 选择版本（`"100"`–`"159"`，默认 159）。在 Windows 上，这会改变该 Context 的 Necko 传输层的 TLS ClientHello、HTTP/2 设置和 HTTP/3 优先级；如需页面内一致，请用 `configure_fingerprint` 设置对应版本的 User-Agent。在 macOS 上，库报告 NSS 3.131 时，直连 HTTPS 走 arm64 `libnecko_core.dylib`。外壳先找 `dist/necko_tls/libnecko_core.dylib`，再找暂存的 `dist/necko` 副本。已经设置的 `NECKO_CORE_LIBRARY` 优先。`Context()` 和 `Context(tls="100")` 都打开了 `https://example.com/`，状态 200，协议 `h2`。测到的握手是 159 为 15 个套件、5 个组、3 个 key share，100 为 17、6、2。簇边界 100、119、132、135、151、154、156、159 的库内 GET 也返回 200 和 `h2`，个数与配置表一致。其余版本没有逐个当页面打开。调用方没写的请求头和 HTTP/2 设置跟随 0.2.1 产品表里这个 `tls=` 的值。调用方写了的头优先。这份表里的 HTTP/3 字段不用，HTTP/3 也没有变。代理、库缺失或 NSS 版本不同仍走 donor xpcshell。这次没有重测代理。明文 HTTP 仍从这份 donor 返回 `necko 804b0033`（`docs/tls-profile.zh-CN.md`）。

```python
with Context(proxy="http://127.0.0.1:7890", timeout=30, tls="120") as context:
    context.load_page("https://example.com/", "")
    context.eval("globalThis.r = null; fetch('/api').then(x => x.json()).then(v => r = v)")
    context.advance(25)               # 推进事件循环直到稳定
    print(context.eval("r"))
```

### Canvas 使用 Firefox 原生 Skia

Canvas 2D 使用 Firefox 官方的 **Skia**（Firefox 159 正式版的 `gfx/skia`）绘制，而不是重新实现。

### Python ↔ JavaScript 互通

用 `set()` 传入数据，取回可调用的 JS 函数，并把 Python 函数暴露给运行中的脚本。含 Worker、Service Worker 与 iframe。

```python
with Context() as context:
    context.expose(lambda a, b: a + b, "add")
    print(context.eval("add(20, 22)"))  # 42
```

### 确定性时钟

用逻辑时钟驱动定时器以获得完全可复现的运行，或用系统时钟进行实时作业。时间由宿主显式推进。

```python
with Context() as context:
    context.configure_clock("logical", epoch_ms=1700000000000)
    context.eval("globalThis.result = 0; setTimeout(() => { result = 42 }, 10)")
    context.advance(10)
    print(context.eval("result"))  # 42
```

### 错误与日志

JavaScript 异常会作为带 name、message、stack 的 `JSError` 抛出。`console_log()` 与 `net_log()` 返回已捕获的 console 和网络记录。

```python
from ruyijs import Context, JSError

with Context(timeout=10) as context:
    try:
        context.eval("throw new TypeError('bad input')", filename="demo.js")
    except JSError as error:
        print(error.name, error.message)  # TypeError bad input
        print(error.stack)
```

## 🛡️ 攻克的验证系统

每个示例都驱动站点自身的 JavaScript 生成其所需参数，再端到端完成流程。全部随包可运行：`python -m ruyijs.examples <名称>`。

| 系统 | 类型 | 示例 |
| :-- | :-- | :-- |
| **Akamai Bot Manager 3.x** | sensor_data + sbsd 挑战 | `akamai` |
| **DataDome** | interstitial device-check（plv2 + plv3） | `datadome` |
| **CreepJS** | 指纹审计 —— 全部项目通过 | `creepjs` |

> 🔒 **严禁用于任何非法用途，仅用于研究新时代 AI 浏览器。** 线上结果取决于当前网络状况与各站点的实时响应。

## 🧪 运行示例

示例随包安装，无需下载仓库：

```sh
python -m ruyijs.examples                      # 列出全部示例及其安装路径
python -m ruyijs.examples akamai               # Akamai Bot Manager 3.x
python -m ruyijs.examples datadome             # DataDome interstitial device-check
python -m ruyijs.examples creepjs              # CreepJS 指纹审计
```

示例均支持 `--direct`（直连）和 `--proxy http://127.0.0.1:7890`，在名称后加 `--help` 查看全部参数，`--output result.json` 可保存结果。各示例目录内附 README。

## 📚 文档与 API

| | |
| :-- | :-- |
| **`Context.eval` / `set` / `expose`** | 执行 JS、传入数据、暴露 Python 回调 |
| **`load_page` / `navigate` / `put_resource`** | 加载 HTML、顶层导航、离线提供资源 |
| **`configure_fingerprint` / `configure_environment`** | Navigator、Screen、视口、时区、区域 |
| **`configure_clock` / `advance` / `tick`** | 逻辑时钟 vs 系统时钟，推进事件循环 |
| **`JSError` / `console_log` / `net_log`** | 异常、console 与网络记录 |
| **`JSObject` / `JSFunction` / `get_runtime_path`** | 存活 JS 句柄、内置运行时路径 |

## 📏 使用范围与限制

ruyiJS 在 **Windows x64** 上提供 JavaScript 执行及广泛的 Web API 环境，**macOS arm64** 则通过源码构建或 fork Release 的 wheel 提供同一外壳。它**不含浏览器窗口，也没有完整的页面布局与渲染** —— 它是面向自动化、分析与研究的工具。请在遵守目标站点服务条款与适用法律的前提下使用。macOS 与 Windows 的比对和后续计划见 [docs/macos.zh-CN.md](docs/macos.zh-CN.md)。

**相关项目：** [**ruyiHttp**](https://gitee.com/losenine888/ruyiHttp) —— 基于 Firefox 原生网络组件的 Requests 风格 Python 请求库。

## ❓ 常见问题

<details>
<summary><b>支持哪些 Python 版本？</b></summary>

Windows x64（64 位）上的 Python 3.10 及以上版本，已测试至 Python 3.14。macOS 源码构建的 Necko helper 使用 Python 3.12。
</details>

<details>
<summary><b>支持 macOS 或 Linux 吗？</b></summary>

macOS arm64 可以用源码构建，也可以打 `macosx_26_0_arm64` wheel。这个 wheel 不上传到 PyPI。维护者 fork 的 Release `v0.2.1` 放着这份 wheel 和 `ruyijs-runtime-macos-arm64.zip`。和 Windows 的比对以及后续计划写在 [docs/macos.zh-CN.md](docs/macos.zh-CN.md)。Linux x86_64 提供实验性源码构建，直接复用 macOS 的 POSIX 层；见 [docs/linux.md](docs/linux.md)。

JS 引擎是 Mozilla 官方的 **SpiderMonkey**，网络库是 Mozilla 官方的 **Necko**。两者都从 Firefox 源码树编译，并且放在本仓库外面。
</details>

<details>
<summary><b>需要下载浏览器吗？</b></summary>

Windows 不需要。wheel 内置锁定版 SpiderMonkey + Necko 运行时，`pip install ruyijs` 后即可离线运行。macOS 先在本仓库旁边编译两份 donor，之后 `setup.py` 可以打出本地 wheel，安装后从包内找到 `dist/js` 和 `dist/necko/xpcshell`。见 [docs/macos.zh-CN.md](docs/macos.zh-CN.md)。PyPI 上的 wheel 是 Windows 运行时。
</details>

<details>
<summary><b>Context 卡住无响应</b></summary>

- 确保使用 `with` 语句（`with Context() as ctx:`），以正确清理进程。
- 如果使用代理，请检查代理是否可达：`Context(proxy="http://127.0.0.1:7890")`。
- 增大超时：`Context(timeout=60)`。
- 在 Wine 上运行请升级至 ≥ 0.1.5，该版本修复了已知的 navigate 死锁。
</details>

<details>
<summary><b>JSError: 脚本报错</b></summary>

站点的 JavaScript 抛出了异常。检查 `error.name`、`error.message` 和 `error.stack` 定位原因。如果页面依赖尚未实现的 API，堆栈中会指向缺失的调用。
</details>

<details>
<summary><b>如何使用代理？</b></summary>

```python
with Context(proxy="http://127.0.0.1:7890") as ctx:
    ctx.navigate("https://example.com/")
```

支持 HTTP、HTTPS、SOCKS5 代理，按上下文隔离。
</details>

<details>
<summary><b>如何运行示例？</b></summary>

```sh
python -m ruyijs.examples              # 列出全部示例
python -m ruyijs.examples akamai --proxy http://127.0.0.1:7890
python -m ruyijs.examples creepjs
```
</details>

## 💬 联系方式

二次分发、商业授权、技术交流，欢迎添加作者微信，或扫码加入交流群。

<table>
<tr>
<td align="center"><img src="assets/wechat-qr.jpg" alt="微信 — 如意" width="220"><br><b>微信：</b>如意</td>
<td align="center"><img src="assets/wechat-group-qr.jpg" alt="交流群 — AI浏览器ruyijs过检测交流群3" width="220"><br><b>交流群：</b>AI浏览器ruyijs过检测交流群3</td>
</tr>
</table>

> 群二维码会定期刷新；如已过期，请添加作者微信（如意）邀请入群。

## 📄 许可证

ff-ruyijs 采用**双许可**：

- **[AGPL-3.0](LICENSE)**：任何用途（包括商业用途）均可免费使用，前提是遵守协议：分发本项目或其修改版本，**或通过网络向用户提供服务**（托管服务、API）时，必须按 AGPL-3.0 公开你所用版本的完整源码。
- **[商业授权](COMMERCIAL-LICENSE.md)**：用于闭源产品或服务，或不希望公开修改时，必须取得作者授权。

商业授权请添加作者微信（**如意**）或在 [GitHub](https://github.com/cutetailbud/ff-ruyijs) 提交标题为 "Commercial license" 的 issue。

源自 Mozilla 的代码（SpiderMonkey、Necko）仍为 MPL-2.0。0.2.1 及以前的版本保持其发布时的许可证。

第三方声明见 [`NOTICE`](NOTICE)。

如果 ff-ruyijs 帮你省下了重写一个浏览器的功夫，点个 ⭐ 能让更多人发现它。
