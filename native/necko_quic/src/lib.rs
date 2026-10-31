//! JSON bridge in front of the Firefox 159 Neqo 0.32.0 crates.
//!
//! Python owns the UDP socket and certificate decision. This library does not
//! start a socket process and does not link neqo_glue. Profile numbers are
//! read from the JSON object the caller already built. ClientHello groups,
//! extra key shares, signature schemes, certificate compression, and ECH grease
//! are applied from that same object while the QUIC connection is still in
//! Init. Transport-parameter order and the Initial padding bonus are carried
//! on the session and applied when a datagram is sent.

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::{CStr, CString, c_void};
use std::net::SocketAddr;
use std::os::raw::{c_char, c_int};
use std::rc::Rc;
use std::time::{Duration, Instant};

use neqo_common::event::Provider;
use neqo_common::{Datagram, Header, Tos};
use neqo_http3::features::extended_connect::session::CloseReason as WebTransportClose;
use neqo_http3::webtransport::ClientSession as _;
use neqo_http3::{
    Http3Client, Http3ClientEvent, Http3Parameters, Http3State, Priority, WebTransportEvent,
};
use neqo_transport::streams::SendGroupId;
use neqo_transport::{
    AppError, CloseReason, Connection, ConnectionParameters, Output, RandomConnectionIdGenerator,
    StreamId, StreamType, Version, set_client_hello_adjustments,
};
use nss::AuthenticationStatus;
use nss::cert::CertificateCompressor;
use serde_json::{Value, json};

const VERSION_OK: &str = "neqo-0.32.0/nss-3.131";
const COMMAND_LIMIT: usize = 0x200001;
const CANCEL_DEFAULT: AppError = 0x10c;

type DecoderFn = unsafe extern "C" fn(u16, *const u8, usize, *mut u8, usize) -> c_int;

struct Session {
    client: Http3Client,
    fd: Option<i32>,
    local: SocketAddr,
    remote: SocketAddr,
    profile: String,
    tp_style: u8,
    mtu_bonus: usize,
}

thread_local! {
    static SESSIONS: RefCell<HashMap<u64, Session>> = RefCell::new(HashMap::new());
    static NEXT_HANDLE: RefCell<u64> = const { RefCell::new(1) };
    static LAST_JSON: RefCell<CString> = RefCell::new(CString::new("{}").unwrap());
    static DECODER: RefCell<Option<DecoderFn>> = const { RefCell::new(None) };
    static NSS_OK: RefCell<bool> = const { RefCell::new(false) };
}

unsafe extern "C" {
    fn NSS_GetVersion() -> *const c_char;
}

fn nss_text() -> String {
    unsafe {
        if NSS_GetVersion().is_null() {
            return String::new();
        }
        CStr::from_ptr(NSS_GetVersion()).to_string_lossy().into_owned()
    }
}

fn version_label() -> &'static str {
    let ok = NSS_OK.with(|slot| *slot.borrow());
    if ok {
        VERSION_OK
    } else if nss_text().contains("3.131") {
        VERSION_OK
    } else {
        "neqo-0.32.0/nss-mismatch"
    }
}

fn store_reply(text: String) -> *const c_char {
    let owned = CString::new(text).unwrap_or_else(|_| CString::new("{\"error\":\"encode\"}").unwrap());
    LAST_JSON.with(|slot| {
        *slot.borrow_mut() = owned;
        slot.borrow().as_ptr()
    })
}

fn reply(value: Value) -> *const c_char {
    let text = serde_json::to_string(&value).unwrap_or_else(|_| "{\"error\":\"encode\"}".to_string());
    store_reply(text)
}

fn fail(message: &str) -> *const c_char {
    reply(json!({"error": message}))
}

fn parse_addr(text: &str) -> Result<SocketAddr, ()> {
    text.parse::<SocketAddr>().map_err(|_| ())
}

fn json_u64(value: &Value, key: &str) -> Option<u64> {
    value.get(key).and_then(Value::as_u64)
}

fn json_bool(value: &Value, key: &str) -> Option<bool> {
    value.get(key).and_then(Value::as_bool)
}

fn apply_profile(profile: &Value) -> (ConnectionParameters, bool) {
    let mut params = ConnectionParameters::default();
    if let Some(size) = json_u64(profile, "datagram_size") {
        params = params.datagram_size(size);
    }
    if let Some(enabled) = json_bool(profile, "grease_quic_bit") {
        params = params.grease(enabled);
    }
    if let Some(enabled) = json_bool(profile, "disable_migration") {
        params = params.disable_migration(enabled);
    }
    if let Some(enabled) = json_bool(profile, "sni_slicing") {
        params = params.sni_slicing(enabled);
    }
    if let Some(enabled) = json_bool(profile, "reliable_stream_reset") {
        params = params.reliable_stream_reset(enabled);
    }
    if let Some(streams) = json_u64(profile, "max_streams") {
        params = params
            .max_streams(StreamType::BiDi, streams)
            .max_streams(StreamType::UniDi, streams);
    }
    if json_bool(profile, "version2") == Some(true) {
        params = params.versions(
            Version::Version1,
            vec![Version::Version1, Version::Version2],
        );
    }
    let datagrams = json_bool(profile, "h3_datagram").unwrap_or(true);
    (params, datagrams)
}

