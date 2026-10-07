#include "play/TurnPanel.h"
#include <algorithm>
#include <cmath>
#include "Input.h"
#include "Render/UITheme.h"
#include "play/BoardLayout.h"
#include "play/BoardRenderer.h"
#include "play/Hud.h"
#include "ui/Audit.h"
#include "ui/TextFit.h"

namespace play {

namespace {

constexpr float kPad = 14.0f;      // inside the panel
constexpr float kRowH = 48.0f;     // one board of the checklist
constexpr float kThumbH = 40.0f;   // height of a thumbnail (a whole board card)
constexpr float kThumbW = kThumbH * (BoardLayout::kBoardSize + 2 * BoardLayout::kCardPad) /
                          (BoardLayout::kBoardSize + 2 * BoardLayout::kCardPad + BoardLayout::kCardFooter);
constexpr float kLineH = 26.0f;    // one move of the opponent's last turn
constexpr float kHeaderH = 26.0f;
constexpr float kScrollbarW = 6.0f, kFadeH = 18.0f;
constexpr float kHudButtonW = 124.0f;

bool s_wanted = true;

float textW(::Font font, const std::string& s, float size) { return MeasureTextEx(font, s.c_str(), size, 0.0f).x; }

::Color withAlpha(::Color c, float a) { c.a = static_cast<unsigned char>(static_cast<float>(c.a) * std::clamp(a, 0.0f, 1.0f)); return c; }

/// Draws `text` cut to `maxW` at (x, y) and reports it to the layout audit against `bounds`.
float drawFitted(const char* what, ::Font font, float size, const std::string& text, float x, float y, float maxW, ::Color color, Rectangle bounds) {
  const std::string shown = ui::ellipsized(text, maxW, [&](const std::string& t) { return textW(font, t, size); });
  DrawTextEx(font, shown.c_str(), {std::floor(x), std::floor(y)}, size, 0.0f, color);
  const float w = textW(font, shown, size);
  if (ui::audit::enabled()) ui::audit::within(what, shown, {std::floor(x), std::floor(y), w, size}, bounds);
  return w;
}

void drawRoundedFill(Rectangle r, float radius, ::Color fill, ::Color line, float thickness) {
  const float roundness = radius * 2.0f / std::min(r.width, r.height);
  DrawRectangleRounded(r, roundness, 8, fill);
  if (thickness > 0.0f) DrawRectangleRoundedLinesEx(r, roundness, 8, thickness, line);
}

void drawCheck(float x, float y, float size, ::Color color) {
  DrawLineEx({x, y + size * 0.55f}, {x + size * 0.38f, y + size * 0.9f}, 2.0f, color);
  DrawLineEx({x + size * 0.38f, y + size * 0.9f}, {x + size, y + size * 0.12f}, 2.0f, color);
}

const char* chipText(RowState s) {
  switch (s) {
    case RowState::MustMove: return "must move";
    case RowState::Optional: return "optional";
    case RowState::Moved: return "moved";
    case RowState::Waiting: return "waiting";
  }
  return "";
}

// The second line of a row that has no move yet
const char* rowNote(const ChecklistRow& r) {
  switch (r.state) {
    case RowState::MustMove: return "Needs a move";
    case RowState::Optional: return r.inactive ? "Inactive timeline" : "You may move here";
    case RowState::Moved: return "";
    case RowState::Waiting: return r.halfTurn % 2 == 0 ? "Waiting for White" : "Waiting for Black";
  }
  return "";
}

::Color stateColor(RowState s, const BoardStyle& st) {
  switch (s) {
    case RowState::MustMove: return st.mandatory;
    case RowState::Optional: return st.optional;
    case RowState::Moved: return st.hudText;
    case RowState::Waiting: return st.hudMuted;
  }
  return st.hudText;
}

BoardRole roleOf(RowState s) {
  return s == RowState::MustMove ? BoardRole::Mandatory : s == RowState::Optional ? BoardRole::Optional : BoardRole::Past;
}

// Rows that run past the viewport fade out into the panel instead of being cut through their text
void drawFade(Rectangle view, float scroll, float maxScroll, ::Color panelFill) {
  if (maxScroll <= 0.0f) return;
  const ::Color clear = {panelFill.r, panelFill.g, panelFill.b, 0};
  if (scroll < maxScroll - 0.5f)
    DrawRectangleGradientV(static_cast<int>(view.x), static_cast<int>(view.y + view.height - kFadeH), static_cast<int>(view.width), static_cast<int>(kFadeH), clear, panelFill);
  if (scroll > 0.5f) DrawRectangleGradientV(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width), static_cast<int>(kFadeH), panelFill, clear);
}

Rectangle scrollbarOf(Rectangle view) { return {view.x + view.width - kScrollbarW, view.y, kScrollbarW, view.height}; }

Rectangle handleOf(Rectangle view, float scroll, float maxScroll, float content) {
  const Rectangle bar = scrollbarOf(view);
  const float h = std::max(20.0f, view.height / content * bar.height);
  return {bar.x, bar.y + (maxScroll <= 0.0f ? 0.0f : scroll / maxScroll * (bar.height - h)), bar.width, h};
}

} // namespace

