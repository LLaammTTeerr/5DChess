#include "play/PlayFeedback.h"
#include <algorithm>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "Render/Motion.h"
#include "Render/UITheme.h"
#include "play/Feedback.h"

namespace play {

using Chess::Core::Coord;

void PlayFeedback::input(const Pointer& in) {
  if (_moveSfx && ++_moveSfxAge > 1) { // a move whose rebuild did not come (it normally does in the same frame)
    App::current().audio.playSfx(static_cast<Sfx>(*_moveSfx));
    _moveSfx.reset();
  }
  std::optional<Coord> piece;                   // hover preview: a piece that could be picked up
  std::optional<std::pair<Coord, Coord>> arc;   // live arc: selected piece -> hovered cross-board target
  std::optional<BoardKey> chrome;               // the frame of the card under the pointer
  const Vector2 pointer = Input::mousePosition();
  if (Input::mousePressed(MOUSE_BUTTON_LEFT)) _previewArmed = false; // a click must be followed by a move before the preview returns
  else if (std::fabs(pointer.x - _lastPointer.x) + std::fabs(pointer.y - _lastPointer.y) > 2.0f) _previewArmed = true;
  _lastPointer = pointer;

  if (in.pointerFree && !in.ended) {
    const Vector2 world = in.camera.screenToWorld(pointer);
    for (const auto& slot : in.layout.boards())
      if (BoardLayout::cardRect(slot.rect).contains(world.x, world.y) && !slot.rect.contains(world.x, world.y))
        chrome = BoardKey{slot.timeline, slot.halfTurn};
    if (!in.blocked) {
      if (in.hover && !in.selection.active() && _previewArmed && Selection::canPickUp(*in.hover, in.game)) piece = in.hover;
      if (in.hover && in.selection.active() && !in.selection.promotionTarget()) {
        const auto& targets = in.selection.targets();
        const Coord from = *in.selection.from();
        if (std::find(targets.begin(), targets.end(), *in.hover) != targets.end() && keyOf(*in.hover) != keyOf(from))
          arc = {from, *in.hover};
      }
    } else if (in.aiToMove && !in.locked && Input::mousePressed(MOUSE_BUTTON_LEFT)) {
      // The computer's turn: a click on a board is ignored, but says so
      if (const auto square = in.layout.hitTest(world.x, world.y)) refused(*square, Intent::Reason::ComputerThinking);
    }
  }
  _a.previewHover(piece);
  _a.liveArcTo(arc);
  _a.hoverChrome(chrome);
  if (_a.previewWantsTargets() && piece) {
    std::vector<Coord> targets;
    for (const auto& m : in.game.legalMovesFrom(*piece))
      if (std::find(targets.begin(), targets.end(), m.to) == targets.end()) targets.push_back(m.to);
    _a.previewTargets(std::move(targets));
  }
}

void PlayFeedback::refused(const Coord& square, Intent::Reason reason) {
  _a.rejected(square, reason);
  App::current().audio.playSfx(Sfx::Illegal);
}

void PlayFeedback::moved(bool newTimeline, bool capture, bool sameBoard, BoardKey from) {
  _moveSfx = static_cast<int>(newTimeline ? Sfx::Branch : capture ? Sfx::Capture : Sfx::Move);
  _moveSfxAge = 0;
  if (!sameBoard) _a.markSource(from); // the board the piece left keeps a mark for a moment
}

void PlayFeedback::rebuilt(const Chess::IGame& game, const MultiverseView& view, int presentBefore) {
  _a.previewReset(); // the game changed: the hovered piece's targets are asked for again
  std::vector<Coord> kings;
  for (const auto& c : view.checks)
    if (std::find(kings.begin(), kings.end(), c.king) == kings.end()) kings.push_back(c.king);
  std::sort(kings.begin(), kings.end());
  const bool newCheck = _checkSeeded && !kings.empty() && kings != _lastKings;
  const bool decided = game.result() != Chess::GameResult::Ongoing;
  if (newCheck) _a.checkStarted(kings);
  _lastKings = std::move(kings);
  if (_checkSeeded && presentBefore != view.presentHalfTurn) _a.presentMoved(presentBefore, view.presentHalfTurn); // the marker slides
  _checkSeeded = true;
  if (_submitPending) {
    std::vector<BoardKey> movers;
    for (const auto& b : view.boards)
      if (b.role != BoardRole::Past) movers.push_back({b.timeline, b.halfTurn});
    _a.liftBoards(movers);
  }
  // One sound for the change: a check wins over the move's own sound, which wins over the submit cue
  AudioManager& audio = App::current().audio;
  if (newCheck && !decided) audio.playSfx(Sfx::Check);
  else if (_moveSfx) audio.playSfx(static_cast<Sfx>(*_moveSfx));
  else if (_submitPending && !decided) audio.playSfx(Sfx::Submit);
  _moveSfx.reset();
  _submitPending = false;
}

// ---------------------------------------------------------------------------------------------------------------------

void PlayFeedback::beforeLanes(BoardScene& scene) const {
  const auto& slide = _a.presentSlide();
  scene.setPresentAt(slide.active ? UI::Motion::lerp(static_cast<float>(slide.from), static_cast<float>(slide.to), slide.progress) : -1.0f);
}

void PlayFeedback::afterLanes(const Draw& d) const {
  if (!_a.unfoldingLanes().empty()) {
    const auto& slide = _a.presentSlide();
    const float present = slide.active ? UI::Motion::lerp(static_cast<float>(slide.from), static_cast<float>(slide.to), slide.progress)
                                       : static_cast<float>(d.view.presentHalfTurn);
    const float presentX = GetWorldToScreen2D({BoardScene::presentColumnX(present), 0.0f}, d.camera).x;
    overlay::drawLaneUnfold(_a.unfoldingLanes(), d.view, d.camera, presentX, [&] { d.scene.drawBackground(d.style); });
  }
}

void PlayFeedback::behindCards(const Draw& d) const {
  const float zoom = d.camera.zoom;
  const auto source = _a.sourceHalo(); // the board a time-travel move left, and the target of the live arc
  if (source.alpha > 0.0f && d.layout.contains(source.key.first, source.key.second))
    overlay::drawBoardGlow(BoardLayout::boardRect(source.key.first, source.key.second), source.alpha, d.style, d.scene.soft(), true, zoom);
  const auto& glow = _a.targetGlow();
  if (glow.alpha > 0.0f && d.layout.contains(glow.key.first, glow.key.second))
    overlay::drawBoardGlow(BoardLayout::boardRect(glow.key.first, glow.key.second), glow.alpha, d.style, d.scene.soft(), false, zoom);
}

void PlayFeedback::overBoards(const Draw& d) const {
  const float zoom = d.camera.zoom;
  const int dim = d.game.dim();

  const auto flash = _a.rejectFlash(); // a refused click
  if (flash.alpha > 0.0f && d.layout.contains(flash.key.first, flash.key.second)) {
    overlay::BoardShift shift(_a.boardOffset(flash.key, zoom));
    overlay::drawRejectFlash(BoardLayout::squareRect(BoardLayout::boardRect(flash.key.first, flash.key.second), dim, flash.x, flash.y), flash.alpha);
  }
  if (!d.pieceSelected) { // what the piece under the pointer could do
    overlay::drawMovePreview(_a.previewFading(), d.game, d.style, zoom, !UI::Motion::reduced());
    overlay::drawMovePreview(_a.previewNow(), d.game, d.style, zoom, !UI::Motion::reduced());
  }
  overlay::drawCheckPulse(_a.checkedKings(), _a.checkPulseNow(), dim, d.style, zoom);
  if (const auto* arc = _a.liveArc(); arc && d.pieceSelected) overlay::drawLiveArc(*arc, dim, d.style, zoom);
}

void PlayFeedback::overHud(const Draw& d) const {
  if (const auto* arc = _a.liveArc(); arc && d.pieceSelected) overlay::drawTargetRuler(arc->to, arc->grow, d.camera, d.style);
  overlay::drawHintAlert(_a.hintAlert().text, _a.hintAlert().alpha, d.style);
}

} // namespace play