#[cfg(target_os = "macos")]
const APPLE_COMPRESSION_ZLIB: u32 = 0x205;
#[cfg(target_os = "macos")]
const APPLE_COMPRESSION_BROTLI: u32 = 0xB02;

unsafe extern "C" {
    #[cfg(target_os = "macos")]
    fn compression_decode_buffer(
        dst: *mut u8,
        dst_size: usize,
        src: *const u8,
        src_size: usize,
        scratch: *mut c_void,
        algorithm: u32,
    ) -> usize;

    fn ZSTD_decompress(
        dst: *mut c_void,
        dst_capacity: usize,
        src: *const c_void,
        compressed_size: usize,
    ) -> usize;

    fn ZSTD_isError(code: usize) -> u32;
}

fn compression_rejected() -> nss::Res<()> {
    Err(nss::Error::CertificateDecoding)
}

#[cfg(target_os = "macos")]
fn decode_apple(algorithm: u32, input: &[u8], output: &mut [u8]) -> nss::Res<()> {
    if input.is_empty() || output.is_empty() {
        return compression_rejected();
    }
    let written = unsafe {
        compression_decode_buffer(
            output.as_mut_ptr(),
            output.len(),
            input.as_ptr(),
            input.len(),
            std::ptr::null_mut(),
            algorithm,
        )
    };
    if written == output.len() {
        Ok(())
    } else {
        compression_rejected()
    }
}

fn decode_zstd(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
    if input.is_empty() || output.is_empty() {
        return compression_rejected();
    }
    let written = unsafe {
        ZSTD_decompress(
            output.as_mut_ptr().cast(),
            output.len(),
            input.as_ptr().cast(),
            input.len(),
        )
    };
    let failed = unsafe { ZSTD_isError(written) } != 0;
    if failed || written != output.len() {
        compression_rejected()
    } else {
        Ok(())
    }
}

#[cfg(target_os = "linux")]
unsafe extern "C" {
    fn uncompress(dst: *mut u8, size: *mut libc::c_ulong,
                  src: *const u8, src_size: libc::c_ulong) -> c_int;
    fn BrotliDecoderDecompress(src_size: usize, src: *const u8,
                               dst_size: *mut usize, dst: *mut u8) -> c_int;
}

fn decode_zlib(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
    #[cfg(target_os = "macos")]
    { decode_apple(APPLE_COMPRESSION_ZLIB, input, output) }
    #[cfg(target_os = "linux")]
    {
        let mut size = output.len() as libc::c_ulong;
        let status = unsafe { uncompress(output.as_mut_ptr(), &mut size,
                                        input.as_ptr(), input.len() as libc::c_ulong) };
        if status == 0 && size as usize == output.len() { Ok(()) }
        else { compression_rejected() }
    }
}

fn decode_brotli(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
    #[cfg(target_os = "macos")]
    { decode_apple(APPLE_COMPRESSION_BROTLI, input, output) }
    #[cfg(target_os = "linux")]
    {
        let mut size = output.len();
        let status = unsafe { BrotliDecoderDecompress(input.len(), input.as_ptr(),
                                                      &mut size, output.as_mut_ptr()) };
        if status == 1 && size == output.len() { Ok(()) }
        else { compression_rejected() }
    }
}

struct ZlibCertDecoder;
struct BrotliCertDecoder;
struct ZstdCertDecoder;

impl CertificateCompressor for ZlibCertDecoder {
    const ID: u16 = 1;
    const NAME: &CStr = c"zlib";

    fn decode(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
        decode_zlib(input, output)
    }
}

impl CertificateCompressor for BrotliCertDecoder {
    const ID: u16 = 2;
    const NAME: &CStr = c"brotli";

    fn decode(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
        decode_brotli(input, output)
    }
}

impl CertificateCompressor for ZstdCertDecoder {
    const ID: u16 = 3;
    const NAME: &CStr = c"zstd";

    fn decode(input: &[u8], output: &mut [u8]) -> nss::Res<()> {
        decode_zstd(input, output)
    }
}

