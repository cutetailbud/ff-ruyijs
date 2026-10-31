"""Text advance and baseline invariants that failed in the initial Linux port."""
from pathlib import Path
import sys
import os
import shutil
import subprocess
import tempfile
from unittest.mock import patch
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from ruyijs import Context


@unittest.skipUnless((ROOT / "dist/js").is_file(), "build native runtime first")
class FontTests(unittest.TestCase):
    def test_empty_text_baselines_kerning_and_combining_marks(self):
        with Context(shell=ROOT / "dist/js") as ctx:
            ctx.load_page("https://fonts.test/", "<html><body></body></html>")
            got = ctx.eval("""(() => {
              const g=document.createElement('canvas').getContext('2d');g.font='32px DejaVu Sans';
              const empty=g.measureText(''), space=g.measureText(' '), e=g.measureText('e');
              return {empty:[empty.width,empty.fontBoundingBoxAscent,empty.fontBoundingBoxDescent],
                baselines:[empty.hangingBaseline,empty.alphabeticBaseline,empty.ideographicBaseline],
                space:[space.width,space.actualBoundingBoxLeft,space.actualBoundingBoxRight],
                kern:g.measureText('AV').width<g.measureText('A').width+g.measureText('V').width,
                mark:g.measureText('e\\u0301').width-e.width,
                trailing:g.measureText('A ').width-g.measureText('A').width};
            })()""")
            self.assertEqual(got["empty"][0], 0)
            self.assertGreater(got["empty"][1], 0)
            self.assertGreater(got["empty"][2], 0)
            self.assertGreater(got["baselines"][0], 0)
            self.assertEqual(got["baselines"][1], 0)
            self.assertLess(got["baselines"][2], 0)
            self.assertEqual(got["space"][1:], [0, 0])
            self.assertTrue(got["kern"])
            self.assertAlmostEqual(got["mark"], 0, places=5)
            self.assertAlmostEqual(got["trailing"], got["space"][0], places=4)

    def test_synthetic_styles_with_regular_font_only(self):
        font = Path(subprocess.check_output(["fc-match", "-f", "%{file}", "DejaVu Sans:style=Book"], text=True))
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            shutil.copy2(font, directory / "regular.ttf")
            config = directory / "fonts.conf"
            config.write_text(f'<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig><dir>{directory}</dir></fontconfig>')
            with patch.dict(os.environ, {"FONTCONFIG_FILE": str(config)}), Context(shell=ROOT / "dist/js") as ctx:
                ctx.load_page("https://fonts.test/", "<html><body></body></html>")
                got = ctx.eval("""(() => {
                    const c=document.createElement('canvas');c.width=220;c.height=90;
                    const g=c.getContext('2d');const results=[];
                    for(const style of ['', 'italic ', 'bold ']){
                        g.clearRect(0,0,220,90);g.font=style+'40px DejaVu Sans';g.fillText('Native',20,60);
                        const m=g.measureText('Native');const p=g.getImageData(0,0,220,90).data;
                        let alpha=0,hash=0;for(let i=0;i<p.length;i++){hash=(hash*31+p[i])>>>0;if(i%4===3)alpha+=p[i]}
                        results.push({width:m.width,left:m.actualBoundingBoxLeft,right:m.actualBoundingBoxRight,alpha,hash});
                    }return results;
                })()""")
                self.assertAlmostEqual(got[0]['width'],got[1]['width'],places=4)
                self.assertNotEqual(got[0]['hash'],got[1]['hash'])
                self.assertGreater(got[2]['alpha'],got[0]['alpha'])
                self.assertGreaterEqual(got[2]['width'],got[0]['width'])

    @unittest.skipUnless(Path('/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf').is_file(), 'install Noto Color Emoji')
    def test_color_emoji_preserves_palette(self):
        with Context(shell=ROOT / "dist/js") as ctx:
            ctx.load_page("https://fonts.test/", "<html><body></body></html>")
            got=ctx.eval("""(() => {
                const c=document.createElement('canvas');c.width=100;c.height=100;const g=c.getContext('2d');
                g.font='64px "Noto Color Emoji"';const results=[];
                for(const fill of ['red','blue']){
                    g.clearRect(0,0,100,100);g.fillStyle=fill;g.fillText('😀',5,70);
                    const p=g.getImageData(0,0,100,100).data;let hash=0,colors=0;
                    for(let i=0;i<p.length;i++){hash=(hash*31+p[i])>>>0;
                        if(i%4===0&&p[i+3]&&p[i]!==p[i+1])colors++}
                    results.push({hash,colors});
                }return results;
            })()""")
            self.assertGreater(got[0]['colors'],100)
            self.assertEqual(got[0]['hash'],got[1]['hash'])



if __name__ == "__main__":
    unittest.main()
