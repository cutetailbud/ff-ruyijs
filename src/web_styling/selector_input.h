#pragma once
#include "domain/styling/css_selector.h"
#include "document_model/style_source.h"
#include <map>
namespace ruyijs::web_styling {
struct State;
// One read session (a computed-style read or one layout pass, during which
// the tree cannot change) owns only the relation frames it reads, never a
// persistent DOM copy. Target() moves it to the next element of the session.
class SelectorInput final:public ruyijs::domain::styling::ElementRelations {
 public:
  SelectorInput(State&,JS::HandleObject);
  const ruyijs::domain::styling::ElementNode* Load(uint64_t document=0,uint64_t key=0) const;
  const ruyijs::domain::styling::ElementNode* Related(const ruyijs::domain::styling::ElementNode&,char) const override;
  bool ok() const {return ok_;}
  void Target(JS::HandleObject element) {element_=element;}
  void ClearTarget() {element_=nullptr;}
 private:
  struct Entry {ruyijs::domain::styling::ElementNode node;document_model::StyleRelationFrame frame;};
  State& state_;
  JS::PersistentRootedObject element_;
  mutable bool ok_=true;
  mutable std::map<std::pair<uint64_t,uint64_t>,Entry> entries_;
};
}
