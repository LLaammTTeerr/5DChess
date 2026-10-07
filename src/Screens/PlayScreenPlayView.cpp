// The Play view of the game screen (play/PlayViewLayout.h, play/PlayView.h): its input, keys and drawing. PlayScreen.cpp calls into this file
// at a handful of places marked "[Play view]"; while the view is on, the multiverse view's camera code does not run.
#include <algorithm>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "Screens/PlayScreen.h"
#include "play/PlayView.h"

using Chess::Core::Coord;
using play::PlayViewLayout;
using play::Rect;

namespace {
constexpr float kDragPixels = 5.0f; // a press that moves farther than this scrolls a long grid instead of clicking
constexpr float kWheelPixels = 48.0f;
} // namespace

Rect PlayScreen::playViewArea() const {
  const float screenW = static_cast<float>(GetScreenWidth()), screenH = static_cast<float>(GetScreenHeight());
  // A side panel's inset is kept whole; the default inset is only the window's margin
  const float right = _rightInset <= UI::Layout::sideInset ? play::pv::kMargin : _rightInset;
  const float top = UI::Layout::hudBottom + 8.0f;
  return {play::pv::kMargin, top, screenW - play::pv::kMargin - right, screenH - UI::Layout::safeBottom - top};
}

void PlayScreen::playViewPlace() {
  const bool ongoing = _game->result() == Chess::GameResult::Ongoing;
  _pv.setTurnActive(ongoing && !inputBlocked());
  _pv.setSelection(_selection.from(), _selection.targets());
  _pv.place(playViewArea(), _game->dim());
}

void PlayScreen::togglePlayView(bool on) {
  if (_embedded || on == _playView) return;
  _animator.finish(); // a flight in the air would be remapped onto the other view's boards
  _arrows.finish();
  cancelDeferredCamera();
  _playView = on;
  _cursorShown = false;
  _pvCursor.reset();
  _pressValid = _dragging = false;
  if (!on && _camera.state() == play::BoardCamera::State::Overview) goHome(); // the multiverse has new boards since it was last in view
  refresh();
}

// ---------------------------------------------------------------------------------------------------------------------
// Pointer

void PlayScreen::playViewInput() {
  _hover.reset();
  const bool consumed = ui::pointerConsumed(); // over a button
  const Vector2 mouse = Input::mousePosition();
  PlayViewLayout::Hit hit;
  if (!consumed) hit = _pv.hit(mouse.x, mouse.y);
  using Kind = PlayViewLayout::Hit::Kind;
  if (!inputBlocked() && hit.kind == Kind::Square) _hover = hit.square;

  if (consumed) {
    _pressValid = false;
  } else {
    if (const float wheel = Input::mouseWheel(); wheel != 0.0f) {
      if (_pv.inspectorTabs().contains(mouse.x, mouse.y)) _pv.scrollTabs(wheel < 0.0f ? 1 : -1);
      else _pv.scrollBy(-wheel * kWheelPixels);
    }
    if (Input::mousePressed(MOUSE_BUTTON_LEFT)) {
      _pressValid = true;
      _dragging = false;
      _pressPos = mouse;
    }
    if (_pressValid && Input::mouseDown(MOUSE_BUTTON_LEFT)) {
      if (!_dragging && std::hypot(mouse.x - _pressPos.x, mouse.y - _pressPos.y) > kDragPixels) _dragging = true;
      if (_dragging) _pv.scrollBy(-Input::mouseDelta().y); // the grid is not panned; a long one scrolls
    } else if (_pressValid) {
      _pressValid = false;
      if (_dragging) _dragging = false;
      else playViewClick(mouse);
    }
  }
  feedbackInput();
}

