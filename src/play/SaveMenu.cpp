#include "play/SaveMenu.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "engine/Notation.h"
#include "play/Hud.h"
#include "services/SaveStore.h"

namespace play {

namespace {

constexpr float kLeft = 25.0f, kTop = 55.0f, kButtonW = 94.0f, kPanelW = 470.0f, kHeader = 38.0f, kRow = 40.0f, kGap = 6.0f, kPad = 10.0f;
constexpr float kMessageSeconds = 3.5f;

bool inside(Rectangle r, Vector2 p) { return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height; }

std::string slotLabel(int slot) {
  return "Slot " + std::to_string(slot + 1) + ": " + App::current().saves.slot(slot).describe();
}

} // namespace

SaveMenu::SaveMenu() {
  _save.rect = {kLeft, kTop, kButtonW, UI::Space::buttonHeight};
  _copy.rect = {kLeft + kButtonW + UI::Space::buttonSpacing, kTop, kButtonW, UI::Space::buttonHeight};
  _save.enterAfter(0.0f);
  _copy.enterAfter(0.0f);
}

Rectangle SaveMenu::panelRect() const {
  const float top = kTop + UI::Space::buttonHeight + kGap + 2.0f;
  return {kLeft, top, kPanelW, kHeader + savegame::kSlots * (kRow + kGap) + kPad - kGap};
}

void SaveMenu::say(std::string text, bool warn) {
  _message = std::move(text);
  _warn = warn;
  _messageClock = 0.0f;
}

void SaveMenu::openPanel() {
  const Rectangle panel = panelRect();
  std::vector<std::string> labels;
  std::vector<Rectangle> slots;
  for (int i = 0; i < savegame::kSlots; ++i) {
    labels.push_back(slotLabel(i));
    slots.push_back({panel.x + kPad, panel.y + kHeader + i * (kRow + kGap), panel.width - 2 * kPad, kRow});
  }
  _slots = ui::ButtonList(labels, slots);
  _slots.selectable = false;
  _open = true;
}

void SaveMenu::update(float dt, bool reachable, const BoardStyle& style, const Chess::IGame& game) {
  _save.skin = _copy.skin = _slots.skin = &style.skin;
  _pending = !game.pendingMoves().empty();
  if (!_message.empty()) {
    _messageClock += dt;
    if (_messageClock > kMessageSeconds) _message.clear();
  }

  const bool saveClicked = _save.update(dt, reachable);
  bool copyClicked = false;
#ifndef __EMSCRIPTEN__
  copyClicked = _copy.update(dt, reachable);
#endif
  if (saveClicked) {
    if (_open) _open = false;
    else openPanel();
  }
  if (copyClicked) {
    try {
      SetClipboardText(Chess::writeRecord(game).c_str());
      say("Record copied to clipboard", false);
    } catch (const std::exception&) {
      say("This game can't be copied", true);
    }
  }
  if (!_open) return;
  if (!reachable) { // ESC hid the navigation controls: the panel goes with them
    _open = false;
    return;
  }

  const int clicked = _slots.update(dt, true);
  if (clicked >= 0) {
    bool dropped = false;
    App& app = App::current();
    if (app.saves.saveSlot(clicked, game, savegame::timestamp(), &dropped)) {
      say("Saved to slot " + std::to_string(clicked + 1) + (dropped ? " (unsubmitted moves left out)" : ""), dropped);
      openPanel(); // the slot list shows the new save
    } else {
      say("Could not save the game", true);
    }
  }
  const Rectangle panel = panelRect();
  const Vector2 mouse = Input::mousePosition();
  if (inside(panel, mouse)) ui::consumePointer();
  else if (Input::mousePressed(MOUSE_BUTTON_LEFT) && !saveClicked && !inside(_save.rect, mouse) && !inside(_copy.rect, mouse)) _open = false;
}

void SaveMenu::draw(const BoardStyle& style, float navAlpha) const {
  if (navAlpha <= 0.003f) return;
  _save.draw(navAlpha);
#ifndef __EMSCRIPTEN__
  _copy.draw(navAlpha);
#endif
  const ::Font font = UI::Fonts::body();
  const float fs = UI::Font::body;
  if (_open) {
    const Rectangle panel = panelRect();
    drawPanel(panel, style, navAlpha, 0.1f);
    // The header: the last result, else the warning that unsubmitted moves are not part of a save, else the invitation
    std::string header = "Save to slot";
    ::Color color = style.hudText;
    if (!_message.empty()) {
      header = _message;
      color = _warn ? style.check : style.hudText;
    } else if (_pending) {
      header = "Unsubmitted moves are not saved";
      color = style.check;
    }
    DrawTextEx(font, header.c_str(), {panel.x + kPad + 4.0f, std::floor(panel.y + (kHeader - fs) / 2 - 1)}, fs, 0, color);
    _slots.draw(navAlpha);
  } else if (!_message.empty()) {
    const float w = MeasureTextEx(font, _message.c_str(), fs, 0).x + 2 * kPad + 8.0f;
    const Rectangle pill = {kLeft, kTop + UI::Space::buttonHeight + kGap + 2.0f, w, 34.0f};
    drawPanel(pill, style, navAlpha);
    DrawTextEx(font, _message.c_str(), {pill.x + kPad + 4.0f, std::floor(pill.y + (pill.height - fs) / 2 - 1)}, fs, 0,
               _warn ? style.check : style.hudText);
  }
}

} // namespace play
