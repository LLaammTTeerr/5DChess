#include <doctest/doctest.h>

#include "test_support.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <climits>
#include <set>

using namespace Chess;
using namespace test;

namespace {

constexpr std::array<unsigned, 6> kSeeds = {1u, 2u, 3u, 7u, 42u, 2024u};
constexpr std::array<unsigned, 4> kFuzzSeeds = {1u, 2u};

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
  int whiteBranches = 0;
  int blackBranches = 0;
  int turns = 0;
  // What the random games actually exercised (every counter is asserted to be > 0 by the tests below).
  int castles = 0;
  int enPassant = 0;
  int promotions = 0;
  int pawnTimelineMoves = 0;
  int inCheckAtTurnStart = 0;
  int optionalBoardMoves = 0;
  int decidedGames = 0;
  int searchRunning = 0; // turns whose legal-turn search did not finish within the node budget (tracked, not asserted)
};

// What kind of move is this, judged before it is made.
void classify(const IGame& game, const Move& m, Coverage& cov) {
  auto piece = m.from.board->getPiece(m.from.position);
  const bool same = m.to.board == m.from.board;
  const int dx = m.to.position.x() - m.from.position.x();
  const int lastRank = piece->color() == PieceColor::PIECEWHITE ? game.dim() - 1 : 0;
  if (piece->type() == PieceType::King and same and std::abs(dx) == 2) ++cov.castles;
  if (piece->type() == PieceType::Pawn) {
    if (same and dx != 0 and m.to.board->getPiece(m.to.position) == nullptr) ++cov.enPassant;
    if (m.to.position.y() == lastRank) ++cov.promotions;
    if (!same) ++cov.pawnTimelineMoves;
  }
  const auto mandatory = game.mandatoryBoards();
  if (std::find(mandatory.begin(), mandatory.end(), m.from.board) == mandatory.end()) ++cov.optionalBoardMoves;
}

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

// Builds a random legal turn (moves on random moveable boards, undoing dead ends) and leaves it pending.
// Returns false if no legal turn was found within a few attempts. `onMove` sees every move that is made.
template <class F>
bool buildRandomTurn(IGame& game, std::mt19937& rng, F&& onMove) {
  for (int attempt = 0; attempt < 12; ++attempt) {
    for (int step = 0; step < 10; ++step) {
      if (game.canSubmit() and test::randInt(rng, 0, 2) != 0) return true;
      auto moves = game.allPseudoLegalMoves();
      if (moves.empty()) break;
      // Pawn moves and castling are favoured, so that double steps, en passant, promotion and castling occur in short games.
      std::vector<int> weight(moves.size(), 1);
      int total = 0;
      for (std::size_t i = 0; i < moves.size(); ++i) {
        auto piece = moves[i].from.board->getPiece(moves[i].from.position);
        if (piece->type() == PieceType::Pawn) {
          weight[i] = 4;
          const bool sameBoard = moves[i].to.board == moves[i].from.board;
          if (sameBoard and moves[i].to.position.x() != moves[i].from.position.x()
              and moves[i].to.board->getPiece(moves[i].to.position) == nullptr)
            weight[i] = 60; // en passant, when it is on offer
          if (moves[i].to.position.y() == (piece->color() == PieceColor::PIECEWHITE ? game.dim() - 1 : 0)) weight[i] = 40; // promotion
        }
        if (piece->type() == PieceType::King and moves[i].to.board == moves[i].from.board
            and std::abs(moves[i].to.position.x() - moves[i].from.position.x()) == 2) weight[i] = 12;
        total += weight[i];
      }
      int pick = test::randInt(rng, 0, total - 1);
      std::size_t idx = 0;
      while (pick >= weight[idx]) pick -= weight[idx++];
      Move move = moves[idx];
      onMove(game, move);
      static const PieceType promos[] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
      game.makeMove(move, promos[test::randInt(rng, 0, 3)]);
      if (test::randInt(rng, 0, 5) == 0) game.undo();
    }
    if (game.canSubmit()) return true;
    while (game.undoable()) game.undo();
  }
  return false;
}

// Plays up to `turns` random legal turns on `game` (used on clones).
void scribble(IGame& game, std::mt19937& rng, int turns) {
  for (int i = 0; i < turns and game.result() == GameResult::Ongoing; ++i) {
    if (!buildRandomTurn(game, rng, [](const IGame&, const Move&) {})) return;
    game.submitTurn();
  }
}

