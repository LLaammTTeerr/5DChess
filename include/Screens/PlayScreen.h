#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "chess.h"
#include "play/BoardCamera.h"
#include "play/BoardLayout.h"
#include "play/Hud.h"
#include "play/MoveAnimator.h"
#include "play/Selection.h"
#include "play/TimelineArrows.h"
#include "ui/Screen.h"

// The game. It orchestrates the play:: modules: pointer input -> Selection -> engine calls, the BoardLayout
// (rebuilt only when the game's state changes), the animation state and the camera; draw() paints them.
class PlayScreen : public Screen {
public:
  explicit PlayScreen(const std::string& modeId); // a GameCatalog id
  explicit PlayScreen(std::shared_ptr<Chess::IGame> game);
  void update(App& app, float dt) override;
  void draw(App& app) const override;

  /// Developer tools / scripted demos: do what a click on a square, or on Submit, would do.
  void click(Chess::Core::Coord square);
  void submit();
  const Chess::IGame& game() const { return *_game; }

private:
  std::shared_ptr<Chess::IGame> _game;
  play::BoardLayout _layout;
  play::Selection _selection;
  play::BoardCamera _camera;
  play::MoveAnimator _animator;
  play::TimelineArrows _arrows;
  play::HudMotion _hudMotion;
  play::HudData _hud;
  play::ActionRow _actions;
  ui::Button _back = backButton();

  std::optional<Chess::Core::Coord> _hover;
  bool _ended = false;

  // Answers that are not free (a threat search over the multiverse), cached until the game's state changes
  bool _canSubmit = false, _noMandatoryBoard = true;
  std::vector<play::BoardKey> _moveable; // boards the side to move may still move from (sorted)
  float _presentHalfTurn = 0.0f;

  void boardInput();
  void perform(const play::Intent& intent);
  void makeMove(const Chess::Core::Move& move);
  void undo();
  void submitTurn();
  void deselect();
  void clearSelection();
  /// After anything changed: rebuild what depends on the game state, button states, HUD, end of game.
  void refresh();
  void rebuild();
  std::string hint() const;
};
