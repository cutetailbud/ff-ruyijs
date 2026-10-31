"""Known-answer crypto checks run against the actual Linux adapter."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BackendTests(unittest.TestCase):
    def test_webcrypto_known_answers_and_key_operations(self):
        with tempfile.TemporaryDirectory(prefix="ruyijs-crypto-") as directory:
            executable = str(Path(directory) / "crypto-test")
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined", "-I", str(ROOT / "src"),
                            str(ROOT / "native_tests/crypto_backend.cpp"),
                            str(ROOT / "src/web_crypto/openssl_backend_linux.cpp"),
                            "-lcrypto", "-o", executable], check=True)
            subprocess.run([executable], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
