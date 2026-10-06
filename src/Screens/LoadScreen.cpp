#include "Screens/LoadScreen.h"
#include "App.h"
#include "Render/UITheme.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/PlayScreen.h"
#include "services/SaveStore.h"

namespace {
constexpr float kListW = 520.0f, kDeleteW = 110.0f, kRowH = UI::Space::buttonHeight + 8.0f, kRowGap = UI::Space::sm + 4.0f;
constexpr float kNavW = 200.0f;
constexpr float kListTop = 140.0f;

std::string slotLabel(App& app, int slot) { return "Slot " + std::to_string(slot + 1) + ": " + app.saves.slot(slot).describe(); }
} // namespace

LoadScreen::LoadScreen() {
  App& app = App::current();
  _back = ui::Button("Back", {});
  _back.enterAfter(0.0f);
  _paste = ui::Button("Paste record", {});
  _paste.enterAfter(0.0f);
  build(app);
  layoutNavRow();
}

// (Re)creates the slot list and the Delete buttons from what the store holds now.
void LoadScreen::build(App& app) {
  const float W = static_cast<float>(GetScreenWidth());
  const float left = (W - (kListW + UI::Space::md + kDeleteW)) / 2.0f;
  std::vector<std::string> labels;
  std::vector<Rectangle> slots;
  for (int i = 0; i < savegame::kSlots; ++i) {
    labels.push_back(slotLabel(app, i));
    slots.push_back({left, kListTop + i * (kRowH + kRowGap), kListW, kRowH});
    _used[i] = app.saves.slot(i).state != savegame::SlotSummary::State::Empty;
    _delete[i] = ui::Button("Delete", {left + kListW + UI::Space::md, slots.back().y, kDeleteW, kRowH});
    _delete[i].enabled = _used[i];
    _delete[i].enterAfter(i * UI::Motion::stagger);
  }
  _slots = ui::ButtonList(labels, slots);
  _slots.selectable = false;
  for (int i = 0; i < savegame::kSlots; ++i) _slots.setEnabled(i, _used[i]);
  _confirming = -1;
}

void LoadScreen::layoutNavRow() {
  const float H = static_cast<float>(GetScreenHeight());
#ifdef __EMSCRIPTEN__
  const int n = 1; // a browser tab cannot read the clipboard
#else
  const int n = 2;
#endif
  const auto slots = ui::row({0.0f, H - 110.0f, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight}, n, kNavW, UI::Space::md);
  _back.rect = slots[0];
  if (n > 1) _paste.rect = slots[1];
}

void LoadScreen::update(App& app, float dt) {
  const bool nav = app.screens.navShown();
  const int clicked = _slots.update(dt, nav);
  if (clicked >= 0) {
    const savegame::LoadResult result = app.saves.loadSlot(clicked);
    if (result) {
      app.screens.replace(std::make_unique<PlayScreen>(result.game));
      return;
    }
    _message = "This save can't be loaded"; // the file stays: it can be deleted here, or looked at in the config folder
    _confirming = -1;
  }
  for (int i = 0; i < savegame::kSlots; ++i) {
    if (!_delete[i].update(dt, nav)) continue;
    if (_confirming == i) {
      app.saves.deleteSlot(i);
      _message.clear();
      build(app);
    } else {
      _confirming = i;
    }
  }
  for (int i = 0; i < savegame::kSlots; ++i) _delete[i].label = _confirming == i ? "Sure?" : "Delete";

  if (_back.update(dt, nav)) {
    app.screens.replace(std::make_unique<MainMenuScreen>());
    return;
  }
#ifndef __EMSCRIPTEN__
  if (_paste.update(dt, nav)) {
    const char* text = GetClipboardText();
    if (!text || !*text) {
      _message = "The clipboard is empty";
    } else if (std::string_view(text).size() > savegame::kMaxBytes) {
      _message = "This record can't be loaded";
    } else if (const savegame::LoadResult result = savegame::loadText(text)) {
      app.screens.replace(std::make_unique<PlayScreen>(result.game));
      return;
    } else {
      _message = "This record can't be loaded";
    }
  }
#endif
}

void LoadScreen::draw(App& app) const {
  UI::drawSceneTitle("Load Game");
  _slots.draw();
  for (const ui::Button& b : _delete) b.draw();
  const float cx = GetScreenWidth() / 2.0f;
  const float H = static_cast<float>(GetScreenHeight());
  if (!_message.empty())
    UI::drawTextCentered(UI::Fonts::button(), _message.c_str(), cx, H - 170.0f, UI::Font::button, UI::Color::capture);
  const char* hint = "Pick a save to continue that game";
  UI::drawTextCentered(UI::Fonts::body(), hint, cx, H - 44.0f, UI::Font::body, UI::Color::textMuted);
  _back.draw(app.screens.navAlpha());
#ifndef __EMSCRIPTEN__
  _paste.draw(app.screens.navAlpha());
#endif
}
