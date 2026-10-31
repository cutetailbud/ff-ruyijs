#include "web_crypto/cng_backend.h"
#include <cassert>
#include <string>

using namespace ruyijs::web_crypto;
namespace ec = ruyijs::domain::crypto;
Bytes Hex(const char* hex) {
  Bytes out;
  auto digit = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
  for (size_t i = 0; hex[i]; i += 2) out.push_back(uint8_t(digit(hex[i]) * 16 + digit(hex[i+1])));
  return out;
}
int main() {
  DigestBytes hash;
  assert(ComputeDigest(HashAlgorithm(1), "abc", 3, &hash));
  assert(Bytes(hash.data, hash.data + hash.size) == Hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  Bytes actual;
  const Bytes message{'H','i',' ','T','h','e','r','e'};
  assert(Hmac(HashAlgorithm(1), Bytes(20, 0x0b), message, &actual));
  assert(actual == Hex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
  assert(AesGcm(Bytes(16), Bytes(12), {}, 16, Bytes(16), true, &actual));
  assert(actual == Hex("0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf"));
  Bytes plaintext;
  assert(AesGcm(Bytes(16), Bytes(12), {}, 16, actual, false, &plaintext));
  assert(plaintext == Bytes(16));
  actual.back() ^= 1;
  plaintext = {42};
  assert(!AesGcm(Bytes(16), Bytes(12), {}, 16, actual, false, &plaintext));
  assert(plaintext == Bytes{42});
  const Bytes key = Hex("2b7e151628aed2a6abf7158809cf4f3c");
  const Bytes iv = Hex("000102030405060708090a0b0c0d0e0f");
  const Bytes block = Hex("6bc1bee22e409f96e93d7e117393172a");
  assert(AesCbc(key, iv, block, true, &actual));
  assert(actual.size() == 32);
  assert(Bytes(actual.begin(), actual.begin() + 16) == Hex("7649abac8119b246cee98e9b12e9197d"));
  assert(AesCbc(key, iv, actual, false, &plaintext) && plaintext == block);
  for (const char* name : {"P-256", "P-384", "P-521"}) {
    const auto& curve = *ec::EcCurveNamed(name);
    ec::EcKey a, b;
    assert(EcGenerate(curve, false, &a) && EcGenerate(curve, true, &b));
    assert(EcValidate(a, false) && EcValidate(b, true));
    Bytes signature, x, y;
    assert(EcdsaSign(a, HashAlgorithm(1), message, &signature));
    bool valid = false;
    assert(EcdsaVerify(a, HashAlgorithm(1), message, signature, &valid) && valid);
    assert(EcdsaVerify(a, HashAlgorithm(1), Bytes{0}, signature, &valid) && !valid);
    signature.resize(1);
    assert(EcdsaVerify(a, HashAlgorithm(1), message, signature, &valid) && !valid);
    assert(EcdhSecret(a, b, &x) && EcdhSecret(b, a, &y));
    assert(x == y && x.size() == curve.size);
    a.x.assign(curve.size, 0);
    a.y.assign(curve.size, 0);
    assert(!EcValidate(a, false));
  }
}