// The official active-timeline rule, derived from scratch: the n-th timeline created by a player is active iff the
// opponent has created at least n - 1 timelines. White creates positive IDs above the original ones, Black negative
// IDs below them.
bool expectedActive(int id, int origMin, int origMax, int whiteCreated, int blackCreated) {
  if (id >= origMin and id <= origMax) return true;
  if (id > origMax) return (id - origMax) <= blackCreated + 1;
  return (origMin - id) <= whiteCreated + 1;
}

// Invariants that must hold at any moment between turns.
void checkTimelineInvariants(const IGame& game, int origMin, int origMax) {
  int whiteCreated = 0, blackCreated = 0;
  for (int id : game.timeLineIds()) {
    if (id > origMax) ++whiteCreated;
    if (id < origMin) ++blackCreated;
  }
  // IDs are allocated contiguously outwards from the original ones.
  CHECK(game.maxTimeLineId() == origMax + whiteCreated);
  CHECK(game.minTimeLineId() == origMin - blackCreated);
  int present = INT_MAX;
  for (int id : game.timeLineIds()) {
    const bool active = expectedActive(id, origMin, origMax, whiteCreated, blackCreated);
    CHECK(game.isTimeLineActive(id) == active);
    if (active) present = std::min(present, game.timeLine(id)->halfTurnNumber());
  }
  CHECK(game.presentHalfTurn() == present);
  CHECK(game.bufferHalfTurn() == present);
  CHECK(present % 2 == int(game.getCurrentTurnColor()));
}

template <class G>
void fuzzGame(unsigned seed, int turns, Coverage& cov) {
  std::mt19937 rng(seed);
  std::shared_ptr<IGame> gamePtr = createGame<G>();
  IGame& game = *gamePtr;
  const int origMin = game.minTimeLineId();
  const int origMax = game.maxTimeLineId();

  for (int turn = 0; turn < turns and game.result() == GameResult::Ongoing; ++turn) {
    CAPTURE(turn);
    checkTimelineInvariants(game, origMin, origMax);

    if (test::randInt(rng, 0, 5) == 0) {
      // Clone, play random turns on the clone: the original must not notice, the clone must stay consistent.
      const std::string pre = snapshot(game);
      auto dumps = dumpAllBoards(game);
      auto copy = game.clone();
      CHECK(snapshot(*copy) == pre);
      CHECK(copy->result() == game.result());
      scribble(*copy, rng, 4);
      checkTimelineInvariants(*copy, origMin, origMax);
      CHECK(snapshot(game) == pre);
      for (const auto& [board, dump] : dumps) CHECK(dumpBoard(board) == dump);
    }

    checkMoveGenSoundness(game, cov);

    const PieceColor mover = game.getCurrentTurnColor();
    const std::vector<int> idList = game.timeLineIds();
    const std::set<int> idsBefore(idList.begin(), idList.end());
    const std::string startSnapshot = snapshot(game);
    auto before = dumpAllBoards(game);
    if (game.inCheck()) ++cov.inCheckAtTurnStart;
    auto onMove = [&](const IGame& g, const Move& m) { ++cov.moves; classify(g, m, cov); };

    if (!buildRandomTurn(game, rng, onMove)) {
      // Dead end for the random player: nothing may have changed.
      CHECK(snapshot(game) == startSnapshot);
      break;
    }
    // A pending turn that may be submitted: nobody can capture the mover's kings, no board was left behind.
    CHECK(game.mandatoryBoards().empty());
    CHECK(game.threatsAgainst(mover).empty());
    for (const auto& [board, dump] : before) CHECK(dumpBoard(board) == dump);
    // Ownership of new timelines by sign of the ID.
    for (int id : game.timeLineIds()) {
      if (idsBefore.count(id)) continue;
      if (mover == PieceColor::PIECEWHITE) { CHECK(id > origMax); ++cov.whiteBranches; }
      else { CHECK(id < origMin); ++cov.blackBranches; }
    }
    game.submitTurn();
    if (!game.resolveResult(50000)) ++cov.searchRunning;
    ++cov.turns;
    CHECK(game.getCurrentTurnColor() == opposite(mover));
    CHECK_FALSE(game.undoable());
    CHECK(game.threatsAgainst(mover).empty());
    if (game.result() != GameResult::Ongoing) {
      ++cov.decidedGames;
      // A decided game really has no legal turn left (unbounded re-check).
      CHECK(game.findLegalTurn(2000000) == TurnSearch::Status::None);
      CHECK(game.inCheck() == (game.result() != GameResult::Draw));
    }
  }
}

} // namespace

