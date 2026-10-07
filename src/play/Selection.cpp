#include "play/Selection.h"
#include <algorithm>

namespace play {

using Chess::Core::Coord;

namespace {
std::optional<Chess::Piece> pieceAt(const Coord& c, const Chess::IGame& game) {
  if (!game.boardExists(c)) return std::nullopt;
  const Chess::Board& board = game.board(c.l, c.t);
  if (c.x < 0 || c.y < 0 || c.x >= board.dim() || c.y >= board.dim()) return std::nullopt;
  return board.at(Chess::Position2D(c.x, c.y));
}
} // namespace

bool Selection::canPickUp(const Coord& c, const Chess::IGame& game) {
  const auto piece = pieceAt(c, game);
  return piece && piece->color == game.getCurrentTurnColor() && game.canMakeMoveFromBoard(game.getBoard(c.l, c.t));
}

Intent::Reason Selection::rejection(const Coord& c, const Chess::IGame& game) {
  const auto piece = pieceAt(c, game);
  if (!piece || game.result() != Chess::GameResult::Ongoing) return Intent::Reason::None;
  if (piece->color != game.getCurrentTurnColor()) return Intent::Reason::NotYourPiece;
  const auto board = game.getBoard(c.l, c.t);
  if (game.canMakeMoveFromBoard(board)) return Intent::Reason::None;
  return game.timeLine(c.l)->back() == board ? Intent::Reason::OtherSidesBoard : Intent::Reason::HistoryBoard;
}

Intent Selection::click(std::optional<Coord> square, const Chess::IGame& game) {
  Intent intent;
  if (!square || game.result() != Chess::GameResult::Ongoing) return intent;
  const Coord c = *square;

  if (_from) {
    _promoting.reset(); // a click anywhere cancels an open promotion choice (a click on its target reopens it below)
    const auto it = std::find_if(_moves.begin(), _moves.end(), [&](const Chess::Core::Move& m) { return m.to == c; });
    if (it != _moves.end()) { // the first move to a square is the Queen promotion
      intent.move = *it;
      if (std::count_if(_moves.begin(), _moves.end(), [&](const Chess::Core::Move& m) { return m.to == c; }) > 1) {
        _promoting = c;
        intent.kind = Intent::Kind::Promote;
        return intent;
      }
      intent.kind = Intent::Kind::Move;
      clear();
      return intent;
    }
    if (c == *_from) {
      clear();
      intent.kind = Intent::Kind::Clear;
      return intent;
    }
  }
  if (!canPickUp(c, game)) {
    intent.reason = rejection(c, game);
    if (intent.reason != Intent::Reason::None) {
      intent.kind = Intent::Kind::Rejected;
      intent.from = c;
    }
    return intent;
  }

  _from = c;
  _moves = game.legalMovesFrom(c);
  _targets.clear();
  for (const auto& m : _moves)
    if (std::find(_targets.begin(), _targets.end(), m.to) == _targets.end()) _targets.push_back(m.to);
  intent.kind = Intent::Kind::Select;
  intent.from = c;
  return intent;
}

std::vector<Chess::Core::Move> Selection::promotionChoices() const {
  std::vector<Chess::Core::Move> choices;
  if (_promoting)
    for (const auto& m : _moves)
      if (m.to == *_promoting) choices.push_back(m);
  return choices;
}

Intent Selection::choosePromotion(Chess::PieceType piece) {
  Intent intent;
  for (const auto& m : promotionChoices()) {
    if (m.promotion != piece) continue;
    intent.kind = Intent::Kind::Move;
    intent.move = m;
    clear();
    return intent;
  }
  return intent;
}

void Selection::clear() {
  _promoting.reset();
  _from.reset();
  _moves.clear();
  _targets.clear();
}

} // namespace play
