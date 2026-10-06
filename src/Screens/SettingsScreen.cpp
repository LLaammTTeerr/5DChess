#include "Screens/SettingsScreen.h"
#include "App.h"
#include "Audio/AudioManager.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "play/BoardView.h"

namespace {
const char* const kThemes[] = {"Classic", "Modern", "Fantasy", "Pixel"};
constexpr float kOptionW = 200.0f, kOptionGap = 20.0f;
}

SettingsScreen::SettingsScreen() {
  const auto slots = ui::row({0.0f, 150.0f, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight}, 3, 160.0f,
                             UI::Space::buttonSpacing + UI::Space::sm);
  _tabs = ui::ButtonList({"Piece Theme", "Music", "Display"}, slots);
}

void SettingsScreen::openTab(App& app, int tab) {
  _tab = tab;
  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  // Themes sit in a column at the left (next to the preview); the other tabs centre theirs
  const float displayW = 300.0f; // "Board view: Deep space" needs more room than the other options
  const Rectangle area = tab == Theme ? Rectangle{50.0f, 0.0f, kOptionW, H}
                         : tab == Display ? Rectangle{(W - displayW) / 2, 100.0f, displayW, H}
                                          : Rectangle{(W - kOptionW) / 2, 100.0f, kOptionW, H};
  const auto& tracks = AudioManager::tracks();
  // Music: Off, tracks, SFX toggle; Display: board view, motion toggle
  const int count = tab == Theme ? static_cast<int>(std::size(kThemes)) : tab == Music ? static_cast<int>(tracks.size()) + 2 : 2;
  const auto slots = ui::column(area, count, UI::Space::buttonHeight, kOptionGap);

  std::vector<std::string> labels;
  _toggle.reset();
  _boardView.reset();
  _options = {};
  if (tab == Theme) {
    labels.assign(std::begin(kThemes), std::end(kThemes));
  } else if (tab == Music) {
    labels.push_back(AudioManager::offName());
    for (const auto& track : tracks) labels.push_back(track.name);
    _toggle.emplace(slots.back(), app.settings.sfx, "Sound effects: On", "Sound effects: Off");
  } else {
    std::vector<std::string> views;
    for (BoardView v : boardview::all) views.push_back(boardview::name(v));
    _boardView.emplace(slots[0], "Board view: ", views, static_cast<int>(app.settings.boardView));
    _boardView->enterAfter(0.0f);
    _toggle.emplace(slots.back(), app.settings.reduceMotion, "Motion: Reduced", "Motion: Full");
  }
  if (_toggle) _toggle->enterAfter((count - 1) * UI::Motion::stagger);
  _options = ui::ButtonList(labels, slots);
  if (tab == Theme) _options.selected = _themeIndex;
  // Highlight the track that is playing (music is global, not per screen)
  if (tab == Music)
    for (size_t i = 0; i < labels.size(); ++i)
      if (labels[i] == app.settings.music) _options.selected = static_cast<int>(i);
}

void SettingsScreen::update(App& app, float dt) {
  if (_tabs.update(dt) >= 0 && _tabs.selected != _tab) openTab(app, _tabs.selected);

  const int picked = _options.update(dt);
  if (picked >= 0 && _tab == Theme) {
    _themeIndex = picked;
    if (const PieceTheme* theme = Themes::byName(kThemes[picked])) app.themes.setTheme(*theme);
  } else if (picked >= 0 && _tab == Music) {
    app.audio.playMusic(picked == 0 ? AudioManager::offName() : AudioManager::tracks()[picked - 1].name);
  }
  if (_toggle) _toggle->update(dt);
  if (_boardView && _boardView->update(dt)) app.settings.boardView = boardview::all[_boardView->value()];

  if (_back.update(dt, app.screens.navShown())) app.screens.replace(std::make_unique<MainMenuScreen>());
}

void SettingsScreen::draw(App& app) const {
  UI::drawSceneTitle("Settings");
  _tabs.draw();
  _options.draw();
  if (_toggle) _toggle->draw();
  if (_boardView) _boardView->draw();

  if (_tab == Theme && _themeIndex >= 0) {  // preview of the chosen piece theme
    for (const auto& [name, x] : {std::pair{"white_pawn", 300.0f}, {"black_king", 450.0f}, {"white_queen", 600.0f}}) {
      Texture2D& piece = app.themes.getPieceTexture(name);
      DrawTexturePro(piece, {0, 0, static_cast<float>(piece.width), static_cast<float>(piece.height)},
                     {x, 300, 100.0f, 100.0f}, {0, 0}, 0.0f, WHITE);
    }
  }
  _back.draw(app.screens.navAlpha());
}
