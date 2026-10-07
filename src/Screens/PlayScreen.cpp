#include "Screens/PlayScreen.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "TestMode.h"
#include "Render/UITheme.h"
#include "Screens/ModeSelectScreen.h"
#include "engine/GameCatalog.h"
#include "engine/Notation.h"
#include "play/BoardRenderer.h"
#include "play/Feedback.h"

using Chess::Core::Coord;
using play::BoardKey;
using play::BoardLayout;
using play::Rect;

namespace {
// The computer's search is advanced against a wall-clock budget each frame (the engine itself is clock-free), in slices of a few
// hundred nodes. The web build runs on the browser's one thread next to rendering, so it gets less.
#ifdef __EMSCRIPTEN__
constexpr auto kAiFrameBudget = std::chrono::milliseconds(4);
#else
constexpr auto kAiFrameBudget = std::chrono::milliseconds(6);
#endif
constexpr int kAiSliceNodes = 150;
constexpr float kAiMinThink = 0.5f; // seconds: an instant answer is still shown as a moment of thought, not a flicker
constexpr float kAiMoveGap = 0.3f;  // seconds between the moves of the computer's turn
} // namespace

PlayScreen::PlayScreen(const std::string& modeId, std::optional<play::VsAi> vs)
    : PlayScreen(Chess::GameCatalog::create(modeId), false, vs) {}

PlayScreen::PlayScreen(std::shared_ptr<Chess::IGame> game, bool isAutosave, std::optional<play::VsAi> vs)
    : _game(std::move(game)), _autosaving(isAutosave), _vs(vs) {
  _camera.setInsets(UI::Layout::safeTop, UI::Layout::sideInset, UI::Layout::safeBottom, UI::Layout::laneLabelW);
  // A game that is already decided (a loaded record) is not a transition: no win sound, and no autosave left to continue
  _ended = _game->result() != Chess::GameResult::Ongoing;
  if (_ended && _autosaving) App::current().saves.clearAutosave();
  // The first frame's input already needs the layout to map clicks onto, and the motion state is seeded from it
  refresh();
}

void PlayScreen::embed(float rightInset) {
  _embedded = true;
  _rightInset = rightInset;
  _camera.setInsets(UI::Layout::safeTop, _rightInset, UI::Layout::safeBottom, UI::Layout::laneLabelW);
  _actions.layout(static_cast<float>(GetScreenWidth()) - _rightInset); // the action row centres on the board view, not on the panel
}

void PlayScreen::update(App& app, float dt) {
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  _actions.setSkin(&style.skin);
  _back.skin = &style.skin;
  // The buttons come first: whatever has the pointer is not a click on the board
  if (!_embedded && _back.update(dt, app.screens.navShown())) leave(app);
  if (!_embedded) _saveMenu.update(dt, app.screens.navShown(), style, *_game, _vs ? &*_vs : nullptr, _aiPlaying);
  if (endCardShown() && !_embedded) { // (embedded boards show no buttons) the end card is modal: its buttons take the pointer before anything else
    _endButtons.setSkin(&style.skin);
    _endButtons.setRematch(!_embedded && Chess::GameCatalog::findById(_game->modeId()) != nullptr);
    switch (_endButtons.update(dt, true)) {
      case play::EndCardButtons::Action::Rematch: rematch(app); return;
      case play::EndCardButtons::Action::Review: _reviewing = true; break;
      case play::EndCardButtons::Action::Menu: leave(app); return;
      case play::EndCardButtons::Action::None: break;
    }
  }
  else if (endCardShown() && ui::hovered(play::endCardRect(false))) ui::consumePointer(); // (an embedded board's card is no way to click through)
  const play::ActionRow::Action action = _actions.update(dt);
  switch (_locked ? play::ActionRow::Action::None : action) { // (Undo stays available on the computer's turn: it takes the turn back)
    case play::ActionRow::Action::Undo: undo(); break;
    case play::ActionRow::Action::Deselect: if (_reviewing) _reviewing = false; else deselect(); break; // (reviewing: "Result" brings the card back)
    case play::ActionRow::Action::Submit: submitTurn(); break;
    case play::ActionRow::Action::Overview: break;  // the camera's Home and Next board are handled once the HUD is given
    case play::ActionRow::Action::NextBoard: break; // HudData::cameraLabel / nextBoardLabel (they show the buttons)
    case play::ActionRow::Action::None: break;
  }
  updatePicker(dt, style);
  boardInput();

  // Motion advances with last frame's layout; the layout catches up below
  const float step = UI::Motion::safeDt(dt);
  _camera.update(step, _layout);
  _arrows.update(step);
  _animator.update(step, _hover);
  _hudMotion.update(step);

  // "Does the side to move have any legal turn?" is searched a little each frame (see IGame::submitTurn)
  if (_game->resultPending()) {
    const auto start = std::chrono::steady_clock::now();
    do {
      _game->stepResultSearch(100);
    } while (_game->resultPending() && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(4));
  }
  updateAi(step);
  refresh();
}

