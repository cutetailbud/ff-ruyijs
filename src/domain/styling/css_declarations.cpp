#include "domain/styling/css_declarations.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include "domain/styling/css_font_value.h"
#include "domain/styling/css_box_value.h"
#include "domain/styling/css_page_value.h"
#include "domain/styling/css_specified_value.h"
#include <algorithm>
#include <charconv>
#include <memory>
#include <unordered_map>
namespace ruyijs::domain::styling {
std::string CssCanonicalDeclarationName(const std::string& input) {
  if(input.starts_with("--")) return input;
  const auto name=CssLower(input);
  if(name=="-moz-transform"||name=="-webkit-transform") return "transform";
  if(name=="-ms-transform"||name=="-o-transform") return {};
  // A Firefox alias (CSS2Properties dash-case accessor) names its property.
  if(auto target=CssPropertyAliasTarget(name);!target.empty()) return target;
  return name;
}
namespace {
std::string DeclarationValue(const std::string& name,std::string value) {
  if(name!="opacity"&&name!="z-index") return value;
  auto number=CssNumericParse(value);
  if(number.expression||(number.kind!=CssNumber&&(name!="opacity"||number.kind!=CssPercent))) return value;
  char buffer[64];auto result=std::to_chars(buffer,buffer+sizeof(buffer),number.number);
  if(result.ec!=std::errc{}) return value;
  return std::string(buffer,result.ptr)+(number.kind==CssPercent?"%":"");
}
}
namespace {
std::vector<CssDeclarationValue> Parse(const std::string&,CssDeclarationContext);
struct ParseMemo { std::string text; CssParsedBlock parsed; };
constexpr size_t kMemoEntries=16;
void BuildBlockIndex(CssParsedBlock* block) {
  if(block->rows.size()<=16) return;
  block->index.reserve(block->rows.size());
  for(size_t i=0;i<block->rows.size();++i) block->index.emplace(block->rows[i].name,i);
}
}
const CssParsedBlock& CssDeclarationsCached(const std::string& text) {
  static const CssParsedBlock kEmpty;
  if(text.empty()) return kEmpty;
  thread_local std::vector<std::unique_ptr<ParseMemo>> memo;
  for(size_t i=0;i<memo.size();++i) if(memo[i]->text==text) {
    if(i) std::rotate(memo.begin(),memo.begin()+i,memo.begin()+i+1);
    return memo.front()->parsed;
  }
  auto entry=std::make_unique<ParseMemo>();
  entry->text=text;
  entry->parsed.rows=Parse(text,CssDeclarationContext::Standard);
  BuildBlockIndex(&entry->parsed);
  if(memo.size()==kMemoEntries) memo.pop_back();
  memo.insert(memo.begin(),std::move(entry));
  return memo.front()->parsed;
}
std::vector<CssDeclarationValue> CssDeclarations(const std::string& text,CssDeclarationContext context) {
  if(context!=CssDeclarationContext::Standard) return Parse(text,context);
  return CssDeclarationsCached(text).rows;
}
namespace {
std::vector<CssDeclarationValue> Parse(const std::string& text,CssDeclarationContext context) {
  std::vector<CssDeclarationValue> slots;
  std::unordered_map<std::string,size_t> seen;
  for(const auto& part:CssSplit(text,';')) {
    const auto colon=CssDelimiter(part,0,":");
    if(colon==part.size()) continue;
    auto name=CssTrim(CssClean(std::string_view(part).substr(0,colon)));
    if(name.empty()) continue;
    name=CssCanonicalDeclarationName(name);
    if(name.empty()) continue;
    // Servo drops a declaration of a property it does not implement.
    if(context==CssDeclarationContext::Standard&&!CssKnownProperty(name)) continue;
    bool valid=true;
    for(char c:name) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||static_cast<unsigned char>(c)>=128)) valid=false;
    if(!valid) continue;
    auto value=context==CssDeclarationContext::Page||name=="font"||CssBoxShorthand(name)?CssTrim(std::string_view(part).substr(colon+1)):
                            CssClean(std::string_view(part).substr(colon+1));
    bool important=false;
    const auto bang=CssDelimiter(value,0,"!");
    if(bang<value.size()&&CssLower(CssClean(std::string_view(value).substr(bang+1)))=="important") {
      important=true;value=CssTrim(std::string_view(value).substr(0,bang));
    }
    if(value.empty()&&name.substr(0,2)!="--") continue;
    value=DeclarationValue(name,std::move(value));
    std::vector<CssDeclarationValue> expanded;
    if(context==CssDeclarationContext::Page) {if(!CssPageExpand(name,value,important,&expanded)) continue;}
    else if(name=="font") { if(!CssFontExpand(value,important,&expanded)) continue; }
    else if(CssBoxShorthand(name)) { if(!CssBoxExpand(name,value,important,&expanded)) continue; }
    else expanded.push_back({std::move(name),std::move(value),important});
    for(auto& row:expanded) {
      if(context==CssDeclarationContext::Standard) row.value=CssSpecifiedValue(row.name,row.value);
      // PropertyDeclarationBlock::push (parsing): a repeated property replaces
      // the earlier declaration by removing it and appending the new one,
      // unless the earlier one is important and this one is not.
      auto old=seen.find(row.name);
      if(old==seen.end()) {
        seen.emplace(row.name,slots.size());
        slots.push_back(std::move(row));
      } else if(!slots[old->second].important||important) {
        slots[old->second].name.clear();
        old->second=slots.size();
        slots.push_back(std::move(row));
      }
    }
  }
  std::vector<CssDeclarationValue> result;
  result.reserve(seen.size());
  for(auto& slot:slots) if(!slot.name.empty()) result.push_back(std::move(slot));
  return result;
}
}
std::string CssDeclarationText(const std::string& text) {
  return CssSerializeBlockCached(CssDeclarationsCached(text));
}
std::string CssSetDeclaration(std::string& block,const std::string& inputName,
                               const std::string& value,bool important,bool remove) {
  const auto name=CssCanonicalDeclarationName(inputName);
  if(name.empty()) return {};
  auto rows=CssDeclarations(block);std::string old;
  if(name=="font") {
    old=CssFontSerialize(rows);
    std::vector<CssDeclarationValue> expanded;
    if(!remove&&!value.empty()&&!CssFontExpand(value,important,&expanded)) return old;
    for(const auto& property:CssFontDefaults())
      rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return row.name==property.name;}),rows.end());
    rows.insert(rows.end(),expanded.begin(),expanded.end());
  } else if(CssBoxShorthand(name)) {
    old=CssBoxSerialize(rows,name);
    std::vector<CssDeclarationValue> expanded;
    if(!remove&&!value.empty()&&!CssBoxExpand(name,value,important,&expanded)) return old;
    const auto names=CssBoxLonghands(name);
    rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){
      return row.name==name||std::find(names.begin(),names.end(),row.name)!=names.end();
    }),rows.end());
    rows.insert(rows.end(),expanded.begin(),expanded.end());
  } else {
  auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.name==name;});
  if(found!=rows.end()) old=found->value;
  if(remove||value.empty()) { if(found!=rows.end()) rows.erase(found); }
  else if(!CssKnownProperty(name)) return old;
  else if(found==rows.end()) rows.push_back({name,CssSpecifiedValue(name,DeclarationValue(name,value)),important});
  else { found->value=CssSpecifiedValue(name,DeclarationValue(name,value));found->important=important; }
  }
  block=CssSerializeBlock(rows);
  return old;
}
}
