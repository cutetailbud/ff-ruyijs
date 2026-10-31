#!/usr/bin/env bash
# Build the Firefox parts ruyijs takes from the firefox/ submodule, the
# official Firefox release it targets. Run from MozillaBuild's shell on
# Windows, or any shell on macOS/Linux.
#
#   build.sh js         SpiderMonkey standalone objdir          -> obj/js
#   build.sh browser    Skia objects and the NSS/NSPR SDK       -> obj/firefox-parts
#   build.sh toolchain  configure only: bootstrap the toolchains into ~/.mozbuild
set -euo pipefail
part="${1:?usage: build.sh js|browser|toolchain}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) platform=windows ;;
  Darwin) platform=macos ;;
  *) platform=linux ;;
esac
export MOZCONFIG="$here/$platform/mozconfig.$part"
[ -f "$MOZCONFIG" ] || { echo "no $MOZCONFIG" >&2; exit 1; }
export MACH_NO_TERMINAL_FOOTER=1
cd "$root/firefox"
mach() { ./mach --no-interactive "$@" </dev/null; }
case "$part" in
  js)
    mach build ;;
  browser)
    mach configure
    mach build pre-export export
    # Only what makes Skia, NSS/NSPR and mozglue: deps.py reads the link lists
    # configure wrote. Consecutive directories go to one mach call; "DIR TARGET"
    # steps link just that library (building all of security/ would recurse
    # into the rest of Gecko's security code).
    # (Windows' Python prints CRLF; bash would keep the CR in the last field.)
    python3 "$here/deps.py" "$root/obj/browser" | tr -d '\r' > "$root/obj/browser-steps.txt"
    cat "$root/obj/browser-steps.txt"
    batch=()
    flush() { if [ ${#batch[@]} -gt 0 ]; then mach build --allow-subdirectory-build "${batch[@]}"; batch=(); fi; }
    while read -r dir target; do
      if [ -n "$target" ]; then flush; mach build --directory "$dir" "$target"; else batch+=("$dir"); fi
    done < "$root/obj/browser-steps.txt"
    flush
    python3 "$here/collect.py" "$root/obj/browser" "$root/obj/firefox-parts" ;;
  toolchain)
    mach configure ;;
  *)
    echo "unknown part: $part" >&2; exit 1 ;;
esac
