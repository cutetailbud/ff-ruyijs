#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace ruyijs::domain::styling {
struct CssDeclarationValue { std::string name,value;bool important=false;std::string pending_shorthand; };
struct CssParsedBlock {
  std::vector<CssDeclarationValue> rows;
  std::unordered_map<std::string, size_t> index;
  mutable std::string serialized;
  mutable bool serialized_ready = false;
};
enum class CssDeclarationContext { Standard, Page };
// Native Firefox transform aliases share one declaration value and priority.
std::string CssCanonicalDeclarationName(const std::string&);
std::vector<CssDeclarationValue> CssDeclarations(const std::string&,
    CssDeclarationContext = CssDeclarationContext::Standard);
const CssParsedBlock& CssDeclarationsCached(const std::string&);
std::string CssDeclarationText(const std::string&);
std::string CssSetDeclaration(std::string&,const std::string&,const std::string&,bool,bool);
// css_block_serialization.cpp: CSSOM block serialization (shorthands recombined)
// and the shorthand-aware value lookup shared by every declaration block.
bool CssShorthandName(std::string_view);
std::string CssSerializeBlock(const std::vector<CssDeclarationValue>&);
const std::string& CssSerializeBlockCached(const CssParsedBlock&);
std::string CssLookupDeclaration(const std::vector<CssDeclarationValue>&,const std::string&,bool* important);
std::string CssLookupDeclaration(const CssParsedBlock&,const std::string&,bool* important);
}
