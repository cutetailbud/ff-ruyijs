# 共享引擎：多个 Context 共用一个进程

普通的 `Context` 会启动自己的 shell 进程（`js.exe`）和自己的 Necko 辅助进程。
`Engine` 只启动一个 shell 进程，在其中把你打开的每个 Context 作为一个**会话**
运行：每个会话一个线程、一个独立的 JSContext。会话之间并行运行，独立程度与各自
拥有进程的 Context 相同；它们共用的只是进程和引擎代码。

```python
from ruyijs import Engine

with Engine() as engine:
    a = engine.context(proxy="http://127.0.0.1:7890")
    b = engine.context(tls="120", timeout=30)
    a.navigate("https://example.com/")
    a.advance(2000)
    print(a.eval("document.title"), b.eval("navigator.userAgent"))
```

`engine.context(...)` 与 `Context(...)` 参数相同（`shell` 除外，由引擎决定），
`Context(engine=engine, ...)` 也一样。已有代码自己调用 `Context(...)` 的，放进
`use_engine` 即可变成引擎会话：

```python
from ruyijs import Engine, use_engine

with Engine() as engine, use_engine(engine):
    run_my_scraper()   # 其中从任意线程创建的每个 Context() 都是一个会话
```

每个会话可以在任意 Python 线程中使用，会话之间互不等待。

## 每个会话独占的

- 自己的 JavaScript 堆、页面、realm、iframe、Worker（仍是独立进程，使用该会话
  的代理和 TLS 配置）、时钟和事件循环；
- Cookie、存储、IndexedDB、Cache API、CORS 预检缓存；
- 宿主函数与数据（`expose`、`set`、`native`）；
- 完整的环境配置：navigator、screen、canvas 噪声、WebRTC、音频、字体、媒体特征、
  语言和设备像素比；
- 自己的代理和 TLS 配置、自己的 Necko 辅助进程及其连接和 TLS 会话缓存；
- 计时精度密钥和原生栈：递归深度与独立进程的 Context 相同。

会话看不到其他会话的全局变量、文档、Cookie 和宿主函数；除非用 `network=`（见下）
主动要求，服务器也无法通过共享连接把两个会话关联起来。

递归能有多深取决于进程栈的起点，而系统给每个进程的起点略有不同：Windows 的 ASLR
会把主线程栈的起点随机下移最多 2 KiB（约十几帧），macOS 还会从中划出进程的命令行
和环境变量。不同 Context 之间本来就相差这么多；同一引擎的所有会话都使用引擎进程的
那一份栈。

## 会话共享的

进程、引擎代码、SpiderMonkey 的自托管运行时数据和辅助线程，以及字体等操作系统
缓存。使用 `network=` 时，会话还会共享 Necko 辅助进程和连接。

内存分配不共享：每个会话线程都从自己的 arena 分配内存，和 Firefox 主线程一样，
所以同时构建页面的会话不会互相等待；会话结束时，它的空闲页归还给系统。

## 资源控制

| 选项 | 作用 |
| --- | --- |
| `Engine(prewarm=1)` | 每个新会话在后台预启动的 Necko 辅助进程数（0-2；Windows 上的普通 Context 预启动 2 个，Linux 和 macOS 则在第一次请求时启动）。之后都会按需启动更多；`engine.context(prewarm=2)` 可按会话设置。引擎会话使用这个数量，不用普通 Context 的默认值。 |
| `engine.context(network="名字")` | 使用相同名字的会话共享辅助进程、连接和 TLS 会话复用，类似同一个浏览器配置下的标签页：进程更少，但服务器可以关联这些会话。它们必须使用相同的 `proxy` 和 `tls`。 |
| `Engine(max_helpers=N)` | 所有会话的辅助进程总预算。用尽时，其他会话中最久未用的空闲辅助进程让出位置。 |

Windows 上简单页面、四个 Context 的实测：

| | 进程 | 私有内存 |
| --- | --- | --- |
| 四个 `Context` | 12（4 个 `js.exe`，8 个辅助进程） | 378 MiB |
| 一个 `Engine`，`prewarm=2` | 9（1 个 `js.exe`，8 个辅助进程） | 352 MiB |
| 一个 `Engine`（默认 `prewarm=1`） | 5（1 个 `js.exe`，4 个辅助进程） | 221 MiB |
| 一个 `Engine`，四个会话同一个 `network` | 3（1 个 `js.exe`，2 个辅助进程） | 151 MiB |

Context 的内存大部分是辅助进程和它自己的 JavaScript 堆，所以辅助进程相关的选项
节省最多；页面越重，每个会话的堆在两种模式下同样会增长。在 22 核机器上让 32 个
上下文同时加载一个含 2000 个列表项的页面：32 个 `Context` 用时 1.8 秒，96 个进程共
3.0 GiB 私有内存；32 个会话 2.2 秒，33 个进程共 1.7 GiB；32 个会话共用一个 `network`
3.5 秒（首次加载要启动共享的辅助进程，之后每次 0.3 秒），7 个进程共 0.88 GiB。Windows 上
这些进程都没有自己的控制台（`conhost.exe`）。

## 故障

- 会话中某个请求超时会抛出 `TimeoutError`；该会话正在运行的脚本被中断，会话结束，
  其他会话不受影响。
- `close()` 只结束一个会话；`Engine.close()` 关闭所有会话后再结束进程；
  `engine.status()` 列出仍在运行的会话。
- 引擎进程崩溃会结束它的所有会话。需要互不牵连时，请使用多个引擎或普通 Context。

## 在一个引擎中运行示例

```sh
python -m ruyijs.examples --shared akamai datadome creepjs truepeople
python -m ruyijs.examples --shared creepjs datadome -- --proxy http://127.0.0.1:7890
```

每个示例在自己的线程中作为同一个引擎的会话运行；`--` 之后的参数传给每个示例，
每行输出以示例名开头。

## 协议

`js.exe --ruyijs-host PIPE [--ruyijs-max-helpers N]` 连接控制管道，按与
`--ruyijs-rpc` 相同的分帧收发 JSON：

| 请求 | 回复 |
| --- | --- |
| `{"op": "open", "pipe", "mode", "proxy", "tls", "network", "prewarm"}` | `{"session": id}`；会话连接到 `pipe`，在其上提供 Context 的 RPC |
| `{"op": "status"}` | `[{"session", "running", "network"}]` |
| `{"op": "interrupt", "session": id}` | 中断该会话正在运行的脚本 |
| `{"op": "close"}` | 等待所有会话线程结束后进程退出 |
