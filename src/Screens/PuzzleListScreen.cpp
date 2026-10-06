#include "Screens/PuzzleListScreen.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "App.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/PuzzleScreen.h"
#include "puzzles/Progress.h"
#include "puzzles/Puzzle.h"

namespace {

constexpr float kColW = 400.0f, kColGap = 24.0f, kHeaderY = 148.0f, kRowsTop = 196.0f, kRowGap = 10.0f, kMaxRowH = 52.0f;
constexpr float kPad = 16.0f;

void drawCheck(float x, float y, float size, ::Color color) {
  DrawLineEx({x, y + size * 0.55f}, {x + size * 0.38f, y + size * 0.9f}, 3.0f, color);
  DrawLineEx({x + size * 0.38f, y + size * 0.9f}, {x + size, y + size * 0.12f}, 3.0f, color);
}

} // namespace

PuzzleListScreen::PuzzleListScreen() {
  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  const auto& all = puzzles::all();
  int perTier[puzzles::kTiers] = {};
  for (const puzzles::Puzzle& p : all) ++perTier[p.tier - 1];
  const int tallest = std::max(1, *std::max_element(perTier, perTier + puzzles::kTiers));
  const float rowsBottom = H - 130.0f; // above the Back button
  const float rowH = std::min(kMaxRowH, (rowsBottom - kRowsTop) / static_cast<float>(tallest) - kRowGap);

  const float left = (W - (puzzles::kTiers * kColW + (puzzles::kTiers - 1) * kColGap)) / 2.0f;
  int placed[puzzles::kTiers] = {};
  for (int t = 0; t < puzzles::kTiers; ++t) _columns[t] = {left + static_cast<float>(t) * (kColW + kColGap), kHeaderY, kColW, 0.0f};
  for (size_t i = 0; i < all.size(); ++i) {
    const int tier = all[i].tier - 1;
    const Rectangle r = {_columns[tier].x, kRowsTop + static_cast<float>(placed[tier]) * (rowH + kRowGap), kColW, rowH};
    Row row;
    row.button = ui::Button("", r);
    row.button.enterAfter(static_cast<float>(_rows.size()) * UI::Motion::stagger * 0.5f);
    row.puzzle = static_cast<int>(i);
    _rows.push_back(std::move(row));
    ++placed[tier];
  }
  _back = ui::Button("Back", {(W - 200.0f) / 2.0f, H - 110.0f, 200.0f, UI::Space::buttonHeight});
  _back.enterAfter(0.0f);
}

void PuzzleListScreen::update(App& app, float dt) {
  const bool nav = app.screens.navShown();
  for (Row& row : _rows) {
    if (row.button.update(dt, nav)) {
      app.screens.replace(std::make_unique<PuzzleScreen>(row.puzzle));
      return;
    }
  }
  if (_back.update(dt, nav)) app.screens.replace(std::make_unique<MainMenuScreen>());
}

void PuzzleListScreen::draw(App& app) const {
  const auto& all = puzzles::all();
  const puzzles::Progress& progress = puzzles::progress();
  const float cx = GetScreenWidth() / 2.0f;
  UI::drawSceneTitle("Puzzles");

  size_t solved = 0;
  int perTier[puzzles::kTiers] = {}, solvedTier[puzzles::kTiers] = {};
  for (const puzzles::Puzzle& p : all) {
    ++perTier[p.tier - 1];
    if (progress.solved(p.id)) {
      ++solved;
      ++solvedTier[p.tier - 1];
    }
  }
  const std::string total = std::to_string(solved) + " of " + std::to_string(all.size()) + " solved";
  UI::drawTextCentered(UI::Fonts::body(), all.empty() ? "No puzzles found" : total.c_str(), cx, 104.0f, UI::Font::body, UI::Color::textMuted);

  for (int t = 0; t < puzzles::kTiers; ++t) {
    const Rectangle c = _columns[t];
    const std::string name = std::string(puzzles::tierName(t + 1));
    DrawTextEx(UI::Fonts::button(), name.c_str(), {std::floor(c.x), std::floor(c.y)}, UI::Font::button, 0.0f, UI::Color::text);
    const std::string count = std::to_string(solvedTier[t]) + "/" + std::to_string(perTier[t]);
    const Vector2 size = MeasureTextEx(UI::Fonts::mono(), count.c_str(), UI::Font::mono, 0.0f);
    DrawTextEx(UI::Fonts::mono(), count.c_str(), {std::floor(c.x + c.width - size.x), std::floor(c.y + 3.0f)}, UI::Font::mono, 0.0f,
               UI::Color::textMuted);
    DrawLineEx({c.x, c.y + 34.0f}, {c.x + c.width, c.y + 34.0f}, 1.0f, UI::Color::border);
  }

  const float alpha = 1.0f;
  for (const Row& row : _rows) {
    row.button.draw(alpha);
    const puzzles::Puzzle& p = all[static_cast<size_t>(row.puzzle)];
    const Rectangle r = row.button.rect;
    const bool done = progress.solved(p.id);
    const float midY = r.y + r.height / 2.0f;
    DrawTextEx(UI::Fonts::button(), p.title.c_str(), {std::floor(r.x + kPad), std::floor(midY - UI::Font::button / 2.0f)}, UI::Font::button,
               0.0f, UI::Color::text);
    float right = r.x + r.width - kPad;
    if (done) {
      drawCheck(right - 20.0f, midY - 10.0f, 20.0f, UI::Color::legalTarget);
      right -= 34.0f;
    }
    const std::string goal = p.goalText();
    const Vector2 size = MeasureTextEx(UI::Fonts::mono(), goal.c_str(), UI::Font::mono, 0.0f);
    DrawTextEx(UI::Fonts::mono(), goal.c_str(), {std::floor(right - size.x), std::floor(midY - UI::Font::mono / 2.0f)}, UI::Font::mono, 0.0f,
               UI::Color::textMuted);
  }
  UI::drawTextCentered(UI::Fonts::body(), "Pick a puzzle. White always moves first.", cx, static_cast<float>(GetScreenHeight()) - 44.0f,
                       UI::Font::body, UI::Color::textMuted);
  _back.draw(app.screens.navAlpha());
}
