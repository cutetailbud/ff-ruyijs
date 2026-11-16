// Table layout in the separate-borders model with the UA defaults Firefox
// applies (border-spacing 2px, 1px cell padding): every column is as wide as
// its widest cell's max-content width, every row as tall as its tallest cell,
// and a definite table width spreads the extra width over the columns. Cells
// are placed by BoxFlow::Place with their content widths forced, so block
// sizing inside a cell stays the single box owner. Only the common
// table > row group > row > cell structure is modelled; there is no column
// balancing, no percentage cell widths and no collapsing borders (#42).
#include "domain/styling/box_layout_internal.h"

namespace ruyijs::domain::styling {
namespace {
struct TableCell {
  size_t node = 0, column = 0, span = 1;
  double extra = 0;  // padding + border of the cell
};
struct TableRow {
  size_t node = 0;
  std::vector<TableCell> cells;
  double height = 0;
};
bool RowLike(const BoxNode& node) {
  const auto& display = node.style.Slot(BoxKnown::Display);
  return display == "table-row-group" || display == "table-header-group" ||
         display == "table-footer-group";
}
}  // namespace

bool BoxFlow::TableChildren(size_t index, size_t positioned, unsigned depth, double* height, double* width,
                            bool table_definite_width) {
  const auto& style = nodes[index].style;
  const BoxRect content = boxes[index].content;
  double spacing = 2;  // UA: table { border-spacing: 2px; }
  if (const auto& text = style.Get("border-spacing"); !text.empty()) {
    const double parsed = BoxLength(style, "border-spacing", BoxAuto(), 2);
    if (std::isfinite(parsed)) spacing = std::max(0.0, parsed);
  }
  std::vector<TableRow> rows;
  const std::vector<size_t> groups{index};
  size_t columns = 0;
  std::vector<size_t> order;
  for (const auto group : groups) {
    for (const auto child : nodes[group].children) {
      const auto& node = nodes[child];
      if (BoxAbsolute(node) || node.style.Slot(BoxKnown::Display) == "none") continue;
      const auto& display = node.style.Slot(BoxKnown::Display);
      if (display == "table-row") order.push_back(child);
      else if (RowLike(node))
        for (const auto row : node.children)
          if (!BoxAbsolute(nodes[row]) && nodes[row].style.Slot(BoxKnown::Display) == "table-row")
            order.push_back(row);
    }
  }
  for (const auto row : order) {
    TableRow entry;
    entry.node = row;
    size_t column = 0;
    for (const auto child : nodes[row].children) {
      const auto& node = nodes[child];
      if (BoxAbsolute(node) || node.style.Slot(BoxKnown::Display) == "none") continue;
      const auto& display = node.style.Slot(BoxKnown::Display);
      if (display != "table-cell") continue;
      TableCell cell;
      cell.node = child;
      cell.column = column;
      cell.span = std::max<size_t>(1, std::min<size_t>(node.column_span, 1000));
      BoxLayout edges;
      BoxEdges(node, content[2], &edges);
      cell.extra = edges.padding[1] + edges.padding[3] + edges.edge[1] + edges.edge[3];
      column += cell.span;
      entry.cells.push_back(cell);
    }
    columns = std::max(columns, column);
    rows.push_back(std::move(entry));
  }
  if (rows.empty() || !columns) { *height = 0; *width = 0; return true; }
  // Column widths from the cells' max-content outer widths.
  std::vector<double> widths(columns, 0);
  for (const auto& row : rows)
    for (const auto& cell : row.cells) {
      const auto& node = nodes[cell.node];
      BoxLayout edges;
      BoxEdges(node, content[2], &edges);
      const double outer = Intrinsic(cell.node, false, depth) +
          edges.padding[1] + edges.padding[3] + edges.edge[1] + edges.edge[3] +
          edges.margin[1] + edges.margin[3];
      const double each = (outer - edges.margin[1] - edges.margin[3]) / double(cell.span);
      for (size_t c = 0; c < cell.span && cell.column + c < columns; ++c)
        widths[cell.column + c] = std::max(widths[cell.column + c], each);
    }
  const double gaps = spacing * double(columns + 1);
  double total = gaps;
  for (const auto value : widths) total += value;
  // A definite table width above the max-content total spreads the extra
  // equally, as Firefox distributes the assignable width.
  if (table_definite_width) {
    const double spare = content[2] - total;
    if (spare > 0) {
      for (auto& value : widths) value += spare / double(columns);
      total = content[2];
    }
  }
  for (auto& row : rows)
    for (const auto& cell : row.cells) {
      double cell_width = 0;
      for (size_t c = 0; c < cell.span; ++c) {
        cell_width += widths[cell.column + c];
        if (c + 1 < cell.span) cell_width += spacing;
      }
      if (cell.node < forced_width.size())
        forced_width[cell.node] = std::max(0.0, cell_width - cell.extra);
    }
  double y = content[1] + spacing;
  double widest = 0;
  for (auto& row : rows) {
    // The row's content box spans every column between the outer spacings.
    double row_width = 0;
    for (size_t c = 0; c < columns; ++c) row_width += widths[c] + (c ? spacing : 0);
    if (row.node < forced_width.size()) forced_width[row.node] = row_width;
    if (!Place(row.node, {content[0] + spacing, y, row_width, BoxAuto()}, false,
               content[0] + spacing, y, positioned, depth + 1)) return false;
    const double row_height = boxes[row.node].border[3];
    // Cells stretch to their row's height.
    for (const auto& cell : row.cells) {
      auto& box = boxes[cell.node];
      const double inner = std::max(0.0, row_height - (box.padding[0] + box.padding[2] +
                                                      box.edge[0] + box.edge[2]));
      if (!box.definite_height && cell.node < forced_height.size()) {
        forced_height[cell.node] = inner;
        const double x = box.content[0];
        if (!Place(cell.node, {content[0] + spacing, y, row_width, row_height}, true,
                   x, box.content[1], positioned, depth + 2)) return false;
      }
    }
    row.height = row_height;
    widest = std::max(widest, row_width);
    y += row_height + spacing;
  }
  *height = BoxRound(y - content[1]);
  *width = BoxRound(std::max(widest + 2 * spacing, total));
  return true;
}
}  // namespace ruyijs::domain::styling
