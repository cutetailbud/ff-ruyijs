# macOS arm64 维护说明

从 0.3.0 起，`macosx_26_0_arm64` wheel 已发布到 PyPI。它由 GitHub Actions 从 `firefox/` 子模块构建（`.github/workflows/build-macos.yml`）：SpiderMonkey、Skia 和 NSS 来自这份源码，`libnecko_core.dylib` 和 `libnecko_quic.dylib` 链接这份 NSS。这个 wheel 不带 xpcshell。没有该二进制时，`Context(proxy=...)` 先发 HTTP CONNECT，再进入 `libnecko_core`。下面的 donor 树路径仍带 xpcshell，代理继续走 Necko 偏好。

同一份事实的英文版在 [macos.md](macos.md)。

## 这个移植加了什么

Python API、DOM、时钟和页面逻辑仍是同一份源码。macOS 增加的是外壳：Unix socket 帧、xpcshell Necko helper、Core Text、ImageIO，以及浏览器 donor 里 Firefox 自己的 Skia 对象。C 客户端的安装名是 `@rpath/libruyijs_client.dylib`。

- 同一套 API：JS 上下文、DOM、Python 桥、WebCrypto 算法，以及静态图格式。
- 可以接受的平台差异：命名管道或 Unix socket，CNG 或 CommonCrypto，WIC 或 ImageIO，DirectWrite 或 Core Text。GIF disposal、字形 gamma 和中文回退跟随系统解码器，不要求与 Windows 像素一致。
- 这次仍不做：socket process 保持关闭。只打开偏好时，这份 xpcshell 在填写 Mac sandbox 参数时以信号 11 退出。donor 的 `plugin-container.app` 由编 `ipc/app` 得到。直连 HTTPS 经 `libnecko_core.dylib` 跟随 `Context(tls=)`，握手个数和 0.2.1 的请求头、HTTP/2 设置表都跟着走。页面复查了 100 和 159。八个边界版本是库内 GET，不是页面。有 xpcshell 时代理仍走它。没有时，HTTPS 走 CONNECT，明文走绝对形式请求；HTTP/3 不走代理。DNS、连接和 TLS 计时保持 0。PyPI 上的 wheel 不带 xpcshell。`macos.url` 指向 fork Release 里的压缩包。直连明文和 HTTP/3 写在 `docs/closed-route.zh-CN.md`。

## 各部分放在哪

macOS 上的 ruyijs 只做外壳。SpiderMonkey 和 Necko/NSS 来自同一份官方 Firefox 树。这棵树放在本仓库外面，不提交进仓库。

| 部分 | 来源 |
| --- | --- |
| `dist/js`、`dist/libmozglue.dylib` | SpiderMonkey standalone donor，再经 `tools/build.py` |
| Necko/NSS 回退与代理 | 浏览器 donor 编出的 `xpcshell` |
| `libnecko_core.dylib` | `native/necko_tls/`，链到 donor NSS 3.131。直连 HTTPS，以及明文套接字和 HTTP/1 交换。查找顺序是 `dist/necko_tls`，然后 `dist/necko` |
| `libnecko_quic.dylib` | `native/necko_quic/`，使用 Firefox 159 vendor 里的 Neqo 0.32.0，链到 donor NSS 3.131。Darwin 上用 `tools/build_necko_quic.py` 编译。这个 dylib 不提交 |
| 帧管道、进程启动、C ABI 客户端 | 本仓库 |
| Python helper venv | `vendor/necko-derived/python`，已被 gitignore。wheel 使用安装它的那个 Python |

维护者的目录是并列的两份：

```
firefox/                 官方 Firefox 源码，不属于 ruyijs
  obj-js-standalone/     SpiderMonkey standalone
  obj-necko/             编出 xpcshell 的浏览器构建
ff-ruyijs/               本仓库
```

xpcshell 的默认路径是本仓库的上一级加上 `firefox-159/obj-necko/dist/bin/xpcshell`。Firefox 目录如果换了名字，就设置 `RUYIJS_XPCSHELL`。装好的 wheel 会先找运行时里的 `dist/necko/xpcshell`，找不到才用 donor 路径。帧脚本默认是 `src/resource_network/necko_fetch.js`，由 client 向上查找仓库根得到；安装后的 client 则使用旁边的 `necko_fetch.js`。需要换位置时设置 `RUYIJS_NECKO_SCRIPT`。

