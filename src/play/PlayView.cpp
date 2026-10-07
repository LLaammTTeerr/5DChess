#include "play/PlayView.h"
#include <algorithm>
#include <cmath>
#include <rlgl.h>
#include "App.h"
#include "Input.h"
#include "Render/Motion.h"
#include "Render/UITheme.h"
#include "play/BoardScene.h"
#include "play/MotionOverlay.h"
#include "play/Paths.h"
#include "ui/Audit.h"
#include "ui/TextFit.h"

namespace play {

namespace {

using Chess::Core::Coord;
constexpr const char* kDot = "\xC2\xB7"; // middle dot (the UI fonts carry it)

Font monoFont(int size) { return App::current().assets.font("ui.mono", size); }
float textWidth(Font f, const std::string& s, float size) { return MeasureTextEx(f, s.c_str(), size, 0.0f).x; }
void drawText(Font f, const std::string& s, float x, float y, float size, Color c) {
  DrawTextEx(f, s.c_str(), {std::floor(x), std::floor(y)}, size, 0.0f, c);
}
void drawTextMid(Font f, const std::string& s, float cx, float y, float size, Color c) { drawText(f, s, cx - textWidth(f, s, size) / 2.0f, y, size, c); }
Rectangle ray(const Rect& r) { return toRay(r); }
Rect grow(const Rect& r, float by) { return {r.x - by, r.y - by, r.w + 2 * by, r.h + 2 * by}; }
bool overlaps(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

std::string cardLabel(int timeline, int halfTurn) { return timelineLabel(timeline) + " " + kDot + " " + boardLabel(halfTurn); }

// The matrix that maps a board's world rectangle onto its rectangle on screen: everything the multiverse view draws for a board
// (halo, card, squares, pieces, dots, outlines) then lands on the card without being told where it is.
class BoardSpace {
public:
  BoardSpace(const Rect& screen, const Rect& world) : _k(screen.w / world.w) {
    rlPushMatrix();
    rlTranslatef(screen.x, screen.y, 0.0f);
    rlScalef(_k, _k, 1.0f);
    rlTranslatef(-world.x, -world.y, 0.0f);
  }
  ~BoardSpace() { rlPopMatrix(); }
  BoardSpace(const BoardSpace&) = delete;
  BoardSpace& operator=(const BoardSpace&) = delete;
  float k() const { return _k; }

private:
  float _k;
};

BoardLook lookOf(const PlayViewFrame& f, BoardKey key, bool dim = false) {
  BoardLook look;
  look.enter = f.animator.enterProgress(key);
  if (const BoardInfo* info = f.view.board(key.first, key.second)) {
    look.role = info->role;
    look.inactive = info->inactive || dim;
    look.whiteToMove = info->whiteToMove;
  } else {
    look.inactive = dim;
    look.whiteToMove = key.second % 2 == 0;
  }
  look.blink = f.blink;
  look.blinkSeed = static_cast<unsigned>((key.first + 1000) * 7919 + key.second);
  if (const auto from = f.selection.from(); from && keyOf(*from) == key) look.hide(from->x, from->y);
  for (const auto& fl : f.animator.flights())
    if (fl.spec.to == key && !fl.move.done()) look.hide(fl.spec.toX, fl.spec.toY);
  for (const auto& c : f.view.checks)
    if (c.king.l == key.first && c.king.t == key.second) look.markChecked(c.king.x, c.king.y);
  return look;
}

// A curved path between two points on screen, bowed upwards in proportion to the distance
void arcPath(path::Poly& poly, Vector2 a, Vector2 b) {
  const float dist = std::hypot(b.x - a.x, b.y - a.y);
  const float h = std::min(130.0f, 26.0f + dist * 0.20f);
  const Vector2 c0 = {a.x + (b.x - a.x) * 0.25f, a.y + (b.y - a.y) * 0.25f - h};
  const Vector2 c1 = {a.x + (b.x - a.x) * 0.75f, a.y + (b.y - a.y) * 0.75f - h};
  path::bezier(poly, a, c0, c1, b, 28);
}

void arrowHead(const path::Poly& poly, float size, Color color) {
  if (poly.n < 2) return;
  const Vector2 tip = poly.p[poly.n - 1], before = poly.p[poly.n - 2];
  const float len = std::max(1.0f, std::hypot(tip.x - before.x, tip.y - before.y));
  const Vector2 dir = {(tip.x - before.x) / len, (tip.y - before.y) / len}, side = {-dir.y, dir.x};
  const Vector2 l = {tip.x - dir.x * size + side.x * size * 0.6f, tip.y - dir.y * size + side.y * size * 0.6f};
  const Vector2 r = {tip.x - dir.x * size - side.x * size * 0.6f, tip.y - dir.y * size - side.y * size * 0.6f};
  DrawTriangle(tip, l, r, color);
  DrawTriangle(tip, r, l, color);
}

Vector2 centreOf(const Rect& r) { return {r.centerX(), r.centerY()}; }

struct Chrome {
  const PlayViewLayout& layout;
  const PlayViewFrame& f;
  const BoardStyle& st;
  Vector2 mouse;
};

Color chipFill(Chip chip, const BoardStyle& st) {
  switch (chip) {
    case Chip::MustMove: return st.mandatory;
    case Chip::Optional: return {88, 112, 62, 255};
    case Chip::Moved: return {46, 122, 108, 255};
    case Chip::Waiting: return {112, 108, 100, 255};
  }
  return st.muted;
}

// The label strip of a card: "L+1 . T5w" at the left, a chip at the right
void drawFooter(const Chrome& c, const Rect& card, float k, const std::string& label, const std::string& chip, Color chipColor, bool whiteToMove,
                bool inactive) {
  const BoardStyle& st = c.st;
  const float footer = BoardLayout::kCardFooter * k;
  const float size = footer >= 17.0f ? 12.0f : 11.0f;
  const Font font = monoFont(static_cast<int>(size));
  const float a = inactive ? 0.55f : 1.0f;
  Color color;
  switch (st.card) {
    case BoardStyle::Card::Paper: color = whiteToMove ? st.muted : Color{232, 220, 196, 255}; break;
    case BoardStyle::Card::Glow: color = st.hudMuted; break;
    default: color = whiteToMove ? st.ink : WHITE; break;
  }
  const float top = card.y + card.h - footer;
  float chipW = 0.0f;
  if (!chip.empty()) {
    const Font cf = monoFont(10);
    chipW = textWidth(cf, chip, 10.0f) + 10.0f;
    const Rectangle r = {std::floor(card.x + card.w - chipW - 6.0f * k - 2.0f), std::floor(top + (footer - 15.0f) / 2.0f - 1.0f), chipW, 15.0f};
    drawRoundedRect(r, 7.5f, fade(chipColor, a));
    drawText(cf, chip, r.x + 5.0f, r.y + 2.5f, 10.0f, fade(WHITE, a));
    if (ui::audit::enabled()) ui::audit::fit("card chip", chip, textWidth(cf, chip, 10.0f), 10.0f, {r.x, r.y, r.width, r.height});
  }
  const float left = 8.0f * k + 2.0f;
  const float room = card.w - left - chipW - 14.0f * k - 4.0f;
  const std::string shown = ui::ellipsized(label, room, [&](const std::string& t) { return textWidth(font, t, size); });
  drawText(font, shown, card.x + left, top + (footer - size) / 2.0f - 1.0f, size, fade(color, a));
  if (ui::audit::enabled()) ui::audit::fit("play card label", shown, textWidth(font, shown, size), size, {0, 0, std::max(room, 1.0f), footer});
}

void drawPlaceholder(const Chrome& c, const Rect& card) {
  const BoardStyle& st = c.st;
  const Rectangle r = ray(card);
  path::Poly poly;
  poly.add({r.x, r.y});
  poly.add({r.x + r.width, r.y});
  poly.add({r.x + r.width, r.y + r.height});
  poly.add({r.x, r.y + r.height});
  poly.add({r.x, r.y});
  path::stroke(poly, 1.0f, fade(st.hudMuted, 0.55f), 1.5f, 7.0f, 5.0f);
  const Font font = monoFont(12);
  const char* lines[] = {"Jump targets", "and history", "show here"};
  float y = r.y + r.height / 2.0f - 30.0f;
  for (const char* line : lines) {
    drawTextMid(font, line, r.x + r.width / 2.0f, y, 12.0f, fade(st.hudMuted, 0.85f));
    y += 18.0f;
  }
}

// Draws everything one board has on top of it, in the board's world space (the BoardSpace matrix is in force)
void drawBoardOverlays(const PlayViewFrame& f, BoardKey key, float k) {
  const BoardStyle& st = f.style;
  const int dim = f.game.dim();
  const Rect world = BoardLayout::boardRect(key.first, key.second);
  auto squareOf = [&](const Coord& c) { return BoardLayout::squareRect(world, dim, c.x, c.y); };
  const MoveAnimator& a = f.animator;
  const auto from = f.selection.from();
  const bool gray = st.grayPieces;

  if (from && keyOf(*from) == key) drawBoardOutline(world, st, k);
  const auto& fading = a.hoverFading();
  if (fading.alpha > 0.0f && fading.key == key) drawHoverSquare(BoardLayout::squareRect(world, dim, fading.x, fading.y), fading.alpha, st);
  const auto& hover = a.hoverNow();
  if (hover.alpha > 0.0f && f.hover && keyOf(*f.hover) == key) drawHoverSquare(squareOf(*f.hover), hover.alpha, st);

  if (from) {
    const auto& targets = f.selection.targets();
    for (size_t i = 0; i < targets.size(); ++i) {
      if (keyOf(targets[i]) != key) continue;
      const bool occupied = f.game.board(targets[i].l, targets[i].t).at(Chess::Position2D(targets[i].x, targets[i].y)).has_value();
      drawLegalTarget(squareOf(targets[i]), occupied, a.dotScale(i), st);
    }
    if (keyOf(*from) == key) {
      const auto piece = f.game.board(from->l, from->t).at(Chess::Position2D(from->x, from->y));
      drawSelectedSquare(squareOf(*from), k, st);
      const BoardInfo* info = f.view.board(from->l, from->t);
      if (piece) drawLiftedPiece(squareOf(*from), pieceKey(*piece), a.lift(), gray || (info && info->inactive));
    }
  }
  if (f.highlight && keyOf(*f.highlight) == key) drawSelectedSquare(squareOf(*f.highlight), k, st);

  const auto flash = a.rejectFlash();
  if (flash.alpha > 0.0f && flash.key == key) overlay::drawRejectFlash(BoardLayout::squareRect(world, dim, flash.x, flash.y), flash.alpha);
  if (!from) { // what the piece under the pointer could do (the arcs between cards are drawn separately)
    for (const MoveAnimator::PreviewLayer* layer : {&a.previewFading(), &a.previewNow()}) {
      MoveAnimator::PreviewLayer here = *layer;
      here.targets.clear();
      for (const Coord& t : layer->targets)
        if (keyOf(t) == key) here.targets.push_back(t);
      if (!here.targets.empty()) overlay::drawMovePreview(here, f.game, st, k, false);
    }
  }
  std::vector<Coord> kings;
  for (const Coord& king : a.checkedKings())
    if (keyOf(king) == key) kings.push_back(king);
  if (!kings.empty()) overlay::drawCheckPulse(kings, a.checkPulseNow(), dim, st, k);
}

// One board of the grid or the inspector: halo first (for all boards before any card), then the card and what lies on it
void haloOf(const Chrome& c, BoardKey key, const Rect& boardPx, bool dim = false) {
  const Rect world = BoardLayout::boardRect(key.first, key.second);
  BoardSpace space(boardPx, world);
  overlay::BoardShift shift(c.f.animator.boardOffset(key, space.k()));
  play::drawBoardHalo(world, lookOf(c.f, key, dim), c.st, c.f.soft);
  const auto source = c.f.animator.sourceHalo();
  if (source.alpha > 0.0f && source.key.first == key.first) overlay::drawBoardGlow(world, source.alpha, c.st, c.f.soft, true, space.k());
  const auto& glow = c.f.animator.targetGlow();
  if (glow.alpha > 0.0f && glow.key == key) overlay::drawBoardGlow(world, glow.alpha, c.st, c.f.soft, false, space.k());
}

void drawBoardBody(const Chrome& c, BoardKey key, const Rect& boardPx, bool dim = false) {
  const Rect world = BoardLayout::boardRect(key.first, key.second);
  BoardSpace space(boardPx, world);
  overlay::BoardShift shift(c.f.animator.boardOffset(key, space.k()));
  play::drawBoard(c.f.game.board(key.first, key.second), world, lookOf(c.f, key, dim), c.st, space.k());
  drawBoardOverlays(c.f, key, space.k());
}

void drawRing(const Chrome& c, const Rect& card, float k, float gap, float thickness, Color color) {
  const Rectangle r = ray(grow(card, gap));
  if (c.st.card == BoardStyle::Card::Ink) DrawRectangleLinesEx(r, thickness, color);
  else drawRoundedLines(r, (12.0f + gap) * k, thickness, color);
}

// A dashed frame: what is not a live card (a past or inactive board in the inspector)
void drawDashedRing(const Rect& card, float gap, float thickness, Color color) {
  const Rect r = grow(card, gap);
  path::Poly poly;
  poly.add({r.x, r.y});
  poly.add({r.x + r.w, r.y});
  poly.add({r.x + r.w, r.y + r.h});
  poly.add({r.x, r.y + r.h});
  poly.add({r.x, r.y});
  path::stroke(poly, 1.0f, color, thickness, 8.0f, 5.0f);
}

// ---------------------------------------------------------------------------------------------------------------------

void drawHistoryStack(const Chrome& c, const PlayCard& card) {
  if (card.history <= 0) return;
  const BoardStyle& st = c.st;
  const Rect h = card.histRect;
  const float ts = std::max(20.0f, h.w - 8.0f);
  const Rect thumb = {h.x + 1.0f, card.board.y, ts, ts};
  const bool hot = !ui::pointerConsumed() && h.contains(c.mouse.x, c.mouse.y);
  // the stack: one or two cards peeking out behind the newest board
  const int layers = std::min(2, card.history - 1);
  for (int i = layers; i >= 1; --i) {
    const Rectangle back = {thumb.x + 3.0f * static_cast<float>(i), thumb.y - 3.0f * static_cast<float>(i), thumb.w, thumb.h};
    drawRoundedRect(back, 3.0f, fade(st.cardWhite, 0.9f));
    drawRoundedLines(back, 3.0f, 1.0f, fade(st.cardEdgeWhite, 0.9f));
  }
  const BoardKey key{card.timeline, card.halfTurn - 1};
  const Rect world = BoardLayout::boardRect(key.first, key.second);
  {
    BoardSpace space(thumb, world);
    BoardLook look;
    look.whiteToMove = key.second % 2 == 0;
    if (const BoardInfo* info = c.f.view.board(key.first, key.second)) look.inactive = info->inactive;
    play::drawBoard(c.f.game.board(key.first, key.second), world, look, st, space.k());
  }
  if (hot) drawRoundedLines(ray(grow(thumb, 2.0f)), 4.0f, 2.0f, st.accent);
  const Font font = monoFont(12);
  std::string caption = boardLabel(key.second);
  if (card.history > 1) caption += " +" + std::to_string(card.history - 1);
  const std::string shown = ui::ellipsized(caption, h.w + 2.0f, [&](const std::string& t) { return textWidth(font, t, 12.0f); });
  drawTextMid(font, shown, h.x + h.w / 2.0f, thumb.y + thumb.h + 5.0f, 12.0f, hot ? st.accent : st.hudMuted);
  if (hot && h.w >= 52.0f) drawTextMid(font, "history", h.x + h.w / 2.0f, thumb.y - 15.0f, 12.0f, st.accent);
  ui::tooltip(ray(h), "History of " + timelineLabel(card.timeline) + ": " + std::to_string(card.history) + (card.history == 1 ? " board" : " boards") +
                          ". Click to read it (E)");
  if (ui::audit::enabled()) {
    ui::audit::fit("history caption", shown, textWidth(font, shown, 12.0f), 12.0f, {0, 0, h.w + 2.0f, 17.0f});
    ui::audit::rect("history " + timelineLabel(card.timeline), ray(h), ui::audit::Kind::Card);
  }
}

void drawPlayCard(const Chrome& c, const PlayCard& card, bool cursor) {
  const BoardStyle& st = c.st;
  const BoardKey key{card.timeline, card.halfTurn};
  const float k = card.board.w / BoardLayout::kBoardSize;
  drawBoardBody(c, key, card.board);
  const bool moved = card.chip == Chip::Moved;
  if (moved && c.f.showChips) drawRing(c, card.card, k, 2.0f, 1.5f, fade(chipFill(Chip::Moved, st), 0.9f));
  if (cursor) drawRing(c, card.card, k, 3.5f, 3.0f, st.accent2);
  const BoardInfo* info = c.f.view.board(card.timeline, card.halfTurn);
  drawFooter(c, card.card, k, cardLabel(card.timeline, card.halfTurn), c.f.showChips ? chipLabel(card.chip) : "", chipFill(card.chip, st),
             card.whiteToMove, info && info->inactive);
  if (ui::audit::enabled()) ui::audit::rect("card " + timelineLabel(card.timeline), ray(card.card), ui::audit::Kind::Card);
  drawHistoryStack(c, card);
}

void drawTab(const Chrome& c, const InspectorTab& tab, bool twoLines) {
  const BoardStyle& st = c.st;
  const bool hot = !ui::pointerConsumed() && tab.rect.contains(c.mouse.x, c.mouse.y);
  const Rectangle r = ray(tab.rect);
  drawRoundedRect(r, 6.0f, tab.current ? st.accent : st.hudFill);
  drawRoundedLines(r, 6.0f, hot ? 2.0f : 1.0f, hot ? st.accent2 : st.hudBorder);
  const Font font = monoFont(12);
  const Color text = tab.current ? WHITE : st.hudText;
  if (twoLines) {
    drawTextMid(font, timelineLabel(tab.key.first), r.x + r.width / 2.0f, r.y + 4.0f, 12.0f, text);
    drawTextMid(font, boardLabel(tab.key.second), r.x + r.width / 2.0f, r.y + 18.0f, 12.0f, text);
  } else {
    drawTextMid(font, boardLabel(tab.key.second), r.x + r.width / 2.0f, r.y + (r.height - 12.0f) / 2.0f - 1.0f, 12.0f, text);
  }
  if (ui::audit::enabled()) {
    const std::string longest = twoLines ? timelineLabel(tab.key.first) : boardLabel(tab.key.second);
    ui::audit::fit("inspector tab", longest, textWidth(font, longest, 12.0f), 12.0f, {0, 0, r.width, r.height});
    ui::audit::rect("tab " + timelineLabel(tab.key.first) + " " + boardLabel(tab.key.second), r, ui::audit::Kind::Button);
  }
}

void drawInspector(const Chrome& c) {
  const PlayViewLayout& layout = c.layout;
  const BoardStyle& st = c.st;
  if (!layout.inspectorVisible()) return;
  const auto key = layout.inspectorKey();
  const Rect card = layout.inspectorCard();
  const float k = layout.inspectorBoard().w / BoardLayout::kBoardSize;
  if (key && c.f.game.boardExists(key->first, key->second)) {
    // Not a live card: the board is dimmed, its frame dashed and its label says what it is
    drawBoardBody(c, *key, layout.inspectorBoard(), true);
    const bool targets = layout.mode() == PlayViewLayout::Mode::Targets;
    drawDashedRing(card, 2.0f, 2.0f, targets ? st.accent2 : Color{74, 98, 140, 255});
    const BoardInfo* info = c.f.view.board(key->first, key->second);
    const TimelineInfo* timeline = c.f.view.timeline(key->first);
    const bool past = timeline && key->second < timeline->lastHalfTurn, inactive = info && info->inactive;
    const std::string what = past ? "past" : inactive ? "inactive" : "";
    drawFooter(c, card, k, cardLabel(key->first, key->second) + (what.empty() ? "" : " (" + what + ")"),
               targets ? "jump target" : what.empty() ? "board" : what, targets ? st.accent : Color{74, 98, 140, 255}, key->second % 2 == 0, false);
  } else {
    drawPlaceholder(c, card);
  }
  if (ui::audit::enabled()) ui::audit::rect("inspector", ray(card), ui::audit::Kind::Card);

  const Rect col = layout.inspectorTabs();
  if (!layout.tabs().empty() || layout.closeRect().w > 0.0f) {
    if (layout.closeRect().w > 0.0f) {
      const Rectangle r = ray(layout.closeRect());
      const bool hot = !ui::pointerConsumed() && layout.closeRect().contains(c.mouse.x, c.mouse.y);
      drawRoundedRect(r, 6.0f, st.hudFill);
      drawRoundedLines(r, 6.0f, hot ? 2.0f : 1.0f, hot ? st.accent2 : st.hudBorder);
      drawTextMid(monoFont(12), "close", r.x + r.width / 2.0f, r.y + (r.height - 12.0f) / 2.0f - 1.0f, 12.0f, st.hudText);
      if (ui::audit::enabled()) ui::audit::rect("inspector close", r, ui::audit::Kind::Button);
    }
    const bool two = layout.mode() == PlayViewLayout::Mode::Targets;
    for (const InspectorTab& tab : layout.tabs()) drawTab(c, tab, two);
    if (ui::audit::enabled()) ui::audit::rect("inspector tabs", ray(col), ui::audit::Kind::Card);
  } else if (ui::audit::enabled()) {
    ui::audit::rect("inspector tabs", ray(col), ui::audit::Kind::Card);
  }
}

void drawInactiveRow(const Chrome& c) {
  const PlayViewLayout& layout = c.layout;
  if (layout.inactiveRow().h <= 0.0f) return;
  const BoardStyle& st = c.st;
  const Rect row = layout.inactiveRow();
  const Font font = monoFont(12);
  drawText(font, "Inactive", row.x + 4.0f, row.y + (row.h - 12.0f) / 2.0f - 1.0f, 12.0f, st.hudMuted);
  if (ui::audit::enabled()) ui::audit::rect("inactive row", ray(row), ui::audit::Kind::Card);
  int hidden = 0;
  for (const InactiveChip& chip : layout.inactive()) {
    if (chip.rect.w <= 0.0f) {
      ++hidden;
      continue;
    }
    const bool hot = !ui::pointerConsumed() && chip.rect.contains(c.mouse.x, c.mouse.y);
    const bool current = layout.inspectorKey() && *layout.inspectorKey() == std::make_pair(chip.timeline, chip.halfTurn);
    const Rectangle r = ray(chip.rect);
    drawRoundedRect(r, r.height / 2.0f, current ? st.accent : st.hudFill);
    drawRoundedLines(r, r.height / 2.0f, hot ? 2.0f : 1.0f, hot ? st.accent2 : st.hudBorder);
    const std::string label = cardLabel(chip.timeline, chip.halfTurn);
    const std::string shown = ui::ellipsized(label, r.width - 12.0f, [&](const std::string& t) { return textWidth(font, t, 12.0f); });
    drawTextMid(font, shown, r.x + r.width / 2.0f, r.y + (r.height - 12.0f) / 2.0f - 1.0f, 12.0f, current ? WHITE : st.hudText);
    if (ui::audit::enabled()) {
      ui::audit::fit("inactive chip", shown, textWidth(font, shown, 12.0f), 12.0f, {0, 0, r.width - 8.0f, r.height});
      ui::audit::rect("inactive " + timelineLabel(chip.timeline), r, ui::audit::Kind::Button);
    }
    ui::tooltip(r, "Inactive timeline " + timelineLabel(chip.timeline) + ": click to look at its board");
  }
  if (hidden > 0) {
    const std::string more = "+" + std::to_string(hidden);
    drawText(font, more, row.x + row.w - textWidth(font, more, 12.0f) - 4.0f, row.y + (row.h - 12.0f) / 2.0f - 1.0f, 12.0f, st.hudMuted);
  }
}

// Where a pending (or last turn's) jump ends: the card of the timeline it landed on
const PlayCard* jumpDest(const PlayViewLayout& layout, const MultiverseView& view, const Chess::Core::Move& move) {
  const PlayCard* dest = layout.card(move.to.l);
  if (!dest || dest->halfTurn != move.to.t + 1) { // a jump into the past starts a timeline of its own
    dest = nullptr;
    for (const TimelineInfo& t : view.timelines)
      if (t.created && t.parent == move.to.l && (t.forkHalfTurn == move.to.t || t.forkHalfTurn == move.to.t + 1))
        if (const PlayCard* cand = layout.card(t.id)) dest = cand;
  }
  return dest;
}

void drawArcs(const Chrome& c) {
  const BoardStyle& st = c.st;
  const PlayViewLayout& layout = c.layout;
  const PlayViewFrame& f = c.f;
  // the jumps of the pending turn (solid); last turn's only while the pointer is on a card they join, so they do not cut across pieces
  for (const JumpInfo& j : f.view.jumps) {
    const PlayCard* src = layout.card(j.move.from.l);
    const PlayCard* dst = jumpDest(layout, f.view, j.move);
    if (!src || !dst) continue;
    if (!j.pending && !(src->card.contains(c.mouse.x, c.mouse.y) || dst->card.contains(c.mouse.x, c.mouse.y))) continue;
    const Rect a = BoardLayout::squareRect(src->board, layout.dim(), j.move.from.x, j.move.from.y);
    const Rect b = BoardLayout::squareRect(dst->board, layout.dim(), j.move.to.x, j.move.to.y);
    path::Poly poly;
    arcPath(poly, centreOf(a), centreOf(b));
    const float alpha = j.pending ? 0.85f : 0.6f;
    path::stroke(poly, 1.0f, fade(st.accent, alpha), 2.2f);
    arrowHead(poly, 8.0f, fade(st.accent, alpha));
  }
  // nothing lifted, the pointer rests on a piece: a dashed arc to every other board its moves lead to
  if (!f.selection.from()) {
    for (const MoveAnimator::PreviewLayer* layer : {&f.animator.previewFading(), &f.animator.previewNow()}) {
      if (!layer->valid || layer->alpha <= 0.0f) continue;
      const auto a = layout.squareRect(layer->from);
      if (!a) continue;
      std::vector<BoardKey> done;
      for (const Coord& t : layer->targets) {
        const BoardKey key = keyOf(t);
        if (key == keyOf(layer->from) || std::find(done.begin(), done.end(), key) != done.end() || done.size() >= 4) continue;
        done.push_back(key);
        if (const auto b = layout.squareRect(t)) {
          path::Poly poly;
          arcPath(poly, centreOf(*a), centreOf(*b));
          path::stroke(poly, 1.0f, fade(st.accent, 0.6f * std::min(1.0f, layer->alpha)), 2.0f, 7.0f, 6.0f);
        }
      }
    }
  }
  // the lifted piece: a dashed arc to every other board it can reach, and the live arc to the one under the pointer
  if (const auto from = f.selection.from()) {
    if (const auto a = layout.squareRect(*from)) {
      std::vector<BoardKey> done;
      for (const Coord& t : f.selection.targets()) {
        const BoardKey key = keyOf(t);
        if (key == keyOf(*from) || std::find(done.begin(), done.end(), key) != done.end() || done.size() >= 5) continue;
        done.push_back(key);
        if (const auto b = layout.squareRect(t)) {
          path::Poly poly;
          arcPath(poly, centreOf(*a), centreOf(*b));
          path::stroke(poly, 1.0f, fade(st.accent, 0.6f), 2.0f, 7.0f, 6.0f);
        }
      }
    }
    if (const auto* arc = f.animator.liveArc()) {
      const auto a = layout.squareRect(arc->from), b = layout.squareRect(arc->to);
      if (a && b) {
        path::Poly poly;
        arcPath(poly, centreOf(*a), centreOf(*b));
        path::stroke(poly, arc->grow, fade(st.accent, 0.28f), 7.0f);
        path::stroke(poly, arc->grow, fade(st.accent, 0.9f), 2.4f, 9.0f, 6.0f);
        if (arc->grow > 0.97f) arrowHead(poly, 11.0f, fade(st.accent, 0.9f));
      }
    }
  }
}

// Pieces in flight, between the squares of the cards (the animator's own endpoints are in the multiverse's world)
void drawFlightsOnCards(const Chrome& c) {
  std::vector<MoveAnimator::Flight> flights;
  for (const auto& fl : c.f.animator.flights()) {
    const auto a = c.layout.squareOnTimeline(fl.spec.from.first, fl.spec.fromX, fl.spec.fromY);
    const auto b = c.layout.squareOnTimeline(fl.spec.to.first, fl.spec.toX, fl.spec.toY);
    if (!a || !b) continue;
    MoveAnimator::Flight copy = fl;
    copy.from = centreOf(*a);
    copy.to = centreOf(*b);
    copy.size = b->w;
    copy.arcHeight = fl.spec.from != fl.spec.to ? std::min(90.0f, 14.0f + 0.2f * std::hypot(copy.to.x - copy.from.x, copy.to.y - copy.from.y)) : 0.0f;
    flights.push_back(std::move(copy));
  }
  drawFlights(flights, c.st.grayPieces);
}

} // namespace

void drawPlayView(const PlayViewLayout& layout, const PlayViewFrame& f) {
  const BoardStyle& st = f.style;
  const Chrome c{layout, f, st, Input::mousePosition()};

  // Cards are clipped to the grid area (a little extra at the sides and bottom for halos) while it scrolls
  const Rect g = layout.gridArea();
  const float pad = layout.scrolls() ? 0.0f : 14.0f;
  BeginScissorMode(static_cast<int>(g.x - 14.0f), static_cast<int>(g.y - pad), static_cast<int>(g.w + 28.0f), static_cast<int>(g.h + pad + 14.0f));

  std::vector<const PlayCard*> shown;
  for (const PlayCard& card : layout.cards())
    if (overlaps(grow(card.card, 40.0f), g) && !(layout.coveredTimeline() && *layout.coveredTimeline() == card.timeline)) shown.push_back(&card);
  const bool inspectorShown = layout.inspectorVisible() && layout.inspectorKey() && f.game.boardExists(layout.inspectorKey()->first, layout.inspectorKey()->second) &&
                              overlaps(layout.inspectorCell(), g);
  for (const PlayCard* card : shown) haloOf(c, {card->timeline, card->halfTurn}, card->board);
  if (inspectorShown) haloOf(c, *layout.inspectorKey(), layout.inspectorBoard(), true);
  for (const PlayCard* card : shown) drawPlayCard(c, *card, f.cursor && *f.cursor == card->timeline);
  drawInspector(c);
  drawArcs(c);
  drawFlightsOnCards(c);
  EndScissorMode();

  drawInactiveRow(c);
}

} // namespace play
