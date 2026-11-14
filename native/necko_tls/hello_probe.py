"""Capture ClientHello counts from libnecko_core. Prints counts only."""

from __future__ import annotations

import ctypes as ct
import importlib.util
import json
import socket
import sys
import threading
from pathlib import Path

_READ = ct.CFUNCTYPE(ct.c_int, ct.c_void_p, ct.POINTER(ct.c_uint8), ct.c_uint32, ct.POINTER(ct.c_uint32))
_WRITE = ct.CFUNCTYPE(ct.c_int, ct.c_void_p, ct.POINTER(ct.c_uint8), ct.c_uint32)
_IDLE = ct.CFUNCTYPE(ct.c_int, ct.c_void_p)
_VERIFY = ct.CFUNCTYPE(ct.c_int, ct.c_void_p, ct.POINTER(ct.c_uint8), ct.c_uint32)
_DECODE = ct.CFUNCTYPE(
    ct.c_int, ct.c_void_p, ct.c_uint16, ct.POINTER(ct.c_uint8), ct.c_uint32, ct.POINTER(ct.c_uint8), ct.c_uint32
)


_U16S = ct.POINTER(ct.c_uint16)


class Profile(ct.Structure):
    _fields_ = [
        ("ciphers", _U16S),
        ("cipher_count", ct.c_uint32),
        ("groups", _U16S),
        ("group_count", ct.c_uint32),
        ("additional_key_shares", ct.c_uint32),
        ("signatures", _U16S),
        ("signature_count", ct.c_uint32),
        ("compression", _U16S),
        ("compression_count", ct.c_uint32),
        ("signed_cert_timestamps", ct.c_int),
        ("grease_ech", ct.c_int),
        ("grease_ech_size", ct.c_uint32),
    ]


