#ifndef RUYIJS_DOCUMENT_MODEL_SELECTOR_H
#define RUYIJS_DOCUMENT_MODEL_SELECTOR_H
#include <cstdint>
#include <memory>
#include <vector>

#include "document_model/model.h"

namespace ruyijs::domain::document_model { class DocumentAggregate; }
namespace ruyijs::domain::styling { class ParsedSelector; }

namespace ruyijs::document_model {

// The DOM selector entry (querySelector[All], matches, closest). Grammar and
// matching belong to the one Styling selector owner (domain/styling
// ParsedSelector); this adapter only parses the DOMString selector list and
// reads the Document Model tree as that owner's ElementNode relations.
class Selector final {
 public:
  static Status Parse(DomStringView text, Selector* result) noexcept;
  // True when the list is exactly one pure id selector (`#id`, escapes decoded).
  bool PureId(std::string* id) const;

  // `scope` is the scoping element for :scope / &; 0 means none (a Document or
  // fragment scoping root, which no element matches as :scope).
  bool Matches(const ruyijs::domain::document_model::DocumentAggregate& owner,
               uint64_t element, uint64_t scope = 0) const;
  void Select(const ruyijs::domain::document_model::DocumentAggregate& owner,
              uint64_t root, std::vector<uint64_t>* result) const;

 private:
  std::vector<std::shared_ptr<const ruyijs::domain::styling::ParsedSelector>> groups_;
};
}  // namespace ruyijs::document_model
#endif
