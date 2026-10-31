"""Regressions reported in the maintainer's review of PR #5."""

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from ruyijs import Context


@unittest.skipUnless((ROOT / "dist/js").is_file(), "build the native runtime first")
class ReviewRegressions(unittest.TestCase):
    def test_reviewed_fragment_shader_compiles_and_links(self):
        with Context(shell=ROOT / "dist/js") as context:
            context.load_page("https://shader.test/", "<html><body></body></html>")
            result = context.eval("""(() => {
                const gl = document.createElement('canvas').getContext('webgl');
                if (!gl) return {error: 'WebGL context unavailable'};
                const vertex = gl.createShader(gl.VERTEX_SHADER);
                gl.shaderSource(vertex,
                    'attribute vec4 position; void main(){ gl_Position = position; }');
                gl.compileShader(vertex);
                const fragment = gl.createShader(gl.FRAGMENT_SHADER);
                gl.shaderSource(fragment, `precision highp float;
float h(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main(){ vec2 q = gl_FragCoord.xy; float v = h(q); if (v > 0.5) { gl_FragColor = vec4(v, 1.0 - v, 0.25, 1.0); } else { gl_FragColor = vec4(0.0, v, mod(q.x, 3.0) / 3.0, 1.0); } }`);
                gl.compileShader(fragment);
                const program = gl.createProgram();
                gl.attachShader(program, vertex);
                gl.attachShader(program, fragment);
                gl.linkProgram(program);
                return {
                    vertex: gl.getShaderParameter(vertex, gl.COMPILE_STATUS),
                    fragment: gl.getShaderParameter(fragment, gl.COMPILE_STATUS),
                    linked: gl.getProgramParameter(program, gl.LINK_STATUS),
                    vertexLog: gl.getShaderInfoLog(vertex),
                    fragmentLog: gl.getShaderInfoLog(fragment),
                    programLog: gl.getProgramInfoLog(program)
                };
            })()""")
            self.assertTrue(result.get("vertex"), result)
            self.assertTrue(result.get("fragment"), result)
            self.assertTrue(result.get("linked"), result)


if __name__ == "__main__":
    unittest.main()
