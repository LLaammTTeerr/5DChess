#include "ui/Screen.h"
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"

ui::Button backButton() {
  ui::Button b("Back", {25.0f, 3.0f, 200.0f, UI::Space::buttonHeight});
  b.enterAfter(0.0f);
  return b;
}

ScreenStack::~ScreenStack() { releaseSnapshot(); }

void ScreenStack::push(std::unique_ptr<Screen> screen) { _pending.push_back({Change::Push, std::move(screen)}); }
void ScreenStack::replace(std::unique_ptr<Screen> screen) { _pending.push_back({Change::Replace, std::move(screen)}); }

void ScreenStack::update(App& app, float dt) {
  // Visual-only fades advance first. A click while the cross-fade runs finishes it instantly and is consumed:
  // it must not also reach the new screen.
  const bool clickConsumed = _hasSnapshot && Input::mousePressed(MOUSE_BUTTON_LEFT);
  if (clickConsumed) _fade.finish();
  _fade.update(dt);
  if (_hasSnapshot && _fade.done()) releaseSnapshot();
  _navFade.update(dt);
  _navAlpha = _navFade.active ? _navFade.value() : (_navShown ? 1.0f : 0.0f);

  if (!_pending.empty()) {
    applyPending(app);  // navigation asked for last frame; the frame that swaps screens runs no screen update
  } else {
    ui::beginFrame(clickConsumed);
    if (!_stack.empty()) _stack.back()->update(app, dt);
  }

  if (Input::keyPressed(KEY_ESCAPE)) {  // show / hide the navigation controls
    _navShown = !_navShown;
    using namespace UI::Motion;
    _navFade.start(_navAlpha, _navShown ? 1.0f : 0.0f, _navShown ? base : exitDuration(base),
                   _navShown ? easeOutCubic : easeInCubic, 0.0f, true);
  }
}

void ScreenStack::draw(App& app) const {
  UI::Cursor::beginFrame();
  if (!_stack.empty()) _stack.back()->draw(app);

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
  _snapshot = RenderTexture2D{};
  _hasSnapshot = false;
}
