#include "Screens/PuzzleScreen.h"
#include <algorithm>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "ui/Audit.h"
#include "Screens/PuzzleListScreen.h"
#include "play/BoardStyle.h"
#include "play/Hud.h"
#include "puzzles/Progress.h"

namespace {

constexpr float kPanelW = 400.0f, kPanelRight = 24.0f, kPanelTop = 64.0f, kPanelBottom = 52.0f; // above the controls bar
constexpr float kPad = 22.0f;
constexpr float kInnerW = kPanelW - 2 * kPad;
constexpr float kTitleLine = 34.0f, kBodyLine = 24.0f, kSmallLine = 21.0f, kBannerLine = 26.0f;
constexpr float kCardPad = 14.0f;
constexpr float kBoardInset = kPanelW + kPanelRight + 16.0f; // what the board keeps free at the right
constexpr float kMoveGap = 0.55f;                            // seconds between the moves of a scripted line
constexpr float kStuckSeconds = 3.0f;                        // a finished script whose turn never shows up resets the puzzle
constexpr double kProofMilliseconds = 6.0;                   // per frame, spent on the proof of a mate in 2
constexpr long long kDefenceNodes = 20000;                   // cap of the engine's search for Black's reply

/// Greedy word wrap to `width` pixels (the same as the Guide's).
std::vector<std::string> wrap(::Font font, float size, const std::string& text, float width) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size()) {
    size_t j = text.find(' ', i);
    if (j == std::string::npos) j = text.size();
    const std::string word = text.substr(i, j - i);
    const std::string trial = line.empty() ? word : line + " " + word;
    if (!line.empty() && MeasureTextEx(font, trial.c_str(), size, 0.0f).x > width) {
      lines.push_back(line);
      line = word;
    } else {
      line = trial;
    }
    i = j + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

void drawLines(::Font font, float size, const std::vector<std::string>& lines, float x, float y, float lineH, ::Color color,
               Rectangle bounds = {}) {
  for (size_t i = 0; i < lines.size(); ++i) {
    const Vector2 at = {std::floor(x), std::floor(y + static_cast<float>(i) * lineH)};
    DrawTextEx(font, lines[i].c_str(), at, size, 0.0f, color);
    if (bounds.width > 0.0f && ui::audit::enabled())
      ui::audit::within("panel text", lines[i], {at.x, at.y, MeasureTextEx(font, lines[i].c_str(), size, 0.0f).x, size}, bounds);
  }
}

void drawCheck(float x, float y, float size, float thickness, ::Color color) {
  DrawLineEx({x, y + size * 0.55f}, {x + size * 0.38f, y + size * 0.9f}, thickness, color);
  DrawLineEx({x + size * 0.38f, y + size * 0.9f}, {x + size, y + size * 0.12f}, thickness, color);
}

void drawCard(Rectangle r, const play::BoardStyle& style, ::Color tint, ::Color edge) {
  const float round = style.hudSquare ? 0.0f : 12.0f / std::min(r.width, r.height);
  if (round > 0.0f) {
    DrawRectangleRounded(r, round, 8, tint);
    DrawRectangleRoundedLinesEx(r, round, 8, 1.5f, edge);
  } else {
    DrawRectangleRec(r, tint);
    DrawRectangleLinesEx(r, 1.5f, edge);
  }
}

} // namespace

PuzzleScreen::PuzzleScreen(int index) : _index(index) {
  const auto& all = puzzles::all();
  _index = all.empty() ? 0 : std::clamp(index, 0, static_cast<int>(all.size()) - 1);
  _puzzle = all.empty() ? nullptr : &all[static_cast<size_t>(_index)];

  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  _panel = {W - kPanelRight - kPanelW, kPanelTop, kPanelW, H - kPanelTop - kPanelBottom};
  const float bottom = _panel.y + _panel.height - kPad;
  const float solutionY = bottom - UI::Space::buttonHeight;
  const float rowY = solutionY - UI::Space::buttonHeight - UI::Space::sm;
  const float half = (kInnerW - UI::Space::sm) / 2.0f;
  _hint = ui::Button("Hint", {_panel.x + kPad, rowY, half, UI::Space::buttonHeight});
  _reset = ui::Button("Reset", {_panel.x + kPad + half + UI::Space::sm, rowY, half, UI::Space::buttonHeight});
  _solution = ui::Button("Show solution", {_panel.x + kPad, solutionY, kInnerW, UI::Space::buttonHeight});

  const float areaW = W - kBoardInset;
  _card = {areaW / 2.0f - 230.0f, H / 2.0f - 110.0f, 460.0f, 250.0f};
  const float by = _card.y + _card.height - kCardPad - UI::Space::buttonHeight - 6.0f;
  const float bw = (_card.width - 2 * 26.0f - UI::Space::sm) / 2.0f;
  _next = ui::Button("Next puzzle", {_card.x + 26.0f, by, bw, UI::Space::buttonHeight}, true);
  _list = ui::Button("Back to list", {_card.x + 26.0f + bw + UI::Space::sm, by, bw, UI::Space::buttonHeight});
  if (_index + 1 >= static_cast<int>(all.size())) _list.rect = {_card.x + 26.0f, by, _card.width - 52.0f, UI::Space::buttonHeight}; // the last puzzle: no Next

  if (_puzzle) openBoard();
  rewrap();
}