fn u16_list(profile: &Value, key: &str) -> Result<Vec<u16>, String> {
    let Some(items) = profile.get(key).and_then(Value::as_array) else {
        return Ok(Vec::new());
    };
    let mut values = Vec::with_capacity(items.len());
    for item in items {
        let number = item.as_u64().ok_or_else(|| format!("HTTP/3 profile {key} is invalid"))?;
        let value = u16::try_from(number).map_err(|_| format!("HTTP/3 profile {key} is invalid"))?;
        values.push(value);
    }
    Ok(values)
}

fn apply_client_hello(conn: &mut Connection, profile: &Value) -> Result<(u8, usize), String> {
    let groups = u16_list(profile, "groups")?;
    if !groups.is_empty() {
        conn.set_groups(&groups)
            .map_err(|_| "HTTP/3 profile groups were rejected".to_string())?;
    }
    if let Some(shares) = json_u64(profile, "additional_key_shares") {
        let count = usize::try_from(shares).map_err(|_| "HTTP/3 profile key shares were rejected".to_string())?;
        conn.send_additional_key_shares(count)
            .map_err(|_| "HTTP/3 profile key shares were rejected".to_string())?;
    }
    let signatures = u16_list(profile, "signatures")?;
    if !signatures.is_empty() {
        conn.set_signature_schemes(&signatures)
            .map_err(|_| "HTTP/3 profile signatures were rejected".to_string())?;
    }
    if let Some(items) = profile.get("compression").and_then(Value::as_array) {
        for item in items {
            let ident = item.as_u64().ok_or("HTTP/3 profile compression is invalid")?;
            let registered = match ident {
                1 => conn.set_certificate_compression::<ZlibCertDecoder>(),
                2 => conn.set_certificate_compression::<BrotliCertDecoder>(),
                3 => conn.set_certificate_compression::<ZstdCertDecoder>(),
                _ => return Err("HTTP/3 profile compression is invalid".to_string()),
            };
            registered.map_err(|_| "HTTP/3 profile compression was rejected".to_string())?;
        }
    }
    if json_bool(profile, "grease_ech") == Some(true) {
        conn.client_enable_ech(&[] as &[u8])
            .map_err(|_| "HTTP/3 profile ECH grease was rejected".to_string())?;
    }
    let style = u8::try_from(json_u64(profile, "tp_style").unwrap_or(0))
        .map_err(|_| "HTTP/3 profile transport style is invalid".to_string())?;
    if style > 3 {
        return Err("HTTP/3 profile transport style is invalid".to_string());
    }
    let mtu_bonus = usize::try_from(json_u64(profile, "mtu_bonus").unwrap_or(0))
        .map_err(|_| "HTTP/3 profile datagram pad is invalid".to_string())?;
    set_client_hello_adjustments(style, mtu_bonus);
    Ok((style, mtu_bonus))
}

fn stream_id(value: u64) -> StreamId {
    StreamId::new(value)
}

fn stream_number(stream: StreamId) -> u64 {
    stream.as_u64()
}

fn b64_encode(data: &[u8]) -> String {
    const TABLE: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::new();
    let mut index = 0;
    while index + 3 <= data.len() {
        let block = u32::from(data[index]) << 16 | u32::from(data[index + 1]) << 8 | u32::from(data[index + 2]);
        out.push(TABLE[((block >> 18) & 63) as usize] as char);
        out.push(TABLE[((block >> 12) & 63) as usize] as char);
        out.push(TABLE[((block >> 6) & 63) as usize] as char);
        out.push(TABLE[(block & 63) as usize] as char);
        index += 3;
    }
    let rest = data.len() - index;
    if rest == 1 {
        let block = u32::from(data[index]) << 16;
        out.push(TABLE[((block >> 18) & 63) as usize] as char);
        out.push(TABLE[((block >> 12) & 63) as usize] as char);
        out.push('=');
        out.push('=');
    } else if rest == 2 {
        let block = u32::from(data[index]) << 16 | u32::from(data[index + 1]) << 8;
        out.push(TABLE[((block >> 18) & 63) as usize] as char);
        out.push(TABLE[((block >> 12) & 63) as usize] as char);
        out.push(TABLE[((block >> 6) & 63) as usize] as char);
        out.push('=');
    }
    out
}

fn b64_value(ch: u8) -> Option<u8> {
    match ch {
        b'A'..=b'Z' => Some(ch - b'A'),
        b'a'..=b'z' => Some(ch - b'a' + 26),
        b'0'..=b'9' => Some(ch - b'0' + 52),
        b'+' => Some(62),
        b'/' => Some(63),
        _ => None,
    }
}

