"""Exercise native C++/Python framing, fragmentation and bounded failure paths."""

import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from ruyijs._pipe import MAX_FRAME, Pipe


class UnixTransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="ruyijs-build-")
        cls.peer = str(Path(cls.build.name) / "pipe-peer")
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                        "-fsanitize=undefined", "-I", str(ROOT / "src"),
                        str(ROOT / "native_tests/pipe_peer.cpp"),
                        *[str(ROOT / "src/runtime" / name) for name in (
                            "pipe.cpp", "pipe_async_connection.cpp", "pipe_async_read.cpp",
                            "pipe_async_write.cpp", "pipe_async_state.cpp")], "-o", cls.peer], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def test_cpp_python_round_trip(self):
        for mode in ("sync", "async"):
            with self.subTest(mode=mode):
                pipe = Pipe()
                peer = subprocess.Popen([self.peer, mode, "connect", pipe.name])
                try:
                    pipe.connect(time.monotonic() + 3)
                    for data in (b"\x00\xffhello", os.urandom(1024 * 1024), b"x" * MAX_FRAME):
                        deadline = time.monotonic() + 5
                        pipe.send(data, deadline)
                        self.assertEqual(pipe.receive(deadline), data)
                finally:
                    pipe.close()
                    peer.wait(timeout=3)
                self.assertEqual(peer.returncode, 0)

    def test_async_cpp_server_fragmentation_and_coalescing(self):
        with tempfile.TemporaryDirectory(prefix="ruyijs-test-", dir="/tmp") as directory:
            name = str(Path(directory) / "ruyijs-0123456789abcdef.sock")
            peer = subprocess.Popen([self.peer, "async", "listen", name])
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.settimeout(3)
            try:
                deadline = time.monotonic() + 3
                while not Path(name).exists():
                    if time.monotonic() >= deadline or peer.poll() is not None:
                        self.fail("C++ server did not listen")
                    time.sleep(0.001)
                sock.connect(name)
                for byte in struct.pack("<I", 9) + b"fragment!":
                    sock.sendall(bytes([byte]))
                    time.sleep(0.001)
                self.assertEqual(self.read_exact(sock, 13), struct.pack("<I", 9) + b"fragment!")
                frames = b"\x01\x00\x00\x00A\x02\x00\x00\x00BC"
                sock.sendall(frames)
                self.assertEqual(self.read_exact(sock, len(frames)), frames)
            finally:
                sock.close()
                peer.wait(timeout=3)
            self.assertEqual(peer.returncode, 0)
            self.assertFalse(Path(name).exists())

    @staticmethod
    def read_exact(sock, count):
        data = b""
        while len(data) < count:
            part = sock.recv(count - len(data))
            if not part:
                raise EOFError("peer closed early")
            data += part
        return data

    def test_invalid_inbound_frames_and_eof(self):
        for frame, exception in ((b"\0" * 4, ValueError),
                                 (struct.pack("<I", MAX_FRAME + 1), ValueError),
                                 (b"\x08\0", EOFError),
                                 (struct.pack("<I", 8) + b"short", EOFError)):
            with self.subTest(frame=frame):
                pipe = Pipe()
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                try:
                    sock.connect(pipe.name)
                    pipe.connect(time.monotonic() + 1)
                    sock.sendall(frame)
                    sock.close()
                    with self.assertRaises(exception):
                        pipe.receive(time.monotonic() + 1)
                finally:
                    sock.close()
                    pipe.close()

    def test_timeout_and_private_socket_cleanup(self):
        pipe = Pipe()
        path = Path(pipe.name)
        self.assertEqual(path.stat().st_mode & 0o777, 0o600)
        try:
            with self.assertRaises(TimeoutError):
                pipe.connect(time.monotonic() + 0.02)
        finally:
            pipe.close()
            pipe.close()
        self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
