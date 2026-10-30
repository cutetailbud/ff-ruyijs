// One Necko/NSS session. Frames on stdin/stdout. Diagnostics stay on stderr.
// Redirects are not followed: redirectionLimit is 0 and the 3xx is the response.

const { NetUtil } = ChromeUtils.importESModule("resource://gre/modules/NetUtil.sys.mjs");
const { ctypes } = ChromeUtils.importESModule("resource://gre/modules/ctypes.sys.mjs");
const prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
const io = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
const tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
prefs.setBoolPref("network.process.enabled", false);
prefs.setBoolPref("network.http.network_access_on_socket_process.enabled", false);

const libc = ctypes.open("/usr/lib/libSystem.B.dylib");
const readFd = libc.declare(
  "read",
  ctypes.default_abi,
  ctypes.ssize_t,
  ctypes.int,
  ctypes.voidptr_t,
  ctypes.size_t
);
const writeFd = libc.declare(
  "write",
  ctypes.default_abi,
  ctypes.ssize_t,
  ctypes.int,
  ctypes.voidptr_t,
  ctypes.size_t
);

const TEXT_LIMIT = 65536;
const BODY_LIMIT = 8 * 1024 * 1024;
const HEADER_LIMIT = 256;

function readExact(n) {
  if (!n) return new Uint8Array(0);
  const array = ctypes.uint8_t.array(n)();
  let off = 0;
  while (off < n) {
    const got = readFd(0, array.addressOfElement(off), n - off);
    if (got <= 0) throw new Error("eof");
    off += got;
  }
  const out = new Uint8Array(n);
  for (let i = 0; i < n; i++) out[i] = array[i];
  return out;
}

function writeAll(bytes) {
  if (!bytes.length) return;
  const array = ctypes.uint8_t.array(bytes.length)();
  for (let i = 0; i < bytes.length; i++) array[i] = bytes[i];
  let off = 0;
  while (off < bytes.length) {
    const got = writeFd(1, array.addressOfElement(off), bytes.length - off);
    if (got <= 0) throw new Error("write");
    off += got;
  }
}

function readU32() {
  const b = readExact(4);
  return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0;
}

function writeU32(n) {
  writeAll(Uint8Array.of(n & 255, (n >>> 8) & 255, (n >>> 16) & 255, (n >>> 24) & 255));
}

function readText() {
  const n = readU32();
  if (n > TEXT_LIMIT) throw new Error("field too large");
  return new TextDecoder("utf-8", { fatal: true }).decode(readExact(n));
}

function writeField(value) {
  const bytes = new TextEncoder().encode(String(value));
  if (bytes.length > TEXT_LIMIT) throw new Error("field too large");
  writeU32(bytes.length);
  writeAll(bytes);
}

function writeError(message) {
  writeU32(0);
  const bytes = new TextEncoder().encode(String(message).slice(0, 300));
  writeU32(bytes.length);
  writeAll(bytes);
}

function configureProxy() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  const raw = env.get("RUYIJS_HTTP_PROXY") || "";
  prefs.setBoolPref("network.proxy.failover_direct", false);
  if (!raw) {
    prefs.setIntPref("network.proxy.type", 0);
    return;
  }
  const uri = io.newURI(raw);
  if ((uri.scheme != "http" && uri.scheme != "https") || !uri.host) {
    throw new Error("invalid proxy");
  }
  const port = uri.port > 0 ? uri.port : uri.scheme == "https" ? 443 : 80;
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setCharPref("network.proxy.http", uri.host);
  prefs.setIntPref("network.proxy.http_port", port);
  prefs.setCharPref("network.proxy.ssl", uri.host);
  prefs.setIntPref("network.proxy.ssl_port", port);
  prefs.setCharPref("network.proxy.no_proxies_on", "");
}

function versionCode(channel) {
  let proto = "";
  try {
    proto = channel.protocolVersion || "";
  } catch (e) {
    proto = "";
  }
  if (proto == "h2" || proto == "http/2") return 20;
  if (proto == "h3" || proto == "http/3") return 30;
  if (proto == "http/1.0") return 10;
  return 11;
}

