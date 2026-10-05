#include <doctest/doctest.h>

#include "test_support.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>

using namespace Chess;
using namespace test;

namespace {

constexpr std::array<unsigned, 6> kSeeds = {1u, 2u, 3u, 7u, 42u, 2024u};

// Dump of a single board's contents, including each piece's back-reference to its board and square.
std::string dumpBoard(const std::shared_ptr<Board>& board) {
  std::string out;
  for (int y = 0; y < board->dim(); ++y) {
    for (int x = 0; x < board->dim(); ++x) {
      auto piece = board->getPiece({x, y});
      if (!piece) {
        out += '.';
        continue;
      }
      char c = piece->symbol();
      if (piece->color() == PieceColor::PIECEBLACK) c = char(c - 'A' + 'a');
      out += c;
      // The piece must know where it is.
      if (piece->getBoard() != board or not(piece->getPosition() == Position2D(x, y))) out += '!';
    }
    out += '/';
  }
  return out;
}

using BoardDumps = std::vector<std::pair<std::shared_ptr<Board>, std::string>>;

BoardDumps dumpAllBoards(const IGame& game) {
  BoardDumps dumps;
  for (const auto& timeLine : game.getTimeLines()) {
    for (const auto& board : timeLine->getBoards()) dumps.emplace_back(board, dumpBoard(board));
  }
  return dumps;
}

std::vector<int> sortedAbsDelta(const Vector4D& a, const Vector4D& b) {
  std::vector<int> d = {std::abs(a.x() - b.x()), std::abs(a.y() - b.y()), std::abs(a.z() - b.z()),
                        std::abs(a.w() - b.w())};
  std::sort(d.begin(), d.end());
  return d;
}

struct Coverage {
  int moves = 0;
  int knightMoves = 0;
  int crossBoardMoves = 0;
};

// Property (a) and (d): checks every target of every own piece on every moveable board.
void checkMoveGenSoundness(const IGame& game, Coverage& cov) {
  const int parity = int(game.getCurrentTurnColor());
  for (const auto& board : game.getMoveableBoards()) {
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece({x, y});
        if (!piece or piece->color() != game.getCurrentTurnColor()) continue;
        SelectedPosition from(board, Position2D(x, y));
        std::set<std::string> seen;
        for (const auto& to : game.getMoveablePositions(from)) {
          ++cov.moves;
          REQUIRE(to.board != nullptr);
          REQUIRE(to.position.x() >= 0);
          REQUIRE(to.position.x() < game.dim());
          REQUIRE(to.position.y() >= 0);
          REQUIRE(to.position.y() < game.dim());
          const int tl = to.board->timeLineId();
          const int ht = to.board->halfTurnNumber();
          CAPTURE(piece->name());
          CAPTURE(key(from));
          CAPTURE(key(to));
          REQUIRE(game.boardExists(tl, ht));
          CHECK(game.getBoard(tl, ht) == to.board);
          CHECK(ht % 2 == parity);
          auto target = to.board->getPiece(to.position);
          CHECK((target == nullptr or target->color() != game.getCurrentTurnColor()));
          CHECK(key(to) != key(from));
          CHECK(seen.insert(key(to)).second);
          if (to.board != board) ++cov.crossBoardMoves;
          if (piece->name() == "knight") {
            ++cov.knightMoves;
            CHECK(sortedAbsDelta(from.toVector4D(), to.toVector4D()) == std::vector<int>{0, 0, 1, 2});
          }
        }
      }
    }
  }
}

// Plays up to `plies` random plies (submitting the turn when no move is left), undoing occasionally.
void scribble(IGame& game, std::mt19937& rng, int plies) {
  for (int i = 0; i < plies and not game.gameEnd(); ++i) {
    if (game.getMoveableBoards().empty()) {
      if (!game.undoable()) return;
      game.submitTurn();
      continue;
    }
    auto moves = game.allPseudoLegalMoves();
    if (moves.empty()) return;
    game.makeMove(moves[std::uniform_int_distribution<std::size_t>(0, moves.size() - 1)(rng)]);
    if (std::uniform_int_distribution<int>(0, 5)(rng) == 0) game.undo();
  }
}