standalone 的 objdir 用 `tools/build.py --donor-obj` 传入。对应的环境变量是 `RUYIJS_DONOR_OBJ`。只在这一次构建里传入。不要把它写进长期的 shell 配置。

## 编译 donor

使用 Firefox 标签 `FIREFOX_159_0_RELEASE`。两份 objdir 都保留，以后的重编在原目录里继续。

SpiderMonkey standalone 的 `mozconfig-js`：

```
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-js-standalone
ac_add_options --enable-project=js
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --enable-rust-simd
```

给 xpcshell 用的浏览器 donor，`mozconfig-necko`：

```
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-necko
ac_add_options --enable-application=browser
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --disable-nodejs
ac_add_options --enable-rust-simd
ac_add_options --without-wasm-sandboxed-libraries
```

`--disable-nodejs` 会让 newtab 的包编不完。helper 不用这个包。`obj-necko/dist/bin/xpcshell` 能启动就可以停。链接出来的 XUL 是 `obj-necko/dist/bin/XUL`。

helper 关闭 socket process。client 设置 `MOZ_DISABLE_SOCKET_PROCESS=1`。`necko_fetch.js` 把 `network.process.enabled` 和 `network.http.network_access_on_socket_process.enabled` 设为 false。这些都保持原样。

## 编译外壳和 helper

helper venv 使用 Python 3.12。`tools/bootstrap_macos_helper.py` 先在 `PATH` 里找 `python3.12`，再看 `~/.local/bin`、Homebrew 和 `/usr/local/bin`。都没有就停下来。`vendor/necko-derived/python/pyvenv.cfg` 已经存在时，脚本保留这个 venv，只安装导入失败的包。

在仓库根目录执行，链接外壳时带上 `MACOSX_DEPLOYMENT_TARGET=26.0`：

```sh
python3.12 -B tools/bootstrap_macos_helper.py
python3.12 -B tools/build.py --donor-obj ../firefox-159/obj-js-standalone
```

bootstrap 安装 `requests`、`brotli`、`zstandard`、`h2`、`cryptography`、`backports.zstd` 和 `publicsuffixlist`。缺的 wheel 只来自 gitignore 的 `vendor/necko-derived/wheels`，用 `pip --no-index`。用 `curl --noproxy '*'` 从一个 simple index 镜像把 wheel 放进这个目录。目录是空的且还有缺包时，脚本停下来。脚本里不写代理地址。这一轮没有再跑 bootstrap。当前 helper 已经能导入 `publicsuffixlist`。`firefox-suffixes.dat` 放在 `necko_win/_firefox.py` 旁边，随这个目录一起复制。直连 HTTPS 用 `h2` 和 `cryptography` 配合这个库。`requests` 仍负责准备 Windows 上 oneshot 已经在用的那一帧。有 xpcshell 时代理仍走它。没有时，HTTPS 走 CONNECT，明文走绝对形式请求。非 HTTP 仍走 donor xpcshell。

`tools/build.py` 随后把已跟踪的源码复制进 venv：

| 源码 | 安装位置 |
| --- | --- |
| `src/resource_network/necko_oneshot.py` | `ruyijs_necko/oneshot.py` |
| `src/resource_network/necko_ruyi_init.py` | `ruyijs_necko/__init__.py` |
| `src/resource_network/necko_timing.py` | `ruyijs_necko/timing.py` |
| `src/resource_network/necko_client.py` | `necko/client.py` |
| `src/resource_network/necko_adapters.py` | `necko/adapters.py` |
| `src/resource_network/necko_pkg_init.py` | `necko/__init__.py` |
| `src/resource_network/necko_core_client.py` | `necko/core_client.py` |
| `src/resource_network/webtransport_worker.py` | `ruyijs_necko/webtransport_worker.py` |
| `src/resource_network/webtransport_wire.py` | `ruyijs_necko/webtransport_wire.py` |
| `src/resource_network/webtransport_verify.py` | `ruyijs_necko/webtransport_verify.py` |
| `src/resource_network/webtransport_session.py` | `ruyijs_necko/webtransport_session.py` |
| `src/resource_network/webtransport_connection.py` | `ruyijs_necko/webtransport_connection.py` |
| `native/necko_tls/necko_win/` | helper 导入路径上的 `necko_win/` |