void PuzzleScreen::openBoard() {
  _board = std::make_unique<PlayScreen>(_puzzle->start());
  _board->embed(kBoardInset);
  _board->showEndCard(false); // the success card is ours
  _turnsSeen = 0;
  _stage = 0;
  _prover.reset();
  _search.reset();
  _script.clear();
  _scriptPos = 0;
  if (_hintLevel >= 2) _board->highlight(hintSquare());
}

void PuzzleScreen::reset() {
  openBoard();
  say("");
  setPhase(Phase::Playing);
}

bool PuzzleScreen::hasNext() const { return _puzzle && _index + 1 < static_cast<int>(puzzles::all().size()); }

// ---------------------------------------------------------------------------------------------------------------------
// Texts

std::string PuzzleScreen::bannerText() const {
  if (!_puzzle) return "";
  const int left = _puzzle->mateIn() - _stage;
  return "White to move - mate in " + std::to_string(left);
}

std::string PuzzleScreen::statusText() const {
  if (!_message.empty()) return _message;
  if (_stage == 1) return "Black has replied. Finish the job.";
  if (_puzzle && _puzzle->goal == puzzles::Goal::MateIn2)
    return "Plan two turns: after your first one the engine picks the best defence for Black.";
  return "Make your move, then press Submit.";
}

std::string PuzzleScreen::hintTextShown() const { return _hintLevel >= 1 && _puzzle ? _puzzle->hint : ""; }

