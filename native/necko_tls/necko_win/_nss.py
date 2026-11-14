# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ctypes as ct
import datetime
import hashlib
import ipaddress
import json
import os
import ssl
import struct
import zlib
from pathlib import Path

import brotli
from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.x509.verification import PolicyBuilder, Store, VerificationError

from ._firefox import DEFAULT_FIREFOX, data_path
from ._tls_profiles import PROFILES as TLS_PROFILES

try:
    from compression import zstd
except ImportError:
    from backports import zstd

_Pointer = ct.c_void_p
_Bytes = ct.POINTER(ct.c_uint8)
_Read = ct.CFUNCTYPE(ct.c_int, _Pointer, _Bytes, ct.c_uint32, ct.POINTER(ct.c_uint32))
_Write = ct.CFUNCTYPE(ct.c_int, _Pointer, _Bytes, ct.c_uint32)
_WriteSome = ct.CFUNCTYPE(
    ct.c_int, _Pointer, _Bytes, ct.c_uint32, ct.POINTER(ct.c_uint32)
)
_Idle = ct.CFUNCTYPE(ct.c_int, _Pointer)
_Verify = ct.CFUNCTYPE(ct.c_int, _Pointer, _Bytes, ct.c_uint32)
_Decode = ct.CFUNCTYPE(
    ct.c_int, _Pointer, ct.c_uint16, _Bytes, ct.c_uint32, _Bytes, ct.c_uint32
)


_U16s = ct.POINTER(ct.c_uint16)


class _Profile(ct.Structure):
    _fields_ = [
        ("ciphers", _U16s),
        ("cipher_count", ct.c_uint32),
        ("groups", _U16s),
        ("group_count", ct.c_uint32),
        ("additional_key_shares", ct.c_uint32),
        ("signatures", _U16s),
        ("signature_count", ct.c_uint32),
        ("compression", _U16s),
        ("compression_count", ct.c_uint32),
        ("signed_cert_timestamps", ct.c_int),
        ("grease_ech", ct.c_int),
        ("grease_ech_size", ct.c_uint32),
    ]


def _array(values):
    return (ct.c_uint16 * max(len(values), 1))(*values)


def native_profile(version):
    """Build the necko_tls_profile for a Firefox version ("100".."160")."""
    profile = TLS_PROFILES[version]
    arrays = [
        _array(profile.ciphers),
        _array(profile.groups),
        _array(profile.signatures),
        _array(profile.compression),
    ]
    native = _Profile(
        ct.cast(arrays[0], _U16s),
        len(profile.ciphers),
        ct.cast(arrays[1], _U16s),
        len(profile.groups),
        profile.additional_key_shares,
        ct.cast(arrays[2], _U16s),
        len(profile.signatures),
        ct.cast(arrays[3], _U16s),
        len(profile.compression),
        int(profile.signed_cert_timestamps),
        int(profile.grease_ech),
        profile.grease_ech_size,
    )
    native._arrays = arrays
    return native


def configure_library(library):
    signatures = {
        "open": (
            [
                ct.c_char_p,
                ct.c_char_p,
                ct.c_int,
                _Read,
                _Write,
                _Idle,
                _Verify,
                _Decode,
                _Pointer,
                ct.c_char_p,
                ct.c_uint32,
                ct.c_char_p,
                ct.c_uint32,
                ct.POINTER(_Pointer),
            ],
            ct.c_int,
        ),
        "read": ([_Pointer, _Bytes, ct.c_uint32, ct.POINTER(ct.c_uint32)], ct.c_int),
        "write": ([_Pointer, _Bytes, ct.c_uint32], ct.c_int),
        "write_some": (
            [_Pointer, _Bytes, ct.c_uint32, ct.POINTER(ct.c_uint32)],
            ct.c_int,
        ),
        "set_nonblocking": ([_Pointer, _WriteSome], None),
        "close": ([_Pointer], None),
        "pending": ([_Pointer], ct.c_int),
        "alpn": ([_Pointer], ct.c_char_p),
        "version": ([_Pointer], ct.c_uint16),
        "cipher": ([_Pointer], ct.c_uint16),
        "resumed": ([_Pointer], ct.c_int),
        "nss_version": ([], ct.c_char_p),
        "nspr_version": ([], ct.c_char_p),
        "error": ([], ct.c_char_p),
    }
    signatures["open_profile"] = (
        [*signatures["open"][0][:3], ct.POINTER(_Profile), *signatures["open"][0][3:]],
        ct.c_int,
    )
    for name, (arguments, result) in signatures.items():
        function = getattr(library, "necko_tls_" + name)
        function.argtypes, function.restype = arguments, result
    if library.necko_tls_nss_version() != b"3.132":
        raise RuntimeError("firefox-160 requires the packaged NSS 3.132 runtime")


# Trust identity -> {subject DER: intermediate certificate} from verified chains.
_INTERMEDIATES = {}
_INTERMEDIATE_LIMIT = 256


def _certificate_bytes(certificate):
    der = certificate.public_bytes(serialization.Encoding.DER)
    return struct.pack("!I", len(der)) + der