Windows 上同一次构建只复制 `oneshot.py`。Darwin 的 venv 不存在时，外壳仍然完成链接，也不会另造一个发布包。

外壳二进制是 `dist/js`。跑示例时把 `RUYIJS_SHELL` 指到它，并把 `python/` 放进 `PYTHONPATH`。客户端库是 `libruyijs_client.dylib`。

本地 wheel 的标签是 `macosx_26_0_arm64`。外壳放在 `_runtime/dist/js`，donor 的 xpcshell 闭包复制到 `_runtime/dist/necko`。库文件存在时，一并复制 `libnecko_quic.dylib`。wheel 里还有 `necko/closed_route.py`、`necko/core_client.py` 和 `necko_win`。wheel 和 `ruyijs-runtime-macos-arm64.zip` 放在维护者 fork 的 Release 上，不放上 PyPI。重新生成压缩包用 `python3.12 -B tools/macos_runtime.py <zip路径>`，再把打印出的 size、sha256 和 Release URL 写进 `runtime.json` 的 `macos`。

## 一次请求怎么走

长度前缀帧和 Windows 相同。macOS 上的管道是 Unix socket。外壳启动 `python -I -B -m ruyijs_necko.oneshot`。直连 HTTPS 使用 `libnecko_core.dylib`。ALPN `h2` 使用产品的 HTTP/2 设置。ALPN 为 `http/1.1` 时在这条套接字上写 HTTP/1.1。其他 ALPN 表示不了，路由可以回退。有 xpcshell 时代理仍走它；没有时，HTTPS 走 CONNECT，明文走绝对形式请求。直连 `http://` 走 `libnecko_core` 里的套接字和 HTTP/1 交换。直连 `https://` 走 `libnecko_core`，直到同一进程记住了响应里的 `Alt-Svc`，下一次到该来源的请求才走 `libnecko_quic`。证书校验失败不回退。不保存 cookie。`redirectionLimit` 仍是 0，xpcshell 路径上也是，所以 3xx 就作为这次的响应返回。同一个 helper 进程里，同源 HTTP/2 可以复用一条连接。页面外壳仍是每个请求启动一次 oneshot，除非使用 `--session`。

直连 HTTPS 的页面验收过 HTTP/2，版本码 20，覆盖 100 和 159。这条套接字上的 HTTP/1.1 做过单元测试。后来的直连 TLS 检查打开了 `https://www.gnu.org/`，状态 200，版本码 11。更早的 xpcshell 路径也曾返回 HTTP/1.1。直连明文和 HTTP/3 使用与 Windows DLL 相同的导出。本机 HTTP/1.1 返回状态 200、版本码 11。本机 Neqo 服务器在记住响应 `Alt-Svc` 之后，第二次请求返回版本码 30。第二次请求的证书不受信任时失败，状态不是 200。公网 `quic.nginx.org` 同样从版本码 20 到 30，两次正文一致。`cloudflare.com` 没有下发 `Alt-Svc`，两次都停在版本码 20。请求如果落到 donor xpcshell，明文仍会返回 `necko 804b0033`。打开 socket process 后，这份 xpcshell 会关掉管道，并且不写错误。不要再写一套 QUIC。

## 系统框架

外壳链接 CoreFoundation、CoreGraphics、CoreText、ImageIO 和 Security。SHA、AES-CBC、AES-GCM 和 HMAC 用 CommonCrypto。ECDSA 和 ECDH 用 Security.framework。GCM 的 oneshot 符号在本地声明，因为 macOS 26 的 SDK 头文件没有它们。

不要加入 OpenSSL、FreeType、fontconfig、libpng、libjpeg 或 libwebp。图像解码用 ImageIO。字体度量和字形掩码用 Core Text。SVG 路径边界用 Core Graphics。

POSIX 改动留在现有 `#ifndef _WIN32` 的非 Windows 一侧。Windows 的 `#else` 段保持原字节。

## 已经能用的部分

