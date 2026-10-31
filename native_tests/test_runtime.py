"""Native runtime acceptance tests using local documents and a local HTTP server."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from ruyijs import Context, JSError

SHELL = ROOT / "dist/js"


@unittest.skipUnless(SHELL.is_file(), "build the native runtime first")
class RuntimeTests(unittest.TestCase):
    def context(self, **options):
        return Context(shell=SHELL, timeout=10, **options)

    def test_shared_and_service_worker_processes(self):
        with self.context() as ctx:
            ctx.load_page('https://workers.test/', '<html><body></body></html>')
            ctx.put_resource('https://workers.test/shared.js',
                'onconnect=e=>{const p=e.ports[0];p.onmessage=m=>p.postMessage(m.data+1);p.start()}',
                headers=[['Content-Type','text/javascript']])
            ctx.put_resource('https://workers.test/sw.js',
                "self.addEventListener('install',e=>e.waitUntil(self.skipWaiting()));"
                "self.addEventListener('activate',e=>e.waitUntil(clients.claim()));"
                "self.addEventListener('message',e=>e.source.postMessage(e.data+2));",
                headers=[['Content-Type','text/javascript']])
            ctx.eval("""globalThis.workerResults={};globalThis.shared=new SharedWorker('/shared.js');
                shared.port.onmessage=e=>workerResults.shared=e.data;shared.port.start();shared.port.postMessage(40);
                (async()=>{const r=await navigator.serviceWorker.register('/sw.js');
                    await navigator.serviceWorker.ready;
                    navigator.serviceWorker.onmessage=async e=>{workerResults.service=e.data;
                        workerResults.unregistered=await r.unregister()};r.active.postMessage(40);
                })().catch(e=>workerResults.error=String(e));void 0;""")
            result = None
            for _ in range(150):
                ctx.advance(25)
                result = ctx.eval('workerResults')
                if result.get('unregistered') or result.get('error'): break
            self.assertEqual(result, {'shared':41,'service':42,'unregistered':True})

    def test_elf_process_javascript_bridge_and_errors(self):
        with self.context() as ctx:
            self.assertEqual(Path(f"/proc/{ctx._process.pid}/exe").resolve(), SHELL)
            self.assertEqual(ctx.eval("6*7"), 42)
            ctx.set("config", {"items": [1, 2, 3], "name": "\u4f60\u597d"})
            self.assertEqual(ctx.eval("config.items.reduce((a,b)=>a+b,0)"), 6)
            ctx.expose(lambda a, b: a + b, "hostAdd")
            self.assertEqual(ctx.eval("hostAdd(20,22)"), 42)
            function = ctx.eval("x => x + 1", to_py=False)
            self.assertEqual(function(41), 42)
            with self.assertRaises(JSError) as error:
                ctx.eval("throw new TypeError('native-test')")
            self.assertEqual(error.exception.name, "TypeError")
            self.assertEqual(ctx.eval("1+1"), 2)

    def test_profile_clock_and_dom_realms(self):
        with self.context() as ctx:
            ctx.configure_fingerprint({"navigator": {"platform": "Win32", "language": "zh-CN"},
                                       "locale": {"timezone": "Asia/Shanghai"}})
            ctx.configure_clock("logical", epoch_ms=1700000000000)
            ctx.load_page("https://example.test/", """<!doctype html><html><head><title>Native</title></head>
              <body><p id='x'>hello</p><script>globalThis.scriptTag=document.currentScript.tagName;
              setTimeout(()=>document.querySelector('#x').textContent='done',25)</script></body></html>""")
            ctx.advance(30)
            self.assertEqual(ctx.eval("[navigator.platform,navigator.language,scriptTag,document.querySelector('#x').textContent]"),
                             ["Win32", "zh-CN", "SCRIPT", "done"])
            self.assertTrue(ctx.eval("(()=>{let f=document.createElement('iframe');document.body.appendChild(f);return f.contentWindow!==window && f.contentDocument.defaultView===f.contentWindow})()"))

    def test_canvas_pixels_font_metrics_and_svg(self):
        with self.context() as ctx:
            ctx.load_page("https://example.test/", "<html><body></body></html>")
            self.assertEqual(ctx.eval("""(()=>{let c=document.createElement('canvas');c.width=8;c.height=8;
                let g=c.getContext('2d');g.fillStyle='#e02030';g.fillRect(0,0,8,8);
                return Array.from(g.getImageData(2,2,1,1).data)})()"""), [224,32,48,255])
            self.assertTrue(ctx.eval("""(()=>{let c=document.createElement('canvas'),g=c.getContext('2d');
                g.font='16px sans-serif';g.fillText('Native AV',0,20);
                return g.measureText('Native AV').width>10 && c.toDataURL().startsWith('data:image/png;base64,')})()"""))
            self.assertEqual(ctx.eval("""(()=>{let p=document.createElementNS('http://www.w3.org/2000/svg','path');
                p.setAttribute('d','M 10 20 L 30 20 L 30 40 Z');document.body.appendChild(p);
                let b=p.getBBox();return [b.x,b.y,b.width,b.height]})()"""), [10,20,20,20])

    def test_webcrypto_urlpattern_and_compression(self):
        with self.context() as ctx:
            ctx.load_page("https://example.test/", "<html><body></body></html>")
            self.assertTrue(ctx.eval("new URLPattern({pathname:'/users/:id'}).test('https://example.test/users/42')"))
            ctx.eval("""globalThis.digest=null;crypto.subtle.digest('SHA-256',new TextEncoder().encode('abc'))
                .then(v=>digest=Array.from(new Uint8Array(v)).map(x=>x.toString(16).padStart(2,'0')).join(''))""")
            ctx.advance(5)
            self.assertEqual(ctx.eval("digest"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            ctx.eval("""globalThis.compressed=null;(async()=>{
                let zip=new CompressionStream('gzip');
                let bytes=new Response(zip.readable).arrayBuffer();let writer=zip.writable.getWriter();
                await writer.write(new TextEncoder().encode('native stream'));await writer.close();
                let unzip=new DecompressionStream('gzip');let text=new Response(unzip.readable).text();
                writer=unzip.writable.getWriter();await writer.write(new Uint8Array(await bytes));
                await writer.close();compressed=await text;})()""")
            for _ in range(20):
                ctx.advance(5)
                if ctx.eval("compressed") is not None:
                    break
            self.assertEqual(ctx.eval("compressed"), "native stream")

    def test_worker_round_trip_and_terminate(self):
        with self.context() as ctx:
            ctx.load_page("https://example.test/", "<html><body></body></html>")
            ctx.eval("""globalThis.answer=null;globalThis.worker=new Worker(URL.createObjectURL(new Blob(
                ['onmessage=e=>postMessage(e.data+1)'],{type:'text/javascript'})));
                worker.onmessage=e=>answer=e.data;worker.postMessage(41);""")
            for _ in range(100):
                ctx.advance(20)
                if ctx.eval("answer") is not None:
                    break
            self.assertEqual(ctx.eval("answer"), 42)
            ctx.eval("worker.terminate()")

    def test_image_decode_preserves_canvas_pixels(self):
        with self.context() as ctx:
            ctx.load_page("https://example.test/", "<html><body></body></html>")
            ctx.eval("""globalThis.imageResult=null;let source=document.createElement('canvas');
                source.width=2;source.height=2;let brush=source.getContext('2d');
                brush.fillStyle='#18a03c';brush.fillRect(0,0,1,2);
                brush.fillStyle='rgba(24,160,60,0.5)';brush.fillRect(1,0,1,2);let image=new Image();
                image.onload=()=>{let target=document.createElement('canvas');target.width=2;target.height=2;
                  let g=target.getContext('2d');g.drawImage(image,0,0);imageResult=Array.from(g.getImageData(0,0,2,1).data);};
                image.onerror=()=>imageResult='decode-error';image.src=source.toDataURL();""")
            for _ in range(40):
                ctx.advance(10)
                if ctx.eval("imageResult") is not None:
                    break
            pixels = ctx.eval("imageResult")
            self.assertEqual(pixels[:4], [24,160,60,255])
            for actual, expected in zip(pixels[4:], [24,160,60,128]):
                self.assertLessEqual(abs(actual - expected), 2)

    def test_local_http_navigation_fetch_and_cookie(self):
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                body = b"<html><head><title>Local native</title></head><body>ok</body></html>" if self.path == "/" else b"native-http"
                self.send_response(200)
                self.send_header("Content-Type", "text/html" if self.path == "/" else "text/plain")
                self.send_header("Set-Cookie", "native=1; Path=/; HttpOnly")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            def log_message(self, *args):
                pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with self.context() as ctx:
                ctx.navigate(f"http://127.0.0.1:{server.server_port}/")
                for _ in range(40):
                    ctx.advance(10)
                    if ctx.eval("document.title") == "Local native":
                        break
                self.assertEqual(ctx.eval("document.title"), "Local native")
                ctx.eval("globalThis.body=null;fetch('/data').then(r=>r.text()).then(t=>body=t)")
                for _ in range(40):
                    ctx.advance(10)
                    if ctx.eval("body") is not None:
                        break
                self.assertEqual(ctx.eval("body"), "native-http")
                self.assertIn("native=1", ctx.cookie_header(f"http://127.0.0.1:{server.server_port}/"))
                self.assertNotIn("native=1", ctx.eval("document.cookie"))
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_timeout_reaps_native_process(self):
        ctx = Context(shell=SHELL, timeout=1)
        process = ctx._process
        with self.assertRaises(TimeoutError):
            ctx.eval("while(true){}")
        self.assertTrue(ctx.closed)
        self.assertIsNotNone(process.poll())
        ctx.close()


if __name__ == "__main__":
    unittest.main()
