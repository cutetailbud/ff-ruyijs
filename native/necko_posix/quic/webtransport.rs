use std::time::Instant;
use base64::{Engine, engine::general_purpose::STANDARD as B64};
use neqo_common::Header;
use neqo_http3::{webtransport::ClientSession, SendGroupId};
use neqo_transport::{DatagramQueueOutcome, StreamId, StreamType};
use serde_json::{Value, json};
use crate::{Result, client::Session, err, number, text};

fn fields(v: &Value) -> Result<Vec<Header>> {
    let input = v["headers"].as_array().ok_or("Missing headers")?;
    if input.len() > 256 { return Err("Too many headers".into()); }
    let mut result = Vec::new(); let mut size = 0;
    for field in input {
        let name = field[0].as_str().ok_or("Invalid header")?;
        let value = B64.decode(field[1].as_str().ok_or("Invalid value")?).map_err(err)?;
        size += name.len() + value.len();
        if size > 65536 { return Err("Headers exceed limit".into()); }
        result.push(Header::new(name, value));
    }
    Ok(result)
}
pub fn call(session: &mut Session, v: &Value) -> Result<Value> {
    let now = Instant::now();
    let id = StreamId::from(v["session"].as_u64().unwrap_or(0));
    let stream = StreamId::from(v["stream"].as_u64().unwrap_or(0));
    let client = &mut session.client;
    match text(v,"action")? {
        "open" => {
            let id = client.webtransport_create_session(now,("https",text(v,"authority")?,text(v,"path")?),&fields(v)?).map_err(err)?;
            Ok(json!({"session":id.as_u64()}))
        },
        "info" => {
            let mut value = session.wt_closed.get(&id.as_u64()).cloned().unwrap_or(json!({}));
            let stats = client.transport_stats();
            value["bytesSent"] = json!(session.sent_bytes);
            value["packetsSent"] = json!(stats.packets_tx);
            value["bytesAcknowledged"] = json!(stats.bytes_acked);
            value["bytesReceived"] = json!(stats.bytes_rx);
            value["packetsReceived"] = json!(stats.packets_rx);
            value["bytesLost"] = json!(stats.bytes_lost);
            value["packetsLost"] = json!(stats.lost);
            value["smoothedRtt"] = json!(stats.rtt.as_secs_f64()*1000.0);
            value["rttVariation"] = json!(stats.rttvar.as_secs_f64()*1000.0);
            value["minRtt"] = json!(stats.min_rtt.as_secs_f64()*1000.0);
            value["lostOutgoing"] = json!(stats.datagram_tx.lost);
            if let Ok(stats) = client.webtransport_session_stats(id) {
                value["expiredOutgoing"] = json!(stats.datagrams_expired_outgoing);
            }
            if let Ok(size) = client.webtransport_max_datagram_size(id) { value["maxDatagramSize"] = json!(size); }
            if let Ok(protocol) = client.webtransport_session_protocol(id) { value["protocol"] = json!(protocol); }
            Ok(value)
        },
        "stream" => {
            let kind = if v["bidirectional"].as_bool().unwrap_or(false) { StreamType::BiDi } else { StreamType::UniDi };
            let group = v["group"].as_u64().map(SendGroupId::new);
            let stream = client.webtransport_create_stream_with_send_group(id,kind,group).map_err(err)?;
            client.webtransport_set_sendorder(stream,v["order"].as_i64()).map_err(err)?;
            Ok(json!({"stream":stream.as_u64()}))
        },
        "close" => {
            let code = u32::try_from(number(v,"code")?).map_err(err)?;
            let stats = client.webtransport_close_session(id,code,text(v,"reason")?,now).map_err(err)?;
            session.wt_closed.insert(id.as_u64(),json!({"expiredOutgoing":stats.datagrams_expired_outgoing}));
            Ok(json!({}))
        },
        "datagram" => {
            let data = B64.decode(text(v,"data")?).map_err(err)?;
            if data.len() > 65536 { return Err("Datagram exceeds limit".into()); }
            // Neqo 0.33: the default send group and order (as its own CONNECT-UDP
            // sends do); only a refused datagram was not queued.
            let outcome = client.webtransport_send_datagram(id,&data,None,now,SendGroupId::new(0),0).map_err(err)?;
            if matches!(outcome, DatagramQueueOutcome::Rejected) {
                return Err("WebTransport datagram queue is full".into());
            }
            Ok(json!({}))
        },
        "reset" | "stop" => {
            let code = number(v,"code")?;
            if code > 255 { return Err("WebTransport stream error must be 0..255".into()); }
            let error = 0x52e4a40fa8db + code + code / 30;
            if text(v,"action")? == "reset" { client.stream_reset_send(stream,error) }
            else { client.stream_stop_sending(stream,error) }.map_err(err)?;
            Ok(json!({}))
        },
        "group" => Ok(json!({"group":client.webtransport_create_send_group(id).map_err(err)?.as_u64()})),
        "set_group" => {
            if let Some(group) = v["group"].as_u64() {
                let group = SendGroupId::new(group);
                if !client.webtransport_validate_send_group(id,group).map_err(err)? { return Err("Unknown send group".into()); }
                client.webtransport_set_sendgroup(stream,group).map_err(err)?;
            } else { client.webtransport_clear_sendgroup(stream).map_err(err)?; }
            Ok(json!({}))
        },
        "order" => { client.webtransport_set_sendorder(stream,v["order"].as_i64()).map_err(err)?; Ok(json!({})) },
        "stream_stats" => {
            if v["sending"].as_bool().unwrap_or(false) {
                let stats = client.webtransport_send_stream_stats(stream).map_err(err)?;
                Ok(json!({"bytesWritten":stats.written,"bytesSent":stats.sent,"bytesAcknowledged":stats.acked}))
            } else {
                let stats = client.webtransport_recv_stream_stats(stream).map_err(err)?;
                Ok(json!({"bytesReceived":stats.bytes_received,"bytesRead":stats.bytes_read}))
            }
        },
        "export" => {
            let label = B64.decode(text(v,"label")?).map_err(err)?;
            let context = B64.decode(text(v,"context")?).map_err(err)?;
            let mut data = [0;32];
            client.webtransport_export_keying_material(id,&label,&context,&mut data).map_err(err)?;
            Ok(json!({"data":B64.encode(data)}))
        },
        action => Err(format!("Unknown WebTransport action: {action}")),
    }
}