template <class G>
void fuzzGame(unsigned seed, int plies, Coverage& cov) {
  std::mt19937 rng(seed);
  std::shared_ptr<IGame> gamePtr = createGame<G>();
  IGame& game = *gamePtr;
  std::vector<int> createdHalfTurns; // half-turn of every board created by the moves of the current turn

  auto play = [&](const Move& move) {
    const std::size_t timeLinesBefore = game.getTimeLines().size();
    const int present = game.presentHalfTurn();
    auto before = dumpAllBoards(game);

    game.makeMove(move);

    CHECK(game.getTimeLines().size() >= timeLinesBefore);
    CHECK(game.getTimeLines().size() <= timeLinesBefore + 1);
    CHECK(game.presentHalfTurn() == present);
    for (const auto& [board, dump] : before) CHECK(dumpBoard(board) == dump);
    createdHalfTurns.push_back(std::min(move.from.board->halfTurnNumber() + 1, game.getNewBoard()->halfTurnNumber()));
  };

  for (int ply = 0; ply < plies and not game.gameEnd(); ++ply) {
    CAPTURE(ply);
    if (std::uniform_int_distribution<int>(0, 7)(rng) == 0) {
      // Clone, play random plies on the clone: the original must not notice.
      const std::string pre = snapshot(game);
      auto dumps = dumpAllBoards(game);
      auto copy = game.clone();
      CHECK(snapshot(*copy) == pre);
      scribble(*copy, rng, 12);
      CHECK(snapshot(game) == pre);
      for (const auto& [board, dump] : dumps) CHECK(dumpBoard(board) == dump);
    }
    if (game.getMoveableBoards().empty()) {
      REQUIRE(game.undoable()); // otherwise the game would be stuck
      const PieceColor colorBefore = game.getCurrentTurnColor();
      const int expectedPresent = *std::min_element(createdHalfTurns.begin(), createdHalfTurns.end());
      game.submitTurn();
      createdHalfTurns.clear();
      CHECK(game.getCurrentTurnColor() == opposite(colorBefore));
      CHECK(game.presentHalfTurn() == expectedPresent);
      CHECK_FALSE(game.undoable());
      continue;
    }

    checkMoveGenSoundness(game, cov);
    auto moves = game.allPseudoLegalMoves();
    if (moves.empty()) break;
    const Move move = moves[std::uniform_int_distribution<std::size_t>(0, moves.size() - 1)(rng)];
    CAPTURE(key(move.from));
    CAPTURE(key(move.to));

    const std::string pre = snapshot(game);
    play(move);
    if (std::uniform_int_distribution<int>(0, 3)(rng) == 0) {
      game.undo();
      createdHalfTurns.pop_back();
      CHECK(snapshot(game) == pre);
      play(move);
    }
  }
}

} // namespace

TEST_CASE_TEMPLATE("random games keep the move generator sound and snapshots immutable", G, StandardGame,
                   CustomGameEmitBishop, CustomGameEmitKnight, CustomGameEmitQueen, CustomGameEmitRook,
                   CustomGameKVB, MiscGameTimeLineInvasion, MiscGameTimeLineBattle, MiscGameTimeLineFragment) {
  Coverage cov;
  for (unsigned seed : kSeeds) {
    CAPTURE(seed);
    CAPTURE(NameOfGame<G>::value);
    fuzzGame<G>(seed, 60, cov);
  }
  CHECK(cov.moves > 0);
}

TEST_CASE("fuzz exercises knights and cross-board moves") {
  Coverage cov;
  fuzzGame<StandardGame>(1u, 80, cov);
  fuzzGame<CustomGameEmitKnight>(2u, 80, cov);
  fuzzGame<MiscGameTimeLineBattle>(3u, 80, cov);
  CHECK(cov.knightMoves > 0);
  CHECK(cov.crossBoardMoves > 0);
}

TEST_CASE("rook, bishop and king moves are subsets of queen moves from the same square") {
  constexpr int N = 6;
  constexpr int kTimeLines = 3;
  for (unsigned seed : kSeeds) {
    for (int trial = 0; trial < 40; ++trial) {
      std::mt19937 rng(seed * 1000 + trial);
      CAPTURE(seed);
      CAPTURE(trial);
      auto uni = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

      struct Blocker { int tl, x, y; PieceColor color; };
      const int tx = uni(0, N - 1), ty = uni(0, N - 1);
      std::vector<Blocker> blockers;
      for (int i = uni(0, 8); i > 0; --i) {
        Blocker b{uni(0, kTimeLines - 1), uni(0, N - 1), uni(0, N - 1),
                  uni(0, 1) ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK};
        if (b.tl == 1 and b.x == tx and b.y == ty) continue;
        blockers.push_back(b);
      }

      // Pawns serve as inert blockers; the layout is identical in each sandbox, only the piece under test changes.
      auto movesOf = [&](auto tag) {
        using P = decltype(tag);
        Sandbox game(N, kTimeLines);
        for (const auto& b : blockers) game.place(b.tl, b.x, b.y, make<Pawn>(b.color));
        game.place(1, tx, ty, make<P>(PieceColor::PIECEWHITE));
        std::set<std::string> keys;
        for (const auto& m : movesAt(game, game.tip(1), tx, ty)) keys.insert(key(m));
        return keys;
      };

      const auto queen = movesOf(Queen(PieceColor::PIECEWHITE));
      for (const auto& [name, subset] : {std::pair<const char*, std::set<std::string>>{"rook", movesOf(Rook(PieceColor::PIECEWHITE))},
                                         {"bishop", movesOf(Bishop(PieceColor::PIECEWHITE))},
                                         {"king", movesOf(King(PieceColor::PIECEWHITE))}}) {
        CAPTURE(name);
        for (const auto& k : subset) {
          CAPTURE(k);
          CHECK(queen.count(k) == 1);
        }
      }
    }
  }
}
