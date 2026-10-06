#include "Screens/ModeSelectScreen.h"
#include "App.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/PlayScreen.h"
#include "engine/GameCatalog.h"

namespace {
constexpr float kRowW = 200.0f, kRowGap = UI::Space::md;
}

ModeSelectScreen::ModeSelectScreen() {
  std::vector<std::string> titles;
  for (const Chess::ModeInfo& mode : Chess::GameCatalog::modes()) titles.push_back(mode.title);

  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  const float itemH = UI::Space::buttonHeight + 4.0f, gap = UI::Space::sm + 4.0f;
  const Rectangle viewport = {W / 2 - 210.0f, 130.0f, 420.0f, H - 130.0f - 150.0f};  // leaves room for the Back / Play row
  const float scrollbarW = 20.0f;
  const auto slots = ui::column({viewport.x + 10.0f, viewport.y, viewport.width - scrollbarW - 20.0f, viewport.height},
                                static_cast<int>(titles.size()), itemH, gap, ui::Align::Top);
  _modes = ui::ButtonList(titles, slots, viewport, scrollbarW);

  _back = ui::Button("Back", {});
  _back.enterAfter(0.0f);
  layoutNavRow();
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
  if (_modes.update(dt) >= 0 && !_canPlay) {  // first selection: Play appears (entering) and Back moves over
    _canPlay = true;
    _play = ui::Button("Play", {}, true);
    _play.enterAfter(0.0f);
    layoutNavRow();
  }
  if (!app.screens.navShown()) return;
  if (_back.update(dt)) app.screens.replace(std::make_unique<MainMenuScreen>());
  if (_canPlay && _play.update(dt))
    app.screens.replace(std::make_unique<PlayScreen>(Chess::GameCatalog::modes()[_modes.selected].id));
}

void ModeSelectScreen::draw(App& app) const {
  UI::drawSceneTitle("Select Game Mode");
  _modes.draw();
  UI::drawTextCentered(UI::Fonts::body(), "Use mouse to select a game mode", GetScreenWidth() / 2.0f,
                       static_cast<float>(GetScreenHeight()) - 44.0f, UI::Font::body, UI::Color::textMuted);
  _back.draw(app.screens.navAlpha());
  if (_canPlay) _play.draw(app.screens.navAlpha());
}
