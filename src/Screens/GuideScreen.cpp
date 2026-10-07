#include "Screens/GuideScreen.h"
#include <algorithm>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "ui/Audit.h"
#include "Screens/MainMenuScreen.h"
#include "guide/Guide.h"
#include "play/BoardStyle.h"

namespace {

constexpr float kPanelW = 400.0f, kPanelRight = 24.0f, kPanelTop = 64.0f, kPanelBottom = 52.0f; // above the controls bar
constexpr float kPad = 22.0f;
constexpr float kInnerW = kPanelW - 2 * kPad;
constexpr float kTitleLine = 34.0f, kTextLine = 25.0f, kSmallLine = 21.0f, kPromptLine = 26.0f, kStatusLine = 24.0f;
constexpr float kCardPad = 14.0f, kCheckW = 28.0f;
constexpr float kDotStep = 16.0f, kDotR = 4.5f;
// What a board keeps free at its right: the panel, a gap
constexpr float kBoardInset = kPanelW + kPanelRight + 16.0f;

/// Greedy word wrap to `width` pixels.
std::vector<std::string> wrap(::Font font, float size, const std::string& text, float width) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size()) {
    size_t j = text.find(' ', i);
    if (j == std::string::npos) j = text.size();
    const std::string word = text.substr(i, j - i);
    const std::string trial = line.empty() ? word : line + " " + word;
    if (!line.empty() && MeasureTextEx(font, trial.c_str(), size, 0.0f).x > width) {
      lines.push_back(line);
      line = word;
    } else {
      line = trial;
    }
    i = j + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

void drawLines(::Font font, float size, const std::vector<std::string>& lines, float x, float y, float lineH, ::Color color,
               Rectangle bounds = {}) {
  for (size_t i = 0; i < lines.size(); ++i) {
    const Vector2 at = {std::floor(x), std::floor(y + static_cast<float>(i) * lineH)};
    DrawTextEx(font, lines[i].c_str(), at, size, 0.0f, color);
    if (bounds.width > 0.0f && ui::audit::enabled())
      ui::audit::within("panel text", lines[i], {at.x, at.y, MeasureTextEx(font, lines[i].c_str(), size, 0.0f).x, size}, bounds);
  }
}

void drawCheck(float x, float y, float size, ::Color color) {
  DrawLineEx({x, y + size * 0.55f}, {x + size * 0.38f, y + size * 0.9f}, 3.0f, color);
  DrawLineEx({x + size * 0.38f, y + size * 0.9f}, {x + size, y + size * 0.12f}, 3.0f, color);
}

} // namespace

GuideScreen::GuideScreen(int page) {
  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  _panel = {W - kPanelRight - kPanelW, kPanelTop, kPanelW, H - kPanelTop - kPanelBottom};
  const float bottom = _panel.y + _panel.height - kPad;
  const float rowY = bottom - UI::Space::buttonHeight;
  _prev = ui::Button("Prev", {_panel.x + kPad, rowY, 100.0f, UI::Space::buttonHeight});
  _next = ui::Button("Next", {_panel.x + kPanelW - kPad - 100.0f, rowY, 100.0f, UI::Space::buttonHeight});
  const float aboveY = rowY - UI::Space::buttonHeight - UI::Space::md;
  _reset = ui::Button("Reset position", {_panel.x + kPad, aboveY, 180.0f, UI::Space::buttonHeight});
  _start = ui::Button("Play a Standard game", {_panel.x + kPad, aboveY, kInnerW, UI::Space::buttonHeight}, true);
  _dots = {_panel.x + kPad + 100.0f, rowY, kInnerW - 200.0f, UI::Space::buttonHeight};
  open(page);
}

void GuideScreen::open(int page) {
  const int count = static_cast<int>(guide::pages().size());
  _page = std::clamp(page, 0, count - 1);
  _board = std::make_unique<PlayScreen>(guide::load(guide::pages()[static_cast<size_t>(_page)]));
  _board->embed(kBoardInset);
  _done = false;
  layout();
}

