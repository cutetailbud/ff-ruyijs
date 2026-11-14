# Linux x86_64

从 0.3.0 起，PyPI 上有 `manylinux_2_39_x86_64` wheel。它需要 glibc 2.39 及以上（Ubuntu 24.04、Debian 13、Fedora 40
及更新版本）。Linux 移植来自 [@Starktomy](https://github.com/Starktomy) 的 #5，接在 macOS 共用的 POSIX 进程、管道和 socket 层上。

同一份事实的英文版在 [linux.md](linux.md)。

## 平台后端

| 组件 | Linux 后端 |
| --- | --- |
| JavaScript、Intl、Wasm | 来自 Firefox 160.0 源码（`firefox/` 子模块）的 SpiderMonkey |
| DOM、iframe realm、事件、Worker | ruyijs 共用源码，跑在 POSIX 进程和 Unix socket 上 |
| Canvas | Firefox 160 自己的 Skia，从子模块编译 |
| 字体 | fontconfig、FreeType 和 HarfBuzz，使用机器上已安装的字体 |
| WebCrypto | OpenSSL 3 |
| 图片 | libwebp、LittleCMS2 和 stb_image；AVIF 用随仓库提供的 libavif、dav1d 和 libyuv |
| HTTP(S)、HTTP/3 | 基于子模块编出的 NSS 3.131 / NSPR 4.40 的 Necko helper，HTTP/3 用 Neqo。它运行在 wheel 自带的 Python 3.14 上 |
| C ABI | `libruyijs_client.so` |

文字用机器上已安装的字体绘制，所以除非装了同样的字体，canvas 文字和字体度量会和 Windows 不同。wheel 自带除 glibc 以外链接到的库；字体和 fontconfig 配置来自系统。

## 构建

PyPI 上的 wheel 由 `.github/workflows/build-linux.yml` 在 Ubuntu 24.04 上构建。本地构建时，在带 `firefox/` 子模块的检出目录里按顺序运行
`ci/linux/build.sh` 的各个步骤：

```sh
ci/linux/build.sh deps     # apt 软件包、clang 21（apt.llvm.org）、cbindgen
ci/linux/build.sh donor    # SpiderMonkey objdir -> obj/js-linux
ci/linux/build.sh shell    # dist/js 和 dist/libruyijs_client.so
ci/linux/build.sh necko    # NSS/NSPR、libnecko_core、Neqo 和 Necko helper（Python 3.14）
ci/linux/build.sh wheel    # wheelhouse/ruyijs-*-manylinux_2_39_x86_64.whl
ci/linux/build.sh test     # 在全新 venv 里安装 wheel 并运行冒烟测试
```

donor 步骤用发行版自带的 Python（`/usr/bin/python3`）运行 configure，necko 步骤需要 `python3` 是 Python 3.14。完整构建约占 11 GB 磁盘。Necko 适配层及其限制见
[native/necko_posix/README.md](../native/necko_posix/README.md)。一次实测构建、冒烟测试和速度基线写在
[linux-baseline.md](linux-baseline.md)。