// ---------------------------------------------------------------------------------------------------------------------
// Input

// The promotion chooser follows the selection: open while a pawn's target is waiting for its piece
void PlayScreen::updatePicker(float dt, const play::BoardStyle& style) {
  const auto target = _selection.promotionTarget();
  if (!target) {
    _picker.hide();
    return;
  }
  const Rect square = BoardLayout::squareRect(BoardLayout::boardRect(target->l, target->t), _game->dim(), target->x, target->y);
  const Vector2 a = _camera.worldToScreen({square.x, square.y}), b = _camera.worldToScreen({square.x + square.w, square.y + square.h});
  _picker.show(_game->getCurrentTurnColor());
  const play::Rect card = BoardLayout::cardRect(BoardLayout::boardRect(target->l, target->t));
  const Vector2 c0 = _camera.worldToScreen({card.x, card.y}), c1 = _camera.worldToScreen({card.x + card.w, card.y + card.h});
  _picker.place({a.x, a.y, b.x - a.x, b.y - a.y},
                {0.0f, UI::Layout::safeTop, static_cast<float>(GetScreenWidth()) - (_embedded ? _rightInset : 0.0f),
                 static_cast<float>(GetScreenHeight()) - UI::Layout::safeTop - UI::Layout::safeBottom},
                {c0.x, c0.y, c1.x - c0.x, c1.y - c0.y});
  if (const auto piece = _picker.update(dt, style.grayPieces, &style.skin)) {
    _animator.finish();
    _arrows.finish();
    perform(_selection.choosePromotion(*piece));
  }
}

void PlayScreen::boardInput() {
  _hover.reset();
  if (!ui::pointerConsumed() && !inputBlocked()) { // the computer's turn, or a puzzle screen driving the game: the boards only look and pan
    const Vector2 world = _camera.screenToWorld(Input::mousePosition());
    const auto square = _layout.hitTest(world.x, world.y);
    _hover = square;
    if (square && Input::mousePressed(MOUSE_BUTTON_LEFT)) click(*square);
  }
  if (!ui::pointerConsumed()) _camera.handleInput(); // not over a button or the Guide's panel
  _feedback.input({*_game, _layout, _camera, _selection, _hover, !ui::pointerConsumed(), inputBlocked(), aiToMove(), _locked, _ended});
}

bool PlayScreen::escape(App&) {
  if (_selection.promotionTarget()) {
    _selection.cancelPromotion();
    return true;
  }
  if (_selection.active()) {
    clearSelection();
    return true;
  }
  return false;
}

void PlayScreen::click(Coord square) {
  if (_ended || inputBlocked()) return;
  _animator.finish(); // new input: running move animations jump to their end
  _arrows.finish();
  perform(_selection.click(square, *_game));
}

void PlayScreen::perform(const play::Intent& intent) {
  using Kind = play::Intent::Kind;
  switch (intent.kind) {
    case Kind::None: break;
    case Kind::Clear: _animator.select(std::nullopt, {}, _game->dim()); break;
    case Kind::Promote: break; // updatePicker() opens the chooser; the move follows its choice
    case Kind::Select:
      _animator.select(intent.from, _selection.targets(), _game->dim());
      _camera.focusSelected(BoardLayout::boardRect(intent.from.l, intent.from.t));
      break;
    case Kind::Move: makeMove(intent.move); break;
    case Kind::Rejected: // nothing happens to the game, but the click is answered
      _feedback.refused(intent.from, intent.reason);
      break;
  }
}

