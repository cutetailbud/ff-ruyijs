#include "domain/styling/box_layout_internal.h"
#include "domain/styling/css_components.h"
#include "domain/styling/css_numeric_value.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ruyijs::domain::styling {
namespace {
using Matrix = std::array<double, 6>;
Matrix Multiply(const Matrix& a, const Matrix& b) {
  return {a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1], a[0]*b[2]+a[2]*b[3],
          a[1]*b[2]+a[3]*b[3], a[0]*b[4]+a[2]*b[5]+a[4], a[1]*b[4]+a[3]*b[5]+a[5]};
}
double Resolve(const BoxStyle& style, const std::string& text, double percent, double fallback) {
  const auto value = CssNumericResolve(text, style.font_size, style.root_font_size, percent);
  return value.kind ? value.number : fallback;
}
double Angle(const std::string& text) {
  const auto number = CssNumericParse(text);
  const double pi = std::acos(-1.0);
  auto ends = [&](const char* suffix) { const std::string s(suffix); return text.size() >= s.size() && text.compare(text.size()-s.size(),s.size(),s)==0; };
  if (ends("rad")) return ends("grad") ? number.number*pi/200 : number.number;
  return ends("turn") ? number.number*pi*2 : number.number*pi/180;
}
Matrix Transform(const BoxStyle& style, const BoxRect& box) {
  Matrix result{1,0,0,1,0,0};
  const auto text = CssLower(style.Get("transform"));
  for (size_t p = 0; p < text.size();) {
    p = CssSkip(text,p);
    const auto open = text.find('(',p);
    if (open == text.npos) break;
    const auto end = CssClosing(text,open);
    if (end >= text.size()) break;
    const auto name = CssTrim(text.substr(p,open-p));
    const auto args = CssSplit(text.substr(open+1,end-open-1), ',');
    auto get = [&](size_t i, double percent, double fallback) {
      return i < args.size() ? Resolve(style,args[i],percent,fallback) : fallback;
    };
    Matrix next{1,0,0,1,0,0};
    if (name == "matrix" && args.size() == 6) for (unsigned i=0;i<6;++i) next[i]=get(i,0,next[i]);
    else if (name == "translate" || name == "translatex") { next[4]=get(0,box[2],0); next[5]=get(1,box[3],0); }
    else if (name == "translatey") next[5]=get(0,box[3],0);
    else if (name == "scale") { next[0]=get(0,0,1); next[3]=get(1,0,next[0]); }
    else if (name == "scalex") next[0]=get(0,0,1);
    else if (name == "scaley") next[3]=get(0,0,1);
    else if (name == "rotate" && !args.empty()) {
      const double angle=Angle(args[0]); next={std::cos(angle),std::sin(angle),-std::sin(angle),std::cos(angle),0,0};
    } else if (name == "skewx" && !args.empty()) next[2]=std::tan(Angle(args[0]));
    else if (name == "skewy" && !args.empty()) next[1]=std::tan(Angle(args[0]));
    result=Multiply(result,next); p=end+1;
  }
  double ox=box[2]/2, oy=box[3]/2;
  const auto origin=style.Get("transform-origin");
  const auto parts=CssSplit(origin,' ');
  auto offset=[&](const std::string& part,double basis) {
    if(part=="left"||part=="top") return 0.0;
    if(part=="right"||part=="bottom") return basis;
    return part=="center"?basis/2:Resolve(style,part,basis,basis/2);
  };
  if(!origin.empty()&&!parts.empty()) { ox=offset(parts[0],box[2]); if(parts.size()>1) oy=offset(parts[1],box[3]); }
  ox+=box[0]; oy+=box[1];
  return Multiply(Multiply({1,0,0,1,ox,oy},result),{1,0,0,1,-ox,-oy});
}

std::pair<double, double> ParseCornerRadius(const BoxStyle& style, const std::string& raw, double w, double h) {
  if (raw.empty()) return {0.0, 0.0};
  const auto parts = CssSplit(raw, ' ');
  if (parts.empty()) return {0.0, 0.0};
  const double rx = std::max(0.0, Resolve(style, parts[0], w, 0.0));
  const double ry = std::max(0.0, Resolve(style, parts.size() > 1 ? parts[1] : parts[0], h, 0.0));
  return {rx, ry};
}