bool TurnPanel::wanted() { return s_wanted; }
void TurnPanel::setWanted(bool open) { s_wanted = open; }

int TurnPanel::List::rowAt(Vector2 p) const {
  if (rowH <= 0.0f || !CheckCollisionPointRec(p, view)) return -1;
  const int i = static_cast<int>(std::floor((p.y - view.y + scroll) / rowH));
  return i >= 0 && i < count ? i : -1;
}

void TurnPanel::setChecklist(TurnChecklist checklist) {
  _checklist = std::move(checklist);
  layout();
}

// Everything flows from the top of the panel: header, the boards, the Submit summary, the opponent's last turn. The panel is as tall as
// that needs (up to the controls bar); the boards list gives way first, whole rows at a time, and scrolls.
void TurnPanel::layout() {
  const float screenW = static_cast<float>(GetScreenWidth()), screenH = static_cast<float>(GetScreenHeight());
  const float x = screenW - kRight - kWidth;
  const float inner = kWidth - 2.0f * kPad;
  const float bottom = screenH - kBottom - kPad; // the lowest a line may end
  float y = kTop + kPad;
  _geo.header = {x + kPad, y, inner, kHeaderH};
  y += kHeaderH + 8.0f;

  const bool over = _checklist.over; // a decided game: the header and the last turn only
  const int nRows = static_cast<int>(_checklist.rows.size());
  const int nLines = static_cast<int>(_checklist.last.size());
  const float submitBlock = over ? 0.0f : 8.0f + 1.0f + 8.0f + 24.0f + 20.0f;
  const float lastHead = 8.0f + 1.0f + 8.0f + 24.0f + 4.0f;
  const float minLines = static_cast<float>(std::clamp(nLines, 1, 4)) * kLineH;
  const int rowsFit = std::max(1, static_cast<int>(std::floor((bottom - y - submitBlock - lastHead - minLines) / kRowH)));
  const float rowsH = static_cast<float>(std::min(nRows, rowsFit)) * kRowH;

  _rows.rowH = kRowH;
  _rows.count = nRows;
  _rows.view = {x + kPad, y, inner, rowsH};
  _rows.scroll = std::clamp(_rows.scroll, 0.0f, _rows.maxScroll());
  y += rowsH;

  if (over) {
    _geo.submit = _geo.submitNote = {};
    y -= 8.0f; // (the gap under the header: the rule under it follows at once)
  } else {
    _geo.dividerA = y + 8.0f;
    _geo.submit = {x + kPad, y + 8.0f + 1.0f + 8.0f, inner, 24.0f};
    _geo.submitNote = {x + kPad, _geo.submit.y + 24.0f, inner, 20.0f};
    y = _geo.submitNote.y + 20.0f;
  }

  _geo.dividerB = y + 8.0f;
  _geo.lastHeader = {x + kPad, _geo.dividerB + 1.0f + 8.0f, inner, 24.0f};
  y = _geo.lastHeader.y + 24.0f + 4.0f;

  const int linesFit = std::max(1, static_cast<int>(std::floor((bottom - y) / kLineH)));
  const float linesH = static_cast<float>(std::max(1, std::min(nLines, linesFit))) * kLineH; // (no moves: one line says so)
  _lines.rowH = kLineH;
  _lines.count = nLines;
  _lines.view = {x + kPad, y, inner, linesH};
  _lines.scroll = std::clamp(_lines.scroll, 0.0f, _lines.maxScroll());
  y += linesH;

  _geo.panel = {x, kTop, kWidth, y + kPad - kTop};
}

