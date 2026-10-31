# PR #5 review checks

Checked against upstream `da1df2ce375131eb37234dba4638a5ce0d96a532` on
2026-10-31, following the [maintainer's review](https://github.com/cutetailbud/ff-ruyijs/pull/5#pullrequestreview-5722324778).

## Windows compilation and exception boundaries

`src/interaction/dispatch.cpp`, `src/interaction/listeners.cpp`,
`src/document_model/conversion.cpp`, and `src/document_model/query.cpp` are
byte-identical to upstream. The old PR's receiver changes that needed additional
`runtime/web_kit.h` includes are excluded.

The old PR added `src/document_model/shadow_search.cpp` with throwing DomString
code. That file is absent in both this tree and upstream. The source list and
`needs_exceptions()` in `tools/product_sources.py` are unchanged from upstream.

A source-only Windows conditional-body comparison covered 22 changed/new C++
files and headers. After excluding external includes and normalizing the
font-face forward declaration, the existing Windows bodies matched upstream;
the new Linux implementation units contribute no Windows body. Clang's
`x86_64-pc-windows-msvc` target also accepts those new Linux units and the
font-face declaration in syntax-only checks.

These checks do not compile external Windows headers or link a Windows shell.
There is no configured Windows donor or Windows SDK on this Linux host. A
native Windows build, C ABI run and the maintainer's 64 Firefox probes remain
unverified here. macOS was also not rebuilt here.

## Rebase and shared POSIX implementations

The current upstream `main` is an ancestor of the branch. `git merge-tree
--write-tree origin/main HEAD` succeeds without conflicts.

Runtime framing, Worker processes, C client lifecycle and Necko helper ownership
use the implementations already shared by macOS. There are no
`src/runtime/pipe_posix.cpp`, `src/web_workers/process_posix.cpp`, or
`src/client/lifecycle_posix.cpp` alternatives. The linked Linux shell has one
definition each of `ruyijs::Pipe::OpenClient` and `OpenServer`.

## Windows-visible browser behavior

The existing WebGL/GLSL, console, DOM, interaction, browser-environment and
scheduling implementations remain unchanged from upstream. The old branch's
browser parity changes are excluded.

`test_review_regressions.py` uses the review's exact fragment shader, with its
helper function, `if`/`else`, `fract`, `sin`, `dot` and `mod`. It requires both
shader `COMPILE_STATUS` and program `LINK_STATUS` to succeed. It passes on the
rebuilt Linux runtime. This does not claim a new Windows/Firefox comparison.

## CreepJS

Both the console formatter and `examples/creepjs/creepjs.py` are unchanged from
upstream, preserving their existing section-marker contract. A fresh run from
the installed Linux wheel exits 0 with `ok=true`, 96 parsed section entries,
and no failed sections, console errors or script errors. The Akamai and DataDome
examples also exit 0 from that installation. No example pass criterion changes.

## Scope and validation

The PR contains the Linux platform backends, shared POSIX integration, native
NSS/Neqo build inputs, relocatable packaging and acceptance tests. Two shared
POSIX transport fixes have their own commits: reading WebTransport information
before closing a session, and using HTTP/1.1 when a TLS peer omits ALPN.

The old `eval_many`/`ContextPool` APIs, Chrome client hints, iframe/input/geometry
parity changes and extra examples are excluded. Existing Python Context APIs
and example files match upstream.

The shell and NSS/Neqo rebuild, 26 native test cases (including 14 helper
checks), upstream smoke test, Python lint and whitespace checks pass. The
installation probes validate runtime/C ABI discovery and relocation, including
Workers launched from a directory containing Chinese characters. Reproduction
and platform limits are in [VALIDATION.md](VALIDATION.md) and
[the Linux guide](../docs/linux.md).