void PlayScreen::makeMove(const Chess::Core::Move& move) {
  // Everything the flight animation needs must be read before the move: afterwards the target square holds the mover.
  const Chess::Board& source = _game->board(move.from.l, move.from.t);
  const Chess::Board& target = _game->board(move.to.l, move.to.t);
  const auto mover = source.at(Chess::Position2D(move.from.x, move.from.y));
  const auto victim = target.at(Chess::Position2D(move.to.x, move.to.y));
  const bool isCapture = mover && victim && victim->color != mover->color;

  play::FlightSpec flight;
  if (mover) {
    // a promoting pawn flies as the piece it becomes
    const bool promotes = mover->type == Chess::PieceType::Pawn &&
                          move.to.y == (mover->color == Chess::PieceColor::PIECEWHITE ? source.dim() - 1 : 0);
    flight.piece = promotes ? play::pieceKey(mover->color, move.promotion) : play::pieceKey(*mover);
  }
  if (isCapture) flight.victim = play::pieceKey(*victim);
  flight.from = play::keyOf(move.from);
  flight.fromX = move.from.x;
  flight.fromY = move.from.y;
  flight.toX = move.to.x;
  flight.toY = move.to.y;
  flight.dim = source.dim();
  const bool sameBoard = flight.from == play::keyOf(move.to);

  _game->makeMove(move);
  clearSelection();

  // The piece travels to its square on the newly created board (same-board moves slide on that new board), and the
  // camera flies there; the new board is not in the layout until refresh()
  const Chess::Board& created = *_game->getNewBoard();
  flight.to = {created.timeLineId(), created.halfTurnNumber()};
  _feedback.moved(_view.timeline(created.timeLineId()) == nullptr, isCapture, sameBoard, flight.from); // sound, source-board mark
  if (sameBoard) flight.from = flight.to;
  if (!flight.piece.empty()) _animator.startFlight(flight);
  _camera.focusNewest(BoardLayout::boardRect(flight.to.first, flight.to.second));
}

void PlayScreen::playMove(const Chess::Core::Move& move) {
  _animator.finish();
  _arrows.finish();
  makeMove(move);
}

Vector2 PlayScreen::squareToScreen(Coord c) const {
  const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(c.l, c.t), _game->dim(), c.x, c.y);
  return _camera.worldToScreen({sq.centerX(), sq.centerY()});
}

// Developer tools and the embedding puzzle screen (which locks the player out and plays the opponent itself): not blocked by the lock.
void PlayScreen::submit() {
  if (_canSubmit && !aiToMove()) doSubmit();
}

void PlayScreen::submitTurn() {
  if (inputBlocked()) return;
  doSubmit();
}

void PlayScreen::doSubmit() {
  if (!_canSubmit || aiToMove()) return;
  _animator.finish();
  _arrows.finish();
  _game->submitTurn();
  _feedback.turnSubmitted();
  clearSelection();
  autosaveNow();
}

// Continue picks up from the last submitted turn: unsubmitted moves are not part of a record. The Guide's practice board
// (embedded) must never overwrite the player's autosave.
void PlayScreen::autosaveNow() {
  if (_embedded || computerOpenedOnly()) return;
  if (App::current().saves.autosave(*_game, _vs ? &*_vs : nullptr)) {
    _autosaving = true;
  } else if (!_autosaveWarned) { // say so once: the player may be playing on with nothing to continue
    _autosaveWarned = true;
    _saveMenu.notify("Could not autosave this game");
  }
}

// Playing Black the computer opens; that alone is not worth a Continue (nothing of the player's is in it).
bool PlayScreen::computerOpenedOnly() const {
  return _vs && _vs->human == Chess::PieceColor::PIECEBLACK && _game->history().size() <= 1;
}

// Back to the mode list; the autosave is brought up to date (it is already, after every submitted turn) or removed once the game is over.
void PlayScreen::leave(App& app) {
  cancelAi();
  if (_autosaving && !_embedded) {
    // A game decided during "Checking position..." has not noticed yet: finish the (bounded) search before deciding
    if (_game->resultPending()) _game->resolveResult(2000000);
    if (_game->result() == Chess::GameResult::Ongoing) {
      if (!computerOpenedOnly()) app.saves.autosave(*_game, _vs ? &*_vs : nullptr);
    }
    else app.saves.clearAutosave();
  }
  app.screens.replace(std::make_unique<ModeSelectScreen>());
}

