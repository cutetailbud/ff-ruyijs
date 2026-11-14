#include "transport/worker_schema_internal.h"

namespace ruyijs::transport::worker::schema {
bool Reserve(const Value& p) {
  if (!Fields(p, {"reservationId", "partition", "deadlineQpc", "qpcFrequency", "method", "url",
                 "headers", "uploadLength", "replayable", "responseLimit", "transportOptions"}) ||
      !U64(p["reservationId"]) || !Number(p["partition"], 0, 3) || !U64(p["deadlineQpc"]) ||
      !U64(p["qpcFrequency"]) || !Token(p["method"], 32) || !TextSize(p["url"], 1, 16384) ||
      !Headers(p["headers"]) || !Number(p["uploadLength"], 0, 16 * 1024 * 1024) ||
      !Boolean(p["replayable"], true) || !Number(p["responseLimit"], 0, 16 * 1024 * 1024)) return false;
  for (unsigned char c : Text(p["url"])) if (c < 32 || c == 127) return false;
  const auto& options = p["transportOptions"];
  return Fields(options, {"trustProfile", "proxyProfile", "connectMs", "readMs"}) &&
      Number(options["trustProfile"]) && (options["proxyProfile"].isNull() || Number(options["proxyProfile"])) &&
      Number(options["connectMs"], 1, 60000) && Number(options["readMs"], 1, 60000);
}
bool Handshake(std::string_view type, const Value& p, Sender sender) {
  if (type == "hello")
    return Fields(p, {"role", "pid", "nonce", "manifest", "qpcFrequency"}) &&
        OneOf(p["role"], {sender == Sender::Parent ? "parent" : "worker"}) &&
        Number(p["pid"], 1) && Hex(p["nonce"], 32, true) && Hex(p["manifest"], 64, false) &&
        U64(p["qpcFrequency"]);
  return type == "ready" && Fields(p, {"manifest", "abi", "nss", "nspr", "wireProfile"}) &&
      Hex(p["manifest"], 64, false) && Number(p["abi"], 1, 1) && OneOf(p["nss"], {"3.131", "3.132"}) &&
      OneOf(p["nspr"], {"4.40"}) && OneOf(p["wireProfile"], {"firefox159", "firefox160"});
}
bool Reservation(std::string_view type, const Value& p) {
  if (type == "reserve") return Reserve(p);
  if (type == "accepted") return Fields(p, {"reservationId", "state"}) &&
      U64(p["reservationId"]) && OneOf(p["state"], {"reserved"});
  if (type == "activate") return Fields(p, {"reservationId", "key"}) &&
      U64(p["reservationId"]) && Key(p["key"]);
  if (type == "activated") return Fields(p, {"reservationId", "key", "state"}) &&
      U64(p["reservationId"]) && Key(p["key"]) && OneOf(p["state"], {"queued", "active"});
  if (type == "release-reservation" || type == "reservationReleased")
    return Fields(p, {"reservationId", "reason"}) && U64(p["reservationId"]) && Reason(p["reason"]);
  return type == "ack-reservation" && Fields(p, {"reservationId"}) && U64(p["reservationId"]);
}
}
