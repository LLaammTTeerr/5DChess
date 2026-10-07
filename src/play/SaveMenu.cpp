#include "play/SaveMenu.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "ui/Audit.h"
#include "engine/Notation.h"
#include "play/Hud.h"
#include "services/SaveStore.h"

namespace play {

namespace {

constexpr float kLeft = 25.0f, kTop = 55.0f, kButtonW = 94.0f, kPanelW = 470.0f, kHeader = 38.0f, kRow = 40.0f, kGap = 6.0f, kPad = 10.0f;
constexpr float kMessageSeconds = 3.5f;

bool inside(Rectangle r, Vector2 p) { return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height; }

std::string slotLabel(int slot) {
  return "Slot " + std::to_string(slot + 1) + ": " + App::current().saves.slot(slot).headline();
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

Rectangle SaveMenu::slotRect(int slot) const {
  const Rectangle panel = panelRect();
  return {panel.x + kPad, panel.y + kHeader + slot * (kRow + kGap), panel.width - 2 * kPad, kRow};
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
    slots.push_back(slotRect(i));
    _occupied[i] = App::current().saves.slot(i).state != savegame::SlotSummary::State::Empty;
  }
  _slots = ui::ButtonList(labels, slots);
  _slots.selectable = false;
  _slots.setEllipsize(true);
  for (int i = 0; i < savegame::kSlots; ++i)
    _slots.setLabel(static_cast<size_t>(i), labels[static_cast<size_t>(i)], App::current().saves.slot(i).detail());
  _overwrite.clear();
  _open = true;
}

void SaveMenu::update(float dt, bool reachable, const BoardStyle& style, const Chess::IGame& game, const VsAi* vs, bool computerMoving) {
  _save.skin = _copy.skin = _slots.skin = &style.skin;
  _pending = !game.pendingMoves().empty() && !computerMoving;
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
      const std::string record = Chess::writeRecord(game);
      SetClipboardText((vs ? withMeta(record, *vs) : record).c_str());
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

  const Vector2 pointer = Input::mousePosition();
  int hovered = -1;
  for (int i = 0; i < savegame::kSlots; ++i)
    if (inside(slotRect(i), pointer)) hovered = i;
  const int armedBefore = _overwrite.armed();
  _overwrite.update(dt, hovered);
  if (_overwrite.armed() != armedBefore && armedBefore >= 0)
    _slots.setLabel(armedBefore, slotLabel(armedBefore), App::current().saves.slot(armedBefore).detail()); // disarmed (timeout or the pointer left)
  int clicked = _slots.update(dt, true);
  if (clicked >= 0 && _occupied[clicked]) {
    // Overwriting asks once more: the first click only arms the slot (and a double-click does not confirm)
    if (!_overwrite.click(clicked)) {
      _slots.setLabel(clicked, "Overwrite slot " + std::to_string(clicked + 1) + "?");
      clicked = -1;
    }
  }
  if (clicked >= 0) {
    bool dropped = false;
    App& app = App::current();
    if (app.saves.saveSlot(clicked, game, savegame::timestamp(), &dropped, vs)) {
      dropped = dropped && !computerMoving;
      say("Saved to slot " + std::to_string(clicked + 1) + (dropped ? " (unsubmitted moves left out)" : ""), dropped);
      openPanel(); // the slot list shows the new save
    } else {
      say("Could not save the game", true);
    }
  }
  const Rectangle panel = panelRect();
  const Vector2 mouse = Input::mousePosition();
  if (inside(panel, mouse)) ui::consumePointer();
  else if (Input::mousePressed(MOUSE_BUTTON_LEFT) && !saveClicked && !inside(_save.rect, mouse) && !inside(_copy.rect, mouse)) {
    _open = false;
    ui::consumePointer(); // this click only closed the panel: it is not a click on the board underneath
  }
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
    ui::audit::within("save panel header", header, {panel.x + kPad + 4.0f, panel.y + (kHeader - fs) / 2,
                                                    MeasureTextEx(font, header.c_str(), fs, 0).x, fs}, {panel.x, panel.y, panel.width - kPad, kHeader});
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
