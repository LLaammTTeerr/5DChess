#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "chess.h"
#include "play/BoardCamera.h"
#include "play/BoardLayout.h"
#include "play/BoardScene.h"
#include "play/MultiverseView.h"
#include "play/PromotionPicker.h"
#include "play/SaveMenu.h"
#include "play/Hud.h"
#include "play/MoveAnimator.h"
#include "play/Selection.h"
#include "play/TimelineArrows.h"
#include "Render/UITheme.h"
#include "ui/Screen.h"

// The game. It orchestrates the play:: modules: pointer input -> Selection -> engine calls, the BoardLayout
// (rebuilt only when the game's state changes), the animation state and the camera; draw() paints them.
class PlayScreen : public Screen {
public:
  explicit PlayScreen(const std::string& modeId); // a GameCatalog id
  /// `isAutosave`: the game is the autosaved one (Continue), so it keeps the autosave up to date and deletes it when the game ends.
  explicit PlayScreen(std::shared_ptr<Chess::IGame> game, bool isAutosave = false);
  void update(App& app, float dt) override;
  void draw(App& app) const override;

  /// Developer tools / scripted demos: do what a click on a square, or on Submit, would do.
  void click(Chess::Core::Coord square);
  void submit();
  const Chess::IGame& game() const { return *_game; }
  /// The Guide shows this screen beside its own panel: the Back button is not shown and the boards keep `rightInset`
  /// pixels at the right of the window free.
  void embed(float rightInset);
  /// Developer tools: where a square is on screen right now (its centre), so scripted clicks name squares, not pixels.
  Vector2 squareToScreen(Chess::Core::Coord square) const;

private:
  std::shared_ptr<Chess::IGame> _game;
  play::BoardLayout _layout;
  play::MultiverseView _view;            // board roles, timelines, checks, jumps: rebuilt with the layout
  mutable play::BoardScene _scene;       // background, lanes, ruler, check lines...; bakes its textures lazily
  play::PromotionPicker _picker;
  play::Selection _selection;
  play::BoardCamera _camera;
  play::MoveAnimator _animator;
  play::TimelineArrows _arrows;
  play::HudMotion _hudMotion;
  play::HudData _hud;
  play::ActionRow _actions;
  play::SaveMenu _saveMenu;
  ui::Button _back = backButton();

  std::optional<Chess::Core::Coord> _hover;
  bool _ended = false;
  bool _embedded = false;
  float _rightInset = UI::Layout::sideInset;
  bool _autosaveWarned = false; // "Could not autosave" was shown
  bool _autosaving = false; // this game writes the autosave after each submitted turn (set by the first one, or by Continue)

  // Answers that are not free (a threat search over the multiverse), cached until the game's state changes
  bool _canSubmit = false, _noMandatoryBoard = true;

  void boardInput();
  void updatePicker(float dt, const play::BoardStyle& style);
  void perform(const play::Intent& intent);
  void makeMove(const Chess::Core::Move& move);
  void undo();
  void submitTurn();
  void leave(App& app);
  void deselect();
  void clearSelection();
  /// After anything changed: rebuild what depends on the game state, button states, HUD, end of game.
  void refresh();
  void rebuild();
  std::string hint() const;
};