fn b64_decode(text: &str) -> Result<Vec<u8>, ()> {
    let bytes = text.as_bytes();
    if bytes.len() % 4 != 0 {
        return Err(());
    }
    let mut out = Vec::new();
    for chunk in bytes.chunks(4) {
        let pad = chunk.iter().filter(|byte| **byte == b'=').count();
        if pad > 2 || chunk[..4 - pad].contains(&b'=') {
            return Err(());
        }
        let mut block = 0u32;
        for (shift, byte) in chunk.iter().take(4 - pad).enumerate() {
            let value = b64_value(*byte).ok_or(())?;
            block |= u32::from(value) << (18 - 6 * shift);
        }
        out.push((block >> 16) as u8);
        if pad < 2 {
            out.push((block >> 8) as u8);
        }
        if pad == 0 {
            out.push(block as u8);
        }
    }
    Ok(out)
}

fn certificates(client: &Http3Client) -> Vec<Value> {
    let Some(info) = client.peer_certificate() else {
        return Vec::new();
    };
    info.iter().map(|der| Value::String(b64_encode(der))).collect()
}

fn close_fields(reason: &CloseReason) -> (bool, String) {
    match reason {
        CloseReason::Transport(error) => (true, error.to_string()),
        CloseReason::Application(code) => (false, code.to_string()),
    }
}

fn push_event(events: &mut Vec<Value>, event: Http3ClientEvent) {
    match event {
        Http3ClientEvent::AuthenticationNeeded => {}
        Http3ClientEvent::EchFallbackAuthenticationNeeded { .. } => {
            events.push(json!({"type": "ech_retry"}));
        }
        Http3ClientEvent::ResumptionToken(_) => {
            events.push(json!({"type": "token", "data": "", "expires_in": 0}));
        }
        Http3ClientEvent::StateChange(Http3State::Connected) => {
            events.push(json!({"type": "state", "state": "connected"}));
        }
        Http3ClientEvent::StateChange(Http3State::GoingAway(stream)) => {
            events.push(json!({"type": "state", "state": "going_away", "last": stream_number(stream)}));
        }
        Http3ClientEvent::StateChange(Http3State::Closing(reason) | Http3State::Closed(reason)) => {
            let (protocol_error, text) = close_fields(&reason);
            events.push(json!({
                "type": "state",
                "state": "closed",
                "protocol_error": protocol_error,
                "reason": text,
            }));
        }
        Http3ClientEvent::HeaderReady { stream_id, headers, interim, fin } => {
            let fields: Vec<Value> = headers
                .iter()
                .map(|header| json!([header.name().to_string(), b64_encode(header.value())]))
                .collect();
            events.push(json!({
                "type": "headers",
                "stream": stream_number(stream_id),
                "headers": fields,
                "interim": interim,
                "fin": fin,
            }));
        }
        Http3ClientEvent::DataReadable { stream_id } => {
            events.push(json!({"type": "readable", "stream": stream_number(stream_id)}));
        }
        Http3ClientEvent::Reset { stream_id, error, .. } => {
            events.push(json!({"type": "reset", "stream": stream_number(stream_id), "error": error}));
        }
        Http3ClientEvent::StopSending { stream_id, error } => {
            events.push(json!({"type": "stop_sending", "stream": stream_number(stream_id), "error": error}));
        }
        Http3ClientEvent::WebTransport(event) => push_webtransport(events, event),
        _ => {}
    }
}

fn push_webtransport(events: &mut Vec<Value>, event: WebTransportEvent) {
    match event {
        WebTransportEvent::Negotiated(available) => {
            events.push(json!({"type": "wt_negotiated", "available": available}));
        }
        WebTransportEvent::NewSession { stream_id, status, .. } => {
            events.push(json!({
                "type": "wt_session",
                "stream": stream_number(stream_id),
                "session": stream_number(stream_id),
                "status": status,
            }));
        }
        WebTransportEvent::Datagram { session_id, datagram } => {
            events.push(json!({
                "type": "wt_datagram",
                "session": stream_number(session_id),
                "data": b64_encode(datagram.as_ref()),
            }));
        }
        WebTransportEvent::NewStream { stream_id, session_id } => {
            let stream = stream_number(stream_id);
            events.push(json!({
                "type": "wt_stream",
                "stream": stream,
                "session": stream_number(session_id),
                "bidirectional": stream & 2 == 0,
            }));
        }
        WebTransportEvent::Draining { stream_id } => {
            events.push(json!({"type": "wt_draining", "stream": stream_number(stream_id)}));
        }
        WebTransportEvent::SessionClosed { stream_id, reason, .. } => {
            let (clean, code, text) = match reason {
                WebTransportClose::Clean { error, message } => (true, u64::from(error), message),
                WebTransportClose::Error(error) => (false, error, error.to_string()),
                WebTransportClose::Status(status) => (false, u64::from(status), status.to_string()),
            };
            events.push(json!({
                "type": "wt_closed",
                "session": stream_number(stream_id),
                "stream": stream_number(stream_id),
                "clean": clean,
                "code": code,
                "reason": text,
            }));
        }
    }
}

