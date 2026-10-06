#pragma once
#include <memory>
#include <vector>
#include <raylib.h>
#include "Render/Motion.h"
#include "ui/Widgets.h"

struct App;

// One screen of the game: owns its widgets and its state, updates on the fixed order the stack calls it in
// and draws itself. Navigation is a direct call on the stack:
//   app.screens.replace(std::make_unique<ModeSelectScreen>());
class Screen {
public:
  virtual ~Screen() = default;
  virtual void update(App& app, float dt) = 0;
  virtual void draw(App& app) const = 0;
};

// The top-left Back button (Settings and the game; a navigation control: ESC hides it, see below).
ui::Button backButton();

// Owns the screens. push / pop / replace are applied at the end of the frame's update (a screen can safely
// navigate from inside its own update) and cross-fade from a snapshot of the outgoing screen. It also owns
// the "navigation controls" visibility that ESC toggles: screens draw their navigation buttons with
// navAlpha() and only update them while navShown().
class ScreenStack {
public:
  ~ScreenStack();

  void push(std::unique_ptr<Screen> screen);
  void replace(std::unique_ptr<Screen> screen);

  void update(App& app, float dt);
  void draw(App& app) const;

  /// The screen on top (nullptr when there is none): for developer tools that drive the game screen.
  Screen* top() const { return _stack.empty() ? nullptr : _stack.back().get(); }

  bool navShown() const { return _navShown; }
  float navAlpha() const { return _navAlpha; }

private:
  struct Change { enum Kind { Push, Replace } kind; std::unique_ptr<Screen> screen; };

  std::vector<std::unique_ptr<Screen>> _stack;
  std::vector<Change> _pending;

  bool _navShown = true;
  float _navAlpha = 1.0f;
  UI::Motion::Tween _navFade;

  UI::Motion::Tween _fade;       // 0 -> 1 progress of the cross-fade
  RenderTexture2D _snapshot{};   // outgoing screen, drawn over the new one with alpha 1 - progress
  bool _hasSnapshot = false;

  void applyPending(App& app);
  void captureSnapshot(App& app);
  void releaseSnapshot();
};