// Another game of the same mode (against the computer: the same side and level, a fresh seed)
void PlayScreen::rematch(App& app) {
  cancelAi();
  const std::string id = _game->modeId();
  if (!_vs) {
    app.screens.replace(std::make_unique<PlayScreen>(id));
    return;
  }
  std::uint64_t seed = 0;
  for (int i = 0; i < 4; ++i) seed = (seed << 16) | static_cast<std::uint64_t>(GetRandomValue(0, 0xFFFF));
  const play::SideChoice side = _vs->human == Chess::PieceColor::PIECEWHITE ? play::SideChoice::White : play::SideChoice::Black;
  app.screens.replace(std::make_unique<PlayScreen>(id, play::makeVsAi(side, _vs->level, seed)));
}

std::string PlayScreen::submitTip(bool ongoing) const {
  if (!ongoing) return "";
  if (aiToMove()) return "The computer is playing its turn";
  if (_canSubmit && !inputBlocked()) {
    std::string text = "Hand in this turn:";
    auto square = [](const Chess::Core::Coord& c) { return std::string(1, static_cast<char>('a' + c.x)) + std::to_string(c.y + 1); };
    for (const Chess::Core::PlayedMove& m : _game->pendingMoves()) {
      const auto& from = m.move.from;
      const auto& to = m.move.to;
      text += "\n" + square(from) + "-" + square(to);
      if (from.l != to.l || from.t != to.t) text += " to " + play::timelineLabel(to.l) + " \xC2\xB7 T" + std::to_string(to.t / 2 + 1);
      if (m.promotes) text += " (promotes)";
    }
    return text;
  }
  if (_selection.promotionTarget()) return "Choose a promotion first";
  if (_noMandatoryBoard && _game->undoable()) return "Your king would be capturable: undo or change a move";
  for (const play::BoardInfo& b : _view.boards)
    if (b.role == play::BoardRole::Mandatory) return "Still to move on " + play::timelineLabel(b.timeline) + " \xC2\xB7 " + play::boardLabel(b.halfTurn);
  return "Make a move first";
}

