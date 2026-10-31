"""Exercise the upstream CSS/cascade and element changes in the Linux runtime."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from ruyijs import Context


@unittest.skipUnless((ROOT / "dist/js").is_file(), "build native runtime first")
class UpstreamParityTests(unittest.TestCase):
    def test_layer_cascade_registered_property_and_spellcheck_inheritance(self):
        with Context(shell=ROOT / "dist/js") as ctx:
            ctx.load_page("https://upstream.test/", """<!doctype html><html><head><style>
              @layer base, theme;
              @layer base { #target {color:red;} }
              @layer theme { #target {color:blue;} }
              @property --probe {syntax:"<length>";inherits:false;initial-value:7px;}
              </style></head><body><div id="target">probe</div>
              <div spellcheck="true"><span id="spell"></span></div></body></html>""")
            self.assertEqual(ctx.eval("""[getComputedStyle(document.querySelector('#target')).color,
              getComputedStyle(document.querySelector('#target')).getPropertyValue('--probe'),
              document.querySelector('#spell').spellcheck,document.createElement('input').spellcheck,
              document.createElement('textarea').spellcheck]"""),
              ["rgb(0, 0, 255)", "7px", True, False, True])
            self.assertEqual(ctx.eval("document.styleSheets[0].cssRules.length"), 4)


if __name__ == "__main__":
    unittest.main()
