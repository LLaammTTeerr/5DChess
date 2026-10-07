#pragma once
#include <functional>
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
#include "play/PlayFeedback.h"
#include "play/PlayViewLayout.h"
#include "play/Selection.h"
#include "play/TurnPanel.h"
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
  /// The player may not move: the computer is to move, or the embedding screen (a puzzle) has locked the board.
  bool inputBlocked() const { return _locked || aiToMove(); }
  /// Puzzles (Screens/PuzzleScreen): the embedding screen drives the game. playMove() makes one move as a click would (with its flight
  /// animation, promotion included); highlight() rings a square (the hint); setInputLocked() ignores the board and the action row
  /// (while the opponent's reply plays); showEndCard(false) leaves the "wins" card to the embedding screen.
  void playMove(const Chess::Core::Move& move);
  void highlight(std::optional<Chess::Core::Coord> square) { _highlight = square; }
  void setInputLocked(bool locked) { _locked = locked; }
  /// Replaces "White to move" / "Black to move" in the HUD pill (and silences the turn banner) while non-empty.
  void setHudTitle(std::string title) { _hudTitle = std::move(title); }
  void showEndCard(bool shown) { _showEndCard = shown; }
  /// Esc: cancels an open promotion choice, else the picked-up piece (true: there was something to cancel).
  bool escape(App& app) override;
  void back(App& app) override { if (!_embedded) leave(app); }
  /// Developer tools: where a square is on screen right now (its centre), so scripted clicks name squares, not pixels.
  Vector2 squareToScreen(Chess::Core::Coord square) const;
  /// Developer tools: the camera this frame (the UI script's `zoom?`).
  struct CameraInfo {
    float zoom, x, y; // zoom and the world point at the centre of the free area
    const char* state;
    bool moving;
  };
  CameraInfo cameraInfo() const;
  /// Developer tools: the middle of a board's label strip (on its card, on none of its squares).
  Vector2 cardStripToScreen(int timeline, int halfTurn) const;
  /// Developer tools: at least `fraction` of the board's card is inside the free area.
  bool boardVisible(int timeline, int halfTurn, float fraction) const;
  /// A Mandatory board lies (mostly) off-screen: the HUD offers "Next board (Space)".
  bool nextMandatoryOffscreen() const;
  /// [Play view] the second way to show the multiverse (play/PlayView.h): P toggles it. Developer tools: the screen point of one
  /// of its parts, for the UI script's `pvclick <kind> <l> <t>`: "hist" (a timeline's history stack), "tab" (an inspector tab of board l,t),
  /// "chip" (an inactive timeline's chip), "close" (the inspector's close button). nullopt: it is not on screen.
  bool playView() const { return _playView; }
  void setPlayView(bool on) { togglePlayView(on); }
  std::optional<Vector2> playViewPoint(const std::string& what, int timeline, int halfTurn) const;

