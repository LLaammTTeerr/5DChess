#include "Screens/PlayScreen.h"
#include <algorithm>
#include <chrono>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "Screens/ModeSelectScreen.h"
#include "engine/GameCatalog.h"
#include "play/BoardRenderer.h"

using Chess::Core::Coord;
using play::BoardKey;
using play::BoardLayout;
using play::Rect;

PlayScreen::PlayScreen(const std::string& modeId) : PlayScreen(Chess::GameCatalog::create(modeId)) {}

PlayScreen::PlayScreen(std::shared_ptr<Chess::IGame> game) : _game(std::move(game)) {
  _camera.setInsets(UI::Layout::safeTop, UI::Layout::sideInset, UI::Layout::safeBottom, UI::Layout::sideInset);
  // The first frame's input already needs the layout to map clicks onto, and the motion state is seeded from it
  refresh();
}

void PlayScreen::update(App& app, float dt) {
  // The buttons come first: whatever has the pointer is not a click on the board
  if (_back.update(dt, app.screens.navShown())) app.screens.replace(std::make_unique<ModeSelectScreen>());
  switch (_actions.update(dt)) {
    case play::ActionRow::Action::Undo: undo(); break;
    case play::ActionRow::Action::Deselect: deselect(); break;
    case play::ActionRow::Action::Submit: submitTurn(); break;
    case play::ActionRow::Action::None: break;
  }
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
  refresh();
}

// ---------------------------------------------------------------------------------------------------------------------
// Input

void PlayScreen::boardInput() {
  _hover.reset();
  if (!ui::pointerConsumed()) {
    const Vector2 world = _camera.screenToWorld(Input::mousePosition());
    const auto square = _layout.hitTest(world.x, world.y);
    _hover = square;
    if (square && Input::mousePressed(MOUSE_BUTTON_LEFT)) click(*square);
  }
  _camera.handleInput();
}

void PlayScreen::click(Coord square) {
  if (_ended) return;
  _animator.finish(); // new input: running move animations jump to their end
  _arrows.finish();
  perform(_selection.click(square, *_game));
}

