#include "web_geometry/box_input.h"
#include "domain/styling/css_numeric_value.h"
#include "web_geometry/font_config.h"
#include <cstdlib>
#include <string>

namespace ruyijs::web_geometry {
bool BoxText(State& s, const document_model::LayoutNodeSource& source, box::BoxNode* node) {
  auto text=source.data;
  const auto space=node->style.Get("white-space");
  if(space!="pre" && space!="pre-wrap" && space!="break-spaces") {
    text.clear(); bool previous_space=true;
    for(const auto ch:source.data) {
      const bool current_space=ch==u' '||ch==u'\t'||ch==u'\r'||ch==u'\n'||ch==u'\f';
      if(!current_space || !previous_space) text+=current_space?u' ':ch;
      previous_space=current_space;
    }
    if(!text.empty()&&text.back()==u' ') text.pop_back();
  }
  if(text.empty()) return true;
  node->breakable=space!="nowrap" && space!="pre" && text.find(u' ')!=text.npos;
  FontInput font;
  font.family=node->style.Get("font-family"); font.size=node->style.font_size;
  font.italic=node->style.Get("font-style")!="normal";
  const auto weight=node->style.Get("font-weight");
  if(!weight.empty()) font.weight=unsigned(std::strtoul(weight.c_str(),nullptr,10));
  // The inherited content language (nsStyleFont::mLanguage) picks the
  // generic families' language group and the fallback order.
  font.language=node->style.Get("-x-lang");
  if(font.language=="-x-none") font.language.clear();
  // System fonts do not change while the font configuration generation holds.
  std::string key=std::to_string(FontConfigGeneration())+'\x1f'+font.family+'\x1f'+font.language+'\x1f'+std::to_string(font.size)+
      '\x1f'+std::to_string(font.weight)+(font.italic?"i":"n");
  key.append(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(char16_t));
  const bool preserve=space=="pre" || space=="pre-wrap" || space=="break-spaces";
  if(preserve) key+='\x1f'+std::string("pre");
  auto hit=s.textMeasures.find(key);
  if(hit==s.textMeasures.end()) {
    ruyijs::domain::geometry::SvgTextMeasurement measured;
    if(!preserve || text.find(u"  ")==text.npos) {
      if(!s.fonts.Measure(text,font,&measured))
        return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
    } else {
      // Measure collapses runs of spaces; a preserved run keeps every space,
      // each one space advance wide (#42). Split at the extra spaces.
      ruyijs::domain::geometry::SvgTextMeasurement one;
      if(!s.fonts.Measure(u"x x",font,&one))
        return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
      ruyijs::domain::geometry::SvgTextMeasurement x;
      if(!s.fonts.Measure(u"xx",font,&x))
        return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
      const float space_width=one.length-x.length;
      std::u16string run;
      float total=0;
      size_t extra=0;
      for(size_t i=0;i<text.size();++i) {
        if(text[i]==u' ' && i>0 && text[i-1]==u' ') { ++extra; continue; }
        run+=text[i];
      }
      if(!s.fonts.Measure(run,font,&measured))
        return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
      total=measured.length+space_width*float(extra);
      measured.length=total;
    }
    double content=0;
    State::TextMeasure entry;
    if(!s.fonts.LineMetrics(font,&content,&entry.normal))
      return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
    // An inline box is the primary font's content area: rounded ascent plus
    // descent (nsFontMetrics), not the run's glyph bounds.
    entry.width=measured.length; entry.height=content>0 ? content : measured.box[3];
    // Wrappable text keeps each word's width and one space's for the line
    // breaking in box_layout_flow.cpp. Collapsed text has single spaces.
    if(space!="nowrap" && space!="pre" && text.find(u' ')!=text.npos && text.size()<=4096) {
      ruyijs::domain::geometry::SvgTextMeasurement one,pair;
      if(!s.fonts.Measure(u"x x",font,&one) || !s.fonts.Measure(u"xx",font,&pair))
        return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
      entry.space=one.length-pair.length;
      size_t start=0;
      while(start<=text.size()) {
        size_t end=text.find(u' ',start);
        if(end==text.npos) end=text.size();
        ruyijs::domain::geometry::SvgTextMeasurement word;
        if(end>start && !s.fonts.Measure(text.substr(start,end-start),font,&word))
          return WebKit::Fail(s.cx,"Element text geometry exceeds supported font resources");
        entry.words.push_back(end>start ? word.length : 0.f);
        start=end+1;
      }
    }
    if(s.textMeasures.size()>=8192) s.textMeasures.clear();
    hit=s.textMeasures.emplace(std::move(key),entry).first;
  }
  const double normal=hit->second.normal;
  node->text_width=std::round(hit->second.width*60)/60;
  node->text_height=hit->second.height;
  node->words=hit->second.words; node->word_space=hit->second.space;
  const auto line=box::CssNumericResolve(node->style.Get("line-height"),font.size,node->style.root_font_size,font.size);
  node->line_height=line.kind==box::CssNumber ? line.number*font.size : line.kind ? line.number : normal;
  return true;
}
}