fn optional_u64(body: &Value, key: &str) -> Option<u64> {
    match body.get(key) {
        None | Some(Value::Null) => None,
        Some(value) => value.as_u64(),
    }
}

fn required_stream(body: &Value, key: &str) -> Result<StreamId, String> {
    let value = body
        .get(key)
        .and_then(Value::as_u64)
        .ok_or_else(|| format!("webtransport {key} is missing"))?;
    Ok(stream_id(value))
}

fn header_rows(body: &Value) -> Result<Vec<Header>, String> {
    let mut headers = Vec::new();
    let Some(rows) = body.get("headers").and_then(Value::as_array) else {
        return Ok(headers);
    };
    for row in rows {
        let pair = row.as_array().ok_or("webtransport header is not a pair")?;
        let name = pair.first().and_then(Value::as_str).unwrap_or("");
        let encoded = pair.get(1).and_then(Value::as_str).unwrap_or("");
        let value = b64_decode(encoded).map_err(|_| "webtransport header value is not base64".to_string())?;
        headers.push(Header::new(name, value));
    }
    Ok(headers)
}

fn command_webtransport(session: &mut Session, body: &Value) -> Result<Value, String> {
    let action = body.get("action").and_then(Value::as_str).unwrap_or("");
    let now = Instant::now();
    match action {
        "open" => {
            let authority = body.get("authority").and_then(Value::as_str).unwrap_or("");
            let path = body.get("path").and_then(Value::as_str).unwrap_or("/");
            let headers = header_rows(body)?;
            let stream = session
                .client
                .webtransport_create_session(now, ("https", authority, path), &headers)
                .map_err(|error| error.to_string())?;
            Ok(json!({"session": stream_number(stream)}))
        }
        "info" => {
            let id = required_stream(body, "session")?;
            let size = session
                .client
                .webtransport_max_datagram_size(id)
                .map_err(|error| error.to_string())?;
            let protocol = session
                .client
                .webtransport_session_protocol(id)
                .map_err(|error| error.to_string())?;
            let stats = session
                .client
                .webtransport_session_stats(id)
                .map_err(|error| error.to_string())?;
            Ok(json!({
                "maxDatagramSize": size,
                "protocol": protocol,
                "datagramsExpiredOutgoing": stats.datagrams_expired_outgoing,
            }))
        }
        "stream" => {
            let id = required_stream(body, "session")?;
            let bidirectional = body.get("bidirectional").and_then(Value::as_bool).unwrap_or(true);
            let kind = if bidirectional { StreamType::BiDi } else { StreamType::UniDi };
            let stream = if let Some(group) = optional_u64(body, "group") {
                session
                    .client
                    .webtransport_create_stream_with_send_group(id, kind, Some(SendGroupId::new(group)))
                    .map_err(|error| error.to_string())?
            } else {
                session
                    .client
                    .webtransport_create_stream(id, kind)
                    .map_err(|error| error.to_string())?
            };
            if let Some(order) = body.get("order").and_then(Value::as_i64) {
                session
                    .client
                    .webtransport_set_sendorder(stream, Some(order))
                    .map_err(|error| error.to_string())?;
            }
            Ok(json!({"stream": stream_number(stream)}))
        }
        "datagram" => {
            let id = required_stream(body, "session")?;
            let encoded = body.get("data").and_then(Value::as_str).unwrap_or("");
            let data = b64_decode(encoded).map_err(|_| "webtransport datagram is not base64".to_string())?;
            let queued = session
                .client
                .webtransport_send_datagram(id, &data, None::<u64>, now)
                .map_err(|error| error.to_string())?;
            Ok(json!({"queued": queued}))
        }
        "reset" => {
            let id = required_stream(body, "stream")?;
            let code = body.get("code").and_then(Value::as_u64).unwrap_or(0);
            session
                .client
                .stream_reset_send(id, code)
                .map_err(|error| error.to_string())?;
            Ok(json!({}))
        }
        "stop" => {
            let id = required_stream(body, "stream")?;
            let code = body.get("code").and_then(Value::as_u64).unwrap_or(0);
            session
                .client
                .stream_stop_sending(id, code)
                .map_err(|error| error.to_string())?;
            Ok(json!({}))
        }
        "group" => {
            let id = required_stream(body, "session")?;
            let group = session
                .client
                .webtransport_create_send_group(id)
                .map_err(|error| error.to_string())?;
            Ok(json!({"group": group.as_u64()}))
        }
        "order" => {
            let id = required_stream(body, "stream")?;
            let order = body.get("order").and_then(Value::as_i64);
            session
                .client
                .webtransport_set_sendorder(id, order)
                .map_err(|error| error.to_string())?;
            Ok(json!({}))
        }
        "set_group" => {
            let id = required_stream(body, "stream")?;
            if let Some(group) = optional_u64(body, "group") {
                session
                    .client
                    .webtransport_set_sendgroup(id, SendGroupId::new(group))
                    .map_err(|error| error.to_string())?;
            } else {
                session
                    .client
                    .webtransport_clear_sendgroup(id)
                    .map_err(|error| error.to_string())?;
            }
            Ok(json!({}))
        }
        "export" => {
            let id = required_stream(body, "session")?;
            let label = b64_decode(body.get("label").and_then(Value::as_str).unwrap_or(""))
                .map_err(|_| "webtransport export label is not base64".to_string())?;
            let context = b64_decode(body.get("context").and_then(Value::as_str).unwrap_or(""))
                .map_err(|_| "webtransport export context is not base64".to_string())?;
            let mut out = vec![0u8; 32];
            session
                .client
                .webtransport_export_keying_material(id, &label, &context, &mut out)
                .map_err(|error| error.to_string())?;
            Ok(json!({"data": b64_encode(&out)}))
        }
        "close" => {
            let id = required_stream(body, "session")?;
            let code = body.get("code").and_then(Value::as_u64).unwrap_or(0);
            let reason = body.get("reason").and_then(Value::as_str).unwrap_or("");
            let code = u32::try_from(code).map_err(|_| "webtransport close code does not fit".to_string())?;
            let stats = session
                .client
                .webtransport_close_session(id, code, reason, now)
                .map_err(|error| error.to_string())?;
            Ok(json!({"datagramsExpiredOutgoing": stats.datagrams_expired_outgoing}))
        }
        "stream_stats" => {
            let id = required_stream(body, "stream")?;
            let sending = body.get("sending").and_then(Value::as_bool).unwrap_or(true);
            if sending {
                let stats = session
                    .client
                    .webtransport_send_stream_stats(id)
                    .map_err(|error| error.to_string())?;
                Ok(json!({
                    "bytesWritten": stats.bytes_written(),
                    "bytesSent": stats.bytes_sent(),
                    "bytesAcknowledged": stats.bytes_acked(),
                }))
            } else {
                let stats = session
                    .client
                    .webtransport_recv_stream_stats(id)
                    .map_err(|error| error.to_string())?;
                Ok(json!({
                    "bytesReceived": stats.bytes_received(),
                    "bytesRead": stats.bytes_read(),
                }))
            }
        }
        _ => Err(format!("unknown webtransport action {action}")),
    }
}

