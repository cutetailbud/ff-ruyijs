# SpiderMonkey standalone build (JS_STANDALONE) that tools/build.py links as
# its donor (--donor-obj obj/js). The same options as the maintainer's local
# donor; toolchains come from mach's bootstrap.
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/../obj/js

ac_add_options --enable-project=js
ac_add_options --enable-bootstrap
ac_add_options --target=x86_64-pc-windows-msvc
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --enable-rust-simd