function fetchOne(method, url, headers, body, budgetMs) {
  const uri = io.newURI(url);
  const channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).QueryInterface(Ci.nsIHttpChannel);
  if (channel instanceof Ci.nsIEncodedChannel) channel.applyConversion = false;
  channel.redirectionLimit = 20;
  channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;
  channel.requestMethod = method;
  for (const [name, value] of headers) channel.setRequestHeader(name, value, false);
  if (body.length) {
    const stream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"].createInstance(
      Ci.nsIArrayBufferInputStream
    );
    stream.setData(body.buffer, body.byteOffset, body.byteLength);
    channel.QueryInterface(Ci.nsIUploadChannel);
    channel.setUploadStream(stream, "", body.length);
    channel.requestMethod = method;
  }
  const chunks = [];
  let settled = false;
  let result = null;
  let failure = null;
  const listener = {
    onStartRequest() {},
    onDataAvailable(request, stream, offset, count) {
      const bin = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
      bin.setInputStream(stream);
      chunks.push(Uint8Array.from(bin.readByteArray(count)));
    },
    onStopRequest(request, status) {
      settled = true;
      if (!Components.isSuccessCode(status)) {
        failure = new Error("necko " + (status >>> 0).toString(16));
        return;
      }
      const pairs = [];
      channel.visitResponseHeaders({
        visitHeader(name, value) {
          pairs.push([name, value]);
        },
      });
      let total = 0;
      for (const part of chunks) total += part.length;
      if (total > BODY_LIMIT) {
        failure = new Error("response body exceeds 8 MiB");
        return;
      }
      const merged = new Uint8Array(total);
      let at = 0;
      for (const part of chunks) {
        merged.set(part, at);
        at += part.length;
      }
      result = {
        status: channel.responseStatus,
        headers: pairs,
        body: merged,
        version: versionCode(channel),
      };
    },
  };
  channel.asyncOpen(listener);
  const deadline = Date.now() + budgetMs;
  tm.spinEventLoopUntil("ruyijs-necko", () => settled || Date.now() >= deadline);
  if (!settled) {
    try {
      channel.cancel(Cr.NS_ERROR_NET_TIMEOUT);
    } catch (e) {}
    tm.spinEventLoopUntil("ruyijs-necko-cancel", () => settled || Date.now() >= deadline + 1000);
  }
  if (failure) throw failure;
  if (!result) throw new Error("necko timeout");
  return result;
}

configureProxy();
writeAll(Uint8Array.of(0x52, 0x44, 0x59, 0x31));
while (true) {
  let head;
  try {
    head = readExact(4);
  } catch (e) {
    break;
  }
  try {
    const methodLen = (head[0] | (head[1] << 8) | (head[2] << 16) | (head[3] << 24)) >>> 0;
    if (methodLen > TEXT_LIMIT) throw new Error("field too large");
    const method = new TextDecoder("utf-8", { fatal: true }).decode(readExact(methodLen));
    const url = readText();
    const count = readU32();
    if (count > HEADER_LIMIT) throw new Error("too many headers");
    const headers = [];
    for (let i = 0; i < count; i++) headers.push([readText(), readText()]);
    const bodyLen = readU32();
    if (bodyLen > BODY_LIMIT) throw new Error("request body exceeds 8 MiB");
    const body = readExact(bodyLen);
    const budgetMs = readU32();
    const response = fetchOne(method, url, headers, body, budgetMs > 0 ? budgetMs : 15000);
    writeU32(1);
    writeU32(response.status >>> 0);
    writeU32(response.headers.length);
    for (const [name, value] of response.headers) {
      writeField(name);
      writeField(value);
    }
    writeU32(response.body.length);
    writeAll(response.body);
    writeU32(response.version);
  } catch (error) {
    if (String(error && error.message) == "eof") break;
    try {
      writeError(error && error.message ? error.message : error);
    } catch (e) {
      break;
    }
  }
}
