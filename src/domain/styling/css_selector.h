// The original Styling selector owner; input relations are read on demand.
#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace ruyijs::domain::styling {
inline uint64_t SelectorBloomHash(char kind,std::string_view token) noexcept {
  uint64_t h=14695981039346656037ull;
  h=(h^static_cast<unsigned char>(kind))*1099511628211ull;
  for(unsigned char ch:token) {
    if(kind=='t'&&ch>='A'&&ch<='Z') ch=static_cast<unsigned char>(ch+32);
    h=(h^ch)*1099511628211ull;
  }
  return (1ull<<(h&63u))|(1ull<<((h>>6u)&63u));
}
struct ElementNode;
struct ElementRelations {
  virtual ~ElementRelations()=default;
  virtual const ElementNode* Related(const ElementNode&,char) const=0;
};
struct ElementNode {
  std::string tag,id,inline_style;
  std::vector<std::string> classes;
  std::map<std::string,std::string> attributes;
  const ElementNode* parent=nullptr;
  const ElementRelations* relations=nullptr;
  uint64_t document=0,key=0;
  mutable uint64_t ancestor_bloom=0;
  mutable bool ancestor_bloom_ready=false;
  bool root=false,empty=true,html=true,focused=false;
  bool has_class(const std::string& name) const {
    return std::find(classes.begin(),classes.end(),name)!=classes.end();
  }
  const ElementNode* related(char relation) const {
    if(relation=='>'&&parent) return parent;
    return relations?relations->Related(*this,relation):nullptr;
  }
  uint64_t own_bloom() const noexcept {
    uint64_t bits=0;
    if(!tag.empty()) bits|=SelectorBloomHash('t',tag);
    if(!id.empty()) bits|=SelectorBloomHash('#',id);
    for(const auto& cls:classes) if(!cls.empty()) bits|=SelectorBloomHash('.',cls);
    return bits;
  }
  uint64_t AncestorBloom(unsigned depth=0) const {
    if(depth>512) return ~0ull;
    if(!ancestor_bloom_ready) {
      const ElementNode* p=related('>');
      ancestor_bloom=p?(p->AncestorBloom(depth+1)|p->own_bloom()):0;
      ancestor_bloom_ready=true;
    }
    return ancestor_bloom;
  }
};
struct SelectorAttribute {std::string name,value,op;char flag=0;};
class ParsedSelector;
struct SelectorFunction {
  std::string name,argument;
  int64_t a=0,b=0;
  std::vector<std::shared_ptr<ParsedSelector>> selectors;
};
struct SelectorCompound {
  char combinator=' ';
  std::string type;
  std::vector<std::string> ids,classes,pseudos;
  std::vector<SelectorAttribute> attributes;
  std::vector<SelectorFunction> functions;
  bool pseudo_element=false;
  int function_a=0,function_b=0,function_c=0;
};
class ParsedSelector {
 public:
  static ParsedSelector Parse(const std::string&,unsigned depth=0,bool strict=false,bool inHas=false);
  bool matches(const ElementNode&) const;
  // DOM query entry (querySelector/matches/closest): `scope` is the scoping
  // element that :scope and a top-level & match; a scope that is no element of
  // the tree (a Document or fragment root) makes them match nothing.
  bool matches(const ElementNode&,const ElementNode* scope) const;
  bool valid() const noexcept {return valid_;}
  bool has_pseudo_element() const noexcept;
  int specificity_a() const noexcept {return a_;}
  int specificity_b() const noexcept {return b_;}
  int specificity_c() const noexcept {return c_;}
  // A necessary condition of matching, read from the subject (rightmost)
  // compound: '#' its first id, '.' its first class, 't' its lower-cased type,
  // 0 none. The cascade buckets rules by it, as Firefox's rule hash does.
  void IndexKey(char* kind,std::string* key) const;
  bool edge_child_sensitive() const noexcept {return edge_child_;}
  bool sibling_sensitive() const noexcept {return sibling_sensitive_;}
 private:
  static bool ParseCompound(const std::string&,SelectorCompound*,unsigned,bool,bool);
  static bool ParseFunction(const std::string&,const std::string&,SelectorCompound*,unsigned,bool,bool);
  static bool MatchCompound(const ElementNode&,const SelectorCompound&,const ElementNode*,size_t&,unsigned);
  static bool MatchFunctions(const ElementNode&,const SelectorCompound&,const ElementNode*,size_t&,unsigned);
  bool MatchAt(size_t,const ElementNode&,const ElementNode*,size_t&,unsigned) const;
  bool Match(const ElementNode&,const ElementNode*,size_t&,unsigned) const;
  std::vector<SelectorCompound> compounds_;
  uint64_t ancestor_bloom_=0;
  int a_=0,b_=0,c_=0;
  bool valid_=true,edge_child_=false,sibling_sensitive_=false;
};
std::vector<ParsedSelector> ParseSelectorList(const std::string&);
bool CssSelectorSyntax(const std::string&,bool relative=false);
std::string CssNestedSelectorText(const std::string&);
}