void TurnPanel::scrollInput(List& list, bool& dragging, Vector2 mouse, bool inside) {
  if (const float wheel = Input::mouseWheel(); wheel != 0.0f && inside && CheckCollisionPointRec(mouse, list.view))
    list.scroll = std::clamp(list.scroll - wheel * list.rowH, 0.0f, list.maxScroll()); // a notch is one whole row
  if (list.maxScroll() > 0.0f) {
    if (inside && Input::mousePressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, scrollbarOf(list.view))) dragging = true;
    if (dragging && scrollbarOf(list.view).height > 0.0f)
      list.scroll = std::clamp((mouse.y - list.view.y) / list.view.height * list.maxScroll(), 0.0f, list.maxScroll());
  }
  if (!Input::mouseDown(MOUSE_BUTTON_LEFT)) dragging = false;
}

TurnPanel::Events TurnPanel::update(float dt, Mode mode, bool buttonReachable, const ui::Skin* skin) {
  Events events;
  _mode = mode;
  _hoverRow = _hoverLine = -1;
  _hoverBoard.reset();
  if (mode == Mode::Off) return events;
  layout();

  // The HUD button, top right (inside the HUD zone while the panel is folded, right of it and above the panel while it is open)
  {
    const float screenW = static_cast<float>(GetScreenWidth());
    const bool open = mode == Mode::Open;
    std::string label = open ? "Hide list" : "Turn list";
    if (!open && _checklist.submit == SubmitState::Ready && !_checklist.over && _human) label += ": ready";
    else if (!open && _checklist.total >= 2 && !_checklist.over) label += " " + std::to_string(_checklist.done) + "/" + std::to_string(_checklist.total);
    const float w = std::max(kHudButtonW, textW(UI::Fonts::button(), label, UI::Font::button) + 2.0f * UI::Space::md);
    _toggle.label = label;
    _toggle.rect = {screenW - UI::Layout::sideInset - w, UI::Layout::hudPillY, w, UI::Space::buttonHeight};
    _toggle.skin = skin;
    _toggle.tip = open ? "Fold the turn list away (C)" : "Show this turn's boards and the opponent's last turn (C)";
    if (_toggle.update(dt, buttonReachable)) events.toggled = true;
  }
  if (mode != Mode::Open) return events;

  const Vector2 mouse = Input::mousePosition();
  const bool free = !ui::pointerConsumed();
  const bool overPanel = CheckCollisionPointRec(mouse, _geo.panel);
  scrollInput(_rows, _draggingRows, mouse, free && overPanel);
  scrollInput(_lines, _draggingLines, mouse, free && overPanel);

  if (free && overPanel) {
    if (!_draggingRows && !_draggingLines) {
      // (the scrollbar strip is not a row)
      const auto row = [&](const List& l) { return l.maxScroll() > 0.0f && mouse.x >= l.view.x + l.view.width - kScrollbarW - 2.0f ? -1 : l.rowAt(mouse); };
      _hoverRow = row(_rows);
      _hoverLine = row(_lines);
    }
    if (_hoverRow >= 0) {
      const ChecklistRow& r = _checklist.rows[static_cast<size_t>(_hoverRow)];
      _hoverBoard = std::make_pair(r.timeline, r.halfTurn);
      if (Input::mousePressed(MOUSE_BUTTON_LEFT)) events.focusBoard = _hoverBoard;
    } else if (_hoverLine >= 0) {
      const LastMove& m = _checklist.last[static_cast<size_t>(_hoverLine)];
      _hoverBoard = std::make_pair(static_cast<int>(m.from.l), static_cast<int>(m.from.t));
      if (Input::mousePressed(MOUSE_BUTTON_LEFT)) events.focusMove = m;
    }
    if (_hoverRow >= 0 || _hoverLine >= 0) UI::Cursor::request(UI::Cursor::Kind::Hand);
    ui::consumePointer(); // the panel is not a board
  }
  return events;
}