void GuideScreen::go(int page) {
  if (page >= 0 && page < static_cast<int>(guide::pages().size()) && page != _page) open(page);
}

std::string GuideScreen::statusText() const {
  const guide::Goal* goal = guide::pages()[static_cast<size_t>(_page)].goal;
  if (!goal) return "";
  if (_done) return goal->success;
  return guide::anyMoveMade(_board->game()) ? std::string("Not yet. ") + goal->hint : goal->hint;
}

void GuideScreen::wrapStatus() {
  _statusText = statusText();
  _status.lines = wrap(UI::Fonts::body(), UI::Font::body, _statusText, kInnerW - 2 * kCardPad - (_done ? kCheckW : 0.0f));
  _status.height = static_cast<float>(_status.lines.size()) * kStatusLine;
  if (_card.height > 0.0f) _card.height = cardHeight();
}

float GuideScreen::cardHeight() const {
  return kCardPad + kSmallLine + 2.0f + _prompt.height + 8.0f + _status.height + kCardPad;
}

// Everything above the buttons flows from the top of the panel: step, title, text, note and the goal card
void GuideScreen::layout() {
  const guide::Page& page = guide::pages()[static_cast<size_t>(_page)];
  float y = _panel.y + kPad;
  auto place = [&](Lines& l, ::Font font, float size, const std::string& text, float lineH, float width, float gap) {
    l.lines = wrap(font, size, text, width);
    l.y = y;
    l.height = static_cast<float>(l.lines.size()) * lineH;
    y += l.height + gap;
  };
  place(_step, UI::Fonts::mono(), UI::Font::mono,
        "STEP " + std::to_string(_page + 1) + " OF " + std::to_string(guide::pages().size()), kSmallLine, kInnerW, 4.0f);
  place(_title, UI::Fonts::section(), UI::Font::section, page.title, kTitleLine, kInnerW, 10.0f);
  place(_text, UI::Fonts::body(), UI::Font::body, page.text, kTextLine, kInnerW, 12.0f);
  place(_note, UI::Fonts::mono(), UI::Font::mono, page.note, kSmallLine, kInnerW, 14.0f);
  _card = {};
  if (page.goal) {
    const float labelH = kSmallLine + 2.0f;
    place(_prompt, UI::Fonts::button(), UI::Font::button, page.goal->prompt, kPromptLine, kInnerW - 2 * kCardPad, 0.0f);
    _prompt.y += kCardPad + labelH;
    y = _prompt.y + _prompt.height + 8.0f;
    _status.y = y;
    wrapStatus();
    _card = {_panel.x + kPad, _prompt.y - kCardPad - labelH, kInnerW, 1.0f};
    _card.height = cardHeight();
  }
}

void GuideScreen::update(App& app, float dt) {
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  for (ui::Button* b : {&_back, &_prev, &_next, &_reset, &_start}) b->skin = &style.skin;
  const guide::Page& page = guide::pages()[static_cast<size_t>(_page)];
  const bool nav = app.screens.navShown();
  const int count = static_cast<int>(guide::pages().size());

  _prev.enabled = _page > 0;
  _next.enabled = _page + 1 < count;
  int target = _page;
  if (_back.update(dt, nav)) {
    app.screens.replace(std::make_unique<MainMenuScreen>());
    return;
  }
  if (_prev.update(dt, nav)) target = _page - 1;
  if (_next.update(dt, nav)) target = _page + 1;
  if (!page.startsGame && _reset.update(dt, nav)) {
    open(_page);
    return;
  }
  if (page.startsGame && _start.update(dt, nav)) {
    app.screens.replace(std::make_unique<PlayScreen>("standard"));
    return;
  }
  if (Input::keyPressed(KEY_LEFT)) target = _page - 1;
  if (Input::keyPressed(KEY_RIGHT)) target = _page + 1;
  if (nav && Input::mousePressed(MOUSE_BUTTON_LEFT)) {
    const Vector2 m = Input::mousePosition();
    for (int i = 0; i < count; ++i) {
      const Vector2 c = {_dots.x + _dots.width / 2 + (static_cast<float>(i) - (count - 1) / 2.0f) * kDotStep, _dots.y + _dots.height / 2};
      if (CheckCollisionPointRec(m, {c.x - kDotStep / 2, c.y - 12.0f, kDotStep, 24.0f})) target = i;
    }
  }
  if (target != _page) {
    go(target);
    return;
  }

  if (CheckCollisionPointRec(Input::mousePosition(), _panel)) ui::consumePointer(); // the panel is not a board
  _board->update(app, dt);

  if (page.goal && !_done && page.goal->met(_board->game())) _done = true;
  if (page.goal && statusText() != _statusText) wrapStatus();
}