// The feedback module asks the camera what is under the pointer; the Play view has none, so its layout answers
void PlayScreen::playViewPointer(play::PlayFeedback::Pointer& in) const {
  const Vector2 mouse = Input::mousePosition();
  const auto hit = _pv.hit(mouse.x, mouse.y);
  in.screenSpace = true;
  if (hit.kind == PlayViewLayout::Hit::Kind::Chrome) in.chrome = hit.key;
  if (hit.kind == PlayViewLayout::Hit::Kind::Square) in.squareUnder = hit.square;
}

void PlayScreen::playViewClick(Vector2 mouse) {
  using Kind = PlayViewLayout::Hit::Kind;
  _cursorShown = false;
  cancelDeferredCamera();
  const auto hit = _pv.hit(mouse.x, mouse.y);
  switch (hit.kind) {
    case Kind::History: _pv.browse(hit.timeline); _pvCursor = hit.timeline; break;
    case Kind::Tab:
      if (_pv.mode() == PlayViewLayout::Mode::Targets) _pv.pickTarget(hit.key);
      else _pv.browseBoard(hit.key.first, hit.key.second);
      break;
    case Kind::Close: _pv.closeBrowse(); break;
    case Kind::Inactive: _pv.browseBoard(hit.key.first, hit.key.second); break;
    case Kind::Chrome:
      if (_pv.card(hit.key.first)) _pvCursor = hit.key.first; // the frame of a card: the keyboard cursor goes there
      break;
    case Kind::Square:
      if (_ended || inputBlocked()) break;
      _pvCursor.reset();
      clickSquare(hit.square);
      break;
    case Kind::None: break;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Keys: P toggles (PlayScreen::handleKeys); here Home / M back to the multiverse, Space / Tab the boards that need a move, the arrows the
// cursor over the grid, E the history of the cursor's timeline, [ and ] older / newer in it, Enter Submit, U Undo.

void PlayScreen::playViewKeys() {
  using Dir = play::BoardLayout::Dir;
  if (Input::keyPressed(KEY_HOME) || Input::keyPressed(KEY_M)) {
    togglePlayView(false);
    return;
  }
  if (Input::keyPressed(KEY_SPACE) || Input::keyPressed(KEY_TAB)) playViewCycle(Input::keyPressed(KEY_TAB) && Input::shiftDown() ? -1 : 1);
  if (_embedded) return;
  if (Input::keyPressed(KEY_LEFT)) playViewCursor(Dir::Left);
  if (Input::keyPressed(KEY_RIGHT)) playViewCursor(Dir::Right);
  if (Input::keyPressed(KEY_UP)) playViewCursor(Dir::Up);
  if (Input::keyPressed(KEY_DOWN)) playViewCursor(Dir::Down);
  if (Input::keyPressed(KEY_E)) {
    if (_pv.browsingTimeline()) {
      _pv.closeBrowse();
    } else {
      int timeline = 0;
      bool found = false;
      if (_pvCursor)
        if (const play::PlayCard* c = _pv.card(*_pvCursor); c && c->history > 0) { timeline = c->timeline; found = true; }
      for (const play::PlayCard& c : _pv.cards())
        if (!found && c.history > 0) { timeline = c.timeline; found = true; }
      if (found) {
        _pv.browse(timeline);
        _pvCursor = timeline;
      }
    }
  }
  if (Input::keyPressed(KEY_LEFT_BRACKET)) _pv.stepBrowse(-1);
  if (Input::keyPressed(KEY_RIGHT_BRACKET)) _pv.stepBrowse(1);
  if ((Input::keyPressed(KEY_ENTER) || Input::keyPressed(KEY_KP_ENTER)) && !_selection.active() && _submitEnabled) submitTurn();
  if (Input::keyPressed(KEY_U) && _undoEnabled && !_locked) undo();
}

// Space / Tab: the next card that still needs a move (must-move cards first, then optional ones), in the order of the grid
void PlayScreen::playViewCycle(int direction) {
  std::vector<int> list;
  for (const play::PlayCard& c : _pv.cards())
    if (c.chip == play::Chip::MustMove) list.push_back(c.timeline);
  for (const play::PlayCard& c : _pv.cards())
    if (c.chip == play::Chip::Optional) list.push_back(c.timeline);
  if (list.empty() || !_pv.turnActive()) return;
  const int n = static_cast<int>(list.size());
  _cycle = direction >= 0 ? (_cycle + 1) % n : (_cycle <= 0 ? n - 1 : _cycle - 1);
  _pvCursor = list[static_cast<size_t>(_cycle)];
  if (const play::PlayCard* c = _pv.card(*_pvCursor)) _pv.reveal(c->card);
}

void PlayScreen::playViewCursor(play::BoardLayout::Dir dir) {
  std::optional<int> to;
  if (_pvCursor && _pv.card(*_pvCursor)) {
    to = _pv.neighbour(*_pvCursor, dir);
  } else {
    for (const play::PlayCard& c : _pv.cards())
      if (!to && (c.chip == play::Chip::MustMove || c.chip == play::Chip::Optional)) to = c.timeline;
    if (!to && !_pv.cards().empty()) to = _pv.cards().front().timeline;
  }
  if (!to) return;
  _pvCursor = to;
  if (const play::PlayCard* c = _pv.card(*to)) _pv.reveal(c->card);
}

// ---------------------------------------------------------------------------------------------------------------------

void PlayScreen::playViewDraw(App& app, const play::BoardStyle& style, const play::PlayFeedback::Draw&) const {
  _scene.drawBackground(style);
  const bool blink = app.themes.currentThemeHasBlink() && !UI::Motion::reduced();
  const play::PlayViewFrame frame{style, _view, *_game, _scene.soft(), _animator, _selection, _hover, _highlight, _pvCursor, blink, _pv.turnActive()};
  play::drawPlayView(_pv, frame);

  // Pointer: a hand over the history stacks, tabs and chips; "not allowed" over squares nothing can be done on
  if (_dragging) {
    UI::Cursor::request(UI::Cursor::Kind::Grab);
  } else if (!ui::pointerConsumed()) {
    using Kind = PlayViewLayout::Hit::Kind;
    const Vector2 mouse = Input::mousePosition();
    const auto hit = _pv.hit(mouse.x, mouse.y);
    switch (hit.kind) {
      case Kind::History:
      case Kind::Tab:
      case Kind::Close:
      case Kind::Inactive: UI::Cursor::request(UI::Cursor::Kind::Hand); break;
      case Kind::Square: {
        const auto& targets = _selection.targets();
        const bool isTarget = _selection.active() && std::find(targets.begin(), targets.end(), hit.square) != targets.end();
        const play::BoardInfo* info = _view.board(hit.square.l, hit.square.t);
        if (!isTarget && (inputBlocked() || _ended || (info && info->role == play::BoardRole::Past))) UI::Cursor::request(UI::Cursor::Kind::NotAllowed);
        break;
      }
      default: break;
    }
  }
}

std::optional<Vector2> PlayScreen::playViewPoint(const std::string& what, int timeline, int halfTurn) const {
  if (!_playView) return std::nullopt;
  auto centre = [](const Rect& r) { return Vector2{r.centerX(), r.centerY()}; };
  if (what == "hist") {
    if (const play::PlayCard* c = _pv.card(timeline); c && c->history > 0) return centre(c->histRect);
  } else if (what == "tab") {
    for (const play::InspectorTab& tab : _pv.tabs())
      if (tab.key == std::make_pair(timeline, halfTurn)) return centre(tab.rect);
  } else if (what == "chip") {
    for (const play::InactiveChip& chip : _pv.inactive())
      if (chip.timeline == timeline && chip.rect.w > 0.0f) return centre(chip.rect);
  } else if (what == "close") {
    if (_pv.closeRect().w > 0.0f) return centre(_pv.closeRect());
  }
  return std::nullopt;
}