void TurnPanel::draw(const Chess::IGame& game, const BoardStyle& style, float buttonAlpha) const {
  if (_mode == Mode::Off) return;
  if (_toggle.rect.width > 0.0f) _toggle.draw(buttonAlpha); // (not before the first update)
  if (_mode != Mode::Open) return;

  const Rectangle panel = _geo.panel;
  const float inner = panel.width - 2.0f * kPad;
  drawPanel(panel, style, 1.0f, 18.0f / std::min(panel.width, panel.height));
  if (ui::audit::enabled()) {
    ui::audit::rect("turn panel", panel, ui::audit::Kind::Panel);
    ui::audit::rect("turn panel header", _geo.header, ui::audit::Kind::Text);
    if (!_checklist.over)
      ui::audit::rect("turn panel submit", {_geo.submit.x, _geo.submit.y, _geo.submit.width, _geo.submit.height + _geo.submitNote.height}, ui::audit::Kind::Text);
    ui::audit::rect("turn panel last turn", _geo.lastHeader, ui::audit::Kind::Text);
    ui::audit::rect("turn panel boards", _rows.view, ui::audit::Kind::List);
    ui::audit::rect("turn panel moves", _lines.view, ui::audit::Kind::List);
  }
  const ::Font button = UI::Fonts::button(), body = UI::Fonts::body(), mono = UI::Fonts::mono();

  // ---- Header: "This turn · 3 / 5 boards" ----
  {
    const float w = drawFitted("turn panel header", button, UI::Font::button, _turnHeading, _geo.header.x, _geo.header.y, inner, style.hudText, _geo.header);
    if (_checklist.total >= 2) { // the count is bright only when Submit is ready (not merely when every board is moved)
      const std::string count = " \xC2\xB7 " + std::to_string(_checklist.done) + " / " + std::to_string(_checklist.total) + " boards";
      drawFitted("turn panel header", button, UI::Font::button, count, _geo.header.x + w, _geo.header.y, inner - w,
                 _checklist.submit == SubmitState::Ready && _human ? style.hudText : style.hudMuted, _geo.header);
    }
  }

  // ---- One row per board ----
  if (_checklist.over) {
    DrawRectangle(static_cast<int>(panel.x + kPad), static_cast<int>(_geo.dividerB), static_cast<int>(inner), 1, style.hudBorder);
    return drawLast(style, panel, inner);
  }
  const bool rowsScroll = _rows.maxScroll() > 0.0f;
  BeginScissorMode(static_cast<int>(_rows.view.x), static_cast<int>(_rows.view.y), static_cast<int>(_rows.view.width), static_cast<int>(_rows.view.height));
  for (int i = 0; i < _rows.count; ++i) {
    const ChecklistRow& r = _checklist.rows[static_cast<size_t>(i)];
    const float y = _rows.view.y + static_cast<float>(i) * kRowH - _rows.scroll;
    if (y + kRowH < _rows.view.y || y > _rows.view.y + _rows.view.height) continue;
    const Rectangle row = {_rows.view.x, y, _rows.view.width - (rowsScroll ? kScrollbarW + 2.0f : 0.0f), kRowH};
    const bool whole = y >= _rows.view.y - 0.5f && y + kRowH <= _rows.view.y + _rows.view.height + 0.5f;
    if (i == _hoverRow) drawRoundedFill({row.x - 4, row.y + 1, row.width + 8, row.height - 2}, 10.0f, withAlpha(style.accent, 0.13f), withAlpha(style.accent, 0.45f), 1.0f);

    // the thumbnail: the real board, scaled to fit the row
    {
      const Rect board = BoardLayout::boardRect(r.timeline, r.halfTurn);
      const Rect card = BoardLayout::cardRect(board);
      const float s = kThumbH / card.h;
      Camera2D camera{};
      camera.target = {card.centerX(), card.centerY()};
      camera.offset = {row.x + 2.0f + card.w * s / 2.0f, row.y + kRowH / 2.0f};
      camera.zoom = s;
      BoardLook look;
      look.role = roleOf(r.state);
      look.inactive = r.inactive;
      look.whiteToMove = r.halfTurn % 2 == 0;
      BeginMode2D(camera);
      // a board that was moved on shows the board it became
      const bool after = r.state == RowState::Moved && game.boardExists(r.timeline, r.halfTurn + 1);
      if (after) look.whiteToMove = (r.halfTurn + 1) % 2 == 0;
      drawBoard(game.board(r.timeline, r.halfTurn + (after ? 1 : 0)), board, look, style, s);
      EndMode2D();
    }
    const float tx = row.x + 2.0f + kThumbW + 10.0f;
    const float right = row.x + row.width - 2.0f;

    // line 1: the label, and the state chip at the right
    const ::Color accent = stateColor(r.state, style);
    const std::string chip = chipText(r.state);
    const float chipTextW = textW(mono, chip, UI::Font::minimum);
    const float checkW = r.state == RowState::Moved ? 14.0f : 0.0f;
    const Rectangle chipR = {right - (chipTextW + checkW + 14.0f), row.y + 6.0f, chipTextW + checkW + 14.0f, 20.0f};
    if (r.state == RowState::Waiting) drawRoundedFill(chipR, 10.0f, {0, 0, 0, 0}, withAlpha(style.hudMuted, 0.55f), 1.0f);
    else drawRoundedFill(chipR, 10.0f, withAlpha(accent, r.state == RowState::Moved ? 0.12f : 0.2f), withAlpha(accent, r.state == RowState::Moved ? 0.4f : 0.85f), 1.0f);
    float cx = chipR.x + 7.0f;
    if (r.state == RowState::Moved) {
      drawCheck(cx, chipR.y + 5.0f, 10.0f, style.hudText);
      cx += checkW;
    }
    DrawTextEx(mono, chip.c_str(), {std::floor(cx), std::floor(chipR.y + (chipR.height - UI::Font::minimum) / 2.0f - 1.0f)}, UI::Font::minimum, 0.0f,
               r.state == RowState::Waiting ? style.hudMuted : style.hudText);
    if (ui::audit::enabled() && whole) ui::audit::within("turn row chip", chip, {cx, chipR.y + 3.0f, chipTextW, static_cast<float>(UI::Font::minimum)}, chipR);
    drawFitted("turn row label", mono, UI::Font::mono, r.label, tx, row.y + 5.0f, chipR.x - 6.0f - tx, r.state == RowState::Waiting ? style.hudMuted : style.hudText,
               whole ? Rectangle{tx, row.y, chipR.x - 6.0f - tx, kRowH} : Rectangle{0, 0, 4000, 4000});

    // line 2: the move played, or what the row means
    const std::string second = r.state == RowState::Moved ? r.detail : _human ? rowNote(r) : std::string();
    drawFitted("turn row detail", mono, UI::Font::minimum, second, tx, row.y + 27.0f, right - tx, r.state == RowState::Moved ? style.hudText : style.hudMuted,
               whole ? Rectangle{tx, row.y, right - tx, kRowH} : Rectangle{0, 0, 4000, 4000});
  }
  drawFade(_rows.view, _rows.scroll, _rows.maxScroll(), style.hudFill);
  EndScissorMode();
  if (rowsScroll) {
    DrawRectangleRounded(scrollbarOf(_rows.view), 1.0f, 6, withAlpha(style.hudBorder, 0.5f));
    DrawRectangleRounded(handleOf(_rows.view, _rows.scroll, _rows.maxScroll(), _rows.content()), 1.0f, 6, style.hudMuted);
  }

  // ---- What Submit hands in ----
  DrawRectangle(static_cast<int>(panel.x + kPad), static_cast<int>(_geo.dividerA), static_cast<int>(inner), 1, style.hudBorder);
  {
    const bool ready = _checklist.submit == SubmitState::Ready && _human;
    DrawCircleV({_geo.submit.x + 6.0f, _geo.submit.y + 12.0f}, 5.0f, !_human ? style.hudMuted : ready ? style.optional : style.mandatory);
    drawFitted("turn panel submit", body, UI::Font::body, _human ? submitLine(_checklist) : std::string("Computer to move"), _geo.submit.x + 18.0f, _geo.submit.y + 1.0f,
               inner - 18.0f, style.hudText, _geo.submit);
    drawFitted("turn panel submit note", mono, UI::Font::minimum, _human ? submitNote(_checklist) : std::string(), _geo.submitNote.x + 18.0f, _geo.submitNote.y + 1.0f,
               inner - 18.0f, style.hudMuted, _geo.submitNote);
  }

  DrawRectangle(static_cast<int>(panel.x + kPad), static_cast<int>(_geo.dividerB), static_cast<int>(inner), 1, style.hudBorder);
  drawLast(style, panel, inner);
}