void GuideScreen::draw(App& app) const {
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  const guide::Page& page = guide::pages()[static_cast<size_t>(_page)];
  _board->draw(app);

  // the HUD's panel with a fixed corner radius of about 18 px
  play::drawPanel(_panel, style, 1.0f, 18.0f / std::min(_panel.width, _panel.height));
  const float x = _panel.x + kPad;
  drawLines(UI::Fonts::mono(), UI::Font::mono, _step.lines, x, _step.y, kSmallLine, style.hudMuted, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});
  drawLines(UI::Fonts::section(), UI::Font::section, _title.lines, x, _title.y, kTitleLine, style.hudText, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});
  drawLines(UI::Fonts::body(), UI::Font::body, _text.lines, x, _text.y, kTextLine, style.hudText, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});
  drawLines(UI::Fonts::mono(), UI::Font::mono, _note.lines, x, _note.y, kSmallLine, style.hudMuted, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});

  if (page.goal) {
    const float cardRound = style.hudSquare ? 0.0f : 12.0f / std::min(_card.width, _card.height);
    const ::Color tint = UI::withAlpha(_done ? style.accent : style.hudBorder, _done ? 34 : 70);
    if (cardRound > 0.0f) DrawRectangleRounded(_card, cardRound, 8, tint);
    else DrawRectangleRec(_card, tint);
    const ::Color edge = _done ? style.accent : style.hudBorder;
    if (cardRound > 0.0f) DrawRectangleRoundedLinesEx(_card, cardRound, 8, 1.5f, edge);
    else DrawRectangleLinesEx(_card, 1.5f, edge);

    const float cx = _card.x + kCardPad;
    DrawTextEx(UI::Fonts::mono(), "TRY IT", {cx, std::floor(_card.y + kCardPad)}, UI::Font::mono, 0.0f, style.accent);
    drawLines(UI::Fonts::button(), UI::Font::button, _prompt.lines, cx, _prompt.y, kPromptLine, style.hudText, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});
    float tx = cx;
    if (_done) {
      drawCheck(cx, _status.y + 3.0f, 18.0f, style.accent);
      tx += kCheckW;
    }
    drawLines(UI::Fonts::body(), UI::Font::body, _status.lines, tx, _status.y, kStatusLine, _done ? style.hudText : style.hudMuted, {_panel.x, _panel.y, _panel.width, std::min(_prev.rect.y, _reset.rect.y) - _panel.y});
  }

  const float a = app.screens.navAlpha();
  if (page.startsGame) _start.draw(a);
  else _reset.draw(a);
  _prev.draw(a);
  _next.draw(a);

  const int count = static_cast<int>(guide::pages().size());
  for (int i = 0; i < count; ++i) {
    const Vector2 c = {_dots.x + _dots.width / 2 + (static_cast<float>(i) - (count - 1) / 2.0f) * kDotStep, _dots.y + _dots.height / 2};
    if (i == _page) DrawCircleV(c, kDotR + 1.0f, UI::withAlpha(style.accent, static_cast<unsigned char>(255 * a)));
    else DrawCircleV(c, kDotR - 1.0f, UI::withAlpha(style.hudMuted, static_cast<unsigned char>(170 * a)));
  }
  _back.draw(a);
}

bool GuideScreen::escape(App& app) { return _board && _board->escape(app); }

void GuideScreen::back(App& app) { app.screens.replace(std::make_unique<MainMenuScreen>()); }
