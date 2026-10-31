"""Run with a fresh wheel installation, with no source/runtime overrides."""

import ctypes as ct
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import tempfile
import threading
import unittest

import ruyijs
from ruyijs import Context
from ruyijs._runtime_paths import get_runtime_path
from ruyijs._values import decode


class InstalledTests(unittest.TestCase):
    def test_relocated_runtime_in_a_unicode_directory(self):
        with tempfile.TemporaryDirectory(prefix="ruyijs-移植-") as directory:
            runtime = Path(directory) / "runtime"
            shutil.copytree(get_runtime_path(), runtime)
            with Context(shell=runtime / "dist/js", timeout=10) as context:
                context.load_page("https://relocated.test/", "<html><body></body></html>")
                context.eval("""globalThis.result=null;globalThis.worker=new Worker(
                    URL.createObjectURL(new Blob(['postMessage(42)'])));
                    worker.onmessage=e=>result=e.data;void 0;""")
                for _ in range(100):
                    context.advance(20)
                    if context.eval("result") is not None:
                        break
                self.assertEqual(context.eval("result"), 42)
                context.eval("worker.terminate()")

    def test_runtime_discovery_bridge_and_worker(self):
        runtime = get_runtime_path()
        self.assertEqual(runtime, Path(ruyijs.__file__).parent / "_runtime")
        with Context(timeout=10) as context:
            self.assertEqual(context.shell, runtime / "dist/js")
            self.assertEqual(context.eval("6*7"), 42)
            context.expose(lambda value: value + 1, "installedHost")
            self.assertEqual(context.eval("installedHost(41)"), 42)
            context.load_page("https://installed.test/", "<html><body></body></html>")
            context.eval("""globalThis.result=null;globalThis.worker=new Worker(
                URL.createObjectURL(new Blob(['onmessage=e=>postMessage(e.data+1)'])));
                worker.onmessage=e=>result=e.data;worker.postMessage(41);""")
            for _ in range(100):
                context.advance(20)
                if context.eval("result") is not None:
                    break
            self.assertEqual(context.eval("result"), 42)
            context.eval("worker.terminate()")

    def test_python_and_c_hosts_discover_the_bundled_network_helper(self):
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                payload = b"<!doctype html><title>installed-native</title>"
                self.send_response(200)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, *args):
                pass

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = f"http://127.0.0.1:{server.server_port}/"
        try:
            with Context(timeout=10) as context:
                context.navigate(url)
                self.assertEqual(context.eval("document.title"), "installed-native")
            runtime = get_runtime_path()
            library = ct.CDLL(str(runtime / "dist/libruyijs_client.so"))
            library.ruyijs_create.argtypes = [ct.c_char_p, ct.c_uint32, ct.POINTER(ct.c_void_p)]
            library.ruyijs_create.restype = ct.c_void_p
            library.ruyijs_call.argtypes = [ct.c_void_p, ct.c_char_p, ct.c_uint32,
                                           ct.POINTER(ct.c_void_p), ct.POINTER(ct.c_uint32)]
            library.ruyijs_free.argtypes = [ct.c_void_p]
            library.ruyijs_close.argtypes = [ct.c_void_p]
            error = ct.c_void_p()
            context = library.ruyijs_create(str(runtime / "dist/js").encode(), 10000, ct.byref(error))
            message = ct.string_at(error).decode() if error else ""
            library.ruyijs_free(error)
            self.assertTrue(context, message)
            try:
                for index, request in enumerate(({"op": "page", "action": "navigate", "url": url},
                                                {"op": "eval", "code": "document.title"})):
                    payload = json.dumps({"id": index, **request}).encode()
                    reply, size = ct.c_void_p(), ct.c_uint32()
                    status = library.ruyijs_call(context, payload, len(payload), ct.byref(reply), ct.byref(size))
                    self.assertEqual(status, 0)
                    try:
                        value = json.loads(ct.string_at(reply, size.value))
                        self.assertTrue(value["ok"], value)
                        if index:
                            self.assertEqual(decode(value["value"]), "installed-native")
                    finally:
                        library.ruyijs_free(reply)
            finally:
                library.ruyijs_close(context)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
