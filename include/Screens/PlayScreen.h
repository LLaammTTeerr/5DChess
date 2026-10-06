#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "ai/Search.h"
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
#include "play/VsAi.h"
#include "Render/UITheme.h"
#include "ui/Screen.h"

// The game. It orchestrates the play:: modules: pointer input -> Selection -> engine calls, the BoardLayout
// (rebuilt only when the game's state changes), the animation state and the camera; draw() paints them.
class PlayScreen : public Screen {
public:
  /// `vs`: a game against the computer (the player's side, the level and the game's seed); none: two players at one screen.
  explicit PlayScreen(const std::string& modeId, std::optional<play::VsAi> vs = std::nullopt); // a GameCatalog id
  /// `isAutosave`: the game is the autosaved one (Continue), so it keeps the autosave up to date and deletes it when the game ends.
  explicit PlayScreen(std::shared_ptr<Chess::IGame> game, bool isAutosave = false, std::optional<play::VsAi> vs = std::nullopt);
  void update(App& app, float dt) override;
  void draw(App& app) const override;

  /// Developer tools / scripted demos: do what a click on a square, or on Submit, would do.
  void click(Chess::Core::Coord square);
  void submit();
  const Chess::IGame& game() const { return *_game; }
  /// The Guide shows this screen beside its own panel: the Back button is not shown and the boards keep `rightInset`
  /// pixels at the right of the window free.
  void embed(float rightInset);
  /// Against the computer and it is not yet quiet: it is the computer's turn (thinking or playing its moves) or a legal-turn search
  /// is still running. The UI test harness waits for this to turn false (`waitai`).
  bool aiBusy() const;
  /// Puzzles (Screens/PuzzleScreen): the embedding screen drives the game. playMove() makes one move as a click would (with its flight
  /// animation, promotion included); highlight() rings a square (the hint); setInputLocked() ignores the board and the action row
  /// (while the opponent's reply plays); showEndCard(false) leaves the "wins" card to the embedding screen.
  void playMove(const Chess::Core::Move& move);
  void highlight(std::optional<Chess::Core::Coord> square) { _highlight = square; }
  void setInputLocked(bool locked) { _locked = locked; }
  void showEndCard(bool shown) { _showEndCard = shown; }
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
  std::optional<Chess::Core::Coord> _highlight;
  bool _locked = false, _showEndCard = true;
  bool _autosaveWarned = false; // "Could not autosave" was shown
  bool _autosaving = false; // this game writes the autosave after each submitted turn (set by the first one, or by Continue)

  // The computer opponent (docs/AI.md, "UI integration"): a search advanced a little each frame, then its moves played one by one
  std::optional<play::VsAi> _vs;
  std::unique_ptr<Chess::ai::Search> _search;
  std::vector<Chess::Core::Move> _aiMoves; // the finished search's turn, played through makeMove one move at a time
  size_t _aiNext = 0;
  bool _aiPlaying = false;     // _aiMoves is being played (the turn ends with its submit)
  std::optional<size_t> _aiGaveUpAt; // submitted turns when the search found no turn although the game goes on: that turn is played by hand
  float _aiClock = 0.0f;       // seconds the computer has been at this turn (a minimum "thinking" time, then the gap between moves)

  // Answers that are not free (a threat search over the multiverse), cached until the game's state changes
  bool _canSubmit = false, _noMandatoryBoard = true;

  void boardInput();
  void updatePicker(float dt, const play::BoardStyle& style);
  void perform(const play::Intent& intent);
  void makeMove(const Chess::Core::Move& move);
  void undo();
  /// Against the computer: the turns that "Undo" takes back now (0: none; 1: the player's turn while the computer thinks; 2: the
  /// player's last turn and the computer's reply).
  int takeBackCount() const;
  bool gaveUpNow() const;
  bool computerOpenedOnly() const;
  void takeBack(int turns);
  void setGame(std::shared_ptr<Chess::IGame> game);
  bool aiToMove() const;
  void updateAi(float dt);
  void cancelAi();
  void finishAiTurn();
  void autosaveNow();
  void submitTurn();
  void leave(App& app);
  void deselect();
  void clearSelection();
  /// After anything changed: rebuild what depends on the game state, button states, HUD, end of game.
  void refresh();
  void rebuild();
  std::string hint() const;
};