- Unix socket 帧、worker 的 `posix_spawn`，以及 C ABI 客户端。安装名是 `@rpath/libruyijs_client.dylib`，rpath 为 `@executable_path`。`ruyijs_create`、`ruyijs_call`、`ruyijs_close` 仍能解析。
- Core Text 字体度量和字形掩码。画布填充和 `fillText` 使用浏览器 donor 里 Firefox 自己的 Skia 对象（`gfx/skia`）。字面来自这块 Core Text 字体上的 `SkMakeTypefaceFromCTFont`。纯色填充不是空的，`fillText` 留下了墨迹。
- ImageIO 静态图：PNG、JPEG、GIF、WebP、BMP、ICO，预乘 BGRA。多帧 GIF 使用 ImageIO 已经合成好的显示帧。
- SVG 路径边界，覆盖 M、L、C、Q、A、Z。
- WebCrypto 的 SHA、AES-CBC、AES-GCM、HMAC、ECDSA、ECDH。ECDH 的参数字典分配失败时返回 false，调用方抛出 `OperationError`。
- 经 `libnecko_core.dylib` 的直连 HTTPS。`Context()` 和 `Context(tls="100")` 打开 `https://example.com/` 都是状态 200、协议 `h2`，握手个数跟着 `tls=`。Akamai 示例曾以 `init_real` 为真通过，协议是 HTTP/2，那次走的是更早的 xpcshell 路径。
- `Context(tls=)` 会写入 `NECKO_FIREFOX`。直连 HTTPS 由这个库读取。`necko_fetch.js` 仍然不读。代理仍走 xpcshell。结果在 `docs/tls-profile.zh-CN.md`。

适配器重新链接之后，CreepJS 再次通过，包含 `svg` 和 `canvas 2d`，没有失败项。

## 已知差异

这些表面现在调用 donor 在 macOS 上用的同一套系统接口和 Firefox 代码，不是另一套图像或字体栈。

| 表面 | macOS 行为 |
| --- | --- |
| 多帧 GIF | ImageIO 的显示帧。disposal 已经应用过，所以 Windows 上被清成透明的帧，在这里可能保留背景色。解码之后，重复次数用 GIF 元数据里的值。 |
| APNG 和动画 WebP | ImageIO 取帧，然后走同一套混合。缩放用 Core Graphics，像素保持预乘。 |
| 图像色彩 | 显示器 ICC 来自 `CGDisplayCopyColorSpace` 和 `CGColorSpaceCopyICCData`，再交给 Firefox 的 qcms。没有配置文件时像素不变。 |
| Canvas 文本 | `CTFontCreatePathForGlyph` 仍负责轮廓。Skia 的 `fillText` 对同一块 Core Text 字体调用 `SkMakeTypefaceFromCTFont`。 |
| 字体别名 | 只有 PostScript 名和字体族都匹配时，才把 `Osaka-Mono` 映射到 `Osaka`。这是 Firefox 的单字体列表，不是新写的一张表。 |
| COLR 与 CPAL | 只做 COLR 版本 0 和 CPAL 调色板 0。表字节来自 `CTFontCopyTable`。不做 COLRv1。 |
| 字形 gamma | `SystemTextGamma` 仍是 1.8，`SystemTextContrast` 仍是 1.0。可以读 `gasp` 和 `maxp`。掩码不伪造 DirectWrite 的渲染模式。 |

`src/web_media/image_codec.cpp` 和 `src/web_crypto/digest_backend.cpp` 已是 ImageIO 和 CommonCrypto 的真实现，不是桩。

DataDome 示例的首页结果是服务端评分，不是缺少的 Mac 绑定。示例的通过条件保持不变。

## 和 Windows 的比对

对照上游 `a626670`。这段范围包含 0.2.1 外壳、CSSOM 与 iframe 计时，以及改为 AGPL-3.0-only 的许可证。写着「同一份源码」的行，两边都还没做完。这张表描述的是合并后的 macOS 外壳。上一份本地 wheel 和运行时压缩包没有重打，那份压缩包也不是 0.2.1。