class Context:
    def __init__(self, verify, cert, data_directory=None):
        self.store = None
        self.certificates = b""
        self.key = b""
        trust = b"unverified"
        self.constraints = {}
        roots_path = data_path("firefox-roots.pem")
        if data_directory is not None:
            base = Path(data_directory)
            if not (base / "firefox-roots.pem").is_file():
                base = base / "necko"
            roots_path = base / "firefox-roots.pem"
        if verify:
            path = Path(
                os.fspath(verify)
                if isinstance(verify, (str, os.PathLike))
                else roots_path
            )
            if not path.exists():
                raise OSError(f"Invalid TLS CA certificate bundle path: {path}")
            roots = []
            for candidate in sorted(path.iterdir()) if path.is_dir() else [path]:
                if candidate.is_file():
                    content = candidate.read_bytes()
                    if b"-----BEGIN CERTIFICATE-----" in content:
                        roots.extend(x509.load_pem_x509_certificates(content))
            extra = os.environ.get("NECKO_EXTRA_CA", "")
            if extra and not isinstance(verify, (str, os.PathLike)):
                extra_path = Path(extra)
                if extra_path.is_file():
                    content = extra_path.read_bytes()
                    if b"-----BEGIN CERTIFICATE-----" in content:
                        roots.extend(x509.load_pem_x509_certificates(content))
            self.store = Store(roots)
            trust = b"".join(_certificate_bytes(root) for root in roots)
            if not isinstance(verify, (str, os.PathLike)):
                self.constraints = json.loads(
                    roots_path.with_name("firefox-root-constraints.json").read_text()
                )
        if cert:
            certificate, key = cert if isinstance(cert, (tuple, list)) else (cert, cert)
            certificates = x509.load_pem_x509_certificates(
                Path(certificate).read_bytes()
            )
            private = serialization.load_pem_private_key(
                Path(key or certificate).read_bytes(), None
            )
            public_format = serialization.PublicFormat.SubjectPublicKeyInfo
            if private.public_key().public_bytes(
                serialization.Encoding.DER, public_format
            ) != certificates[0].public_key().public_bytes(
                serialization.Encoding.DER, public_format
            ):
                raise ssl.SSLError("Client certificate and private key do not match")
            self.certificates = b"".join(
                _certificate_bytes(item) for item in certificates
            )
            self.key = private.private_bytes(
                serialization.Encoding.DER,
                serialization.PrivateFormat.PKCS8,
                serialization.NoEncryption(),
            )
        self.identity = hashlib.sha256(trust + self.certificates).hexdigest()

    def verify(self, host, data):
        if self.store is None:
            return
        certificates = []
        while data:
            length = struct.unpack("!I", data[:4])[0]
            certificates.append(x509.load_der_x509_certificate(data[4 : 4 + length]))
            data = data[4 + length :]
        try:
            subject = x509.IPAddress(ipaddress.ip_address(host))
        except ValueError:
            subject = x509.DNSName(host.encode("idna").decode("ascii"))
        verifier = PolicyBuilder().store(self.store).build_server_verifier(subject)
        known = _INTERMEDIATES.setdefault(self.identity, {})
        try:
            chain = verifier.verify(certificates[0], certificates[1:])
        except VerificationError:
            presented = {item.public_bytes(serialization.Encoding.DER) for item in certificates}
            cached = [item for item in list(known.values())
                      if item.public_bytes(serialization.Encoding.DER) not in presented]
            if not cached:
                raise
            chain = verifier.verify(certificates[0], certificates[1:] + cached)
        for item in chain[1:-1]:
            if len(known) < _INTERMEDIATE_LIMIT:
                known[item.subject.public_bytes()] = item
        anchor = chain[-1].public_bytes(serialization.Encoding.DER)
        cutoff = self.constraints.get(hashlib.sha256(anchor).hexdigest())
        if cutoff and chain[0].not_valid_before_utc > datetime.datetime.strptime(
            cutoff, "%y%m%d%H%M%SZ"
        ).replace(tzinfo=datetime.timezone.utc):
            raise ssl.SSLCertVerificationError(
                "Certificate issued after NSS root distrust cutoff"
            )


