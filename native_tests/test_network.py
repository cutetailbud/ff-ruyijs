"""Run network peers with the isolated helper's dependencies."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PYTHON = ROOT / "vendor/necko-derived/python/bin/python3"


@unittest.skipUnless(PYTHON.is_file(), "build the Linux Necko helper first")
class NetworkTests(unittest.TestCase):
    def test_shared_posix_network_stack(self):
        subprocess.run([str(PYTHON), "-I", "-B", str(ROOT / "native_tests/network_probe.py"), "-v"],
                       check=True, timeout=90)
    def test_native_image_codecs(self):
        subprocess.run([str(PYTHON), "-I", "-B", str(ROOT / "native_tests/image_probe.py")],
                       check=True, timeout=90)

    def test_shared_webtransport_stack(self):
        subprocess.run([str(PYTHON), "-I", "-B", str(ROOT / "native_tests/webtransport_probe.py"), "-v"],
                       check=True, timeout=90)


if __name__ == "__main__":
    unittest.main()