| 表面 | Windows | macOS |
| --- | --- | --- |
| JS 上下文、DOM、指纹、时钟、Python 桥 | 已实现 | 同一套 API |
| Necko HTTP/1.1 与 HTTP/2 | 已实现 | 这一轮直连 HTTPS 页面在 100 和 159 都返回状态 200、版本码 20（`h2`）。ALPN 为 `http/1.1` 时写 HTTP/1.1。解析器做过单元测试。后来的直连 TLS 检查打开了 `https://www.gnu.org/`，状态 200，版本码 11。直连明文走收回的 HTTP/1 交换，本机服务返回状态 200、版本码 11。xpcshell 自己仍会返回 `necko 804b0033` |
| HTTP/3 | `necko_quic.dll` 与 Neqo 0.32.0 | `libnecko_quic.dylib` 使用 Firefox 159 的 Neqo 0.32.0 和 donor NSS 3.131。同一进程的证明：第一次响应版本码 11，记住响应 `Alt-Svc` 后第二次是版本码 30。错误证书失败。公网 `quic.nginx.org` 从版本码 20 到 30，正文一致。socket process 保持关闭。页面外壳默认每个请求启动一次 oneshot，除非使用 `--session`，所以缓存不会跨过这些进程 |
| TLS 配置 100–159 | `Context(tls=)` | 直连 HTTPS 走 `libnecko_core.dylib`。页面 100 和 159 跟着配置个数：100 是 17 个套件、6 个组、2 个 key share，159 是 15、5、3。八个边界的库内 GET 与表一致，并返回 200 和 `h2`。没写的请求头和 HTTP/2 设置跟随 0.2.1 的表。没有把全部 60 个版本当页面打开（`docs/tls-profile.zh-CN.md`） |
| WebCrypto：SHA、AES-CBC、AES-GCM、HMAC、ECDSA、ECDH | CNG | CommonCrypto 与 Security.framework |
| 静态图：PNG、JPEG、GIF、WebP、BMP、ICO | WIC | ImageIO，预乘 BGRA |
| 多帧 GIF | WIC disposal。被清掉的帧可以是透明的 | ImageIO 显示帧。同一区域可能留下背景色。重复次数来自 GIF 元数据 |
| APNG 与动画 WebP | 共用的混合 | ImageIO 取帧后再走这套混合。Core Graphics 缩放后像素仍是预乘的 |
| 图像色彩 | 显示器配置文件，再交给 qcms | `CGDisplayCopyColorSpace` 与 `CGColorSpaceCopyICCData`，再交给 qcms。没有配置文件时像素不变 |
| Canvas 文本路径与 `fillText` | Skia，DirectWrite 字面 | 浏览器 donor 里的 Skia 对象。字面是 `SkMakeTypefaceFromCTFont`。轮廓仍走 `CTFontCreatePathForGlyph` |
| `strokeText` | 不绘制 | 不绘制 |
| 字体别名 | 平台字体列表 | PostScript 名和字体族都匹配时，`Osaka-Mono` 映射到 `Osaka`。这一条来自 Firefox 的单字体列表 |
| COLR 与 CPAL | 版本 0，调色板 0 | 版本 0，调色板 0。表字节来自 `CTFontCopyTable` |
| 字形 gamma | DirectWrite 渲染模式 | `SystemTextGamma` 为 1.8，`SystemTextContrast` 为 1.0。可以读 `gasp` 和 `maxp` |
| WebTransport | 共用的 C++ 帧。helper 模块已经在树里。0.2.1 安装仍走 `necko._http3.Connection` 和 `necko_quic.dll` | 同一套帧走 `libnecko_quic.dylib`。`webtransport=true` 只在这条长连接上。普通 HTTP/3 仍是 `webtransport=False`。哈希规则仍是 ECDSA P-256，有效期最多 14 天。页面外壳拉起 `oneshot --webtransport` 打本机 `127.0.0.1:9443`：协商成功，状态 200，数据报 `ping` 被回声。不带哈希时 PKI 校验失败。QUIC ClientHello 按 100 到 159 的分叉点写入。本机 Initial 个数一致。记录不要求逐字节相同，也不做 JA3。 |
| iframe 文档加载 | 同一份源码。Necko 时刻经 QueryPerformanceCounter 换算 | 同一份源码。时刻经 `steady_clock` 的毫秒换算，与 `page_navigation.cpp` 相同。一个 iframe 文档返回状态 200 和 `h2` |
| WebCodecs、音频解码、WebGPU、自定义内置元素、XML、表单目标 | 同一份源码 | 只有接口，或者明确返回未实现 |
| DataDome 首页 | 服务端评分 | 服务端评分。示例的通过条件不变 |
| 安装 | PyPI 的 0.2.1 `win_amd64` wheel | `macosx_26_0_arm64` wheel 和 `ruyijs-runtime-macos-arm64.zip` 在 fork Release `v0.2.1`。`runtime.json` 的 `macos.url` 指向这份压缩包。两者都不在 PyPI |
| C ABI 客户端 | 外壳旁边的 DLL | `libruyijs_client.dylib` 的安装名是 `@rpath/libruyijs_client.dylib`，rpath 为 `@executable_path`。XUL、xpcshell、NSS 的安装名保持 Firefox 写好的样子 |