// ---- The opponent's last turn (or, in a decided game, the turn that decided it) ----
void TurnPanel::drawLast(const BoardStyle& style, Rectangle panel, float inner) const {
  const ::Font body = UI::Fonts::body(), mono = UI::Fonts::mono();
  (void)panel;
  {
    std::string head = _lastHeading;
    if (!_checklist.lastLabel.empty()) head += " \xC2\xB7 " + _checklist.lastLabel;
    drawFitted("turn panel last turn", body, UI::Font::body, head, _geo.lastHeader.x, _geo.lastHeader.y, inner, style.hudText, _geo.lastHeader);
  }
  if (_lines.count == 0) {
    drawFitted("turn panel no moves", mono, UI::Font::minimum, "No moves played yet", _lines.view.x, _lines.view.y + 4.0f, inner, style.hudMuted, _lines.view);
    return;
  }
  const bool linesScroll = _lines.maxScroll() > 0.0f;
  BeginScissorMode(static_cast<int>(_lines.view.x), static_cast<int>(_lines.view.y), static_cast<int>(_lines.view.width), static_cast<int>(_lines.view.height));
  for (int i = 0; i < _lines.count; ++i) {
    const float y = _lines.view.y + static_cast<float>(i) * kLineH - _lines.scroll;
    if (y + kLineH < _lines.view.y || y > _lines.view.y + _lines.view.height) continue;
    const bool whole = y >= _lines.view.y - 0.5f && y + kLineH <= _lines.view.y + _lines.view.height + 0.5f;
    const float w = _lines.view.width - (linesScroll ? kScrollbarW + 2.0f : 0.0f);
    if (i == _hoverLine) drawRoundedFill({_lines.view.x - 4, y + 1, w + 8, kLineH - 2}, 8.0f, withAlpha(style.accent, 0.13f), withAlpha(style.accent, 0.45f), 1.0f);
    drawFitted("turn panel move", mono, UI::Font::mono, _checklist.last[static_cast<size_t>(i)].text, _lines.view.x + 2.0f, y + 3.0f, w - 4.0f, style.hudText,
               whole ? Rectangle{_lines.view.x, y, w, kLineH} : Rectangle{0, 0, 4000, 4000});
  }
  drawFade(_lines.view, _lines.scroll, _lines.maxScroll(), style.hudFill);
  EndScissorMode();
  if (linesScroll) {
    DrawRectangleRounded(scrollbarOf(_lines.view), 1.0f, 6, withAlpha(style.hudBorder, 0.5f));
    DrawRectangleRounded(handleOf(_lines.view, _lines.scroll, _lines.maxScroll(), _lines.content()), 1.0f, 6, style.hudMuted);
  }
}

} // namespace play
