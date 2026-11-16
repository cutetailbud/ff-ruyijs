#include "document_model/model_internal.h"
#include "domain/document_model/meter_values.h"
#include <cmath>

namespace ruyijs::document_model {
Status Model::MeterValue(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] { *out = domain::meter_value(State().element(key), name); });
}
Status Model::GaugeValue(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] {
    const auto& element = State().element(key);
    *out = element.tag_name().local16() == u"progress" ? domain::progress_value(element, name)
                                                        : domain::meter_value(element, name);
  });
}
Status Model::SetGaugeValue(NodeKey key, DomStringView name, double value) noexcept {
  bool progress = false;
  auto status = Protect([&] { progress = State().element(key).tag_name().local16() == u"progress"; });
  if (status.code != ErrorCode::None) return status;
  if (progress && name == u"max" && !(value > 0)) return {};
  if (!progress) return SetMeterValue(key, name, value);
  // The shortest round-trip form, as Firefox serializes a reflected double.
  DomString text;
  status = Protect([&] { text = domain::from_utf8(domain::progress_text(value)); });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
Status Model::SetMeterValue(NodeKey key, DomStringView name, double value) noexcept {
  DomString text;
  auto status = Protect([&] {
    if (std::isnan(value)) text = u"nan";
    else if (std::isinf(value)) text = value < 0 ? u"-inf" : u"inf";
    else text = domain::from_utf8(std::to_string(value));
  });
  return status.code == ErrorCode::None ? SetAttribute(key, name, text) : status;
}
}