`image_wic.h` 里仍有一段 POSIX 桩。macOS 的解码走 `image_codec.cpp` 和 `image_gif.cpp` 里的 ImageIO。

## 后续开发计划

外壳按 `macosx_26_0_arm64` 链接。Linux、Intel Mac，以及低于 macOS 26 的系统不在这份计划里。新行为只接到已经在用的 Firefox donor 或系统框架：ImageIO、Core Text、Core Graphics、CommonCrypto、Security。

1. 本机明文和 HTTP/3 已经走 Windows 的导出名字。同一进程的证明连续通过两遍，公网 `quic.nginx.org` 的第二次请求也是版本码 30。socket process 保持关闭。只打开偏好时，这只 xpcshell 仍在填写 Mac sandbox 参数时以信号 11 退出。WebTransport 现在用同一份库维持一条长连接。页面外壳拉起 `oneshot --webtransport` 打本机回声 `127.0.0.1:9443`：协商成功，状态 200，数据报 `ping` 被原样回声。不带证书哈希时，PKI 校验这张证书失败。不用 `--session` 的页面请求不会保留 `Alt-Svc` 缓存。这个进程边界来自两边共用的 oneshot。
2. TLS 配置。直连 HTTPS 的握手个数，以及产品请求头和 HTTP/2 设置表，都跟着 `tls=`。页面复查了 100 和 159。记录长度不要求逐字节相同。不要加 JA3 开关。
3. C 客户端安装名已经完成。`otool -D` 打印 `@rpath/libruyijs_client.dylib`，rpath 是 `@executable_path`。
4. 图像像素。继续用 ImageIO。可以用一条夹具记下 GIF 背景色差异。只有 ImageIO 已经提供对应 disposal 时才改像素。
5. 字体别名。Mac 只保留 `Osaka-Mono` 到 `Osaka`。字体选择仍走 Core Text 回退。上游的语言偏好、SimSun 和 Arial 列表留在 `font_collection.cpp` 的 Windows 一侧。Windows 的缓存分隔符保持源码转义 `'\0'`，文件因此仍是文本。
6. 发布。`runtime.json` 的 `macos.url` 是 fork Release `v0.2.1` 的压缩包。同一份 Release 里有 `ruyijs-0.2.1-py3-none-macosx_26_0_arm64.whl`。两者都不在 PyPI。干净安装可以导入。直连 HTTPS 返回状态 200、版本码 20。本机 HTTP/3 的第二次请求返回状态 200、版本码 30。`libnecko_quic.dylib` 从打包后的 `dist/necko` 加载。COLRv1、Linux、Intel Mac，以及低于 macOS 26 的系统都不做。

## 不要提交的东西

不要提交 Firefox 树、两份 objdir、`dist/js`、helper venv、macOS 运行时压缩包、`libnecko_quic.dylib`、`neqo-server`、`native/necko_quic/Cargo.lock`、cookie、token、代理凭据或响应正文。`runtime.json` 的 macOS 对象记下大小、sha256 和 fork Release 的 URL。不要把压缩包本身提交进 git。

## 报告 macOS 问题

请带上 ruyijs 的提交、macOS 版本、Xcode 版本、Python 版本、Firefox 标签，以及构建用的是哪一份 objdir。

## 维护方自己的 fork

上游 `main` 仍是 Windows 发布线。在 macOS 维护方的 fork 上，修正先进入 `release/macos`，该 fork 的 `main` 再快进到同一提交。上游更新从 `release/macos` 拉出分支合并、审查，再合并回去，不直接合并进该 fork 的 `main`。

## 闭源路线

明文 HTTP 与 HTTP/3 的语义适配写在 `docs/closed-route.zh-CN.md`。socket process 仍然关闭。
