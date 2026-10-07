#include "play/TurnChecklist.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include "play/MultiverseView.h"

namespace play {

using Chess::Core::Coord;
using Chess::Core::PlayedMove;

namespace {
constexpr const char* kDot = "\xC2\xB7";

std::string square(const Coord& c) { return std::string(1, static_cast<char>('a' + c.x)) + std::to_string(c.y + 1); }

std::string boardTitle(int timeline, int halfTurn) { return timelineLabel(timeline) + " " + kDot + " " + boardLabel(halfTurn); }

using Key = std::pair<int, int>;
Key keyOf(const Chess::Board& b) { return {b.timeLineId(), b.halfTurnNumber()}; }
bool has(const std::vector<Key>& v, const Key& k) { return std::find(v.begin(), v.end(), k) != v.end(); }

std::optional<Chess::Piece> pieceAt(const Chess::IGame& game, const Coord& c) {
  if (!game.boardExists(c)) return std::nullopt;
  return game.board(c.l, c.t).at(Chess::Position2D(c.x, c.y));
}
} // namespace

std::string moveText(const Chess::IGame& game, const PlayedMove& played) {
  const Coord& from = played.move.from;
  const Coord& to = played.move.to;
  const auto mover = pieceAt(game, from);
  std::string text;
  if (mover && mover->type != Chess::PieceType::Pawn) text += Chess::pieceSymbol(mover->type);
  text += square(from);
  if (from.l != to.l || from.t != to.t) {
    text += " -> " + boardTitle(to.l, to.t) + " " + square(to);
  } else {
    const auto victim = pieceAt(game, to);
    text += victim && mover && victim->color != mover->color ? "x" : "-";
    text += square(to);
  }
  if (played.promotes) text += std::string("=") + Chess::pieceSymbol(played.move.promotion);
  return text;
}

std::string submitLine(const TurnChecklist& c) {
  switch (c.submit) {
    case SubmitState::Ready: return "Submit: ready";
    case SubmitState::BoardsLeft: return "Submit: locked - " + std::to_string(c.boardsLeft) + (c.boardsLeft == 1 ? " board left" : " boards left");
    case SubmitState::MoveFirst: return "Submit: locked - make a move";
    case SubmitState::KingExposed: return "Submit: locked - a king is exposed";
  }
  return "";
}

TurnChecklist turnChecklist(const Chess::IGame& game) {
  TurnChecklist out;
  if (game.result() != Chess::GameResult::Ongoing) return out;

  // The turn as it began: the pending moves taken back on a copy (a board that was moved on is then a board to move on again)
  std::unique_ptr<Chess::IGame> start;
  if (!game.pendingMoves().empty()) {
    start = game.clone();
    while (start->undoable()) start->undo();
  }
  const Chess::IGame& began = start ? *start : game;

  std::vector<Key> mandatory, moveable;
  for (const auto& b : began.mandatoryBoards()) mandatory.push_back(keyOf(*b));
  for (const auto& b : began.getMoveableBoards()) moveable.push_back(keyOf(*b));
  // ... and as it stands now: a move into the past can take the present back, so boards that had to be moved on need not be any more
  std::vector<Key> mandatoryNow, moveableNow;
  for (const auto& b : game.mandatoryBoards()) mandatoryNow.push_back(keyOf(*b));
  for (const auto& b : game.getMoveableBoards()) moveableNow.push_back(keyOf(*b));

  struct Entry { Key key; RowState group; bool inactive; };
  std::vector<Entry> entries;
  for (const int id : began.timeLineIds()) {
    const auto line = began.timeLine(id);
    if (line->getBoards().empty()) continue;
    const Key key = keyOf(*line->back());
    const bool active = began.isTimeLineActive(id);
    if (has(mandatory, key)) entries.push_back({key, RowState::MustMove, !active});
    else if (has(moveable, key)) entries.push_back({key, RowState::Optional, !active});
    else if (active) entries.push_back({key, RowState::Waiting, false});
  }
  // Mandatory first, then Optional, then Waiting; the timeline that is drawn highest (the largest id) first within each
  std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    if (a.group != b.group) return static_cast<int>(a.group) < static_cast<int>(b.group);
    return a.key.first > b.key.first;
  });

  const auto& pending = game.pendingMoves();
  for (const Entry& e : entries) {
    ChecklistRow row;
    row.timeline = e.key.first;
    row.halfTurn = e.key.second;
    row.label = boardTitle(row.timeline, row.halfTurn);
    row.inactive = e.inactive;
    row.state = e.group;
    if (e.group != RowState::Waiting && game.timeLine(row.timeline)->back()->halfTurnNumber() <= row.halfTurn) {
      // not moved on: what it is now
      row.state = has(mandatoryNow, e.key) ? RowState::MustMove : has(moveableNow, e.key) ? RowState::Optional : RowState::Waiting;
    } else if (e.group != RowState::Waiting) {
      // Moved: it has a successor now. Said by the move made on it, else by the move that landed on it.
      row.state = RowState::Moved;
      for (const PlayedMove& m : pending)
        if (m.move.from.l == row.timeline && m.move.from.t == row.halfTurn) { row.detail = moveText(game, m); break; }
      if (row.detail.empty())
        for (const PlayedMove& m : pending)
          if (m.move.to.l == row.timeline && m.move.to.t == row.halfTurn) {
            const auto piece = pieceAt(game, m.move.from);
            row.detail = (piece ? Chess::pieceName(piece->type) : std::string("piece")) + " arrived from " + timelineLabel(m.move.from.l);
            break;
          }
    }
    out.rows.push_back(row);
  }

  // "3 / 5 boards": the boards of the turn that had to be moved on and were, of those plus the ones that still have to be
  // (no board had to be moved on: the optional ones)
  const bool anyMandatory = !mandatory.empty();
  for (const ChecklistRow& r : out.rows) {
    const bool counted = anyMandatory ? has(mandatory, {r.timeline, r.halfTurn}) : r.state != RowState::Waiting;
    if (counted && r.state == RowState::Moved) ++out.done;
  }
  out.boardsLeft = static_cast<int>(mandatoryNow.size());
  out.total = anyMandatory ? out.done + out.boardsLeft
                           : static_cast<int>(std::count_if(out.rows.begin(), out.rows.end(), [](const ChecklistRow& r) { return r.state != RowState::Waiting; }));
  out.submit = game.canSubmit() ? SubmitState::Ready
               : out.boardsLeft > 0 ? SubmitState::BoardsLeft
               : pending.empty() ? SubmitState::MoveFirst : SubmitState::KingExposed;

  if (!game.history().empty()) {
    const Chess::Core::PlayedTurn& turn = game.history().back();
    out.lastByWhite = turn.presentHalfTurn % 2 == 0;
    out.lastLabel = boardLabel(turn.presentHalfTurn);
    for (const PlayedMove& m : turn.moves) {
      LastMove l;
      l.from = m.move.from;
      l.to = m.move.to;
      l.text = timelineLabel(m.move.from.l) + "  " + moveText(game, m);
      out.last.push_back(std::move(l));
    }
  }
  return out;
}

} // namespace play
