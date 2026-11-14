# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""TLS ClientHello inputs of each Firefox release (Windows x64, release channel).

Generated from the release tags' sources and checked against ClientHellos
captured from the official builds. Fields map onto necko_tls_profile.
"""

from collections import namedtuple

TLSProfile = namedtuple(
    "TLSProfile",
    "ciphers groups additional_key_shares signatures compression"
    " signed_cert_timestamps grease_ech grease_ech_size",
)

_P1 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc00a, 0xc009, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=1,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(),
    signed_cert_timestamps=False,
    grease_ech=False,
    grease_ech_size=0,
)
_P2 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc00a, 0xc009, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=1,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(),
    signed_cert_timestamps=False,
    grease_ech=True,
    grease_ech_size=100,
)
_P3 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc00a, 0xc009, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x11ec, 0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=2,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(1, 2, 3),
    signed_cert_timestamps=False,
    grease_ech=True,
    grease_ech_size=100,
)
_P4 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc00a, 0xc009, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x11ec, 0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=2,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(1, 2, 3),
    signed_cert_timestamps=True,
    grease_ech=True,
    grease_ech_size=100,
)
_P5 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc00a, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x11ec, 0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=2,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(1, 2, 3),
    signed_cert_timestamps=True,
    grease_ech=True,
    grease_ech_size=100,
)
_P6 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x11ec, 0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101),
    additional_key_shares=2,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(1, 2, 3),
    signed_cert_timestamps=True,
    grease_ech=True,
    grease_ech_size=100,
)
_P7 = TLSProfile(
    ciphers=(0x1301, 0x1303, 0x1302, 0xc02b, 0xc02f, 0xcca9, 0xcca8, 0xc02c, 0xc030, 0xc013, 0xc014, 0x009c, 0x009d, 0x002f, 0x0035),
    groups=(0x11ec, 0x001d, 0x0017, 0x0018, 0x0019),
    additional_key_shares=2,
    signatures=(0x0403, 0x0503, 0x0603, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0203, 0x0201),
    compression=(1, 2, 3),
    signed_cert_timestamps=True,
    grease_ech=True,
    grease_ech_size=100,
)

PROFILES = {
    "100": _P1,
    "101": _P1,
    "102": _P1,
    "103": _P1,
    "104": _P1,
    "105": _P1,
    "106": _P1,
    "107": _P1,
    "108": _P1,
    "109": _P1,
    "110": _P1,
    "111": _P1,
    "112": _P1,
    "113": _P1,
    "114": _P1,
    "115": _P1,
    "116": _P1,
    "117": _P1,
    "118": _P1,
    "119": _P2,
    "120": _P2,
    "121": _P2,
    "122": _P2,
    "123": _P2,
    "124": _P2,
    "125": _P2,
    "126": _P2,
    "127": _P2,
    "128": _P2,
    "129": _P2,
    "130": _P2,
    "131": _P2,
    "132": _P3,
    "133": _P3,
    "134": _P3,
    "135": _P4,
    "136": _P4,
    "137": _P4,
    "138": _P4,
    "139": _P4,
    "140": _P4,
    "141": _P4,
    "142": _P4,
    "143": _P4,
    "144": _P4,
    "145": _P4,
    "146": _P4,
    "147": _P4,
    "148": _P4,
    "149": _P4,
    "150": _P4,
    "151": _P5,
    "152": _P5,
    "153": _P5,
    "154": _P6,
    "155": _P6,
    "156": _P7,
    "157": _P7,
    "158": _P7,
    "159": _P7,
    "160": _P7,
}