class TLS:
    def __init__(
        self, transport, context, host, library, peer, http2=True, firefox=None
    ):
        self.transport = transport
        self.library = library
        self.handle = _Pointer()
        self.error = None
        self.nonblocking = False
        self.context = context
        self.host = host
        self._callbacks = (
            _Read(self._read),
            _Write(self._write),
            _Idle(self._idle),
            _Verify(self._verify),
            _Decode(self._decode),
        )
        self.firefox = firefox or DEFAULT_FIREFOX
        self._profile = native_profile(self.firefox)
        self._check(
            library.necko_tls_open_profile(
                host.encode("idna"),
                # The version is part of the resumption key, so a session
                # ticket from one ClientHello profile never resumes another.
                hashlib
                .sha256(
                    (peer + ":" + context.identity + ":" + self.firefox).encode()
                )
                .hexdigest()
                .encode(),
                http2,
                ct.byref(self._profile),
                *self._callbacks,
                None,
                context.certificates,
                len(context.certificates),
                context.key,
                len(context.key),
                ct.byref(self.handle),
            )
        )
        self.context.key = b""
        self.alpn = library.necko_tls_alpn(self.handle).decode("ascii")
        if self.alpn == "h2" and library.necko_tls_cipher(self.handle) not in (
            0x1301,
            0x1302,
            0x1303,
            0xC02B,
            0xC02C,
            0xC02F,
            0xC030,
            0xCCA8,
            0xCCA9,
        ):
            self.close()
            raise ssl.SSLError("HTTP/2 requires Firefox's TLS cipher profile")
        self.info = {
            "backend": "NSS",
            "nss": library.necko_tls_nss_version().decode(),
            "nspr": library.necko_tls_nspr_version().decode(),
            "profile": f"firefox-{self.firefox}",
            "version": library.necko_tls_version(self.handle),
            "alpn": self.alpn,
            "cipher_suite": library.necko_tls_cipher(self.handle),
            "resumed": bool(library.necko_tls_resumed(self.handle)),
        }

    @property
    def timeout(self):
        return self.transport.timeout

    @timeout.setter
    def timeout(self, value):
        self.transport.timeout = value

    def _read(self, _closure, buffer, size, length):
        try:
            data = (
                self.transport.read_some(size)
                if self.nonblocking
                else self.transport.read(size)
            )
            ct.memmove(buffer, data, len(data))
            length[0] = len(data)
            return 0
        except BlockingIOError:
            return -2
        except Exception as error:
            self.error = error
            return -1

    def _write(self, _closure, buffer, length):
        try:
            self.transport.write(ct.string_at(buffer, length))
            return 0
        except Exception as error:
            self.error = error
            return -1

    def _write_some(self, _closure, buffer, size, length):
        try:
            length[0] = self.transport.write_some(ct.string_at(buffer, size))
            return 0
        except BlockingIOError:
            return -2
        except Exception as error:
            self.error = error
            return -1

    def enable_nonblocking(self):
        if self.nonblocking:
            return
        if isinstance(self.transport, TLS):
            self.transport.enable_nonblocking()
        self._write_some_callback = _WriteSome(self._write_some)
        self.library.necko_tls_set_nonblocking(self.handle, self._write_some_callback)
        self.nonblocking = True

    def _verify(self, _closure, buffer, length):
        try:
            self.certificate_chain = ct.string_at(buffer, length)
            self.context.verify(self.host, self.certificate_chain)
            return 0
        except Exception as error:
            self.error = ssl.SSLCertVerificationError(str(error))
            return -1

    def _idle(self, _closure):
        try:
            return int(bool(self.transport.idle()))
        except Exception as error:
            self.error = error
            return 0

    def _decode(self, _closure, algorithm, buffer, length, output, capacity):
        try:
            data = ct.string_at(buffer, length)
            if algorithm == 1:
                decoder = zlib.decompressobj()
                decoded = decoder.decompress(data, capacity + 1)
                complete = decoder.eof and not decoder.unused_data
            elif algorithm == 2:
                decoder = brotli.Decompressor()
                decoded = decoder.process(data, output_buffer_limit=capacity + 1)
                complete = decoder.is_finished()
            else:
                decoder = zstd.ZstdDecompressor()
                decoded = decoder.decompress(data, max_length=capacity + 1)
                complete = decoder.eof and not decoder.unused_data
            if len(decoded) != capacity or not complete:
                raise ValueError("Invalid compressed TLS certificate length")
            ct.memmove(output, decoded, capacity)
            return 0
        except Exception as error:
            self.error = ssl.SSLError(str(error))
            return -1

    def _check(self, result):
        if self.error is not None:
            error, self.error = self.error, None
            raise error
        if result == 1:
            raise BlockingIOError("NSS I/O would block")
        if result:
            raise ssl.SSLError(self.library.necko_tls_error().decode())

    def read(self, size):
        buffer = (ct.c_uint8 * size)()
        length = ct.c_uint32()
        self._check(
            self.library.necko_tls_read(self.handle, buffer, size, ct.byref(length))
        )
        return bytes(buffer[: length.value])

    def write(self, data):
        buffer = (ct.c_uint8 * len(data)).from_buffer_copy(data)
        self._check(self.library.necko_tls_write(self.handle, buffer, len(data)))

    def read_some(self, size):
        return self.read(size)

    def write_some(self, data):
        buffer = (ct.c_uint8 * len(data)).from_buffer_copy(data)
        length = ct.c_uint32()
        self._check(
            self.library.necko_tls_write_some(
                self.handle, buffer, len(data), ct.byref(length)
            )
        )
        return length.value

    def idle(self):
        if getattr(self, "h2", None) is not None:
            return self.h2.idle()
        return self.handle and self.library.necko_tls_pending(self.handle) == 0

    def close(self):
        if getattr(self, "h2", None) is not None:
            self.h2.close()
        else:
            self._close_native()

    def _close_native(self):
        if self.handle:
            self.library.necko_tls_close(self.handle)
            self.handle = _Pointer()
        self.transport.close()

    def __del__(self):
        if getattr(self, "handle", None):
            try:
                self.close()
            except Exception:
                pass
