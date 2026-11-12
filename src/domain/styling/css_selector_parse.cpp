#include "domain/styling/css_selector.h"
#include "domain/styling/css_selector_tokens.h"
#include "domain/styling/css_components.h"
namespace ruyijs::domain::styling {
ParsedSelector ParsedSelector::Parse(const std::string& input,unsigned depth,bool strict,bool inHas) {
  ParsedSelector result;const auto text=CssClean(input);
  if(depth>64||text.empty()||text.size()>8*1024*1024) {result.valid_=false;return result;}
  size_t p=0;char combinator=' ';bool need=true;
  while(p<text.size()) {
    p=CssSkip(text,p);if(p==text.size()) break;
    if(text[p]=='>'||text[p]=='+'||text[p]=='~') {
      if(need) {result.valid_=false;return result;}
      combinator=text[p++];need=true;continue;
    }
    size_t start=p;
    while(p<text.size()&&!CssSpace(text[p])&&text[p]!='>'&&text[p]!='+'&&text[p]!='~') {
      if(text[p]=='\\') {std::string ignored;if(!SelectorIdent(text,&p,&ignored)) {result.valid_=false;return result;}continue;}
      if(text[p]=='['||text[p]=='(') {
        auto end=CssClosing(text,p);if(end==text.size()) {result.valid_=false;return result;}p=end;
      }
      ++p;
    }
    SelectorCompound part;part.combinator=combinator;
    if(!ParseCompound(text.substr(start,p-start),&part,depth,strict,inHas)) {result.valid_=false;return result;}
    result.a_+=int(part.ids.size())+part.function_a;
    result.b_+=int(part.classes.size()+part.attributes.size()+part.pseudos.size())+part.function_b;
    for(const auto& pseudo:part.pseudos) if(pseudo=="&") --result.b_;
    result.c_+=int(!part.type.empty())+int(part.pseudo_element)+part.function_c;
    result.compounds_.push_back(std::move(part));need=false;combinator=' ';
  }
  if(need||result.compounds_.empty()) {result.valid_=false;return result;}
  for(size_t i=result.compounds_.size()-1;i>0;--i) {
    char rel=result.compounds_[i].combinator;
    if(rel!=' '&&rel!='>') break;
    const auto& anc=result.compounds_[i-1];
    if(!anc.type.empty()&&anc.type!="*") result.ancestor_bloom_|=SelectorBloomHash('t',anc.type);
    for(const auto& id:anc.ids) if(!id.empty()) result.ancestor_bloom_|=SelectorBloomHash('#',id);
    for(const auto& cls:anc.classes) if(!cls.empty()) result.ancestor_bloom_|=SelectorBloomHash('.',cls);
  }
  for(size_t i=0;i<result.compounds_.size();++i) {
    const auto& part=result.compounds_[i];
    if(i>0&&(part.combinator=='+'||part.combinator=='~')) result.sibling_sensitive_=true;
    for(const auto& pseudo:part.pseudos) {
      if(pseudo=="first-child"||pseudo=="last-child"||pseudo=="only-child") result.edge_child_=true;
      else if(pseudo=="first-of-type"||pseudo=="last-of-type"||pseudo=="only-of-type") result.sibling_sensitive_=true;
    }
    for(const auto& fn:part.functions) {
      if(fn.name=="has"||fn.name.starts_with("nth-")) result.sibling_sensitive_=true;
      for(const auto& sub:fn.selectors) {
        if(!sub) continue;
        result.edge_child_|=sub->edge_child_;
        result.sibling_sensitive_|=sub->sibling_sensitive_;
      }
    }
  }
  return result;
}
bool ParsedSelector::ParseCompound(const std::string& text,SelectorCompound* out,unsigned depth,bool strict,bool inHas) {
  size_t p=0;bool type=false;
  while(p<text.size()) {
    char c=text[p];
    if(c=='*') {if(p||type) return false;type=true;++p;continue;}
    if(c=='&') {if(inHas) return false;out->pseudos.push_back("&");++p;continue;}
    if(c=='#'||c=='.') {
      ++p;std::string name;if(!SelectorIdent(text,&p,&name)) return false;
      (c=='#'?out->ids:out->classes).push_back(std::move(name));continue;
    }
    if(c=='[') {
      auto end=CssClosing(text,p);if(end==text.size()) return false;
      SelectorAttribute attr;if(!SelectorAttributeParse(std::string_view(text).substr(p+1,end-p-1),&attr)) return false;
      out->attributes.push_back(std::move(attr));p=end+1;continue;
    }
    if(c==':') {
      bool element=p+1<text.size()&&text[p+1]==':';p+=element?2:1;
      std::string name;if(!SelectorIdent(text,&p,&name)) return false;name=CssLower(name);
      if(!element&&(name=="before"||name=="after"||name=="first-line"||name=="first-letter")) element=true;
      if(element) {
        if(inHas) return false;
        if(name=="part"||name=="slotted") {
          if(p==text.size()||text[p]!='(') return false;
          auto end=CssClosing(text,p);if(end==text.size()) return false;
          auto body=CssTrim(std::string_view(text).substr(p+1,end-p-1));if(body.empty()) return false;
          if(name=="slotted") {
            auto selector=Parse(body,depth+1,strict,inHas);
            if(!selector.valid()||selector.compounds_.size()!=1||selector.has_pseudo_element()) return false;
          } else {
            size_t pos=0;
            while(pos<body.size()) {
              std::string ident;if(!SelectorIdent(body,&pos,&ident)) return false;
              size_t skip=CssSkip(body,pos);if(skip==pos&&skip<body.size()) return false;pos=skip;
            }
          }
          out->pseudo_element=true;p=end+1;continue;
        }
        if(!SelectorPseudoKnown(name,true)) return false;
        out->pseudo_element=true;
        if(p<text.size()&&text[p]=='(') return false;
      } else if(p<text.size()&&text[p]=='(') {
        auto end=CssClosing(text,p);if(end==text.size()) return false;
        if(!ParseFunction(name,text.substr(p+1,end-p-1),out,depth,strict,inHas)) return false;
        p=end+1;
      } else {
        if(!SelectorPseudoKnown(name)) return false;
        out->pseudos.push_back(name);
      }
      continue;
    }
    if(p||type) return false;
    if(!SelectorIdent(text,&p,&out->type)) return false;type=true;
  }
  return !text.empty();
}
bool ParsedSelector::pure_id(std::string* id) const {
  if (!valid_ || compounds_.size() != 1) return false;
  const auto& part = compounds_.front();
  if (part.combinator != ' ' || part.pseudo_element || !part.type.empty() ||
      !part.classes.empty() || !part.attributes.empty() || !part.pseudos.empty() ||
      !part.functions.empty() || part.ids.size() != 1 || part.ids.front().empty())
    return false;
  if (id) *id = part.ids.front();
  return true;
}
bool ParsedSelector::has_pseudo_element() const noexcept {
  for(const auto& part:compounds_) if(part.pseudo_element) return true;
  return false;
}
std::vector<ParsedSelector> ParseSelectorList(const std::string& text) {
  std::vector<ParsedSelector> out;
  for(const auto& group:CssSplit(text,',')) {
    auto selector=ParsedSelector::Parse(group);
    if(!selector.valid()) return {};
    out.push_back(std::move(selector));
  }
  return out;
}
bool CssSelectorSyntax(const std::string& text,bool relative) {
  for(auto group:CssSplit(text,',')) {
    if(relative&&!group.empty()&&(group[0]=='>'||group[0]=='+'||group[0]=='~')) group="& "+group;
    if(!ParsedSelector::Parse(group).valid()) return false;
  }
  return !CssTrim(text).empty();
}
}
