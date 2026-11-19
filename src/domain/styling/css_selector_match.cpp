#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
bool ParsedSelector::MatchCompound(const ElementNode& node,const SelectorCompound& part,
                                  const ElementNode* scope,size_t& budget,unsigned depth) {
  if(!budget||depth>128||part.pseudo_element) return false;
  --budget;
  if(!part.type.empty()&&(node.html?CssLower(part.type)!=CssLower(node.tag):part.type!=node.tag)) return false;
  for(const auto& id:part.ids) if(node.id!=id) return false;
  for(const auto& name:part.classes) if(!node.has_class(name)) return false;
  for(const auto& attr:part.attributes) if(!SelectorAttributeMatches(node,attr)) return false;
  for(const auto& pseudo:part.pseudos) if(!SelectorPseudoMatches(node,pseudo,scope)) return false;
  return MatchFunctions(node,part,scope,budget,depth);
}
bool ParsedSelector::MatchAt(size_t index,const ElementNode& node,const ElementNode* scope,
                            size_t& budget,unsigned depth) const {
  if(!MatchCompound(node,compounds_[index],scope,budget,depth)) return false;
  if(!index) return true;
  char relation=compounds_[index].combinator;
  const bool once=relation=='>'||relation=='+';
  char step=(relation=='+'||relation=='~')?'-':'>';
  for(auto previous=node.related(step);previous&&budget;previous=previous->related(step)) {
    if(MatchAt(index-1,*previous,scope,budget,depth+1)) return true;
    if(once) break;
  }
  return false;
}
bool ParsedSelector::Match(const ElementNode& node,const ElementNode* scope,size_t& budget,unsigned depth) const {
  if(!valid_||compounds_.empty()||depth>128) return false;
  if(ancestor_bloom_&&(node.AncestorBloom()&ancestor_bloom_)!=ancestor_bloom_) return false;
  return MatchAt(compounds_.size()-1,node,scope,budget,depth);
}
void ParsedSelector::IndexKey(char* kind,std::string* key) const {
  *kind=0;key->clear();
  if(!valid_||compounds_.empty()) return;
  const SelectorCompound& subject=compounds_.back();
  if(!subject.ids.empty()) {*kind='#';*key=subject.ids.front();}
  else if(!subject.classes.empty()) {*kind='.';*key=subject.classes.front();}
  else if(!subject.type.empty()&&subject.type!="*") {*kind='t';*key=CssLower(subject.type);}
}
bool ParsedSelector::matches(const ElementNode& node) const {
  size_t budget=1000000;return Match(node,nullptr,budget,0);
}
bool ParsedSelector::matches(const ElementNode& node,const ElementNode* scope) const {
  size_t budget=1000000;return Match(node,scope,budget,0);
}
namespace {
// HTML "actually disabled" for the elements that support :enabled/:disabled:
// the disabled attribute, an option inside a disabled optgroup, or a form
// control inside a disabled fieldset other than in that fieldset's first legend.
bool FormControl(const ElementNode& node) {
  static const std::string tags="|button|input|select|textarea|optgroup|option|fieldset|";
  return node.html&&tags.find("|"+node.tag+"|")!=std::string::npos;
}
bool ActuallyDisabled(const ElementNode& node) {
  if(node.attributes.count("disabled")&&node.tag!="option") return true;
  if(node.tag=="option") {
    if(node.attributes.count("disabled")) return true;
    auto parent=node.related('>');
    return parent&&parent->html&&parent->tag=="optgroup"&&parent->attributes.count("disabled");
  }
  if(node.tag=="optgroup") return false;
  const ElementNode* child=&node;size_t budget=10000;
  for(auto parent=node.related('>');parent&&budget;child=parent,parent=parent->related('>'),--budget) {
    if(!(parent->html&&parent->tag=="fieldset"&&parent->attributes.count("disabled"))) continue;
    const ElementNode* legend=parent->related('v');
    while(legend&&!(legend->html&&legend->tag=="legend")) legend=legend->related('+');
    if(legend!=child) return true;
  }
  return false;
}
}
bool SelectorPseudoKnown(const std::string& name,bool element) {
  static const std::string classes="|active|any-link|autofill|checked|default|defined|disabled|empty|enabled|first-child|first-of-type|focus|focus-visible|focus-within|fullscreen|hover|in-range|indeterminate|invalid|last-child|last-of-type|link|modal|only-child|only-of-type|optional|out-of-range|placeholder-shown|read-only|read-write|required|root|scope|target|valid|visited|user-valid|user-invalid|host|";
  static const std::string elements="|before|after|first-letter|first-line|selection|backdrop|marker|placeholder|file-selector-button|";
  return (element?elements:classes).find("|"+name+"|")!=std::string::npos;
}
bool SelectorPseudoMatches(const ElementNode& node,const std::string& name,const ElementNode* scope) {
  if(name=="&"||name=="scope") return scope?scope==&node:node.root;
  if(name=="root") return node.root;
  if(name=="empty") return node.empty;
  if(name=="defined") return node.tag.find('-')==node.tag.npos;
  if(name=="focus"||name=="focus-visible") return node.focused;
  bool first=name=="first-child"||name=="first-of-type"||name=="only-child"||name=="only-of-type";
  bool last=name=="last-child"||name=="last-of-type"||name=="only-child"||name=="only-of-type";
  if(first||last) {
    bool typed=name.find("type")!=name.npos;size_t budget=100000;
    for(char relation:{'-','+'}) {
      if(relation=='-'?!first:!last) continue;
      for(auto next=node.related(relation);next&&budget;next=next->related(relation)) {
        --budget;if(!typed||next->tag==node.tag) return false;
      }
    }
    return budget!=0;
  }
  if(name=="link"||name=="any-link") return (node.tag=="a"||node.tag=="area")&&node.attributes.count("href");
  if(name=="disabled"||name=="enabled") return FormControl(node)&&ActuallyDisabled(node)==(name=="disabled");
  // Transient states need their established interaction/form owner. They are
  // never silently removed from a compound and therefore never match by default.
  return false;
}
}