void PuzzleScreen::rewrap() {
  if (!_puzzle) return;
  const std::string step = "PUZZLE " + std::to_string(_index + 1) + " OF " + std::to_string(puzzles::all().size()) + " - " +
                           std::string(puzzles::tierName(_puzzle->tier));
  const std::string banner = bannerText(), status = statusText(), hint = hintTextShown();
  _wrapped[0] = step;
  _wrapped[1] = _puzzle->title;
  _wrapped[2] = banner;
  _wrapped[3] = status;
  _wrapped[4] = hint;

  float y = _panel.y + kPad;
  auto place = [&](Lines& l, ::Font font, float size, const std::string& text, float lineH, float width, float gap) {
    l.lines = wrap(font, size, text, width);
    l.y = y;
    l.height = static_cast<float>(l.lines.size()) * lineH;
    y += l.height + gap;
  };
  place(_step, UI::Fonts::mono(), UI::Font::mono, step, kSmallLine, kInnerW, 4.0f);
  place(_title, UI::Fonts::section(), UI::Font::section, _puzzle->title, kTitleLine, kInnerW, 12.0f);
  // the goal banner sits in a card
  _banner.lines = wrap(UI::Fonts::button(), UI::Font::button, banner, kInnerW - 2 * kCardPad);
  _banner.y = y + kCardPad;
  _banner.height = static_cast<float>(_banner.lines.size()) * kBannerLine;
  y += _banner.height + 2 * kCardPad + 14.0f;
  place(_status, UI::Fonts::body(), UI::Font::body, status, kBodyLine, kInnerW, 14.0f);
  if (!hint.empty()) {
    _hintText.lines = wrap(UI::Fonts::body(), UI::Font::body, hint, kInnerW - 2 * kCardPad);
    _hintText.y = y + kCardPad + kSmallLine + 2.0f;
    _hintText.height = static_cast<float>(_hintText.lines.size()) * kBodyLine;
  } else {
    _hintText = {};
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// The hint: its text first, then the piece to move

std::optional<Chess::Core::Coord> PuzzleScreen::hintSquare() const {
  if (!_puzzle || !_board) return std::nullopt;
  const Chess::IGame& game = _board->game();
  // The stored line is for the first turn; the second turn of a mate in 2 follows Black's reply, which the engine chose, so
  // ask the engine for a mating turn of the position as it is.
  if (_stage == 1 && game.pendingMoves().empty() && game.result() == Chess::GameResult::Ongoing && !game.resultPending()) {
    try {
      if (const auto mate = puzzles::findMate(game); mate && !mate->empty()) return mate->front().from;
    } catch (const std::exception&) {
    }
  }
  const size_t turn = _puzzle->goal == puzzles::Goal::MateIn1 || _stage == 0 ? 0 : 2;
  return _puzzle->solution[turn].front().from;
}

void PuzzleScreen::applyHint() {
  if (_hintLevel == 0) {
    _hintLevel = 1;
  } else {
    _hintLevel = 2;
    _board->highlight(hintSquare());
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Judging

void PuzzleScreen::wrong(const std::string& why) {
  say(why, Tone::Bad);
  _board->setInputLocked(true);
  _board->highlight(std::nullopt);
  setPhase(Phase::Wrong);
}

void PuzzleScreen::solve() {
  puzzles::markSolved(_puzzle->id);
  say("Checkmate!", Tone::Good);
  _board->setInputLocked(true);
  _board->highlight(std::nullopt);
  _solvedClock = 0.0f;
  setPhase(Phase::Solved);
}

// The submitted turn of the player has been resolved: win, or not
void PuzzleScreen::judge() {
  const Chess::IGame& game = _board->game();
  switch (game.result()) {
    case Chess::GameResult::WhiteWins: solve(); return;
    case Chess::GameResult::Draw: wrong("Not quite - that is stalemate, a draw. Try again."); return;
    case Chess::GameResult::BlackWins: wrong("Not quite - try again"); return;
    case Chess::GameResult::Ongoing: break;
  }
  if (_puzzle->goal == puzzles::Goal::MateIn2 && _stage == 0) {
    try {
      _prover = std::make_unique<puzzles::DefenceProver>(game); // only clones the game: the work happens a few ms per frame
    } catch (const std::exception&) {
      wrong("Not quite - try again");
      return;
    }
    say("Checking your idea...");
    setPhase(Phase::Proving);
    return;
  }
  wrong("Not quite - try again");
}

// The proof is in: Black replies with the engine's choice, played on the board like a player's turn
void PuzzleScreen::startReply() {
  say("Black replies...");
  const Chess::IGame& game = _board->game();
  Chess::ai::Options options;
  options.level = Chess::ai::Level::Hard;
  options.seed = 1;
  options.maxNodes = kDefenceNodes;
  _search = std::make_unique<Chess::ai::Search>(game, options);
  setPhase(Phase::Searching);
}

void PuzzleScreen::startSolution() {
  _script.clear();
  _scriptPos = 0;
  // After Black's reply (stage 1) the reply on the board is the engine's, not the stored one: the solution is a mating turn of the
  // position as it is.
  if (_stage == 1 && _phase == Phase::Playing) {
    const Chess::IGame& game = _board->game();
    if (game.pendingMoves().empty() && game.result() == Chess::GameResult::Ongoing && !game.resultPending()) {
      try {
        if (const auto mate = puzzles::findMate(game)) {
          for (const Chess::Core::Move& move : *mate) _script.push_back({false, move, kMoveGap});
          _script.push_back({true, {}, 0.0f});
        }
      } catch (const std::exception&) {
        _script.clear();
      }
    }
  }
  if (_script.empty()) {
    openBoard(); // from the start position, whatever happened so far
    for (size_t i = 0; i < _puzzle->solution.size(); ++i) {
      for (const Chess::Core::Move& move : _puzzle->solution[i]) _script.push_back({false, move, kMoveGap});
      _script.push_back({true, {}, i + 1 < _puzzle->solution.size() ? 1.1f : 0.0f});
    }
  }
  _board->setInputLocked(true);
  _board->highlight(std::nullopt);
  _scriptClock = 0.4f;
  _doneClock = 0.0f;
  say("Solution: " + std::string(_puzzle->goal == puzzles::Goal::MateIn1 ? "one turn" : "White, Black's reply, White") + ".");
  setPhase(Phase::Solution);
}

bool PuzzleScreen::busy() const {
  if (!_puzzle || !_board) return false;
  switch (_phase) {
    case Phase::Playing: return _board->game().history().size() > _turnsSeen; // submitted, not judged yet
    case Phase::Judging:
    case Phase::Proving:
    case Phase::Searching:
    case Phase::Replying:
    case Phase::Solution: return true;
    default: return false;
  }
}

void PuzzleScreen::runScript(float dt) {
  _scriptClock -= dt;
  if (scriptDone() || _scriptClock > 0.0f) return;
  const Step& step = _script[_scriptPos++];
  if (step.submit) _board->submit(); // (the moves of the line made the turn complete a frame ago)
  else _board->playMove(step.move);
  _scriptClock = step.after;
}

// ---------------------------------------------------------------------------------------------------------------------
// Frame

void PuzzleScreen::update(App& app, float dt) {
  if (!_puzzle) {
    app.screens.replace(std::make_unique<PuzzleListScreen>());
    return;
  }
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  for (ui::Button* b : {&_back, &_hint, &_reset, &_solution, &_next, &_list}) b->skin = &style.skin;
  const bool nav = app.screens.navShown();
  const float step = UI::Motion::safeDt(dt);
  _clock += step;

  const bool playing = _phase == Phase::Playing;
  const bool solved = _phase == Phase::Solved;
  _hint.enabled = playing && _hintLevel < 2; // nothing more to show after the piece
  _hint.label = _hintLevel == 0 ? "Hint" : _hintLevel == 1 ? "Show piece" : "No more hints";
  _solution.enabled = !solved && _phase != Phase::Solution;
  _reset.label = _phase == Phase::Wrong ? "Try again" : "Reset";
  _reset.primary = _phase == Phase::Wrong;
  _reset.enabled = _phase != Phase::Solution && _phase != Phase::Judging && _phase != Phase::Proving && _phase != Phase::Searching &&
                   _phase != Phase::Replying && !solved;

  // The success card overlays the board: its buttons take the pointer first
  if (solved) {
    _next.enabled = hasNext();
    if (_next.update(dt, nav && hasNext())) {
      app.screens.replace(std::make_unique<PuzzleScreen>(_index + 1));
      return;
    }
    if (_list.update(dt, nav)) {
      app.screens.replace(std::make_unique<PuzzleListScreen>());
      return;
    }
    if (CheckCollisionPointRec(Input::mousePosition(), _card)) ui::consumePointer();
  }
  if (_back.update(dt, nav)) {
    app.screens.replace(std::make_unique<PuzzleListScreen>());
    return;
  }
  if (_hint.update(dt, nav)) applyHint();
  if (_reset.update(dt, nav)) reset();
  if (_solution.update(dt, nav)) startSolution();
  if (CheckCollisionPointRec(Input::mousePosition(), _panel)) ui::consumePointer(); // the panel is not a board

  _board->setInputLocked(!playing);
  // A decided puzzle is not "Black to move" any more
  _board->setHudTitle(_phase == Phase::Wrong ? "Not quite" : solved ? "Checkmate" : _phase == Phase::SolutionDone ? "Solution" : "");
  _board->update(app, dt);

  const Chess::IGame& game = _board->game();
  switch (_phase) {
    case Phase::Playing:
      if (game.history().size() > _turnsSeen) { // the player submitted a turn
        _turnsSeen = game.history().size();
        _board->highlight(std::nullopt);
        _board->setInputLocked(true);
        say("");
        setPhase(Phase::Judging);
      }
      break;
    case Phase::Judging:
      if (!game.resultPending()) judge();
      break;
    case Phase::Proving:
      try {
        const auto status = _prover->step(kProofMilliseconds);
        if (status == puzzles::DefenceProver::Status::Proven) {
          startReply();
        } else if (status == puzzles::DefenceProver::Status::Refuted) {
          // Show why: Black plays the defence that escapes, then the puzzle resets
          _defence = _prover->refutation();
          if (_defence.empty()) {
            wrong("Not quite - try again");
          } else {
            _script.clear();
            for (const Chess::Core::Move& move : _defence) _script.push_back({false, move, kMoveGap});
            _script.push_back({true, {}, 0.0f});
            _scriptPos = 0;
            _scriptClock = 0.35f;
            _doneClock = 0.0f;
            _refuting = true;
            say("Black has a defence...");
            setPhase(Phase::Replying);
          }
        } else {
          say("Checking your idea... " + std::to_string(static_cast<int>(_prover->fraction() * 100.0)) + "%");
        }
      } catch (const std::exception&) { // a proof that ran out of budget: never take the app down
        wrong("Not quite - try again");
      }
      break;
    case Phase::Searching:
      if (_search->step(400) == Chess::ai::Search::Status::Done) {
        _defence = _search->hasTurn() ? _search->bestTurn() : _prover->defences().front();
        _script.clear();
        for (const Chess::Core::Move& move : _defence) _script.push_back({false, move, kMoveGap});
        _script.push_back({true, {}, 0.0f});
        _scriptPos = 0;
        _scriptClock = 0.35f;
        _doneClock = 0.0f;
        _refuting = false;
        setPhase(Phase::Replying);
      }
      break;
    case Phase::Replying:
      runScript(step);
      if (scriptDone()) _doneClock += step;
      if (scriptDone() && _doneClock > kStuckSeconds && !(game.history().size() > _turnsSeen)) { // the reply never showed up: do not hang
        reset();
        break;
      }
      if (scriptDone() && !game.resultPending() && game.history().size() > _turnsSeen && _refuting) {
        _turnsSeen = game.history().size();
        _refuting = false;
        wrong("Not quite - Black has a defence. Try again.");
        break;
      }
      if (scriptDone() && !game.resultPending() && game.history().size() > _turnsSeen) {
        _turnsSeen = game.history().size();
        _stage = 1;
        say("");
        if (_hintLevel >= 2) _hintLevel = 1; // the ring was for the first turn; ask again
        setPhase(Phase::Playing);
        _board->setInputLocked(false);
      }
      break;
    case Phase::Wrong: // the position stays for the player to study; "Try again" (the Reset button) resets it
      break;
    case Phase::Solved:
      _solvedClock += step;
      break;
    case Phase::Solution:
      runScript(step);
      if (scriptDone()) _doneClock += step;
      if (scriptDone() && _doneClock > kStuckSeconds && game.history().size() != _puzzle->solution.size()) { // a submit that did not take
        reset();
        break;
      }
      if (scriptDone() && !game.resultPending() && game.history().size() == _puzzle->solution.size()) {
        say("That was the solution. Reset to try it yourself.");
        setPhase(Phase::SolutionDone);
      }
      break;
    case Phase::SolutionDone:
      break;
  }

  const std::string texts[5] = {_wrapped[0], _puzzle->title, bannerText(), statusText(), hintTextShown()};
  bool changed = false;
  for (int i = 1; i < 5; ++i) changed = changed || texts[i] != _wrapped[i];
  if (changed) rewrap();
}

void PuzzleScreen::draw(App& app) const {
  if (!_puzzle) return;
  const play::BoardStyle& style = play::boardStyle(app.settings.boardView);
  _board->draw(app);

  play::drawPanel(_panel, style, 1.0f, 18.0f / std::min(_panel.width, _panel.height));
  if (ui::audit::enabled()) ui::audit::rect("puzzle panel", _panel, ui::audit::Kind::Panel);
  const float x = _panel.x + kPad;
  drawLines(UI::Fonts::mono(), UI::Font::mono, _step.lines, x, _step.y, kSmallLine, style.hudMuted, {_panel.x, _panel.y, _panel.width, _hint.rect.y - _panel.y});
  drawLines(UI::Fonts::section(), UI::Font::section, _title.lines, x, _title.y, kTitleLine, style.hudText, {_panel.x, _panel.y, _panel.width, _hint.rect.y - _panel.y});

  // the goal banner
  const Rectangle bannerCard = {x, _banner.y - kCardPad, kInnerW, _banner.height + 2 * kCardPad};
  drawCard(bannerCard, style, UI::withAlpha(style.accent, 34), style.accent);
  drawLines(UI::Fonts::button(), UI::Font::button, _banner.lines, x + kCardPad, _banner.y, kBannerLine, style.hudText, {_panel.x, _panel.y, _panel.width, _hint.rect.y - _panel.y});

  const ::Color statusColor = _tone == Tone::Bad ? style.check : _tone == Tone::Good ? style.accent : style.hudMuted;
  drawLines(UI::Fonts::body(), UI::Font::body, _status.lines, x, _status.y, kBodyLine, _message.empty() ? style.hudMuted : statusColor, {_panel.x, _panel.y, _panel.width, _hint.rect.y - _panel.y});

  if (!_hintText.lines.empty()) {
    const Rectangle card = {x, _hintText.y - kCardPad - kSmallLine - 2.0f, kInnerW, _hintText.height + 2 * kCardPad + kSmallLine + 2.0f};
    drawCard(card, style, UI::withAlpha(style.hudBorder, 70), style.hudBorder);
    DrawTextEx(UI::Fonts::mono(), "HINT", {x + kCardPad, std::floor(card.y + kCardPad)}, UI::Font::mono, 0.0f, style.accent);
    drawLines(UI::Fonts::body(), UI::Font::body, _hintText.lines, x + kCardPad, _hintText.y, kBodyLine, style.hudText, {_panel.x, _panel.y, _panel.width, _hint.rect.y - _panel.y});
  }

  const float a = app.screens.navAlpha();
  _hint.draw(a);
  _reset.draw(a);
  _solution.draw(a);
  _back.draw(a);

  if (_phase == Phase::Solved) {
    const bool reduced = UI::Motion::reduced();
    const Vector2 center = {_card.x + _card.width / 2.0f, _card.y + _card.height / 2.0f};
    const float pop = reduced ? 1.0f : 0.94f + 0.06f * UI::Motion::easeOutBack(UI::Motion::clamp01(_solvedClock / 0.35f));
    // A scrim over the board, then the card
    DrawRectangleRec({0.0f, UI::Layout::safeTop - 10.0f, static_cast<float>(GetScreenWidth()) - kBoardInset + 16.0f,
                      static_cast<float>(GetScreenHeight()) - UI::Layout::safeTop + 10.0f},
                     UI::withAlpha(style.ink, 40));
    if (!reduced) { // a few soft rings spreading from the card, once
      for (int i = 0; i < 3; ++i) {
        const float t = (_solvedClock - 0.12f * static_cast<float>(i)) / 1.1f;
        if (t <= 0.0f || t >= 1.0f) continue;
        const float r = 120.0f + 150.0f * UI::Motion::easeOutCubic(t);
        DrawRing(center, r, r + 3.0f, 0.0f, 360.0f, 72, UI::withAlpha(style.accent, static_cast<unsigned char>(110.0f * (1.0f - t))));
      }
    }
    const Rectangle card = {center.x - _card.width * pop / 2.0f, center.y - _card.height * pop / 2.0f, _card.width * pop, _card.height * pop};
    play::drawPanel(card, style, 1.0f, 18.0f / std::min(card.width, card.height));
    const float cx = _card.x + 26.0f;
    DrawTextEx(UI::Fonts::mono(), "SOLVED", {cx, std::floor(_card.y + 22.0f)}, UI::Font::mono, 0.0f, style.accent);
    drawCheck(_card.x + _card.width - 26.0f - 26.0f, _card.y + 20.0f, 26.0f, 4.0f, style.accent);
    DrawTextEx(UI::Fonts::section(), "Checkmate!", {cx, std::floor(_card.y + 48.0f)}, UI::Font::section, 0.0f, style.hudText);
    const std::string line = _puzzle->title + " - mate in " + std::to_string(_puzzle->mateIn());
    DrawTextEx(UI::Fonts::body(), line.c_str(), {cx, std::floor(_card.y + 98.0f)}, UI::Font::body, 0.0f, style.hudText);
    const auto& all = puzzles::all();
    size_t done = 0;
    for (const puzzles::Puzzle& p : all) done += puzzles::progress().solved(p.id);
    const std::string total = std::to_string(done) + " of " + std::to_string(all.size()) + " puzzles solved";
    DrawTextEx(UI::Fonts::body(), total.c_str(), {cx, std::floor(_card.y + 126.0f)}, UI::Font::body, 0.0f, style.hudMuted);
    if (hasNext()) _next.draw(a);
    else DrawTextEx(UI::Fonts::body(), "That was the last one.", {cx, std::floor(_card.y + 154.0f)}, UI::Font::body, 0.0f, style.hudMuted);
    _list.draw(a);
  }
}

bool PuzzleScreen::escape(App& app) { return _board && _board->escape(app); }

void PuzzleScreen::back(App& app) { app.screens.replace(std::make_unique<PuzzleListScreen>()); }
