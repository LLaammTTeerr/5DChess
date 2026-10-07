#pragma once
#include <memory>
#include <typeindex>
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
  /// Esc was pressed: cancel whatever is open (a promotion choice, a picked-up piece). Returns whether there was something to
  /// cancel. Esc never hides the navigation controls any more (that is the H key).
  virtual bool escape(App&) { return false; }
  /// Esc with nothing to cancel on a screen that is not the first one: go back (the screen does what its Back button does).
  /// Screens that have a Back button override this; the default does nothing.
  virtual void back(App&) {}
};

// The top-left Back button (Settings and the game; a navigation control: H hides it, see below).
ui::Button backButton();

// Owns the screens. push / pop / replace are applied at the end of the frame's update (a screen can safely
// navigate from inside its own update) and cross-fade from a snapshot of the outgoing screen. It also owns
// the "navigation controls" visibility that H toggles: screens draw their navigation buttons with
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

  // Where the screens came from, as the types visited (a browser-like trail): the new screen slides in from the right when it is
  // a step forward and from the left when it is the screen before the one that is leaving.
  std::vector<std::type_index> _trail;
  int _slideDir = 0;             // +1 forward, -1 back, 0 none (the same screen type again)

  UI::Motion::Tween _fade;       // 0 -> 1 progress of the cross-fade
  RenderTexture2D _snapshot{};   // outgoing screen, drawn over the new one with alpha 1 - progress
  bool _hasSnapshot = false;
  mutable RenderTexture2D _incoming{}; // the incoming screen while it slides in (see draw())

  void applyPending(App& app);
  void captureSnapshot(App& app);
  void releaseSnapshot();
};