void PlayScreen::undo() {
  _animator.finish();
  _arrows.finish();
  if (_game->undoable()) {
    _game->undo(); // the moves of the unsubmitted turn go one at a time
    clearSelection();
  } else if (const int turns = takeBackCount()) {
    takeBack(turns); // against the computer: the submitted turns
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// The computer opponent

bool PlayScreen::aiToMove() const {
  return _vs && !_embedded && !gaveUpNow() && _game->result() == Chess::GameResult::Ongoing &&
         _game->getCurrentTurnColor() == _vs->aiColor();
}

bool PlayScreen::aiBusy() const { return _vs && (aiToMove() || _game->resultPending()); }

// A hand-over lasts for the turn it happened in: once a turn is submitted the computer is asked again.
bool PlayScreen::gaveUpNow() const { return _aiGaveUpAt && *_aiGaveUpAt == _game->history().size(); }

int PlayScreen::takeBackCount() const {
  if (!_vs || _embedded) return 0;
  return play::takeBackCount(*_vs, _game->getCurrentTurnColor(), _game->history().size(), !_game->pendingMoves().empty(), _aiPlaying);
}

void PlayScreen::takeBack(int turns) {
  cancelAi();
  _aiGaveUpAt.reset();
  const auto earlier = play::replayPrefix(*_game, _game->history().size() - static_cast<size_t>(turns));
  if (!earlier) return;
  setGame(earlier);
  if (_autosaving && !_embedded) {
    if (_game->history().empty()) App::current().saves.clearAutosave();
    else autosaveNow();
  }
}

void PlayScreen::setGame(std::shared_ptr<Chess::IGame> game) {
  _animator.finish();
  _arrows.finish();
  _game = std::move(game);
  _layout = play::BoardLayout(); // another game object: rebuilt from scratch by the next refresh()
  _selection.clear();
  _hover.reset();
  _canSubmit = false;
  _animator.select(std::nullopt, {}, _game->dim());
  refresh(); // the buttons and the HUD follow at once
}

void PlayScreen::cancelAi() {
  _search.reset();
  _aiMoves.clear();
  _aiNext = 0;
  _aiPlaying = false;
  _aiClock = 0.0f;
}

// Called every frame. A search is created when it becomes the computer's turn (once the game has decided whether it has any turn:
// the legal-turn search armed by the last submit), advanced with a few milliseconds a frame, and its moves are then played one at
// a time through the same path as a click (flight animation, sound, camera) before the turn is submitted.
void PlayScreen::updateAi(float dt) {
  if (!aiToMove()) {
    if (_search || _aiPlaying) cancelAi();
    return;
  }
  if (_game->resultPending()) return;
  _aiClock += dt;

  if (_aiPlaying) {
    if (!_animator.flights().empty() || _aiClock < kAiMoveGap) return; // the previous move is still travelling
    _aiClock = 0.0f;
    if (_aiNext < _aiMoves.size()) {
      makeMove(_aiMoves[_aiNext++]);
    } else if (_game->canSubmit()) {
      _game->submitTurn();
      _feedback.turnSubmitted();
      cancelAi();
      autosaveNow();
    } else { // cannot happen (the search verifies its turn): hand the side over rather than stall
      while (_game->undoable()) _game->undo();
      _aiGaveUpAt = _game->history().size();
      cancelAi();
    }
    return;
  }

  if (!_search) { // the clone of the game and the first slice do not share a frame
    _search = std::make_unique<Chess::ai::Search>(*_game, Chess::ai::Options{_vs->level, play::searchSeed(_vs->seed, _game->history().size())});
    _aiClock = 0.0f;
    return;
  }
  if (_search->status() == Chess::ai::Search::Status::Running) {
    const TestMode& test = TestMode::get();
    const int fixed = test.active ? test.aiNodesPerFrame : -1;
    if (fixed > 0) {
      _search->step(fixed);
    } else if (fixed < 0) {
      // Keep stepping while the next slice, taken to cost as much as the longest so far, still fits in the frame's budget
      using Clock = std::chrono::steady_clock;
      const auto start = Clock::now();
      Clock::duration longest{};
      for (;;) {
        const auto t0 = Clock::now();
        if (_search->step(kAiSliceNodes) != Chess::ai::Search::Status::Running) break;
        const auto t1 = Clock::now();
        longest = std::max(longest, t1 - t0);
        if ((t1 - start) + longest > kAiFrameBudget) break;
      }
    } // fixed == 0: frozen by the test harness
  }
  if (_search->status() == Chess::ai::Search::Status::Done && _aiClock >= kAiMinThink) {
    if (_search->hasTurn()) {
      _aiMoves = _search->bestTurn();
      _aiNext = 0;
      _aiPlaying = true;
      _search.reset();
      _aiClock = kAiMoveGap; // the first move follows at once
    } else {
      _aiGaveUpAt = _game->history().size(); // the game says it goes on but the search found no turn: play this side by hand
      cancelAi();
    }
  }
}

void PlayScreen::deselect() { clearSelection(); }

void PlayScreen::clearSelection() {
  _selection.clear();
  _animator.select(std::nullopt, {}, _game->dim());
}

// ---------------------------------------------------------------------------------------------------------------------
// State that follows the game

void PlayScreen::rebuild() {
  _animator.syncBoards(_layout); // new boards grow in
  _arrows.set(play::timelineArrows(*_game));
  _canSubmit = _game->canSubmit();
  const int presentBefore = _view.presentHalfTurn;
  _view = play::MultiverseView::build(*_game);
  _camera.setExtraBounds(play::BoardScene::jumpBounds(play::boardStyle(App::current().settings.boardView), _view));
  _noMandatoryBoard = std::none_of(_view.boards.begin(), _view.boards.end(),
                                   [](const play::BoardInfo& b) { return b.role == play::BoardRole::Mandatory; });
  _feedback.rebuilt(*_game, _view, presentBefore);
}

std::string PlayScreen::hint() const {
  if (_game->result() != Chess::GameResult::Ongoing) return "";
  if (aiToMove()) return _aiPlaying ? "Computer is moving" : "Computer is thinking"; // also while its turn is still being proved to exist
  if (_game->resultPending()) return "Checking position...";
  if (_vs && !_embedded && gaveUpNow() && _game->getCurrentTurnColor() == _vs->aiColor()) return "Computer found no move - play its side";
  if (_canSubmit) return "Submit your turn";
  if (_noMandatoryBoard && _game->undoable()) return "Your king would be capturable";
  if (_selection.promotionTarget()) return "Choose a promotion";
  return _selection.active() ? "Select a target" : "Select a piece";
}

void PlayScreen::refresh() {
  if (_layout.sync(*_game)) rebuild();

  const Chess::GameResult result = _game->result();
  const bool ongoing = result == Chess::GameResult::Ongoing;
  const bool aiTurn = aiToMove(); // the computer's turn: nothing to submit or deselect, and Undo only takes the player's turn back
  const bool blocked = inputBlocked();
  _actions.setEnabled(ongoing && ((_game->undoable() && !blocked) || takeBackCount() > 0), _selection.active() && ongoing && !_locked,
                      _canSubmit && !blocked);

  _hudMotion.setTurn(_game->getCurrentTurnColor() == Chess::PieceColor::PIECEWHITE);
  _hud.whiteToMove = _game->getCurrentTurnColor() == Chess::PieceColor::PIECEWHITE;
  _hud.fullTurn = _game->presentFullTurn() + 1;
  _hud.timelineCount = _game->timeLineCount();
  _hud.hint = hint();
  _hud.title = _hudTitle;
  // The indicator shows for the whole of the computer's thinking; the bar once the search itself runs (its own progress, as is)
  _hudMotion.setThinking(aiTurn && !_aiPlaying, _search ? static_cast<float>(_search->progress().fraction) : -1.0f);
  _hudMotion.apply(_hud);
  if (!_hudTitle.empty() || aiTurn) _hud.bannerActive = false; // (the computer's turn is already in the pill: "Computer is thinking")
  _hud.rightInset = _embedded ? _rightInset : 0.0f;
  _hud.undoLabel = takeBackCount() > 0 ? "Undo turn" : "Undo move";
  _hud.undoCount = static_cast<int>(_game->pendingMoves().size());
  _hud.submitTip = submitTip(ongoing);
  if (_reviewing && _hudTitle.empty())
    _hud.title = result == Chess::GameResult::WhiteWins ? "White wins" : result == Chess::GameResult::BlackWins ? "Black wins" : "Draw";
  _actions.setReview(_reviewing);
  _actions.sync(_hud);
  _hudMotion.setEnded(!ongoing, result == Chess::GameResult::WhiteWins, result == Chess::GameResult::Draw);

  if (!ongoing) {
    if (!_ended) {
      App::current().audio.playSfx(Sfx::Win); // once, on the transition
      if (_autosaving) App::current().saves.clearAutosave(); // nothing left to continue
    }
    _ended = true;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

void PlayScreen::draw(App& app) const {
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  const Camera2D& camera = _camera.view();
  const float zoom = camera.zoom;
  const bool ongoing = _game->result() == Chess::GameResult::Ongoing;
  const int dim = _game->dim();
  const float screenW = static_cast<float>(GetScreenWidth()), screenH = static_cast<float>(GetScreenHeight());
  const Rectangle safe = {UI::Layout::laneLabelW, UI::Layout::safeTop, screenW - UI::Layout::laneLabelW - _rightInset,
                          screenH - UI::Layout::safeTop - UI::Layout::safeBottom};
  const play::SceneFrame frame{style, _view, camera, dim, safe};
  const bool gray = style.grayPieces;

  // Behind everything: the background, then lanes and the present column (they run under the HUD)
  _scene.drawBackground(style);
  const play::PlayFeedback::Draw fx{style, _view, _layout, *_game, camera, _scene, _selection.from().has_value()};
  _feedback.beforeLanes(_scene);
  if (!_layout.boards().empty()) _scene.drawLanes(frame, UI::Layout::rulerY);
  _feedback.afterLanes(fx);

  // The boards never draw over the HUD bars: they are clipped to the free area (plus a little for halos)
  constexpr float kClipPad = 12.0f;
  BeginScissorMode(static_cast<int>(safe.x - kClipPad), static_cast<int>(safe.y - kClipPad), static_cast<int>(safe.width + 2 * kClipPad),
                   static_cast<int>(safe.height + 2 * kClipPad));
  BeginMode2D(camera);
  _scene.drawTails(frame);
  _arrows.draw(style, zoom);
  _scene.drawJumpArcs(frame);

  const bool blink = app.themes.currentThemeHasBlink() && !UI::Motion::reduced() && zoom >= 0.8f; // not when zoomed far out
  const auto from = _selection.from();
  const auto& slots = _layout.boards();
  auto lookOf = [&](size_t i) {
    const auto& slot = slots[i];
    const BoardKey key{slot.timeline, slot.halfTurn};
    play::BoardLook look;
    look.enter = _animator.enterProgress(key);
    if (i < _view.boards.size()) {
      const play::BoardInfo& info = _view.boards[i];
      look.role = info.role;
      look.inactive = info.inactive;
      look.whiteToMove = info.whiteToMove;
    }
    look.blink = blink;
    look.blinkSeed = static_cast<unsigned>((key.first + 1000) * 7919 + key.second);
    if (from && play::keyOf(*from) == key) look.hide(from->x, from->y); // drawn lifted by an overlay
    for (const auto& f : _animator.flights())
      if (f.spec.to == key && !f.move.done()) look.hide(f.spec.toX, f.spec.toY);
    for (const auto& c : _view.checks)
      if (c.king.l == slot.timeline && c.king.t == slot.halfTurn) look.markChecked(c.king.x, c.king.y);
    return look;
  };
  for (size_t i = 0; i < slots.size(); ++i) {
    play::overlay::BoardShift shift(_animator.boardOffset({slots[i].timeline, slots[i].halfTurn}, zoom));
    play::drawBoardHalo(slots[i].rect, lookOf(i), style, _scene.soft());
  }
  _feedback.behindCards(fx);
  for (size_t i = 0; i < slots.size(); ++i) {
    play::overlay::BoardShift shift(_animator.boardOffset({slots[i].timeline, slots[i].halfTurn}, zoom));
    play::drawBoard(_game->board(slots[i].timeline, slots[i].halfTurn), slots[i].rect, lookOf(i), style, zoom);
  }

  if (from) play::drawBoardOutline(BoardLayout::boardRect(from->l, from->t), style, zoom);

  auto squareOf = [dim](const Coord& c) {
    return BoardLayout::squareRect(BoardLayout::boardRect(c.l, c.t), dim, c.x, c.y);
  };
  const auto& fading = _animator.hoverFading();
  if (fading.alpha > 0.0f && _layout.contains(fading.key.first, fading.key.second))
    play::drawHoverSquare(BoardLayout::squareRect(BoardLayout::boardRect(fading.key.first, fading.key.second), dim, fading.x, fading.y),
                          fading.alpha, style);
  const auto& hover = _animator.hoverNow();
  if (hover.alpha > 0.0f && _hover) play::drawHoverSquare(squareOf(*_hover), hover.alpha, style);

  if (from) {
    const auto& targets = _selection.targets();
    for (size_t i = 0; i < targets.size(); ++i) {
      const bool occupied = _game->board(targets[i].l, targets[i].t).at(Chess::Position2D(targets[i].x, targets[i].y)).has_value();
      play::drawLegalTarget(squareOf(targets[i]), occupied, _animator.dotScale(i), style);
    }
    const auto piece = _game->board(from->l, from->t).at(Chess::Position2D(from->x, from->y));
    play::drawSelectedSquare(squareOf(*from), zoom, style);
    const play::BoardInfo* info = _view.board(from->l, from->t);
    if (piece) play::drawLiftedPiece(squareOf(*from), play::pieceKey(*piece), _animator.lift(), gray || (info && info->inactive));
  }
  if (_highlight && _layout.contains(_highlight->l, _highlight->t))
    play::drawSelectedSquare(squareOf(*_highlight), zoom, style);
  _feedback.overBoards(fx);
  _scene.drawJumpBadges(frame);
  _scene.drawChecks(frame, _animator.checkDrawOn());
  play::drawFlights(_animator.flights(), gray);
  EndMode2D();

  _scene.drawCardLabels(frame, _layout);
  _scene.drawJumpLabels(frame);
  EndScissorMode();

  if (!_layout.boards().empty()) {
    _scene.drawRuler(frame, UI::Layout::rulerY, UI::Layout::rulerH);
    _scene.drawLaneLabels(frame, {0.0f, UI::Layout::safeTop - 8.0f, UI::Layout::laneLabelW, safe.height + 16.0f});
  }

  play::drawHud(_hud, style);
  _feedback.overHud(fx);
  _actions.draw();
  if (!_embedded) _saveMenu.draw(style, app.screens.navAlpha());
  _picker.draw(style);
  if (endCardShown()) {
    const Chess::GameResult result = _game->result();
    play::EndCard card = _hudMotion.endCard();
    card.title = result == Chess::GameResult::WhiteWins ? "White wins!" : result == Chess::GameResult::BlackWins ? "Black wins!" : "Draw";
    card.reason = result == Chess::GameResult::Draw ? "Stalemate" : "Checkmate";
    if (_embedded) card.footer = "Use Reset position to try again";
    card.buttons = !_embedded;
    play::drawEndCard(card, &_endButtons);
  }
  if (!_embedded) _back.draw(app.screens.navAlpha());
}