private:
  std::shared_ptr<Chess::IGame> _game;
  play::BoardLayout _layout;
  play::MultiverseView _view;            // board roles, timelines, checks, jumps: rebuilt with the layout
  mutable play::BoardScene _scene;       // background, lanes, ruler, check lines...; bakes its textures lazily
  play::PromotionPicker _picker;
  play::Selection _selection;
  play::BoardCamera _camera;
  // Pointer: a press that has not moved 5 px is a click on release; farther it is a drag that pans (and never selects)
  bool _pressValid = false, _dragging = false;
  Vector2 _pressPos{}, _flingVelocity{};
  double _lastClickTime = -1e9;
  Vector2 _lastClickPos{};
  bool _selectedBeforeClick = false; // ... and a piece was already picked up
  bool _focusedBeforeClick = false; // the first click of a (possible) double-click found its board already focused
  // Keyboard: the board cursor (a ring around a card), the Space / Tab cycle through the boards that need a move
  std::optional<std::pair<int, int>> _cursor; // (timeline, half-turn)
  bool _cursorShown = false;
  UI::Motion::Spring _cursorX, _cursorY;     // the ring's card position (world)
  int _cycle = -1;
  float _selectedFor = 0.0f;                  // seconds a piece has been picked up (the ghost cards fade after 2 s)
  // A camera action that waits (the computer's / scripted moves: 150 ms, so the flight reads first); player input cancels it
  std::function<void()> _deferredCamera;
  float _deferredDelay = 0.0f;
  bool _submitRule = false, _submitRuleDelayed = false; // a turn was submitted: frame the next boards once the layout has them
  play::MoveAnimator _animator;
  play::PlayFeedback _feedback{_animator}; // feedback motion and cues (play/PlayFeedback)
  play::TimelineArrows _arrows;
  play::HudMotion _hudMotion;
  play::HudData _hud;
  play::ActionRow _actions;
  play::EndCardButtons _endButtons;   // Rematch / Review board / Back to menu on the end card
  bool _reviewing = false;            // "Review board": the end card is dismissed, the final position stays on screen
  play::SaveMenu _saveMenu;
  ui::Button _back = backButton();

  // [TurnPanel] the docked turn checklist (play/TurnPanel): what it asks of the board view, and the flash of a clicked opponent move
  play::TurnPanel _turnPanel;
  struct Flash { Chess::Core::Coord from, to; float left = 0.0f; } _flash;

  std::optional<Chess::Core::Coord> _hover;
  bool _ended = false;
  bool _embedded = false;
  float _rightInset = UI::Layout::sideInset;
  bool _insetsApplied = false; // [TurnPanel]
  std::optional<Chess::Core::Coord> _highlight;
  bool _locked = false, _showEndCard = true;
  std::string _hudTitle;
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
  bool _undoEnabled = false, _submitEnabled = false; // what the action row shows, for the U / Enter keys

  // [TurnPanel]
  play::TurnPanel::Mode turnPanelMode() const;
  float boardInset() const; // what the boards keep free at the right: the Guide's / puzzle's panel, the turn panel, or the margin
  void applyInsets();      // the boards' free area follows the panel (opened, folded, the game ended)
  void toggleTurnPanel();
  void focusBoardFromPanel(int timeline, int halfTurn);
  void showMoveFromPanel(const play::LastMove& move);

  // [Play view] state and the code in Screens/PlayScreenPlayView.cpp (the multiverse view's camera code never runs while it is on)
  bool _playView = false;
  play::PlayViewLayout _pv;
  std::optional<int> _pvCursor; // the timeline whose card the keyboard cursor rings
  void togglePlayView(bool on);
  void playViewInput();
  void playViewPointer(play::PlayFeedback::Pointer& in) const;
  void playViewClick(Vector2 mouse);
  void playViewKeys();
  void playViewPlace();
  void playViewCycle(int direction);
  void playViewCursor(play::BoardLayout::Dir dir);
  void playViewDraw(App& app, const play::BoardStyle& style, const play::PlayFeedback::Draw& fx) const;
  play::Rect playViewArea() const;
  void drawHudLayer(App& app, const play::BoardStyle& style, const play::PlayFeedback::Draw& fx) const;

  void boardInput();
  void onClick(Vector2 mouse);
  /// Click a square as the player would; returns what the click meant.
  play::Intent::Kind clickSquare(Chess::Core::Coord square);
  void handleKeys();
  void goHome(bool snap = false);
  void nextBoard(int direction);
  void setCursor(int timeline, int halfTurn);
  void moveCursor(play::BoardLayout::Dir dir);
  void focusCursor();
  void zoomStep(int direction);
  void feedbackInput();
  void syncLock();
  void scheduleCamera(float delay, std::function<void()> action);
  void cancelDeferredCamera() { _deferredCamera = nullptr; }
  void afterMove(const Chess::Core::Move& move, const play::BoardKey& created, bool delayed);
  void applySubmitRule();
  void markSubmitted(bool delayed);
  struct Ghost {
    Rectangle rect;
    Vector2 direction;
    std::string label;
    int timeline, halfTurn;
  };
  std::vector<Ghost> ghostCards() const;
  std::string nextMandatoryLabel() const; // "L+1 · T3b" of the first Mandatory board that is off-screen, else empty
  std::vector<std::pair<int, int>> boardsToMoveOn() const; // Mandatory boards, then Optional ones
  void updatePicker(float dt, const play::BoardStyle& style);
  void perform(const play::Intent& intent);
  void makeMove(const Chess::Core::Move& move, bool delayedCamera = false);
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
  void doSubmit(bool delayedCamera = false);
  void leave(App& app);
  void rematch(App& app);
  bool endCardShown() const { return _game->result() != Chess::GameResult::Ongoing && _showEndCard && !_reviewing; }
  /// Submit's tooltip: why it is disabled, or the moves it hands in.
  std::string submitTip(bool ongoing) const;
  void deselect();
  void clearSelection();
  /// After anything changed: rebuild what depends on the game state, button states, HUD, end of game.
  void refresh();
  void rebuild();
  std::string hint() const;
};
