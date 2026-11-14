// Native NSS 3.132 implementation of the upstream necko_tls ABI.
#include "tls_io.h"
#include <nss.h>
#include <ssl.h>
#include <sslexp.h>
#include <sslproto.h>
#include <prinit.h>
#include <prerror.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
namespace {
bool Initialize() {
  static const bool ready = [] { return NSS_NoDB_Init(nullptr) == SECSuccess && NSS_SetDomesticPolicy() == SECSuccess; }();
  return ready;
}
struct Active { Tls* previous; Active(Tls* value) : previous(active_tls) { active_tls = value; } ~Active() { active_tls = previous; } };
bool Configure(Tls* t, const Profile& p, const char* host, const char* peer, bool h2) {
  auto* fd = t->fd;
  SSLVersionRange range{SSL_LIBRARY_VERSION_TLS_1_2, SSL_LIBRARY_VERSION_TLS_1_3};
  if (SSL_VersionRangeSet(fd, &range) != SECSuccess || SSL_SetURL(fd, host) != SECSuccess ||
      SSL_SetSockPeerID(fd, peer) != SECSuccess || SSL_AuthCertificateHook(fd, VerifyPeer, t) != SECSuccess) return false;
  // Firefox 160's ClientHello (ci/firefox160-hello.json, checked by
  // ci/hello_check.py) carries status_request, delegated_credentials and a
  // 32-byte legacy session id, and no post_handshake_auth; the Windows
  // necko_core.dll sends the same.
  const std::pair<int, int> options[] = {{SSL_SECURITY, PR_TRUE}, {SSL_HANDSHAKE_AS_CLIENT, PR_TRUE},
    {SSL_ENABLE_SESSION_TICKETS, PR_TRUE}, {SSL_ENABLE_EXTENDED_MASTER_SECRET, PR_TRUE},
    {SSL_ENABLE_OCSP_STAPLING, PR_TRUE}, {SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, p.signed_cert_timestamps},
    {SSL_ENABLE_ALPN, PR_TRUE}, {SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE},
    {SSL_ENABLE_DELEGATED_CREDENTIALS, PR_TRUE}, {SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_FALSE},
    {SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_REQUIRES_XTN}};
  for (auto [option, value] : options) if (SSL_OptionSet(fd, option, value) != SECSuccess) return false;
  for (unsigned i = 0; i < SSL_NumImplementedCiphers; ++i)
    if (SSL_CipherPrefSet(fd, SSL_ImplementedCiphers[i], PR_FALSE) != SECSuccess) return false;
  if (!p.cipher_count || p.cipher_count > 256 || !p.group_count || p.group_count > 256 ||
      !p.signature_count || p.signature_count > 256 || p.compression_count > 3) return false;
  for (unsigned i = 0; i < p.cipher_count; ++i)
    if (SSL_CipherPrefSet(fd, p.ciphers[i], PR_TRUE) != SECSuccess) return false;
  std::vector<SSLNamedGroup> groups;
  for (unsigned i = 0; i < p.group_count; ++i) groups.push_back(SSLNamedGroup(p.groups[i]));
  std::vector<SSLSignatureScheme> signatures;
  for (unsigned i = 0; i < p.signature_count; ++i) signatures.push_back(SSLSignatureScheme(p.signatures[i]));
  if (SSL_CipherSuiteOrderSet(fd, p.ciphers, p.cipher_count) != SECSuccess ||
      SSL_NamedGroupConfig(fd, groups.data(), groups.size()) != SECSuccess ||
      SSL_SendAdditionalKeyShares(fd, p.additional_key_shares) != SECSuccess ||
      SSL_SignatureSchemePrefSet(fd, signatures.data(), signatures.size()) != SECSuccess ||
      SSL_EnableTls13GreaseEch(fd, p.grease_ech) != SECSuccess) return false;
  if (p.grease_ech && SSL_SetTls13GreaseEchSize(fd, p.grease_ech_size) != SECSuccess) return false;
  SSLCertificateCompressionAlgorithm algorithms[] = {{1, "zlib", nullptr, DecodeZlib},
      {2, "brotli", nullptr, DecodeBrotli}, {3, "zstd", nullptr, DecodeZstd}};
  for (unsigned i = 0; i < p.compression_count; ++i) {
    const unsigned id = p.compression[i];
    if (id < 1 || id > 3) return false;
    auto algorithm = algorithms[id - 1];
    if (SSL_SetCertificateCompressionAlgorithm(fd, algorithm) != SECSuccess) return false;
  }
  // NSS moves the first protocol (the NPN fallback) to the end of the ALPN
  // list, so http/1.1 goes first here to send h2, http/1.1 as Firefox does.
  const auto* protocols = reinterpret_cast<const uint8_t*>(h2 ? "\10http/1.1\2h2" : "\10http/1.1");
  return SSL_SetNextProtoNego(fd, protocols, h2 ? 12 : 9) == SECSuccess && SSL_ResetHandshake(fd, PR_FALSE) == SECSuccess;
}
}
extern "C" {
int necko_tls_open_profile(const char* host, const char* peer, int h2, const Profile* profile,
    Read read, Write write, Idle idle, Verify verify, Decode decode, void* user,
    const char* cert, uint32_t cert_len, const char* key, uint32_t key_len, void** output) {
  if (!output || !profile || !host || !peer || !read || !write || !verify) return -1;
  *output = nullptr; tls_error.clear();
  if (!Initialize()) return TlsStatus(-1);
  auto tls = std::make_unique<Tls>();
  tls->read = read; tls->write = write; tls->idle = idle; tls->verify = verify; tls->decode = decode; tls->user = user;
  Active active(tls.get());
  auto* layer = TlsLayer(tls.get());
  if (!layer) return TlsStatus(-1);
  tls->fd = SSL_ImportFD(nullptr, layer);
  if (!tls->fd) { PR_Close(layer); return TlsStatus(-1); }
  if (!ImportClientIdentity(tls.get(), cert, cert_len, key, key_len)) return TlsStatus(-1);
  if (!Configure(tls.get(), *profile, host, peer, h2) || SSL_ForceHandshake(tls->fd) != SECSuccess ||
      SSL_GetChannelInfo(tls->fd, &tls->info, sizeof(tls->info)) != SECSuccess) return TlsStatus(-1);
  unsigned char protocol[256]; unsigned int length = 0; SSLNextProtoState state;
  if (SSL_GetNextProto(tls->fd, &state, protocol, &length, sizeof(protocol)) != SECSuccess) return TlsStatus(-1);
  tls->alpn.assign(reinterpret_cast<char*>(protocol), length);
  *output = tls.release(); return 0;
}
int necko_tls_open(const char* host, const char* peer, int h2, Read read, Write write,
    Idle idle, Verify verify, Decode decode, void* user, const char* cert, uint32_t cert_len,
    const char* key, uint32_t key_len, void** output) {
  tls_error = "Select an explicit Firefox TLS profile"; return -1;
}
int necko_tls_read(void* value, uint8_t* data, uint32_t size, uint32_t* used) {
  if (!value || !used || size > INT32_MAX) return -1;
  auto* tls = static_cast<Tls*>(value); Active active(tls); *used = 0;
  const auto n = PR_Read(tls->fd, data, size);
  if (n >= 0) *used = uint32_t(n);
  return TlsStatus(n);
}
int necko_tls_write_some(void* value, const uint8_t* data, uint32_t size, uint32_t* used) {
  if (!value || !used || size > INT32_MAX) return -1;
  auto* tls = static_cast<Tls*>(value); Active active(tls); *used = 0;
  const auto n = PR_Write(tls->fd, data, size);
  if (n >= 0) *used = uint32_t(n);
  return TlsStatus(n);
}
int necko_tls_write(void* value, const uint8_t* data, uint32_t size) {
  while (size) {
    uint32_t used = 0; const auto error = necko_tls_write_some(value, data, size, &used);
    if (error || !used) return error ? error : -1;
    data += used; size -= used;
  }
  return 0;
}
void necko_tls_set_nonblocking(void* value, WriteSome write) { static_cast<Tls*>(value)->write_some = write; }
void necko_tls_close(void* value) { delete static_cast<Tls*>(value); }
int necko_tls_pending(void* value) {
  if (!value) return -1;
  auto* tls = static_cast<Tls*>(value); Active active(tls);
  // Firefox's folded libnss3.dylib keeps SSL_DataPending as a local symbol.
#if !defined(__APPLE__)
  const auto buffered = SSL_DataPending(tls->fd);
  if (buffered > 0 || !tls->write_some) return buffered;
#endif
  if (!tls->write_some) return 0;
  // The upstream H2 pump asks whether socket data can become application data,
  // including records not yet pulled into NSS. SSL_DataPending alone cannot
  // observe a newly readable TCP socket and would stall every H2 response.
  uint8_t byte;
  const auto n = PR_Recv(tls->fd, &byte, 1, PR_MSG_PEEK, PR_INTERVAL_NO_WAIT);
  if (n > 0) {
#if defined(__APPLE__)
    return 1;
#else
    return std::max(1, SSL_DataPending(tls->fd));
#endif
  }
  if (n < 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR) return 0;
  if (n < 0) TlsStatus(n);
  return -1;
}
const char* necko_tls_alpn(void* value) { return static_cast<Tls*>(value)->alpn.c_str(); }
uint16_t necko_tls_version(void* value) { return static_cast<Tls*>(value)->info.protocolVersion; }
uint16_t necko_tls_cipher(void* value) { return static_cast<Tls*>(value)->info.cipherSuite; }
int necko_tls_resumed(void* value) { return static_cast<Tls*>(value)->info.resumed; }
const char* necko_tls_nss_version() { return NSS_GetVersion(); }
const char* necko_tls_nspr_version() { return PR_GetVersion(); }
const char* necko_tls_error() { return tls_error.c_str(); }
}
