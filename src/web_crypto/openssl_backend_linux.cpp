#if defined(__linux__)
// Linux WebCrypto backend; the public byte-oriented contract matches CNG.
#include "web_crypto/cng_backend.h"
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <climits>
#include <memory>

namespace ruyijs::web_crypto {
namespace {
namespace ec = domain::crypto;
template<class T, void (*Free)(T*)> using Owner = std::unique_ptr<T, decltype(Free)>;
using Key = Owner<EVP_PKEY, EVP_PKEY_free>;
const EVP_MD* Digest(HashAlgorithm algorithm) {
  switch (unsigned(algorithm)) {
    case 0: return EVP_sha1();
    case 1: return EVP_sha256();
    case 2: return EVP_sha384();
    case 3: return EVP_sha512();
    default: return nullptr;
  }
}
const EVP_CIPHER* Cipher(size_t bytes, bool gcm) {
  switch (bytes) {
    case 16: return gcm ? EVP_aes_128_gcm() : EVP_aes_128_cbc();
    case 24: return gcm ? EVP_aes_192_gcm() : EVP_aes_192_cbc();
    case 32: return gcm ? EVP_aes_256_gcm() : EVP_aes_256_cbc();
    default: return nullptr;
  }
}
Key Import(const ec::EcKey& key, bool privateKey) {
  if (!key.curve || key.x.size() != key.curve->size || key.y.size() != key.curve->size ||
      (privateKey && key.d.size() != key.curve->size)) return {nullptr, EVP_PKEY_free};
  const auto der = privateKey ? ec::EncodeEcPkcs8(key) : ec::EncodeEcSpki(key);
  const auto* at = der.data();
  EVP_PKEY* result = privateKey ? d2i_AutoPrivateKey(nullptr, &at, long(der.size()))
                                : d2i_PUBKEY(nullptr, &at, long(der.size()));
  return {result, EVP_PKEY_free};
}
bool Coordinate(EVP_PKEY* key, const char* name, size_t size, Bytes* out) {
  BIGNUM* raw = nullptr;
  if (EVP_PKEY_get_bn_param(key, name, &raw) != 1) return false;
  Owner<BIGNUM, BN_clear_free> value(raw, BN_clear_free);
  out->resize(size);
  return BN_bn2binpad(value.get(), out->data(), int(size)) == int(size);
}
}  // namespace

bool ComputeDigest(HashAlgorithm algorithm, const void* input, size_t size, DigestBytes* out) noexcept {
  const auto* md = Digest(algorithm);
  unsigned bytes = 0;
  if (!md || EVP_Digest(input, size, out->data, &bytes, md, nullptr) != 1) return false;
  out->size = bytes;
  return true;
}
bool Hmac(HashAlgorithm algorithm, const Bytes& key, const Bytes& data, Bytes* out) {
  const auto* md = Digest(algorithm);
  if (!md) return false;
  Bytes result(EVP_MAX_MD_SIZE);
  size_t bytes = 0;
  const auto* secret = key.empty() ? reinterpret_cast<const unsigned char*>("") : key.data();
  if (!EVP_Q_mac(nullptr, "HMAC", nullptr, EVP_MD_get0_name(md), nullptr, secret, key.size(),
                  data.data(), data.size(), result.data(), result.size(), &bytes)) return false;
  result.resize(bytes);
  *out = std::move(result);
  return true;
}
bool AesCbc(const Bytes& key, const Bytes& iv, const Bytes& input, bool encrypt, Bytes* out) {
  const auto* cipher = Cipher(key.size(), false);
  if (!cipher || iv.size() != 16 || input.size() > INT_MAX - 16) return false;
  Owner<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  Bytes result(input.size() + 16);
  int bytes = 0, tail = 0;
  if (!ctx || EVP_CipherInit_ex(ctx.get(), cipher, nullptr, key.data(), iv.data(), encrypt) != 1 ||
      EVP_CipherUpdate(ctx.get(), result.data(), &bytes, input.data(), int(input.size())) != 1 ||
      EVP_CipherFinal_ex(ctx.get(), result.data() + bytes, &tail) != 1) return false;
  result.resize(size_t(bytes + tail));
  *out = std::move(result);
  return true;
}
bool AesGcm(const Bytes& key, const Bytes& iv, const Bytes& aad, size_t tagBytes,
            const Bytes& input, bool encrypt, Bytes* out) {
  const auto* cipher = Cipher(key.size(), true);
  if (!cipher || iv.empty() || iv.size() > INT_MAX || aad.size() > INT_MAX ||
      input.size() > INT_MAX || tagBytes < 4 || tagBytes > 16 || (!encrypt && input.size() < tagBytes)) return false;
  const size_t body = encrypt ? input.size() : input.size() - tagBytes;
  Owner<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  Bytes result(body + 16), tag(tagBytes);
  int bytes = 0, tail = 0, ignored = 0;
  if (!ctx || EVP_CipherInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr, encrypt) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, int(iv.size()), nullptr) != 1 ||
      EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data(), encrypt) != 1 ||
      (!aad.empty() && EVP_CipherUpdate(ctx.get(), nullptr, &ignored, aad.data(), int(aad.size())) != 1) ||
      EVP_CipherUpdate(ctx.get(), result.data(), &bytes, input.data(), int(body)) != 1) return false;
  if (!encrypt) {
    tag.assign(input.begin() + body, input.end());
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, int(tagBytes), tag.data()) != 1) return false;
  }
  if (EVP_CipherFinal_ex(ctx.get(), result.data() + bytes, &tail) != 1) return false;
  result.resize(size_t(bytes + tail));
  if (encrypt) {
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, int(tagBytes), tag.data()) != 1) return false;
    result.insert(result.end(), tag.begin(), tag.end());
  }
  *out = std::move(result);
  return true;
}
bool EcGenerate(const ec::EcCurve& curve, bool, ec::EcKey* out) {
  const char* group = curve.size == 32 ? "prime256v1" : curve.size == 48 ? "secp384r1" : curve.size == 66 ? "secp521r1" : nullptr;
  if (!group) return false;
  Owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
  EVP_PKEY* raw = nullptr;
  if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 || EVP_PKEY_CTX_set_group_name(ctx.get(), group) != 1 ||
      EVP_PKEY_generate(ctx.get(), &raw) != 1) return false;
  Key key(raw, EVP_PKEY_free);
  ec::EcKey result;
  result.curve = &curve;
  if (!Coordinate(raw, OSSL_PKEY_PARAM_EC_PUB_X, curve.size, &result.x) ||
      !Coordinate(raw, OSSL_PKEY_PARAM_EC_PUB_Y, curve.size, &result.y) ||
      !Coordinate(raw, OSSL_PKEY_PARAM_PRIV_KEY, curve.size, &result.d)) return false;
  *out = std::move(result);
  return true;
}
bool EcValidate(const ec::EcKey& value, bool) {
  auto key = Import(value, !value.d.empty());
  if (!key) return false;
  Owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
  return ctx && EVP_PKEY_public_check(ctx.get()) == 1 &&
      (value.d.empty() || EVP_PKEY_pairwise_check(ctx.get()) == 1);
}
bool EcdsaSign(const ec::EcKey& value, HashAlgorithm algorithm, const Bytes& message, Bytes* out) {
  auto key = Import(value, true);
  const auto* md = Digest(algorithm);
  Owner<EVP_MD_CTX, EVP_MD_CTX_free> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  size_t size = 0;
  if (!key || !md || !ctx || EVP_DigestSignInit(ctx.get(), nullptr, md, nullptr, key.get()) != 1 ||
      EVP_DigestSign(ctx.get(), nullptr, &size, message.data(), message.size()) != 1) return false;
  Bytes der(size);
  if (EVP_DigestSign(ctx.get(), der.data(), &size, message.data(), message.size()) != 1) return false;
  const auto* at = der.data();
  Owner<ECDSA_SIG, ECDSA_SIG_free> signature(d2i_ECDSA_SIG(nullptr, &at, long(size)), ECDSA_SIG_free);
  if (!signature) return false;
  Bytes result(value.curve->size * 2);
  if (BN_bn2binpad(ECDSA_SIG_get0_r(signature.get()), result.data(), int(value.curve->size)) < 0 ||
      BN_bn2binpad(ECDSA_SIG_get0_s(signature.get()), result.data() + value.curve->size, int(value.curve->size)) < 0) return false;
  *out = std::move(result);
  return true;
}
bool EcdsaVerify(const ec::EcKey& value, HashAlgorithm algorithm, const Bytes& message,
                 const Bytes& signature, bool* valid) {
  *valid = false;
  auto key = Import(value, false);
  const auto* md = Digest(algorithm);
  if (!key || !md) return false;
  if (signature.size() != 2 * value.curve->size) return true;
  Owner<ECDSA_SIG, ECDSA_SIG_free> sig(ECDSA_SIG_new(), ECDSA_SIG_free);
  BIGNUM* r = BN_bin2bn(signature.data(), int(value.curve->size), nullptr);
  BIGNUM* s = BN_bin2bn(signature.data() + value.curve->size, int(value.curve->size), nullptr);
  if (!sig || !r || !s || ECDSA_SIG_set0(sig.get(), r, s) != 1) { BN_free(r); BN_free(s); return false; }
  const int length = i2d_ECDSA_SIG(sig.get(), nullptr);
  if (length <= 0) return false;
  Bytes der(size_t(length), 0);
  auto* at = der.data();
  if (i2d_ECDSA_SIG(sig.get(), &at) != length) return false;
  Owner<EVP_MD_CTX, EVP_MD_CTX_free> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, md, nullptr, key.get()) != 1) return false;
  const int result = EVP_DigestVerify(ctx.get(), der.data(), der.size(), message.data(), message.size());
  if (result < 0) return false;
  *valid = result == 1;
  return true;
}
bool EcdhSecret(const ec::EcKey& own, const ec::EcKey& peer, Bytes* out) {
  if (!own.curve || own.curve != peer.curve) return false;
  auto mine = Import(own, true), theirs = Import(peer, false);
  if (!mine || !theirs) return false;
  Owner<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new(mine.get(), nullptr), EVP_PKEY_CTX_free);
  size_t size = 0;
  if (!ctx || EVP_PKEY_derive_init(ctx.get()) != 1 || EVP_PKEY_derive_set_peer(ctx.get(), theirs.get()) != 1 ||
      EVP_PKEY_derive(ctx.get(), nullptr, &size) != 1) return false;
  Bytes result(size);
  if (EVP_PKEY_derive(ctx.get(), result.data(), &size) != 1) return false;
  result.resize(size);
  *out = std::move(result);
  return true;
}
}

#endif  // __linux__