fn take_events(session: &mut Session) -> Value {
    let mut events = Vec::new();
    let mut saw_auth = false;
    for _ in 0..64 {
        if !session.client.has_events() {
            break;
        }
        let Some(event) = session.client.next_event() else {
            break;
        };
        if matches!(event, Http3ClientEvent::AuthenticationNeeded) {
            saw_auth = true;
        }
        push_event(&mut events, event);
        if events.len() >= 32 {
            break;
        }
    }
    if saw_auth {
        let certs = certificates(&session.client);
        events.insert(0, json!({"type": "auth", "certificates": certs}));
    }
    let more = session.client.has_events();
    json!({"events": events, "more": more})
}

fn recv_payload(fd: i32) -> Result<Option<Vec<u8>>, String> {
    let mut buffer = vec![0u8; 65535];
    let size = unsafe { libc::recv(fd, buffer.as_mut_ptr().cast(), buffer.len(), 0) };
    if size < 0 {
        let error = std::io::Error::last_os_error();
        if error.raw_os_error() == Some(libc::EAGAIN) || error.raw_os_error() == Some(libc::EWOULDBLOCK) {
            return Ok(None);
        }
        return Err(error.to_string());
    }
    if size == 0 {
        return Ok(None);
    }
    buffer.truncate(size as usize);
    Ok(Some(buffer))
}

fn send_payload(fd: i32, data: &[u8]) -> Result<(), String> {
    let mut sent = 0;
    while sent < data.len() {
        let size = unsafe {
            libc::send(fd, data[sent..].as_ptr().cast(), data.len() - sent, 0)
        };
        if size < 0 {
            let error = std::io::Error::last_os_error();
            if error.raw_os_error() == Some(libc::EAGAIN) || error.raw_os_error() == Some(libc::EWOULDBLOCK) {
                return Err("UDP send would block".to_string());
            }
            return Err(error.to_string());
        }
        if size == 0 {
            return Err("UDP send returned no bytes".to_string());
        }
        sent += size as usize;
    }
    Ok(())
}

