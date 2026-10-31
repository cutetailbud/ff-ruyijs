"""Exercise the public C ABI across real POSIX processes and callbacks."""
import ctypes as c
import json
from pathlib import Path
import unittest
import concurrent.futures
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from ruyijs._values import decode, encode

ROOT = Path(__file__).resolve().parents[1]
CALLBACK = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint32, c.POINTER(c.c_void_p), c.POINTER(c.c_uint32), c.c_void_p)


@unittest.skipUnless((ROOT / 'dist/libruyijs_client.so').is_file(), 'build Linux C client first')
class ClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib = lib = c.CDLL(str(ROOT / 'dist/libruyijs_client.so'))
        lib.ruyijs_create_mode.argtypes = [c.c_char_p, c.c_uint32, c.c_char_p, c.POINTER(c.c_void_p)]
        lib.ruyijs_create_mode.restype = c.c_void_p
        lib.ruyijs_call.argtypes = [c.c_void_p, c.c_char_p, c.c_uint32, c.POINTER(c.c_void_p), c.POINTER(c.c_uint32)]
        lib.ruyijs_close.argtypes = [c.c_void_p]
        lib.ruyijs_free.argtypes = [c.c_void_p]
        lib.ruyijs_alloc.argtypes = [c.c_uint32]
        lib.ruyijs_alloc.restype = c.c_void_p
        lib.ruyijs_set_callback.argtypes = [c.c_void_p, CALLBACK, c.c_void_p]

    def create(self, mode=b'default', timeout=4000, shell=None):
        error = c.c_void_p()
        ctx = self.lib.ruyijs_create_mode(shell or str(ROOT / 'dist/js').encode(), timeout, mode, c.byref(error))
        message = c.string_at(error).decode() if error else ''
        self.lib.ruyijs_free(error)
        return ctx, message

    def call(self, ctx, **request):
        data = json.dumps({'id': 1, **request}).encode()
        reply, length = c.c_void_p(), c.c_uint32()
        status = self.lib.ruyijs_call(ctx, data, len(data), c.byref(reply), c.byref(length))
        try:
            return status, json.loads(c.string_at(reply, length.value)) if reply else None
        finally:
            self.lib.ruyijs_free(reply)

    def test_eval_large_frame_errors_and_debug(self):
        for mode in (b'default', b'debug'):
            ctx, error = self.create(mode)
            self.assertTrue(ctx, error)
            try:
                status, reply = self.call(ctx, op='eval', code='21 * 2')
                self.assertEqual(status, 0)
                self.assertEqual(decode(reply['value']), 42)
                _, reply = self.call(ctx, op='eval', code="'x'.repeat(200000)")
                self.assertEqual(len(decode(reply['value'])), 200000)
                _, reply = self.call(ctx, op='eval', code="throw new TypeError('native')")
                self.assertFalse(reply['ok'])
                self.assertEqual(reply['error']['name'], 'TypeError')
            finally:
                self.lib.ruyijs_close(ctx)

    def test_callback_and_reentry(self):
        ctx, error = self.create()
        self.assertTrue(ctx, error)
        events, reentries = [], []

        @CALLBACK
        def callback(event, size, output, length, user):
            frame = json.loads(c.string_at(event, size))
            events.append(frame)
            reentries.append(self.call(ctx, op='eval', code='1')[0])
            data = json.dumps({'op': 'callback-result', 'callId': frame['callId'], 'ok': True, 'value': encode(43)}).encode()
            output[0] = self.lib.ruyijs_alloc(len(data))
            c.memmove(output[0], data, len(data))
            length[0] = len(data)
            return 0

        try:
            self.assertEqual(self.lib.ruyijs_set_callback(ctx, callback, None), 0)
            status, reply = self.call(ctx, op='expose', name='nativeHost')
            self.assertEqual(status, 0)
            self.assertTrue(reply['ok'], reply)
            _, reply = self.call(ctx, op='eval', code='nativeHost(42)')
            self.assertEqual(decode(reply['value']), 43)
            self.assertEqual(events[0]['name'], 'nativeHost')
            self.assertEqual(reentries, [-2])
        finally:
            self.lib.ruyijs_close(ctx)

    def test_creator_thread_can_exit_before_context(self):
        with concurrent.futures.ThreadPoolExecutor(1) as executor:
            ctx,error=executor.submit(self.create).result()
        self.assertTrue(ctx,error)
        try:
            self.assertEqual(decode(self.call(ctx,op='eval',code='6*7')[1]['value']),42)
        finally:self.lib.ruyijs_close(ctx)

    def test_timeout_and_startup_failure(self):
        for mode, shell in ((b'bad', None), (b'default', b'/nonexistent/ruyijs')):
            ctx, error = self.create(mode, shell=shell)
            self.assertFalse(ctx)
            self.assertTrue(error)
        ctx, error = self.create(timeout=500)
        self.assertTrue(ctx, error)
        try:
            self.assertEqual(self.call(ctx, op='eval', code='while(true) {}')[0], -1)
            self.assertEqual(self.call(ctx, op='eval', code='1')[0], -1)
        finally:
            self.lib.ruyijs_close(ctx)
