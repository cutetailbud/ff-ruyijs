"""getImageData un-premultiplies the way Firefox does (rounded, not truncated).

usage: python ci/canvas_unpremultiply_check.py      (RUYIJS_SHELL picks js.exe)

The canvas store is premultiplied; getImageData divides alpha back out with
gfx's 8.16 reciprocal table and rounds the product (Swizzle.cpp
UnpremultiplyChunkFallback: (c * q + 0x8000) >> 16). The engine truncated it,
so a semi-transparent pixel could read one less than in Firefox -- every
anti-aliased edge of a canvas fingerprint differed (DataDome's device check,
#54, hashes such a canvas in a Worker). Flat fills keep this check
independent of the rasterizer: the expected pixels are Firefox 160's and
161's own (they agree) for the same fills, on HTMLCanvasElement and
OffscreenCanvas.
"""
import sys

from ruyijs import Context, JSError

COLOURS = [(17, 32), (100, 32), (151, 77), (203, 77), (17, 128), (250, 128), (100, 191),
           (151, 230), (203, 230), (250, 32), (100, 77), (17, 191)]
FIREFOX = ("16,16,16,32 104,104,104,32 152,152,152,77 202,202,202,77 18,18,18,128 249,249,249,128 "
           "100,100,100,191 151,151,151,230 203,203,203,230 247,247,247,32 99,99,99,77 17,17,17,191")
PROBE = """(() => {
  const colours = %s, out = {};
  for (const where of ['element', 'offscreen']) {
    const c = where === 'element' ? document.createElement('canvas') : new OffscreenCanvas(4, 4);
    c.width = 4; c.height = 4;
    const g = c.getContext('2d'), row = [];
    for (const [v, a] of colours) {
      g.clearRect(0, 0, 4, 4);
      g.fillStyle = `rgba(${v}, ${v}, ${v}, ${a / 255})`;
      g.fillRect(0, 0, 4, 4);
      row.push(Array.from(g.getImageData(1, 1, 1, 1).data).join(','));
    }
    out[where] = row.join(' ');
  }
  return out;
})()"""


def main():
    failures = 0
    with Context(timeout=60) as context:
        context.load_page("http://127.0.0.1/u.html", "<!doctype html><title>unpremultiply</title>")
        try:
            got = context.eval(PROBE % [list(c) for c in COLOURS])
        except JSError as error:
            got = {"element": f"ERROR {error}", "offscreen": f"ERROR {error}"}
    for where in ("element", "offscreen"):
        ok = got.get(where) == FIREFOX
        failures += not ok
        print(("ok  " if ok else "FAIL") + f"  {where}" + ("" if ok else f"\n      got  {got.get(where)}\n      want {FIREFOX}"))
    print("canvas unpremultiply:", "matches Firefox" if not failures else f"{failures} rows differ")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
