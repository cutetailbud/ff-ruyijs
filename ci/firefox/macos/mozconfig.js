# SpiderMonkey standalone build (JS_STANDALONE) on Apple Silicon, linked by
# tools/build.py as its donor (--donor-obj obj/js); options as in docs/macos.md.
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/../obj/js

ac_add_options --enable-project=js
ac_add_options --enable-bootstrap
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-tests
ac_add_options --enable-rust-simd
