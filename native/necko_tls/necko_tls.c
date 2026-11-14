/* arm64 adapter for the necko_tls_* entry points.
   The caller supplies the profile. This file does not contain a cipher list.
   It links the donor NSS already staged with xpcshell and lets that NSS
   build the ClientHello. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nss.h"
#include "prio.h"
#include "prerror.h"
#include "prinit.h"
#include "prnetdb.h"
#include "secitem.h"
#include "ssl.h"
#include "sslexp.h"
#include "sslproto.h"
#include "sslt.h"

#define NECKO_EXPORT __attribute__((visibility("default")))

typedef struct necko_tls_profile {
    const uint16_t *ciphers;
    uint32_t cipher_count;
    const uint16_t *groups;
    uint32_t group_count;
    uint32_t additional_key_shares;
    const uint16_t *signatures;
    uint32_t signature_count;
    const uint16_t *compression;
    uint32_t compression_count;
    int32_t signed_cert_timestamps;
    int32_t grease_ech;
    uint32_t grease_ech_size;
} necko_tls_profile;

_Static_assert(sizeof(necko_tls_profile) == 72, "necko_tls_profile size");
_Static_assert(offsetof(necko_tls_profile, grease_ech_size) == 68, "necko_tls_profile layout");

typedef int (*necko_read_fn)(void *, uint8_t *, uint32_t, uint32_t *);
typedef int (*necko_write_fn)(void *, const uint8_t *, uint32_t);
typedef int (*necko_write_some_fn)(void *, const uint8_t *, uint32_t, uint32_t *);
typedef int (*necko_idle_fn)(void *);
typedef int (*necko_verify_fn)(void *, const uint8_t *, uint32_t);
typedef int (*necko_decode_fn)(void *, uint16_t, const uint8_t *, uint32_t, uint8_t *, uint32_t);

typedef struct necko_conn {
    PRFileDesc *ssl;
    PRFileDesc *lower;
    necko_read_fn read_cb;
    necko_write_fn write_cb;
    necko_write_some_fn write_some_cb;
    necko_idle_fn idle_cb;
    necko_verify_fn verify_cb;
    necko_decode_fn decode_cb;
    void *closure;
    int nonblocking;
    int in_pending;
    int http2;
    char alpn[32];
    uint16_t cipher;
    uint16_t version;
    int resumed;
} necko_conn;

static __thread char g_error[384];
static __thread necko_conn *g_current;
static __thread uint16_t g_decode_ids[8];
static PRDescIdentity g_identity = PR_INVALID_IO_LAYER;
static int g_nss_ready;

static void set_error(const char *text) {
    snprintf(g_error, sizeof(g_error), "%s", text ? text : "nss");
}

static void set_nss_error(const char *what) {
    const char *name = PR_ErrorToName(PR_GetError());
    snprintf(g_error, sizeof(g_error), "%s (%s)", what, name ? name : "nss");
}

static int ensure_nss(void) {
    if (g_nss_ready) {
        return 0;
    }
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    if (NSS_IsInitialized()) {
        g_nss_ready = 1;
        return 0;
    }
    if (NSS_NoDB_Init(NULL) != SECSuccess) {
        set_nss_error("NSS_NoDB_Init");
        return -1;
    }
    if (NSS_SetDomesticPolicy() != SECSuccess) {
        set_nss_error("NSS_SetDomesticPolicy");
        return -1;
    }
    g_nss_ready = 1;
    return 0;
}

static necko_conn *conn_from(PRFileDesc *fd) {
    return (necko_conn *)fd->secret;
}

static PRStatus layer_close(PRFileDesc *fd) {
    if (!fd) {
        return PR_FAILURE;
    }
    fd->secret = NULL;
    if (fd->dtor) {
        fd->dtor(fd);
    }
    return PR_SUCCESS;
}

static PRInt32 layer_read(PRFileDesc *fd, void *buf, PRInt32 amount) {
    necko_conn *conn = conn_from(fd);
    uint32_t got = 0;
    int rc;
    if (!conn || !conn->read_cb || !buf || amount < 0) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    if (amount == 0) {
        return 0;
    }
    if (conn->in_pending && conn->idle_cb && conn->idle_cb(conn->closure) != 0) {
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
    }
    rc = conn->read_cb(conn->closure, buf, (uint32_t)amount, &got);
    if (rc == -2) {
        PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
        return -1;
    }
    if (rc != 0) {
        PR_SetError(PR_IO_ERROR, 0);
        return -1;
    }
    return (PRInt32)got;
}

static PRInt32 layer_write(PRFileDesc *fd, const void *buf, PRInt32 amount) {
    necko_conn *conn = conn_from(fd);
    int rc;
    if (!conn || amount < 0) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    if (conn->nonblocking && conn->write_some_cb) {
        uint32_t wrote = 0;
        rc = conn->write_some_cb(conn->closure, buf, (uint32_t)amount, &wrote);
        if (rc == -2) {
            PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
            return -1;
        }
        if (rc != 0) {
            PR_SetError(PR_IO_ERROR, 0);
            return -1;
        }
        return (PRInt32)wrote;
    }
    if (!conn->write_cb) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    rc = conn->write_cb(conn->closure, buf, (uint32_t)amount);
    if (rc != 0) {
        PR_SetError(PR_IO_ERROR, 0);
        return -1;
    }
    return amount;
}

static PRInt32 layer_available(PRFileDesc *fd) {
    (void)fd;
    return 0;
}

static PRInt64 layer_available64(PRFileDesc *fd) {
    (void)fd;
    return 0;
}

static PRInt32 layer_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                          PRIntervalTime timeout) {
    (void)timeout;
    if (flags != 0) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    return layer_read(fd, buf, amount);
}

static PRInt32 layer_send(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
                          PRIntervalTime timeout) {
    (void)timeout;
    if (flags != 0) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    return layer_write(fd, buf, amount);
}

static PRInt16 layer_poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags) {
    (void)fd;
    *out_flags = in_flags;
    return in_flags;
}

static PRStatus layer_getopt(PRFileDesc *fd, PRSocketOptionData *data) {
    necko_conn *conn = conn_from(fd);
    if (!data) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (data->option == PR_SockOpt_Nonblocking) {
        data->value.non_blocking =
            (conn && (conn->in_pending || conn->nonblocking)) ? PR_TRUE : PR_FALSE;
        return PR_SUCCESS;
    }
    PR_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus layer_setopt(PRFileDesc *fd, const PRSocketOptionData *data) {
    necko_conn *conn = conn_from(fd);
    if (data->option == PR_SockOpt_Nonblocking && conn) {
        conn->nonblocking = data->value.non_blocking ? 1 : 0;
        return PR_SUCCESS;
    }
    if (data->option == PR_SockOpt_NoDelay) {
        return PR_SUCCESS;
    }
    PR_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR, 0);
    return PR_FAILURE;
}

static PRStatus layer_connect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout) {
    (void)fd;
    (void)addr;
    (void)timeout;
    return PR_SUCCESS;
}

static PRStatus layer_getname(PRFileDesc *fd, PRNetAddr *addr) {
    (void)fd;
    return PR_InitializeNetAddr(PR_IpAddrLoopback, 443, addr);
}

static PRStatus layer_shutdown(PRFileDesc *fd, PRIntn how) {
    (void)fd;
    (void)how;
    return PR_SUCCESS;
}

static const PRIOMethods k_methods = {
    PR_DESC_SOCKET_TCP,
    layer_close,
    layer_read,
    layer_write,
    layer_available,
    layer_available64,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    layer_connect,
    NULL,
    NULL,
    NULL,
    layer_shutdown,
    layer_recv,
    layer_send,
    NULL,
    NULL,
    layer_poll,
    NULL,
    NULL,
    layer_getname,
    layer_getname,
    NULL,
    NULL,
    layer_getopt,
    layer_setopt,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static SECStatus decode_alg(uint16_t algorithm, const SECItem *input, unsigned char *output,
                            size_t output_len, size_t *used_len) {
    necko_conn *conn = g_current;
    int rc;
    if (!conn || !conn->decode_cb || !input || !output || output_len > UINT32_MAX) {
        return SECFailure;
    }
    rc = conn->decode_cb(conn->closure, algorithm, input->data, input->len, output,
                         (uint32_t)output_len);
    if (rc != 0) {
        return SECFailure;
    }
    *used_len = output_len;
    return SECSuccess;
}

#define DECODE_SLOT(slot)                                                                  \
    static SECStatus decode_slot_##slot(const SECItem *input, unsigned char *output,       \
                                        size_t output_len, size_t *used_len) {             \
        return decode_alg(g_decode_ids[slot], input, output, output_len, used_len);        \
    }

DECODE_SLOT(0)
DECODE_SLOT(1)
DECODE_SLOT(2)
DECODE_SLOT(3)
DECODE_SLOT(4)
DECODE_SLOT(5)
DECODE_SLOT(6)
DECODE_SLOT(7)

static SECStatus (*const k_decoders[8])(const SECItem *, unsigned char *, size_t, size_t *) = {
    decode_slot_0, decode_slot_1, decode_slot_2, decode_slot_3,
    decode_slot_4, decode_slot_5, decode_slot_6, decode_slot_7,
};

static SECStatus encode_unused(const SECItem *input, SECItem *output) {
    (void)input;
    (void)output;
    return SECFailure;
}

static SECStatus auth_hook(void *arg, PRFileDesc *fd, PRBool check_sig, PRBool is_server) {
    necko_conn *conn = arg;
    SECItemArray *chain = NULL;
    unsigned char *packed = NULL;
    size_t total = 0;
    size_t offset = 0;
    unsigned int i;
    int rc;
    (void)check_sig;
    (void)is_server;
    if (!conn->verify_cb) {
        return SECSuccess;
    }
    if (SSL_PeerCertificateChainDER(fd, &chain) != SECSuccess || !chain) {
        set_nss_error("certificate chain");
        return SECFailure;
    }
    for (i = 0; i < chain->len; i++) {
        total += 4u + chain->items[i].len;
    }
    packed = malloc(total ? total : 1);
    if (!packed) {
        SECITEM_FreeArray(chain, PR_TRUE);
        set_error("out of memory");
        return SECFailure;
    }
    for (i = 0; i < chain->len; i++) {
        uint32_t length = chain->items[i].len;
        packed[offset++] = (unsigned char)(length >> 24);
        packed[offset++] = (unsigned char)(length >> 16);
        packed[offset++] = (unsigned char)(length >> 8);
        packed[offset++] = (unsigned char)length;
        memcpy(packed + offset, chain->items[i].data, length);
        offset += length;
    }
    rc = conn->verify_cb(conn->closure, packed, (uint32_t)offset);
    free(packed);
    SECITEM_FreeArray(chain, PR_TRUE);
    if (rc != 0) {
        set_error("certificate verify failed");
        return SECFailure;
    }
    return SECSuccess;
}

static int apply_profile(PRFileDesc *fd, const necko_tls_profile *profile) {
    const PRUint16 *implemented;
    PRUint16 implemented_count;
    PRUint16 i;
    SSLVersionRange range;
    /* SSL_SetNextProtoNego treats the first protocol as the NPN fallback and
       moves it to the end of the wire ALPN list, so put http/1.1 first to
       emit ["h2", "http/1.1"] on the wire. */
    unsigned char alpn_h2[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1', 2, 'h', '2'};
    unsigned char alpn_h1[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    uint32_t n;

    /* Safe renegotiation is not required, as in Firefox 159
       (security.ssl.require_safe_negotiation is false): a server without
       RFC 5746 still connects, it just cannot renegotiate. */
    if (SSL_OptionSet(fd, SSL_SECURITY, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_NO_CACHE, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_SSL2, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_SESSION_TICKETS, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_DEFLATE, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_FALLBACK_SCSV, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_REQUIRES_XTN) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_EXTENDED_MASTER_SECRET, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_HELLO_DOWNGRADE_CHECK, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_REQUIRE_SAFE_NEGOTIATION, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_ALPN, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_0RTT_DATA, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_POST_HANDSHAKE_AUTH, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_DELEGATED_CREDENTIALS, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_OCSP_STAPLING, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(fd, SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
                      profile->signed_cert_timestamps ? PR_TRUE : PR_FALSE) != SECSuccess) {
        set_nss_error("ssl options");
        return -1;
    }

    range.min = SSL_LIBRARY_VERSION_TLS_1_2;
    range.max = SSL_LIBRARY_VERSION_TLS_1_3;
    if (SSL_VersionRangeSet(fd, &range) != SECSuccess) {
        set_nss_error("version range");
        return -1;
    }
    implemented = SSL_GetImplementedCiphers();
    implemented_count = SSL_GetNumImplementedCiphers();
    for (i = 0; i < implemented_count; i++) {
        if (SSL_CipherPrefSet(fd, implemented[i], PR_FALSE) != SECSuccess) {
            set_nss_error("cipher disable");
            return -1;
        }
    }
    for (n = 0; n < profile->cipher_count; n++) {
        if (SSL_CipherPrefSet(fd, profile->ciphers[n], PR_TRUE) != SECSuccess) {
            set_nss_error("cipher enable");
            return -1;
        }
    }
    if (profile->cipher_count > 0) {
        if (!SSL_GetExperimentalAPI("SSL_CipherSuiteOrderSet") ||
            SSL_CipherSuiteOrderSet(fd, profile->ciphers, (PRUint16)profile->cipher_count) !=
                SECSuccess) {
            set_nss_error("cipher order");
            return -1;
        }
    }
    {
        SSLNamedGroup *groups = NULL;
        if (profile->group_count) {
            groups = calloc(profile->group_count, sizeof(*groups));
            if (!groups) {
                set_error("out of memory");
                return -1;
            }
            for (n = 0; n < profile->group_count; n++) {
                groups[n] = (SSLNamedGroup)profile->groups[n];
            }
        }
        if (SSL_NamedGroupConfig(fd, groups, profile->group_count) != SECSuccess) {
            free(groups);
            set_nss_error("groups");
            return -1;
        }
        free(groups);
    }
    if (SSL_SendAdditionalKeyShares(fd, profile->additional_key_shares) != SECSuccess) {
        set_nss_error("key shares");
        return -1;
    }
    if (profile->signature_count > 0) {
        SSLSignatureScheme *schemes = calloc(profile->signature_count, sizeof(*schemes));
        if (!schemes) {
            set_error("out of memory");
            return -1;
        }
        for (n = 0; n < profile->signature_count; n++) {
            schemes[n] = (SSLSignatureScheme)profile->signatures[n];
        }
        if (SSL_SignatureSchemePrefSet(fd, schemes, profile->signature_count) != SECSuccess) {
            free(schemes);
            set_nss_error("signatures");
            return -1;
        }
        free(schemes);
    }
    if (profile->grease_ech) {
        uint32_t size = profile->grease_ech_size ? profile->grease_ech_size : 100;
        if (size > 255) {
            size = 255;
        }
        if (!SSL_GetExperimentalAPI("SSL_EnableTls13GreaseEch") ||
            SSL_EnableTls13GreaseEch(fd, PR_TRUE) != SECSuccess) {
            set_nss_error("grease ech");
            return -1;
        }
        if (SSL_SetTls13GreaseEchSize(fd, (PRUint8)size) != SECSuccess) {
            set_nss_error("grease ech size");
            return -1;
        }
    } else if (SSL_GetExperimentalAPI("SSL_EnableTls13GreaseEch")) {
        SSL_EnableTls13GreaseEch(fd, PR_FALSE);
    }
    if (profile->compression_count > 8) {
        set_error("too many compression algorithms");
        return -1;
    }
    for (n = 0; n < profile->compression_count; n++) {
        SSLCertificateCompressionAlgorithm algorithm;
        g_decode_ids[n] = profile->compression[n];
        memset(&algorithm, 0, sizeof(algorithm));
        algorithm.id = profile->compression[n];
        algorithm.name = profile->compression[n] == 1
                             ? "zlib"
                             : (profile->compression[n] == 2
                                    ? "brotli"
                                    : (profile->compression[n] == 3 ? "zstd" : "profile"));
        algorithm.encode = encode_unused;
        algorithm.decode = k_decoders[n];
        if (SSL_SetCertificateCompressionAlgorithm(fd, algorithm) != SECSuccess) {
            set_nss_error("compression");
            return -1;
        }
    }
    if (SSL_SetNextProtoNego(fd, g_current && g_current->http2 ? alpn_h2 : alpn_h1,
                             g_current && g_current->http2 ? sizeof(alpn_h2) : sizeof(alpn_h1)) !=
        SECSuccess) {
        set_nss_error("alpn");
        return -1;
    }
    return 0;
}

static int finish_open(necko_conn *conn, const char *host, const char *peer_id) {
    SSLNextProtoState state;
    unsigned char alpn[sizeof(conn->alpn)];
    unsigned int alpn_len = 0;
    SSLChannelInfo info;
    if (SSL_AuthCertificateHook(conn->ssl, auth_hook, conn) != SECSuccess) {
        set_nss_error("auth hook");
        return -1;
    }
    if (host && SSL_SetURL(conn->ssl, host) != SECSuccess) {
        set_nss_error("sni");
        return -1;
    }
    if (peer_id && peer_id[0] && SSL_SetSockPeerID(conn->ssl, peer_id) != SECSuccess) {
        set_nss_error("peer id");
        return -1;
    }
    g_current = conn;
    if (SSL_ForceHandshake(conn->ssl) != SECSuccess) {
        int blocked = PR_GetError() == PR_WOULD_BLOCK_ERROR;
        if (!blocked) {
            set_nss_error("handshake");
        }
        g_current = NULL;
        return blocked ? 1 : -1;
    }
    if (SSL_GetNextProto(conn->ssl, &state, alpn, &alpn_len, sizeof(alpn) - 1) == SECSuccess &&
        alpn_len < sizeof(conn->alpn)) {
        memcpy(conn->alpn, alpn, alpn_len);
        conn->alpn[alpn_len] = '\0';
    }
    memset(&info, 0, sizeof(info));
    if (SSL_GetChannelInfo(conn->ssl, &info, sizeof(info)) == SECSuccess) {
        conn->cipher = info.cipherSuite;
        conn->version = info.protocolVersion;
        conn->resumed = info.resumed ? 1 : 0;
    }
    g_current = NULL;
    return 0;
}

static int open_common(const char *host, const char *peer_id, int http2,
                       const necko_tls_profile *profile, necko_read_fn read_cb,
                       necko_write_fn write_cb, necko_idle_fn idle_cb, necko_verify_fn verify_cb,
                       necko_decode_fn decode_cb, void *closure, const char *certificates,
                       uint32_t certificates_len, const char *key, uint32_t key_len,
                       necko_conn **out) {
    necko_conn *conn;
    PRFileDesc *lower;
    (void)key;
    (void)certificates;
    if (certificates_len || (key && key_len)) {
        set_error("client certificate is not wired");
        return -1;
    }
    if (!read_cb || !write_cb || !out) {
        set_error("missing callback");
        return -1;
    }
    if (ensure_nss() != 0) {
        return -1;
    }
    if (g_identity == PR_INVALID_IO_LAYER) {
        g_identity = PR_GetUniqueIdentity("necko_tls");
    }
    conn = calloc(1, sizeof(*conn));
    if (!conn) {
        set_error("out of memory");
        return -1;
    }
    conn->read_cb = read_cb;
    conn->write_cb = write_cb;
    conn->idle_cb = idle_cb;
    conn->verify_cb = verify_cb;
    conn->decode_cb = decode_cb;
    conn->closure = closure;
    conn->http2 = http2 ? 1 : 0;
    lower = PR_CreateIOLayerStub(g_identity, &k_methods);
    if (!lower) {
        free(conn);
        set_nss_error("io layer");
        return -1;
    }
    lower->secret = (PRFilePrivate *)conn;
    conn->lower = lower;
    conn->ssl = SSL_ImportFD(NULL, lower);
    if (!conn->ssl) {
        PR_Close(lower);
        free(conn);
        set_nss_error("SSL_ImportFD");
        return -1;
    }
    if (SSL_ResetHandshake(conn->ssl, PR_FALSE) != SECSuccess ||
        SSL_OptionSet(conn->ssl, SSL_SECURITY, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(conn->ssl, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE) != SECSuccess ||
        SSL_OptionSet(conn->ssl, SSL_ENABLE_SESSION_TICKETS, PR_TRUE) != SECSuccess) {
        set_nss_error("socket setup");
        PR_Close(conn->ssl);
        free(conn);
        return -1;
    }
    g_current = conn;
    if (profile && apply_profile(conn->ssl, profile) != 0) {
        g_current = NULL;
        PR_Close(conn->ssl);
        free(conn);
        return -1;
    }
    if (!profile) {
        unsigned char alpn_h2[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1', 2, 'h', '2'};
        unsigned char alpn_h1[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
        SSL_OptionSet(conn->ssl, SSL_ENABLE_ALPN, PR_TRUE);
        SSL_SetNextProtoNego(conn->ssl, http2 ? alpn_h2 : alpn_h1,
                             http2 ? sizeof(alpn_h2) : sizeof(alpn_h1));
    }
    {
        int rc = finish_open(conn, host, peer_id);
        if (rc != 0) {
            PR_Close(conn->ssl);
            free(conn);
            return rc;
        }
    }
    *out = conn;
    return 0;
}

NECKO_EXPORT const char *necko_tls_nss_version(void) {
    const char *version;
    if (ensure_nss() != 0) {
        return "";
    }
    version = NSS_GetVersion();
    if (version && strncmp(version, "3.132", 5) == 0 &&
        (version[5] == '\0' || version[5] == ' ' || version[5] == '.')) {
        return "3.132";
    }
    return version ? version : "";
}

NECKO_EXPORT const char *necko_tls_nspr_version(void) {
    if (ensure_nss() != 0) {
        return "";
    }
    return PR_GetVersion();
}

NECKO_EXPORT const char *necko_tls_error(void) {
    return g_error[0] ? g_error : "nss";
}

NECKO_EXPORT int necko_tls_open(const char *host, const char *peer_id, int http2,
                                necko_read_fn read_cb, necko_write_fn write_cb,
                                necko_idle_fn idle_cb, necko_verify_fn verify_cb,
                                necko_decode_fn decode_cb, void *closure,
                                const char *certificates, uint32_t certificates_len,
                                const char *key, uint32_t key_len, necko_conn **out) {
    return open_common(host, peer_id, http2, NULL, read_cb, write_cb, idle_cb, verify_cb,
                       decode_cb, closure, certificates, certificates_len, key, key_len, out);
}

NECKO_EXPORT int necko_tls_open_profile(const char *host, const char *peer_id, int http2,
                                        const necko_tls_profile *profile, necko_read_fn read_cb,
                                        necko_write_fn write_cb, necko_idle_fn idle_cb,
                                        necko_verify_fn verify_cb, necko_decode_fn decode_cb,
                                        void *closure, const char *certificates,
                                        uint32_t certificates_len, const char *key,
                                        uint32_t key_len, necko_conn **out) {
    return open_common(host, peer_id, http2, profile, read_cb, write_cb, idle_cb, verify_cb,
                       decode_cb, closure, certificates, certificates_len, key, key_len, out);
}

NECKO_EXPORT int necko_tls_read(necko_conn *conn, uint8_t *buf, uint32_t size, uint32_t *length) {
    PRInt32 n;
    if (!conn || !length) {
        set_error("missing handle");
        return -1;
    }
    g_current = conn;
    n = PR_Read(conn->ssl, buf, (PRInt32)size);
    g_current = NULL;
    if (n >= 0) {
        *length = (uint32_t)n;
        return 0;
    }
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
        return 1;
    }
    set_nss_error("read");
    return -1;
}

NECKO_EXPORT int necko_tls_write(necko_conn *conn, const uint8_t *buf, uint32_t size) {
    uint32_t offset = 0;
    if (!conn) {
        set_error("missing handle");
        return -1;
    }
    if (size == 0) {
        return 0;
    }
    if (!buf) {
        set_error("missing buffer");
        return -1;
    }
    g_current = conn;
    while (offset < size) {
        uint32_t chunk = size - offset;
        PRInt32 n;
        if (chunk > 0x4000u) {
            chunk = 0x4000u;
        }
        n = PR_Write(conn->ssl, buf + offset, (PRInt32)chunk);
        if (n < 0) {
            int blocked = PR_GetError() == PR_WOULD_BLOCK_ERROR;
            g_current = NULL;
            if (blocked) {
                return 1;
            }
            set_nss_error("write");
            return -1;
        }
        if (n == 0) {
            g_current = NULL;
            set_error("short write");
            return -1;
        }
        offset += (uint32_t)n;
    }
    g_current = NULL;
    return 0;
}

NECKO_EXPORT int necko_tls_write_some(necko_conn *conn, const uint8_t *buf, uint32_t size,
                                      uint32_t *length) {
    PRInt32 n;
    uint32_t chunk;
    if (!conn || !length) {
        set_error("missing handle");
        return -1;
    }
    *length = 0;
    if (size == 0) {
        return 0;
    }
    if (!buf) {
        set_error("missing buffer");
        return -1;
    }
    chunk = size > 0x4000u ? 0x4000u : size;
    g_current = conn;
    n = PR_Write(conn->ssl, buf, (PRInt32)chunk);
    g_current = NULL;
    if (n >= 0) {
        *length = (uint32_t)n;
        return 0;
    }
    if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
        return 1;
    }
    set_nss_error("write_some");
    return -1;
}

NECKO_EXPORT void necko_tls_set_nonblocking(necko_conn *conn, necko_write_some_fn write_some) {
    if (!conn) {
        return;
    }
    conn->write_some_cb = write_some;
    conn->nonblocking = write_some ? 1 : 0;
}

NECKO_EXPORT void necko_tls_close(necko_conn *conn) {
    if (!conn) {
        return;
    }
    if (conn->ssl) {
        PR_Close(conn->ssl);
    }
    free(conn);
}

NECKO_EXPORT int necko_tls_pending(necko_conn *conn) {
    uint8_t byte = 0;
    PRInt32 rc;
    int32_t err;
    if (!conn || !conn->ssl) {
        set_error("missing handle");
        return -1;
    }
    g_current = conn;
    conn->in_pending = 1;
    rc = PR_Recv(conn->ssl, &byte, 1, PR_MSG_PEEK, PR_INTERVAL_NO_WAIT);
    conn->in_pending = 0;
    g_current = NULL;
    if (rc > 0) {
        return 1;
    }
    if (rc == 0) {
        set_error("NSS peer closed the connection");
        return -1;
    }
    err = PR_GetError();
    if (err == PR_WOULD_BLOCK_ERROR) {
        return 0;
    }
    set_nss_error("pending");
    return -1;
}

NECKO_EXPORT const char *necko_tls_alpn(necko_conn *conn) {
    return conn ? conn->alpn : "";
}

NECKO_EXPORT uint16_t necko_tls_version(necko_conn *conn) {
    return conn ? conn->version : 0;
}

NECKO_EXPORT uint16_t necko_tls_cipher(necko_conn *conn) {
    return conn ? conn->cipher : 0;
}

NECKO_EXPORT int necko_tls_resumed(necko_conn *conn) {
    return conn ? conn->resumed : 0;
}