bool InsideRoundedBorder(const BoxStyle& style, double w, double h, double ux, double uy) {
  static const char* const kCorners[4] = {
      "border-top-left-radius", "border-top-right-radius",
      "border-bottom-right-radius", "border-bottom-left-radius"};
  std::pair<double, double> r[4]{{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  bool any = false;
  for (int i = 0; i < 4; ++i) {
    r[i] = ParseCornerRadius(style, style.Get(kCorners[i]), w, h);
    if (r[i].first > 0.0 && r[i].second > 0.0) any = true;
  }
  if (!any) {
    const auto sh = style.Get("border-radius");
    if (!sh.empty()) {
      const auto slash = sh.find('/');
      const auto hparts = CssSplit(slash == std::string::npos ? sh : sh.substr(0, slash), ' ');
      const auto vparts = slash == std::string::npos ? hparts : CssSplit(sh.substr(slash + 1), ' ');
      if (!hparts.empty() && !vparts.empty()) {
        auto pick = [](const std::vector<std::string>& p, size_t i) -> const std::string& {
          if (i < p.size()) return p[i];
          if (i == 2 && p.size() >= 1) return p[0];
          if (i == 3 && p.size() >= 2) return p[1];
          return p[0];
        };
        for (int i = 0; i < 4; ++i) {
          r[i].first = std::max(0.0, Resolve(style, pick(hparts, i), w, 0.0));
          r[i].second = std::max(0.0, Resolve(style, pick(vparts, i), h, 0.0));
          if (r[i].first > 0.0 && r[i].second > 0.0) any = true;
        }
      }
    }
  }
  if (!any) return true;
  // CSS Backgrounds 3 §5.5: overlapping curves scale down proportionally.
  double f = 1.0;
  const double top_sum = r[0].first + r[1].first, bot_sum = r[3].first + r[2].first;
  const double left_sum = r[0].second + r[3].second, right_sum = r[1].second + r[2].second;
  if (top_sum > w && top_sum > 0) f = std::min(f, w / top_sum);
  if (bot_sum > w && bot_sum > 0) f = std::min(f, w / bot_sum);
  if (left_sum > h && left_sum > 0) f = std::min(f, h / left_sum);
  if (right_sum > h && right_sum > 0) f = std::min(f, h / right_sum);
  if (f < 1.0) {
    for (int i = 0; i < 4; ++i) { r[i].first *= f; r[i].second *= f; }
  }
  auto check_ellipse = [](double dx, double dy, double rx, double ry) {
    if (rx <= 0.0 || ry <= 0.0) return true;
    const double nx = dx / rx, ny = dy / ry;
    return nx * nx + ny * ny <= 1.0 + 1e-9;
  };
  if (ux < r[0].first && uy < r[0].second)
    return check_ellipse(r[0].first - ux, r[0].second - uy, r[0].first, r[0].second);
  if (ux > w - r[1].first && uy < r[1].second)
    return check_ellipse(ux - (w - r[1].first), r[1].second - uy, r[1].first, r[1].second);
  if (ux > w - r[2].first && uy > h - r[2].second)
    return check_ellipse(ux - (w - r[2].first), uy - (h - r[2].second), r[2].first, r[2].second);
  if (ux < r[3].first && uy > h - r[3].second)
    return check_ellipse(r[3].first - ux, uy - (h - r[3].second), r[3].first, r[3].second);
  return true;
}
}
BoxRect ProjectBoxRect(const std::vector<BoxNode>& nodes, const std::vector<BoxLayout>& boxes,
                       const BoxViewport& viewport, size_t target) {
  Matrix matrix{1,0,0,1,0,0};
  for(size_t i=target;i!=kNoBox;i=nodes[i].parent)
    matrix=Multiply(Transform(nodes[i].style,boxes[i].border),matrix);
  const auto& box=boxes[target].border;
  double xs[4],ys[4];
  for(unsigned i=0;i<4;++i) {
    const double x=box[0]+(i&1?box[2]:0), y=box[1]+(i&2?box[3]:0);
    xs[i]=matrix[0]*x+matrix[2]*y+matrix[4]-viewport.scroll_x;
    ys[i]=matrix[1]*x+matrix[3]*y+matrix[5]-viewport.scroll_y;
  }
  const auto x=std::minmax_element(xs,xs+4),y=std::minmax_element(ys,ys+4);
  // nsLayoutUtils clips transformed bounds to the coordinate range. A span
  // wider than nscoord_MAX is centered and shrunk, rather than overflowing.
  auto saturated=[](double low, double high) {
    const double limit=kBoxCoordMax/60;
    low=std::clamp(low,-limit,limit);high=std::clamp(high,-limit,limit);
    if(high-low>limit) {low+=(high-low-limit)*.5;high=low+limit;}
    return std::array<double,2>{low,high};
  };
  const auto horizontal=saturated(*x.first,*x.second),vertical=saturated(*y.first,*y.second);
  // Firefox DOMRect::SetLayoutRect quantizes app-unit edges to 1/65536 CSS px,
  // then stores each component through SetRect(float, float, float, float).
  auto edge=[](double value) { return std::floor(BoxRound(value)*65536+.5)/65536; };
  const double left=edge(horizontal[0]),top=edge(vertical[0]);
  return {double(float(left)),double(float(top)),double(float(edge(horizontal[1])-left)),
          double(float(edge(vertical[1])-top))};
}

bool HitBoxPoint(const std::vector<BoxNode>& nodes, const std::vector<BoxLayout>& boxes,
                 const BoxViewport& viewport, size_t target, double vx, double vy) {
  const auto& box = boxes[target].border;
  if (box[2] <= 0.0 || box[3] <= 0.0) return false;
  Matrix matrix{1, 0, 0, 1, 0, 0};
  for (size_t i = target; i != kNoBox; i = nodes[i].parent)
    matrix = Multiply(Transform(nodes[i].style, boxes[i].border), matrix);
  const double det = matrix[0] * matrix[3] - matrix[1] * matrix[2];
  if (!std::isfinite(det) || std::abs(det) < 1e-12) return false;
  const double px = vx + viewport.scroll_x - matrix[4];
  const double py = vy + viewport.scroll_y - matrix[5];
  const double lx = (matrix[3] * px - matrix[2] * py) / det;
  const double ly = (-matrix[1] * px + matrix[0] * py) / det;
  if (!(lx >= box[0] && lx < box[0] + box[2] && ly >= box[1] && ly < box[1] + box[3]))
    return false;
  return InsideRoundedBorder(nodes[target].style, box[2], box[3], lx - box[0], ly - box[1]);
}
}