fn pump(session: &mut Session) -> Result<f64, String> {
    set_client_hello_adjustments(session.tp_style, session.mtu_bonus);
    let fd = session.fd.ok_or("HTTP/3 socket is not attached")?;
    let now = Instant::now();
    loop {
        match recv_payload(fd)? {
            Some(payload) => {
                let datagram = Datagram::new(session.remote, session.local, Tos::from(0u8), payload);
                session.client.process_input(datagram, now);
            }
            None => break,
        }
    }
    let mut delay = 0.05_f64;
    for _ in 0..64 {
        match session.client.process_output(now) {
            Output::Datagram(datagram) => send_payload(fd, &datagram)?,
            Output::Callback(duration) => {
                delay = duration.as_secs_f64();
                break;
            }
            Output::None => {
                delay = 0.05;
                break;
            }
        }
    }
    Ok(delay)
}

fn command_new(body: &Value) -> Result<Value, String> {
    let host = body.get("host").and_then(Value::as_str).unwrap_or("");
    if host.is_empty() {
        return Err("HTTP/3 new requires a host".to_string());
    }
    let local = parse_addr(body.get("local").and_then(Value::as_str).unwrap_or(""))
        .map_err(|_| "HTTP/3 local address is invalid".to_string())?;
    let remote = parse_addr(body.get("remote").and_then(Value::as_str).unwrap_or(""))
        .map_err(|_| "HTTP/3 remote address is invalid".to_string())?;
    let profile = body.get("profile").cloned().unwrap_or_else(|| json!({}));
    let (params, h3_datagram) = apply_profile(&profile);
    let webtransport = body.get("webtransport").and_then(Value::as_bool).unwrap_or(false);
    let http3 = Http3Parameters::default()
        .connection_parameters(params)
        .http3_datagram(h3_datagram)
        .webtransport(webtransport);
    let mut connection = Connection::new_client(
        host,
        &["h3"],
        Rc::new(RefCell::new(RandomConnectionIdGenerator::new(8))),
        local,
        remote,
        http3.get_connection_parameters().clone(),
        Instant::now(),
    )
    .map_err(|error| error.to_string())?;
    let (tp_style, mtu_bonus) = apply_client_hello(&mut connection, &profile)?;
    let client = Http3Client::new_with_conn(connection, http3);
    let handle = NEXT_HANDLE.with(|slot| {
        let mut next = slot.borrow_mut();
        let handle = *next;
        *next = next.saturating_add(1);
        handle
    });
    SESSIONS.with(|slot| {
        slot.borrow_mut().insert(
            handle,
            Session {
                client,
                fd: None,
                local,
                remote,
                profile: profile.to_string(),
                tp_style,
                mtu_bonus,
            },
        );
    });
    Ok(json!({"handle": handle}))
}

fn with_session<T>(handle: u64, body: impl FnOnce(&mut Session) -> Result<T, String>) -> Result<T, String> {
    SESSIONS.with(|slot| {
        let mut sessions = slot.borrow_mut();
        let session = sessions.get_mut(&handle).ok_or("unknown HTTP/3 handle")?;
        body(session)
    })
}