def load_profiles(path: Path):
    spec = importlib.util.spec_from_file_location("necko_tls_profiles", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("profile table failed to load")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.PROFILES


def native_profile(profile) -> tuple[Profile, list]:
    def array(values):
        return (ct.c_uint16 * max(len(values), 1))(*values)

    arrays = [
        array(profile.ciphers),
        array(profile.groups),
        array(profile.signatures),
        array(profile.compression),
    ]
    native = Profile(
        ct.cast(arrays[0], _U16S),
        len(profile.ciphers),
        ct.cast(arrays[1], _U16S),
        len(profile.groups),
        profile.additional_key_shares,
        ct.cast(arrays[2], _U16S),
        len(profile.signatures),
        ct.cast(arrays[3], _U16S),
        len(profile.compression),
        int(profile.signed_cert_timestamps),
        int(profile.grease_ech),
        profile.grease_ech_size,
    )
    return native, arrays


def is_grease(value: int) -> bool:
    return (value & 0x0F0F) == 0x0A0A


def parse_client_hello(data: bytes) -> dict:
    if len(data) < 5 or data[0] != 22:
        raise ValueError(f"not a handshake record, bytes={len(data)} lead={data[:1].hex()}")
    record_len = int.from_bytes(data[3:5], "big")
    record = data[5 : 5 + record_len]
    if len(record) < 4 or record[0] != 1:
        raise ValueError("record is not a ClientHello")
    body = record[4:]
    index = 34
    session_len = body[index]
    index += 1 + session_len
    cipher_bytes = int.from_bytes(body[index : index + 2], "big")
    index += 2
    cipher_count = 0
    for offset in range(0, cipher_bytes, 2):
        value = int.from_bytes(body[index + offset : index + offset + 2], "big")
        if not is_grease(value):
            cipher_count += 1
    index += cipher_bytes
    compression_len = body[index]
    index += 1 + compression_len
    extension_bytes = int.from_bytes(body[index : index + 2], "big")
    index += 2
    end = index + extension_bytes
    group_count = 0
    key_share_lengths = []
    signed_cert_timestamps = False
    ech = False
    compression_algorithms = 0
    while index + 4 <= end:
        extension_type = int.from_bytes(body[index : index + 2], "big")
        extension_len = int.from_bytes(body[index + 2 : index + 4], "big")
        index += 4
        payload = body[index : index + extension_len]
        index += extension_len
        if extension_type == 10 and len(payload) >= 2:
            group_bytes = int.from_bytes(payload[:2], "big")
            for offset in range(2, 2 + group_bytes, 2):
                value = int.from_bytes(payload[offset : offset + 2], "big")
                if not is_grease(value):
                    group_count += 1
        elif extension_type == 51 and len(payload) >= 2:
            share_bytes = int.from_bytes(payload[:2], "big")
            cursor = 2
            stop = 2 + share_bytes
            while cursor + 4 <= stop:
                key_len = int.from_bytes(payload[cursor + 2 : cursor + 4], "big")
                key_share_lengths.append(key_len)
                cursor += 4 + key_len
        elif extension_type == 18:
            signed_cert_timestamps = True
        elif extension_type == 0xFE0D:
            ech = True
        elif extension_type == 27 and payload:
            compression_algorithms = payload[0] // 2
    return {
        "cipher_count": cipher_count,
        "group_count": group_count,
        "key_share_count": len(key_share_lengths),
        "key_share_lengths": key_share_lengths,
        "signed_cert_timestamps": signed_cert_timestamps,
        "ech": ech,
        "compression_algorithms": compression_algorithms,
        "nbytes": 5 + record_len,
    }


def capture(library, profile, port: int) -> dict:
    native, arrays = native_profile(profile)
    box = {}

    def serve(server):
        connection, _ = server.accept()
        connection.settimeout(3)
        try:
            header = b""
            while len(header) < 5:
                chunk = connection.recv(5 - len(header))
                if not chunk:
                    break
                header += chunk
            body = b""
            if len(header) == 5:
                need = int.from_bytes(header[3:5], "big")
                while len(body) < need:
                    chunk = connection.recv(need - len(body))
                    if not chunk:
                        break
                    body += chunk
            box["data"] = header + body
        except OSError as error:
            box["error"] = type(error).__name__
        finally:
            connection.close()

    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", port))
    server.listen(1)
    server.settimeout(5)
    thread = threading.Thread(target=serve, args=(server,), daemon=True)
    thread.start()
    client = socket.create_connection(("127.0.0.1", server.getsockname()[1]), timeout=3)
    client.settimeout(3)
    sent = bytearray()

    def read_cb(_closure, buffer, size, length):
        try:
            data = client.recv(size)
        except socket.timeout:
            return -1
        except OSError:
            return -1
        if data:
            ct.memmove(buffer, data, len(data))
        length[0] = len(data)
        return 0

    def write_cb(_closure, buffer, length):
        data = ct.string_at(buffer, length)
        sent.extend(data)
        client.sendall(data)
        return 0

    def idle_cb(_closure):
        return 0

    def verify_cb(_closure, _buffer, _length):
        return 0

    def decode_cb(_closure, _algorithm, _buffer, _length, _output, _capacity):
        return -1

    read = _READ(read_cb)
    write = _WRITE(write_cb)
    idle = _IDLE(idle_cb)
    verify = _VERIFY(verify_cb)
    decode = _DECODE(decode_cb)
    library.necko_tls_open_profile.argtypes = [
        ct.c_char_p,
        ct.c_char_p,
        ct.c_int,
        ct.POINTER(Profile),
        _READ,
        _WRITE,
        _IDLE,
        _VERIFY,
        _DECODE,
        ct.c_void_p,
        ct.c_char_p,
        ct.c_uint32,
        ct.c_char_p,
        ct.c_uint32,
        ct.POINTER(ct.c_void_p),
    ]
    handle = ct.c_void_p()
    rc = library.necko_tls_open_profile(
        b"localhost",
        b"probe",
        1,
        ct.byref(native),
        read,
        write,
        idle,
        verify,
        decode,
        None,
        None,
        0,
        None,
        0,
        ct.byref(handle),
    )
    error = library.necko_tls_error()
    if handle:
        library.necko_tls_close.argtypes = [ct.c_void_p]
        library.necko_tls_close(handle)
    client.close()
    thread.join(timeout=5)
    server.close()
    data = box.get("data") or bytes(sent)
    parsed = parse_client_hello(data)
    parsed["open_result"] = rc
    parsed["open_error"] = (error or b"").decode("ascii", "replace")
    del arrays
    return parsed


def main() -> int:
    library_path = Path(sys.argv[1])
    profile_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parent / "necko_win" / "_tls_profiles.py"
    library = ct.CDLL(str(library_path))
    library.necko_tls_nss_version.restype = ct.c_char_p
    library.necko_tls_error.restype = ct.c_char_p
    library.necko_tls_open_profile.restype = ct.c_int
    version = library.necko_tls_nss_version()
    profiles = load_profiles(profile_path)
    report = {"nss": (version or b"").decode("ascii", "replace")}
    if version != b"3.132":
        report["pass"] = False
        print(json.dumps(report))
        return 1
    for name in ("100", "159"):
        report[name] = capture(library, profiles[name], 0)
    shape = lambda item: (item["cipher_count"], item["group_count"], item["key_share_count"])
    report["pass"] = shape(report["159"]) == (15, 5, 3) and shape(report["100"]) != (15, 5, 3)
    print(json.dumps(report, sort_keys=True))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