void PlayScreen::perform(const play::Intent& intent) {
  using Kind = play::Intent::Kind;
  switch (intent.kind) {
    case Kind::None: break;
    case Kind::Clear: _animator.select(std::nullopt, {}, _game->dim()); break;
    case Kind::Select:
      _animator.select(intent.from, _selection.targets(), _game->dim());
      _camera.focusSelected(BoardLayout::boardRect(intent.from.l, intent.from.t));
      break;
    case Kind::Move: makeMove(intent.move); break;
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
  if (mover) flight.piece = play::pieceKey(*mover);
  if (isCapture) flight.victim = play::pieceKey(*victim);
  flight.from = play::keyOf(move.from);
  flight.fromX = move.from.x;
  flight.fromY = move.from.y;
  flight.toX = move.to.x;
  flight.toY = move.to.y;
  flight.dim = source.dim();
  const bool sameBoard = flight.from == play::keyOf(move.to);

  _game->makeMove(move);
  // TODO: Sfx::Check / Sfx::Castle / Sfx::Promote / Sfx::Draw once the rules implement them.
  App::current().audio.playSfx(isCapture ? Sfx::Capture : Sfx::Move);
  clearSelection();

  // The piece travels to its square on the newly created board (same-board moves slide on that new board), and the
  // camera flies there; the new board is not in the layout until refresh()
  const Chess::Board& created = *_game->getNewBoard();
  flight.to = {created.timeLineId(), created.halfTurnNumber()};
  if (sameBoard) flight.from = flight.to;
  if (!flight.piece.empty()) _animator.startFlight(flight);
  _camera.focusNewest(_layout, BoardLayout::boardRect(flight.to.first, flight.to.second));
}

void PlayScreen::submit() {
  if (_canSubmit) submitTurn();
}

void PlayScreen::submitTurn() {
  if (!_canSubmit) return;
  _animator.finish();
  _arrows.finish();
  _game->submitTurn();
  clearSelection();
}

void PlayScreen::undo() {
  _animator.finish();
  _arrows.finish();
  _game->undo(); // the button is disabled when there is nothing to undo
  clearSelection();
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
  _noMandatoryBoard = _game->mandatoryBoards().empty();
  _presentHalfTurn = static_cast<float>(_game->bufferHalfTurn());
  _moveable.clear();
  for (const auto& board : _game->getMoveableBoards()) _moveable.push_back({board->timeLineId(), board->halfTurnNumber()});
  std::sort(_moveable.begin(), _moveable.end());
}

std::string PlayScreen::hint() const {
  if (_game->result() != Chess::GameResult::Ongoing) return "";
  if (_game->resultPending()) return "Checking position...";
  if (_canSubmit) return "Submit your turn";
  if (_noMandatoryBoard && _game->undoable()) return "Your king would be capturable";
  return _selection.active() ? "Select a target" : "Select a board";
}

void PlayScreen::refresh() {
  if (_layout.sync(*_game)) rebuild();

  const Chess::GameResult result = _game->result();
  const bool ongoing = result == Chess::GameResult::Ongoing;
  _actions.setEnabled(_game->undoable() && ongoing, _selection.active() && ongoing, _canSubmit);

  _hudMotion.setTurn(_game->getCurrentTurnColor() == Chess::PieceColor::PIECEWHITE);
  _hud.whiteToMove = _game->getCurrentTurnColor() == Chess::PieceColor::PIECEWHITE;
  _hud.fullTurn = _game->presentFullTurn() + 1;
  _hud.timelineCount = _game->timeLineCount();
  _hud.hint = hint();
  _hudMotion.apply(_hud);
  _hudMotion.setEnded(!ongoing, result == Chess::GameResult::WhiteWins, result == Chess::GameResult::Draw);

  if (!ongoing) {
    if (!_ended) App::current().audio.playSfx(Sfx::Win); // once, on the transition
    _ended = true;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

void PlayScreen::draw(App& app) const {
  const Camera2D& camera = _camera.view();
  const float zoom = camera.zoom;
  const bool ongoing = _game->result() == Chess::GameResult::Ongoing;
  const int dim = _game->dim();

  BeginMode2D(camera);
  // Behind everything: the present line, then the timeline arrows
  if (!_layout.boards().empty()) play::drawPresentLine(_presentHalfTurn, _layout.bounds(), zoom);
  _arrows.draw();

  const bool blink = app.themes.currentThemeHasBlink() && !UI::Motion::reduced() && zoom >= 0.8f; // not when zoomed far out
  const auto from = _selection.from();
  for (const auto& slot : _layout.boards()) {
    const BoardKey key{slot.timeline, slot.halfTurn};
    play::BoardLook look;
    look.enter = _animator.enterProgress(key);
    look.moveable = ongoing && std::binary_search(_moveable.begin(), _moveable.end(), key);
    look.blink = blink;
    look.blinkSeed = static_cast<unsigned>((key.first + 1000) * 7919 + key.second);
    if (from && play::keyOf(*from) == key) look.hide(from->x, from->y); // drawn lifted by an overlay
    for (const auto& f : _animator.flights())
      if (f.spec.to == key && !f.move.done()) look.hide(f.spec.toX, f.spec.toY);
    play::drawBoard(_game->board(slot.timeline, slot.halfTurn), slot.rect, look, zoom);
  }

  if (from) play::drawBoardOutline(BoardLayout::boardRect(from->l, from->t), zoom);

  auto squareOf = [dim](const Coord& c) {
    return BoardLayout::squareRect(BoardLayout::boardRect(c.l, c.t), dim, c.x, c.y);
  };
  const auto& fading = _animator.hoverFading();
  if (fading.alpha > 0.0f && _layout.contains(fading.key.first, fading.key.second))
    play::drawHoverSquare(BoardLayout::squareRect(BoardLayout::boardRect(fading.key.first, fading.key.second), dim, fading.x, fading.y),
                          fading.alpha);
  const auto& hover = _animator.hoverNow();
  if (hover.alpha > 0.0f && _hover) play::drawHoverSquare(squareOf(*_hover), hover.alpha);

  if (from) {
    const auto& targets = _selection.targets();
    for (size_t i = 0; i < targets.size(); ++i) {
      const bool occupied = _game->board(targets[i].l, targets[i].t).at(Chess::Position2D(targets[i].x, targets[i].y)).has_value();
      play::drawLegalTarget(squareOf(targets[i]), occupied, _animator.dotScale(i));
    }
    const auto piece = _game->board(from->l, from->t).at(Chess::Position2D(from->x, from->y));
    play::drawSelectedSquare(squareOf(*from), zoom);
    if (piece) play::drawLiftedPiece(squareOf(*from), play::pieceKey(*piece), _animator.lift());
  }
  play::drawFlights(_animator.flights());
  EndMode2D();

  play::drawHud(_hud);
  _actions.draw();
  if (!ongoing) {
    const Chess::GameResult result = _game->result();
    play::EndCard card = _hudMotion.endCard();
    card.title = result == Chess::GameResult::WhiteWins ? "White wins!" : result == Chess::GameResult::BlackWins ? "Black wins!" : "Draw";
    card.reason = result == Chess::GameResult::Draw ? "Stalemate" : "Checkmate";
    play::drawEndCard(card);
  }
  _back.draw(app.screens.navAlpha());
}
