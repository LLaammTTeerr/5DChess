#include "Screens/ModeSelectScreen.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "App.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/PlayScreen.h"
#include "engine/GameCatalog.h"

// The buttons of the side and level rows are in the order of these enums (the clicked index is cast to them)
static_assert(static_cast<int>(play::SideChoice::White) == 0 && static_cast<int>(play::SideChoice::Black) == 1 &&
              static_cast<int>(play::SideChoice::Random) == 2);
static_assert(static_cast<int>(play::AiLevel::Easy) == 0 && static_cast<int>(play::AiLevel::Normal) == 1 &&
              static_cast<int>(play::AiLevel::Hard) == 2);

namespace {
constexpr float kRowW = 200.0f, kRowGap = UI::Space::md;
constexpr float kColW = 420.0f, kCaptionW = 104.0f;                // the setup rows: a caption, then the choices
constexpr float kOpponentY = 100.0f, kSideY = 152.0f, kLevelY = 204.0f, kDescriptionY = 258.0f;
constexpr float kListYHotSeat = 160.0f, kListYVsComputer = 296.0f;

// The segments of a setup row: `n` equal buttons filling the column right of the caption.
std::vector<Rectangle> segments(float y, int n) {
  const float x = static_cast<float>(GetScreenWidth()) / 2 - kColW / 2 + kCaptionW;
  const float w = (kColW - kCaptionW - (n - 1) * UI::Space::sm) / n;
  return ui::row({x, y, kColW - kCaptionW, UI::Space::buttonHeight}, n, w, UI::Space::sm);
}

void drawCaption(const char* text, float y, float alpha) {
  const float x = static_cast<float>(GetScreenWidth()) / 2 - kColW / 2;
  DrawTextEx(UI::Fonts::body(), text, {x, y + (UI::Space::buttonHeight - UI::Font::body) / 2 - 1}, UI::Font::body, 0,
             UI::withAlpha(UI::Color::textMuted, static_cast<unsigned char>(255.0f * alpha)));
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

  _opponent = ui::ButtonList({"Two players", "Computer"}, segments(kOpponentY, 2));
  _opponent.selected = _vs ? 1 : 0;
  _sides = ui::ButtonList({play::title(play::SideChoice::White), play::title(play::SideChoice::Black), play::title(play::SideChoice::Random)},
                          segments(kSideY, 3));
  _sides.selected = static_cast<int>(_side);
  _levels = ui::ButtonList({play::title(play::AiLevel::Easy), play::title(play::AiLevel::Normal), play::title(play::AiLevel::Hard)},
                           segments(kLevelY, 3));
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

  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  const float top = _vs ? kListYVsComputer : kListYHotSeat;
  const float itemH = UI::Space::buttonHeight + 4.0f, gap = UI::Space::sm + 4.0f;
  // Leaves room for the Back / Play row, and holds a whole number of rows: the last visible one is never cut through its text
  const float rowPitch = itemH + gap;
  const float rows = std::max(1.0f, std::floor((H - top - 120.0f + gap) / rowPitch));
  const Rectangle viewport = {W / 2 - 210.0f, top, 420.0f, rows * rowPitch - gap};
  const float scrollbarW = 20.0f;
  const auto slots = ui::column({viewport.x + 10.0f, viewport.y, viewport.width - scrollbarW - 20.0f, viewport.height},
                                static_cast<int>(titles.size()), itemH, gap, ui::Align::Top);
  const int selected = _modes.selected;
  _modes = ui::ButtonList(titles, slots, viewport, scrollbarW);
  _modes.selected = selected;
}

// Back alone sits in the middle; with Play the pair is centred.
void ModeSelectScreen::layoutNavRow() {
  const float H = static_cast<float>(GetScreenHeight());
  const auto slots = ui::row({0.0f, H - 110.0f, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight},
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
    buildList();
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
  UI::drawSceneTitle("Select Game Mode");
  drawCaption("Opponent", kOpponentY, 1.0f);
  _opponent.draw();
  if (_vs) {
    const float fade = _vsFade.value();
    drawCaption("You play", kSideY, fade);
    _sides.draw(fade);
    drawCaption("Level", kLevelY, fade);
    _levels.draw(fade);
    UI::drawTextCentered(UI::Fonts::body(), play::describe(_level), GetScreenWidth() / 2.0f, kDescriptionY, UI::Font::body,
                         UI::withAlpha(UI::Color::textMuted, static_cast<unsigned char>(255.0f * fade)));
  }
  _modes.draw();
  UI::drawTextCentered(UI::Fonts::body(), "Use mouse to select a game mode", GetScreenWidth() / 2.0f,
                       static_cast<float>(GetScreenHeight()) - 44.0f, UI::Font::body, UI::Color::textMuted);
  _back.draw(app.screens.navAlpha());
  if (_canPlay) _play.draw(app.screens.navAlpha());
}

void ModeSelectScreen::back(App& app) { app.screens.replace(std::make_unique<MainMenuScreen>()); }
