#include "Screens/SettingsScreen.h"
#include "App.h"
#include "Audio/AudioManager.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "play/BoardView.h"
#include "ui/Audit.h"
#include <cmath>
#include <string>
#include <vector>

namespace {
// Every tab has the same two columns: the options at the left (a column from kColumnTop down), what they do at the right
constexpr float kColumnX = 50.0f, kOptionW = 300.0f, kOptionGap = 20.0f, kColumnTop = 222.0f;
constexpr float kPaneX = kColumnX + kOptionW + 60.0f;

// Greedy word wrap to `width` pixels
std::vector<std::string> wrapText(::Font font, float size, const std::string& text, float width) {
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

const char* describeView(BoardView view) {
  switch (view) {
    case BoardView::DeepSpace: return "Deep space: a dark starfield with glowing boards, easy on the eyes at night.";
    case BoardView::Atlas: return "Atlas: warm paper, a coloured band for every timeline.";
    case BoardView::Blueprint: return "Blueprint: sharp ink lines on white, like a technical drawing.";
  }
  return "";
}
}

SettingsScreen::SettingsScreen() {
  const auto slots = ui::row({0.0f, 150.0f, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight}, 3, 160.0f,
                             UI::Space::buttonSpacing + UI::Space::sm);
  _tabs = ui::ButtonList({"Piece Theme", "Music", "Display"}, slots);
}

void SettingsScreen::openTab(App& app, int tab) {
  _tab = tab;
  if (_themeIndex < 0)  // highlight the theme in use (the saved one, or the default)
    for (int i = 0; i < Themes::count; ++i)
      if (Themes::all[i].theme->prefix == std::string(app.settings.theme.prefix)) _themeIndex = i;
  const float H = static_cast<float>(GetScreenHeight());
  const Rectangle area = {kColumnX, kColumnTop, kOptionW, H - kColumnTop};
  const auto& tracks = AudioManager::tracks();
  // Music: Off, tracks, SFX toggle; Display: board view, motion toggle
  const int count = tab == Theme ? Themes::count : tab == Music ? static_cast<int>(tracks.size()) + 2 : 2;
  const auto slots = ui::column(area, count, UI::Space::buttonHeight, kOptionGap, ui::Align::Top);

  std::vector<std::string> labels;
  _toggle.reset();
  _boardView.reset();
  _options = {};
  if (tab == Theme) {
    for (const auto& e : Themes::all) labels.push_back(e.name);
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
    app.themes.setTheme(*Themes::all[picked].theme);
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
    for (const auto& [name, x] : {std::pair{"white_pawn", kPaneX}, {"black_king", kPaneX + 150.0f}, {"white_queen", kPaneX + 300.0f}}) {
      Texture2D& piece = app.themes.getPieceTexture(name);
      DrawTexturePro(piece, {0, 0, static_cast<float>(piece.width), static_cast<float>(piece.height)},
                     {x, kColumnTop + 40.0f, 100.0f, 100.0f}, {0, 0}, 0.0f, WHITE);
    }
  }
  // What the options of the open tab do, at the right
  std::string note;
  if (_tab == Music) note = "Music plays on every screen. Choose Off to silence it; sound effects are separate.";
  else if (_tab == Display && _boardView)
    note = std::string(describeView(boardview::all[_boardView->value()])) + " Reduced motion swaps slides and bounces for quick fades.";
  if (!note.empty()) {
    const ::Font font = UI::Fonts::body();
    const float room = static_cast<float>(GetScreenWidth()) - kPaneX - 50.0f;
    float y = kColumnTop + 6.0f;
    if (note != _noteText) { // wrapped once per change of tab or board view, not every frame
      _noteText = note;
      _noteLines = wrapText(font, UI::Font::body, note, room);
    }
    for (const std::string& line : _noteLines) {
      DrawTextEx(font, line.c_str(), {kPaneX, std::floor(y)}, UI::Font::body, 0.0f, UI::Color::textMuted);
      if (ui::audit::enabled()) ui::audit::fit("settings note", line, MeasureTextEx(font, line.c_str(), UI::Font::body, 0.0f).x, UI::Font::body, {0, 0, room, 30.0f});
      y += 26.0f;
    }
  }
  _back.draw(app.screens.navAlpha());
}