TEST_CASE_TEMPLATE("random games keep the move generator sound and snapshots immutable", G, StandardGame,
                   CustomGameEmitBishop, CustomGameEmitKnight, CustomGameEmitQueen, CustomGameEmitRook,
                   CustomGameKVB, MiscGameTimeLineInvasion, MiscGameTimeLineBattle, MiscGameTimeLineFragment) {
  Coverage cov;
  for (unsigned seed : kFuzzSeeds) {
    CAPTURE(seed);
    CAPTURE(NameOfGame<G>::value);
    fuzzGame<G>(seed, 8, cov);
  }
  CHECK(cov.moves > 0);
}

TEST_CASE("fuzz exercises knights and cross-board moves") {
  Coverage cov;
  fuzzGame<StandardGame>(1u, 16, cov);
  fuzzGame<CustomGameEmitKnight>(2u, 16, cov);
  fuzzGame<MiscGameTimeLineBattle>(3u, 16, cov);
  CHECK(cov.knightMoves > 0);
  CHECK(cov.crossBoardMoves > 0);
  CHECK(cov.turns > 12);
  CHECK(cov.whiteBranches > 0);
  CHECK(cov.blackBranches > 0);
}

// Plays random legal turns without any of the property checks (fast), only to count what the games reach.
template <class G>
void coverWalk(unsigned seed, int turns, Coverage& cov) {
  std::mt19937 rng(seed);
  std::shared_ptr<IGame> game = createGame<G>();
  for (int turn = 0; turn < turns and game->result() == GameResult::Ongoing; ++turn) {
    if (game->inCheck()) ++cov.inCheckAtTurnStart;
    if (!buildRandomTurn(*game, rng, [&](const IGame& g, const Move& m) { ++cov.moves; classify(g, m, cov); })) break;
    game->submitTurn();
    if (!game->resolveResult(50000)) ++cov.searchRunning;
    ++cov.turns;
    if (game->result() != GameResult::Ongoing) ++cov.decidedGames;
  }
}

TEST_CASE("random games reach castling, en passant, promotion, pawn timeline moves, check, optional boards and a decision") {
  // These property tests only show that the engine is self-consistent (soundness, immutability, invariants); they are NOT
  // evidence that the rules are right. That is what the directed tests and tools/refcheck (a comparison with 5d-chess-js)
  // are for. This test makes sure the random games get anywhere near the interesting rules at all.
  Coverage cov;
  for (unsigned seed = 1; seed <= 24; ++seed) coverWalk<StandardGame>(seed, 20, cov);
  for (unsigned seed = 1; seed <= 5; ++seed) coverWalk<CustomGameKVB>(seed, 20, cov);
  for (unsigned seed = 1; seed <= 10; ++seed) coverWalk<MiscGameTimeLineFragment>(seed, 20, cov);
  MESSAGE("castles " << cov.castles << ", en passant " << cov.enPassant << ", promotions " << cov.promotions
                     << ", pawn timeline moves " << cov.pawnTimelineMoves << ", in check at turn start "
                     << cov.inCheckAtTurnStart << ", optional-board moves " << cov.optionalBoardMoves << ", decided games "
                     << cov.decidedGames << ", searches still running " << cov.searchRunning << ", turns " << cov.turns);
  CHECK(cov.castles > 0);
  CHECK(cov.enPassant > 0);
  CHECK(cov.promotions > 0);
  CHECK(cov.pawnTimelineMoves > 0);
  CHECK(cov.inCheckAtTurnStart > 0);
  CHECK(cov.optionalBoardMoves > 0);
  CHECK(cov.decidedGames > 0);
}

TEST_CASE("rook, bishop and king moves are subsets of queen moves from the same square") {
  constexpr int N = 6;
  constexpr int kTimeLines = 3;
  for (unsigned seed : kSeeds) {
    for (int trial = 0; trial < 40; ++trial) {
      std::mt19937 rng(seed * 1000 + trial);
      CAPTURE(seed);
      CAPTURE(trial);
      auto uni = [&](int lo, int hi) { return test::randInt(rng, lo, hi); };

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