fn dispatch(handle: u64, body: &Value) -> Result<Value, String> {
    let op = body.get("op").and_then(Value::as_str).unwrap_or("");
    match op {
        "new" => command_new(body),
        "attach" => {
            let fd = body.get("socket").and_then(Value::as_i64).ok_or("attach requires a socket")?;
            with_session(handle, |session| {
                let fd = i32::try_from(fd).map_err(|_| "attach socket does not fit".to_string())?;
                let flags = unsafe { libc::fcntl(fd, libc::F_GETFL) };
                if flags >= 0 {
                    unsafe { libc::fcntl(fd, libc::F_SETFL, flags | libc::O_NONBLOCK); }
                }
                session.fd = Some(fd);
                let _ = &session.profile;
                Ok(json!({}))
            })
        }
        "pump" => with_session(handle, |session| Ok(json!({"delay": pump(session)?}))),
        "events" => with_session(handle, |session| Ok(take_events(session))),
        "auth" => {
            let accepted = body.get("accepted").and_then(Value::as_bool).unwrap_or(false);
            with_session(handle, |session| {
                let status = if accepted {
                    AuthenticationStatus::Ok
                } else {
                    AuthenticationStatus::CertUntrusted
                };
                session.client.authenticated(status, Instant::now());
                Ok(json!({}))
            })
        }
        "info" => with_session(handle, |session| {
            Ok(json!({"tls": null, "certificates": certificates(&session.client)}))
        }),
        "fetch" => with_session(handle, |session| {
            let method = body.get("method").and_then(Value::as_str).unwrap_or("GET");
            let authority = body.get("authority").and_then(Value::as_str).unwrap_or("");
            let path = body.get("path").and_then(Value::as_str).unwrap_or("/");
            let urgency = body.get("urgency").and_then(Value::as_u64).unwrap_or(4).min(7) as u8;
            let incremental = body.get("incremental").and_then(Value::as_bool).unwrap_or(false);
            let mut headers = Vec::new();
            if let Some(rows) = body.get("headers").and_then(Value::as_array) {
                for row in rows {
                    let pair = row.as_array().ok_or("fetch header is not a pair")?;
                    let name = pair.first().and_then(Value::as_str).unwrap_or("");
                    let encoded = pair.get(1).and_then(Value::as_str).unwrap_or("");
                    let value = b64_decode(encoded).map_err(|_| "fetch header value is not base64".to_string())?;
                    headers.push(Header::new(name, value));
                }
            }
            let stream = session
                .client
                .fetch(
                    Instant::now(),
                    method,
                    ("https", authority, path),
                    &headers,
                    Priority::new(urgency, incremental),
                )
                .map_err(|error| error.to_string())?;
            Ok(json!({"stream": stream_number(stream)}))
        }),
        "read" => with_session(handle, |session| {
            let stream = stream_id(body.get("stream").and_then(Value::as_u64).ok_or("read requires a stream")?);
            let size = body.get("size").and_then(Value::as_u64).unwrap_or(65536).clamp(1, 65536) as usize;
            let mut buffer = vec![0u8; size];
            let (count, fin) = session
                .client
                .read_data(Instant::now(), stream, &mut buffer)
                .map_err(|error| error.to_string())?;
            Ok(json!({"data": b64_encode(&buffer[..count]), "fin": fin}))
        }),
        "write" => with_session(handle, |session| {
            let stream = stream_id(body.get("stream").and_then(Value::as_u64).ok_or("write requires a stream")?);
            let encoded = body.get("data").and_then(Value::as_str).unwrap_or("");
            let data = b64_decode(encoded).map_err(|_| "write data is not base64".to_string())?;
            let count = session
                .client
                .send_data(stream, &data, Instant::now())
                .map_err(|error| error.to_string())?;
            Ok(json!({"count": count}))
        }),
        "finish" => with_session(handle, |session| {
            let stream = stream_id(body.get("stream").and_then(Value::as_u64).ok_or("finish requires a stream")?);
            session
                .client
                .stream_close_send(stream, Instant::now())
                .map_err(|error| error.to_string())?;
            Ok(json!({}))
        }),
        "cancel" => with_session(handle, |session| {
            let stream = stream_id(body.get("stream").and_then(Value::as_u64).ok_or("cancel requires a stream")?);
            let code = body.get("error").and_then(Value::as_u64).unwrap_or(CANCEL_DEFAULT);
            session
                .client
                .cancel_fetch(stream, code)
                .map_err(|error| error.to_string())?;
            Ok(json!({}))
        }),
        "close" => {
            SESSIONS.with(|slot| {
                slot.borrow_mut().remove(&handle);
            });
            Ok(json!({}))
        }
        "webtransport" => with_session(handle, |session| command_webtransport(session, body)),
        _ => Err(format!("unknown HTTP/3 command {op}")),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn necko_quic_init() -> c_int {
    // Same NoDB startup the cleartext/TLS library uses. If that library
    // already called NSS_NoDB_Init, nss::init records the external state.
    if nss::init().is_err() {
        NSS_OK.with(|slot| *slot.borrow_mut() = false);
        return 1;
    }
    let ok = nss_text().contains("3.131");
    NSS_OK.with(|slot| *slot.borrow_mut() = ok);
    if ok { 0 } else { 1 }
}

#[unsafe(no_mangle)]
pub extern "C" fn necko_quic_version() -> *const c_char {
    store_reply(version_label().to_string())
}

#[unsafe(no_mangle)]
pub extern "C" fn necko_quic_set_decoder(callback: Option<DecoderFn>) {
    DECODER.with(|slot| *slot.borrow_mut() = callback);
    let _kept = DECODER.with(|slot| slot.borrow().is_some());
    let _ = _kept;
}

#[unsafe(no_mangle)]
pub extern "C" fn necko_quic_call(handle: u64, data: *const c_char, length: usize) -> *const c_char {
    if data.is_null() || length == 0 || length >= COMMAND_LIMIT {
        return fail("Invalid command buffer");
    }
    let bytes = unsafe { std::slice::from_raw_parts(data.cast::<u8>(), length) };
    let text = match std::str::from_utf8(bytes) {
        Ok(text) => text,
        Err(_) => return fail("Invalid command buffer"),
    };
    let parsed = match serde_json::from_str::<Value>(text) {
        Ok(value) => value,
        Err(error) => return fail(&error.to_string()),
    };
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| dispatch(handle, &parsed)));
    match result {
        Ok(Ok(value)) => reply(value),
        Ok(Err(message)) => fail(&message),
        Err(_) => fail("HTTP/3 command panicked"),
    }
}

#[allow(dead_code)]
fn engine_linked() -> usize {
    std::mem::size_of::<Http3Client>() + std::mem::size_of::<Duration>()
}
