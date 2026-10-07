#include "Screens/ModeSelectScreen.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include "App.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/PlayScreen.h"
#include "engine/GameCatalog.h"
#include "ui/Audit.h"

// The buttons of the side and level rows are in the order of these enums (the clicked index is cast to them)
static_assert(static_cast<int>(play::SideChoice::White) == 0 && static_cast<int>(play::SideChoice::Black) == 1 &&
              static_cast<int>(play::SideChoice::Random) == 2);
static_assert(static_cast<int>(play::AiLevel::Easy) == 0 && static_cast<int>(play::AiLevel::Normal) == 1 &&
              static_cast<int>(play::AiLevel::Hard) == 2);

namespace {
constexpr float kRowW = 200.0f, kRowGap = UI::Space::md;  // Back / Play

// Two columns under the title: the Setup panel at the left (each label above its row of choices, the level's description at the
// bottom) and the mode list in a panel at the right. Both start at kTop; the nav row and the footer keep clear below them.
constexpr float kTop = 128.0f, kSetupW = 400.0f, kModesW = 560.0f, kColumnGap = 40.0f, kPad = UI::Space::lg;
constexpr float kHeaderH = 34.0f, kHeaderGap = 14.0f;                    // "Setup" / "Game mode" (section font), then the content
constexpr float kLabelH = 24.0f, kLabelGap = UI::Space::sm, kGroupGap = 20.0f;  // a label above its row, groups below each other
constexpr float kNoteGap = UI::Space::md, kNoteLine = 24.0f, kNoteLines = 2.0f;  // the level description: room for two lines
constexpr float kNavY = 110.0f;                                           // Back / Play: this far above the window bottom
constexpr float kItemH = UI::Space::buttonHeight + 4.0f, kItemGap = UI::Space::sm + 4.0f;

float leftX() { return std::floor(static_cast<float>(GetScreenWidth()) / 2 - (kSetupW + kColumnGap + kModesW) / 2); }
float contentTop() { return kTop + kPad + kHeaderH + kHeaderGap; }          // where the first label / the list begins
float groupY(int group) { return contentTop() + static_cast<float>(group) * (kLabelH + kLabelGap + UI::Space::buttonHeight + kGroupGap); }
float rowY(int group) { return groupY(group) + kLabelH + kLabelGap; }

// The segments of a setup row: `n` equal buttons filling the panel's inner width.
std::vector<Rectangle> segments(int group, int n) {
  const float w = (kSetupW - 2 * kPad - (n - 1) * UI::Space::sm) / n;
  return ui::row({leftX() + kPad, rowY(group), kSetupW - 2 * kPad, UI::Space::buttonHeight}, n, w, UI::Space::sm);
}

Rectangle setupPanel(bool vs) {
  const float bottom = vs ? rowY(2) + UI::Space::buttonHeight + kNoteGap + kNoteLines * kNoteLine : rowY(0) + UI::Space::buttonHeight + kNoteGap + kNoteLine;
  return {leftX(), kTop, kSetupW, bottom + kPad - kTop};
}

Rectangle modesPanel(float viewportH) {
  return {leftX() + kSetupW + kColumnGap, kTop, kModesW, contentTop() + viewportH + kPad - kTop};
}

// A card with a 14 px corner radius (UI::drawRoundedPanel's roundness is relative to the short side: far too round for a tall panel)
void drawCard(Rectangle r) {
  const float roundness = 14.0f / std::min(r.width, r.height);
  DrawRectangleRounded(r, roundness, 12, UI::Color::surfaceAlt);
  DrawRectangleRoundedLinesEx(r, roundness, 12, 1.0f, UI::Color::border);
}

unsigned char alphaOf(float a) { return static_cast<unsigned char>(255.0f * std::clamp(a, 0.0f, 1.0f)); }

void drawLabel(const char* text, int group, float alpha) {
  const Rectangle box = {leftX() + kPad, groupY(group), kSetupW - 2 * kPad, kLabelH};
  const Vector2 size = MeasureTextEx(UI::Fonts::button(), text, UI::Font::button, 0);
  if (ui::audit::enabled()) {
    ui::audit::within("setup label", text, {box.x, box.y, size.x, size.y}, box);
    ui::audit::rect(std::string("label ") + text, {box.x, box.y, size.x, kLabelH}, ui::audit::Kind::Text);
  }
  DrawTextEx(UI::Fonts::button(), text, {box.x, std::floor(box.y + (kLabelH - size.y) / 2)}, UI::Font::button, 0,
             UI::withAlpha(UI::Color::text, alphaOf(alpha)));
}

// Greedy word wrap of `text` to `width` pixels
std::vector<std::string> wrapped(const std::string& text, float width) {
  const ::Font font = UI::Fonts::body();
  std::vector<std::string> lines;
  std::string line;
  std::istringstream words(text);
  for (std::string word; words >> word;) {
    const std::string trial = line.empty() ? word : line + " " + word;
    if (!line.empty() && MeasureTextEx(font, trial.c_str(), UI::Font::body, 0).x > width) { lines.push_back(line); line = word; }
    else line = trial;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

void drawNote(const std::string& text, float y, float lines, float alpha) {
  const Rectangle box = {leftX() + kPad, y, kSetupW - 2 * kPad, lines * kNoteLine};
  int i = 0;
  for (const std::string& line : wrapped(text, box.width)) {
    const Vector2 size = MeasureTextEx(UI::Fonts::body(), line.c_str(), UI::Font::body, 0);
    const Rectangle at = {box.x, box.y + static_cast<float>(i) * kNoteLine, size.x, kNoteLine};
    if (ui::audit::enabled()) ui::audit::within("setup note", line, at, box);
    DrawTextEx(UI::Fonts::body(), line.c_str(), {at.x, std::floor(at.y + (kNoteLine - size.y) / 2)}, UI::Font::body, 0,
               UI::withAlpha(UI::Color::textMuted, alphaOf(alpha)));
    ++i;
  }
}

// A seed for a new game against the computer (the UI test harness seeds raylib's generator, so scripts stay reproducible).
std::uint64_t freshSeed() {
  std::uint64_t seed = 0;
  for (int i = 0; i < 4; ++i) seed = (seed << 16) | static_cast<std::uint64_t>(GetRandomValue(0, 0xFFFF));
  return seed;
}
} // namespace

ModeSelectScreen::ModeSelectScreen() {
  const Settings& settings = App::current().settings;
  _vs = settings.vsComputer;
  _side = settings.vsSide;
  _level = settings.vsLevel;

  _opponent = ui::ButtonList({"Two players", "Computer"}, segments(0, 2));
  _opponent.selected = _vs ? 1 : 0;
  _sides = ui::ButtonList({play::title(play::SideChoice::White), play::title(play::SideChoice::Black), play::title(play::SideChoice::Random)},
                          segments(1, 3));
  _sides.selected = static_cast<int>(_side);
  _levels = ui::ButtonList({play::title(play::AiLevel::Easy), play::title(play::AiLevel::Normal), play::title(play::AiLevel::Hard)},
                           segments(2, 3));
  _levels.selected = static_cast<int>(_level);
  if (_vs) _vsFade.start(1.0f, 1.0f, 0.0f);

  buildList();
  _back = ui::Button("Back", {});
  _back.enterAfter(0.0f);
  layoutNavRow();
}

void ModeSelectScreen::buildList() {
  std::vector<std::string> titles;
  for (const Chess::ModeInfo& mode : Chess::GameCatalog::modes()) titles.push_back(mode.title);

  const float H = static_cast<float>(GetScreenHeight());
  // Whole rows only (the last visible one is never cut through its text); the panel ends above the Back / Play row
  const float rowPitch = kItemH + kItemGap;
  const float room = H - kNavY - UI::Space::lg - kPad - contentTop();
  const float rows = std::max(1.0f, std::floor((room + kItemGap) / rowPitch));
  const float viewportH = rows * rowPitch - kItemGap;
  _modesPanel = modesPanel(viewportH);
  _setupPanel = setupPanel(_vs);
  const float scrollbarW = 20.0f, edge = 6.0f;  // `edge`: room for the selection outline inside the clip
  const Rectangle viewport = {_modesPanel.x + kPad, contentTop() - edge, _modesPanel.width - 2 * kPad, viewportH + edge};  // (the rows start `edge` inside)
  const auto slots = ui::column({viewport.x + edge, contentTop(), viewport.width - scrollbarW - UI::Space::md - edge, viewport.height},
                                static_cast<int>(titles.size()), kItemH, kItemGap, ui::Align::Top);
  const int selected = _modes.selected;
  _modes = ui::ButtonList(titles, slots, viewport, scrollbarW);
  _modes.selected = selected;
  _modes.fadeTo = UI::Color::surfaceAlt;
}

// Back alone sits in the middle; with Play the pair is centred.
void ModeSelectScreen::layoutNavRow() {
  const float H = static_cast<float>(GetScreenHeight());
  const auto slots = ui::row({0.0f, H - kNavY, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight},
                             _canPlay ? 2 : 1, kRowW, kRowGap);
  _back.rect = slots[0];
  if (_canPlay) _play.rect = slots[1];
}

void ModeSelectScreen::update(App& app, float dt) {
  const bool nav = app.screens.navShown();  // hidden: still updated (hover eases out) but unreachable
  _vsFade.update(dt);

  const int opponent = _opponent.update(dt, nav);
  if (opponent >= 0 && (opponent == 1) != _vs) {
    _vs = opponent == 1;
    app.settings.vsComputer = _vs;
    if (_vs) _vsFade.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
    _setupPanel = setupPanel(_vs);  // the list does not move: only the panel beside it grows or shrinks
  }
  if (_vs) {
    if (const int side = _sides.update(dt, nav); side >= 0) {
      _side = static_cast<play::SideChoice>(side);
      app.settings.vsSide = _side;
    }
    if (const int level = _levels.update(dt, nav); level >= 0) {
      _level = static_cast<play::AiLevel>(level);
      app.settings.vsLevel = _level;
    }
  }

  if (_modes.update(dt) >= 0 && !_canPlay) {  // first selection: Play appears (entering) and Back moves over
    _canPlay = true;
    _play = ui::Button("Play", {}, true);
    _play.enterAfter(0.0f);
    layoutNavRow();
  }
  if (_back.update(dt, nav)) app.screens.replace(std::make_unique<MainMenuScreen>());
  if (_canPlay && _play.update(dt, nav)) {
    const std::string& id = Chess::GameCatalog::modes()[_modes.selected].id;
    if (_vs) app.screens.replace(std::make_unique<PlayScreen>(id, play::makeVsAi(_side, _level, freshSeed())));
    else app.screens.replace(std::make_unique<PlayScreen>(id));
  }
}

void ModeSelectScreen::draw(App& app) const {
  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  UI::drawSceneTitle("Select Game Mode");
  if (ui::audit::enabled()) ui::audit::rect("title", {W / 2 - 200.0f, 48.0f, 400.0f, static_cast<float>(UI::Font::title)}, ui::audit::Kind::Text);

  // Left: the Setup panel. Two players needs only the opponent; against the computer the side, the level and its description follow.
  drawCard(_setupPanel);
  if (ui::audit::enabled()) ui::audit::rect("setup panel", _setupPanel, ui::audit::Kind::Panel);
  DrawTextEx(UI::Fonts::section(), "Setup", {_setupPanel.x + kPad, std::floor(_setupPanel.y + kPad)}, UI::Font::section, 0, UI::Color::text);
  drawLabel("Opponent", 0, 1.0f);
  _opponent.draw();
  if (_vs) {
    const float fade = _vsFade.value();
    drawLabel("You play", 1, fade);
    _sides.draw(fade);
    drawLabel("Level", 2, fade);
    _levels.draw(fade);
    drawNote(play::describe(_level), rowY(2) + UI::Space::buttonHeight + kNoteGap, kNoteLines, fade);
  } else {
    drawNote("Both players move on this computer.", rowY(0) + UI::Space::buttonHeight + kNoteGap, 1.0f, 1.0f);
  }

  // Right: the mode list
  drawCard(_modesPanel);
  if (ui::audit::enabled()) ui::audit::rect("modes panel", _modesPanel, ui::audit::Kind::Panel);
  DrawTextEx(UI::Fonts::section(), "Game mode", {_modesPanel.x + kPad, std::floor(_modesPanel.y + kPad)}, UI::Font::section, 0, UI::Color::text);
  _modes.draw();

  const float footerY = H - 44.0f;
  if (ui::audit::enabled()) ui::audit::rect("footer", {W / 2 - 160.0f, footerY, 320.0f, static_cast<float>(UI::Font::body)}, ui::audit::Kind::Text);
  UI::drawTextCentered(UI::Fonts::body(), "Use mouse to select a game mode", W / 2.0f, footerY, UI::Font::body, UI::Color::textMuted);
  _back.draw(app.screens.navAlpha());
  if (_canPlay) _play.draw(app.screens.navAlpha());
}

void ModeSelectScreen::back(App& app) { app.screens.replace(std::make_unique<MainMenuScreen>()); }
