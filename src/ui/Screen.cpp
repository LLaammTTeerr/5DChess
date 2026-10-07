#include "ui/Screen.h"
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include <rlgl.h>

ui::Button backButton() {
  ui::Button b("Back", {25.0f, 3.0f, 200.0f, UI::Space::buttonHeight});
  b.enterAfter(0.0f);
  return b;
}

ScreenStack::~ScreenStack() { releaseSnapshot(); }

void ScreenStack::push(std::unique_ptr<Screen> screen) { _pending.push_back({Change::Push, std::move(screen)}); }
void ScreenStack::replace(std::unique_ptr<Screen> screen) { _pending.push_back({Change::Replace, std::move(screen)}); }

void ScreenStack::update(App& app, float dt) {
  // Visual-only fades advance first. A click while the cross-fade runs finishes it, and still reaches the new screen (which
  // already takes input): the player's first click after a transition must not be lost.
  if (_hasSnapshot && Input::mousePressed(MOUSE_BUTTON_LEFT)) _fade.finish();
  _fade.update(dt);
  if (_hasSnapshot && _fade.done()) releaseSnapshot();
  _navFade.update(dt);
  _navAlpha = _navFade.active ? _navFade.value() : (_navShown ? 1.0f : 0.0f);

  if (!_pending.empty()) {
    applyPending(app);  // navigation asked for last frame; the frame that swaps screens runs no screen update
  } else {
    ui::beginFrame(false);
    if (!_stack.empty()) _stack.back()->update(app, dt);
  }

  if (Input::keyPressed(KEY_ESCAPE) && !_stack.empty() && _pending.empty()) {  // Esc cancels what is open, else goes back; it hides nothing
    if (!_stack.back()->escape(app) && _trail.size() > 1) _stack.back()->back(app);
  }
  if (Input::keyPressed(KEY_H)) {  // show / hide the navigation controls
    _navShown = !_navShown;
    using namespace UI::Motion;
    _navFade.start(_navAlpha, _navShown ? 1.0f : 0.0f, _navShown ? base : exitDuration(base),
                   _navShown ? easeOutCubic : easeInCubic, 0.0f, true);
  }
}

void ScreenStack::draw(App& app) const {
  UI::Cursor::beginFrame();
  if (!_stack.empty()) {
    // The incoming screen slides 12 px in the direction of travel while the old one fades out over it (no slide under Reduce motion)
    const float slide = (_hasSnapshot && !UI::Motion::reduced()) ? 12.0f * static_cast<float>(_slideDir) * (1.0f - _fade.progress()) : 0.0f;
    if (slide != 0.0f) {
      // The screen is drawn to a texture and blitted offset: a matrix would not move scissor clips or 2D-camera modes
      if (_incoming.id == 0 || _incoming.texture.width != GetScreenWidth() || _incoming.texture.height != GetScreenHeight()) {
        if (_incoming.id != 0) UnloadRenderTexture(_incoming);
        _incoming = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
      }
      BeginTextureMode(_incoming);
      ClearBackground(UI::Color::bg);
      _stack.back()->draw(app);
      UI::restoreOpaqueAlpha(GetScreenWidth(), GetScreenHeight());
      EndTextureMode();
      ClearBackground(UI::Color::bg);
      const Texture2D& t = _incoming.texture;
      DrawTextureRec(t, {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)}, {slide, 0}, WHITE);
    } else {
      _stack.back()->draw(app);
    }
  }

  if (_hasSnapshot) {  // the outgoing screen fades out on top of the new one (which already takes input)
    const float a = 1.0f - _fade.progress();
    if (a > 0.003f) {
      const Texture2D& t = _snapshot.texture;
      DrawTextureRec(t, {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)}, {0, 0},
                     UI::withAlpha(WHITE, static_cast<unsigned char>(255.0f * a)));
    }
  }
}

void ScreenStack::applyPending(App& app) {
  if (_pending.empty()) return;
  captureSnapshot(app);  // cross-fade from what is on screen now
  for (Change& c : _pending) {
    if (c.kind == Change::Replace && !_stack.empty()) _stack.pop_back();
    _stack.push_back(std::move(c.screen));
  }
  _pending.clear();
  // Forward unless this is the screen before the one that was showing (Back)
  const std::type_index now(typeid(*_stack.back()));
  if (!_trail.empty() && _trail.back() == now) {
    _slideDir = 0;
  } else if (_trail.size() >= 2 && _trail[_trail.size() - 2] == now) {
    _trail.pop_back();
    _slideDir = -1;
  } else {
    _trail.push_back(now);
    if (_trail.size() > 32) _trail.erase(_trail.begin());
    _slideDir = 1;
  }
  if (_hasSnapshot) _fade.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
}

void ScreenStack::captureSnapshot(App& app) {
  releaseSnapshot();
  if (_stack.empty()) return;
  _snapshot = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  if (_snapshot.id == 0) return;
  BeginTextureMode(_snapshot);
  ClearBackground(UI::Color::bg);
  _stack.back()->draw(app);
  UI::restoreOpaqueAlpha(GetScreenWidth(), GetScreenHeight());
  EndTextureMode();
  _hasSnapshot = true;
}

void ScreenStack::releaseSnapshot() {
  if (_hasSnapshot) UnloadRenderTexture(_snapshot);
  if (_incoming.id != 0) UnloadRenderTexture(_incoming);
  _incoming = RenderTexture2D{};
  _snapshot = RenderTexture2D{};
  _hasSnapshot = false;
}
