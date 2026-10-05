// refcheck: plays a random legal game on the 5DChess engine and prints, as JSON lines, every position the player
// meets (the start of each turn and after each move of the turn) together with the complete list of pseudo-legal
// moves, so that tools/refcheck/compare.js can replay the very same game in 5d-chess-js and diff the move lists.
//
//   refcheck --mode standard --seed 1 --turns 25      one game
//   refcheck --vectors                                the movement vector tables (compare.js checks them too)
//
// Coordinates are printed in the ENGINE's terms (timeline ID, half-turn, file x, rank y); compare.js does the
// mapping to 5d-chess-js (see the header of compare.js).
#include "chess.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace Chess;

namespace {

struct Args {
  std::string mode = "standard";
  unsigned seed = 1;
  int turns = 25;
  bool vectors = false;
  int searchBudget = 200000;
};

int refCode(const std::shared_ptr<Piece>& piece) {
  static const int base[] = {11, 9, 7, 3, 5, 1}; // King, Queen, Rook, Bishop, Knight, Pawn (black = odd, white = even)
  const int code = base[int(piece->type())] + (piece->color() == PieceColor::PIECEWHITE ? 1 : 0);
  return piece->unmoved() ? -code : code;
}

std::string tipsJson(const IGame& game) {
  std::string out = "[";
  bool first = true;
  const int n = game.dim();
  for (const auto& line : game.getTimeLines()) {
    const auto board = line->back();
    if (!first) out += ",";
    first = false;
    out += "[" + std::to_string(line->ID()) + "," + std::to_string(board->halfTurnNumber()) + ",[";
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x) {
        auto piece = board->getPiece({x, y});
        out += std::to_string(piece ? refCode(piece) : 0);
        if (y != n - 1 || x != n - 1) out += ",";
      }
    }
    out += "]]";
  }
  return out + "]";
}

struct Played {
  Move move;
  PieceType promotion = PieceType::Queen;
};

int tagOf(const IGame& game, const Move& m) {
  auto piece = m.from.board->getPiece(m.from.position);
  const bool same = m.to.board == m.from.board;
  const int dx = m.to.position.x() - m.from.position.x();
  const int dy = m.to.position.y() - m.from.position.y();
  const int n = game.dim();
  if (piece->type() == PieceType::King and same and dy == 0 and std::abs(dx) == 2) return 2;
  if (piece->type() == PieceType::Pawn) {
    const int lastRank = piece->color() == PieceColor::PIECEWHITE ? n - 1 : 0;
    if (m.to.position.y() == lastRank) return 1;
    if (same and dx != 0 and m.to.board->getPiece(m.to.position) == nullptr) return 3;
  }
  return 0;
}

std::string moveJson(const Move& m, int tag) {
  char buf[160];
  std::snprintf(buf, sizeof buf, "[%d,%d,%d,%d,%d,%d,%d,%d,%d]", m.from.board->timeLineId(), m.from.board->halfTurnNumber(),
                m.from.position.x(), m.from.position.y(), m.to.board->timeLineId(), m.to.board->halfTurnNumber(),
                m.to.position.x(), m.to.position.y(), tag);
  return buf;
}

int promoCode(PieceType type, PieceColor color) {
  static const int base[] = {11, 9, 7, 3, 5, 1};
  return base[int(type)] + (color == PieceColor::PIECEWHITE ? 1 : 0);
}

std::string stateJson(const IGame& game, int turn, int index, bool turnStart, const Args& args) {
  std::string out = "{\"t\":\"state\",\"turn\":" + std::to_string(turn) + ",\"i\":" + std::to_string(index) +
                    ",\"color\":" + std::to_string(int(game.getCurrentTurnColor())) +
                    ",\"tips\":" + tipsJson(game) + ",\"moves\":[";
  bool first = true;
  for (const auto& m : game.allPseudoLegalMoves()) {
    if (!first) out += ",";
    first = false;
    out += moveJson(m, tagOf(game, m));
  }
  out += "],\"mand\":[";
  first = true;
  for (const auto& b : game.mandatoryBoards()) {
    if (!first) out += ",";
    first = false;
    out += std::to_string(b->timeLineId());
  }
  out += "],\"active\":[";
  first = true;
  for (int id : game.activeTimeLineIds()) {
    if (!first) out += ",";
    first = false;
    out += std::to_string(id);
  }
  out += "],\"inCheck\":" + std::to_string(game.inCheck() ? 1 : 0) + ",\"canSubmit\":" + std::to_string(game.canSubmit() ? 1 : 0);
  if (turnStart && args.searchBudget > 0) {
    const char* legal = "unknown";
    switch (game.findLegalTurn(args.searchBudget)) {
      case TurnSearch::Status::Found: legal = "found"; break;
      case TurnSearch::Status::None: legal = "none"; break;
      case TurnSearch::Status::Running: legal = "unknown"; break;
    }
    out += std::string(",\"legal\":\"") + legal + "\"";
  }
  return out;
}

// A random legal turn, built with backtracking. Pawn moves are favoured early so that double steps and en passant occur.
bool buildTurn(IGame& game, std::mt19937& rng, int turn, std::vector<Played>& turnMoves) {
  auto uni = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  for (int attempt = 0; attempt < 30; ++attempt) {
    turnMoves.clear();
    for (int step = 0; step < 12; ++step) {
      if (game.canSubmit() and uni(0, 2) != 0) return true;
      auto moves = game.allPseudoLegalMoves();
      if (moves.empty()) break;
      std::vector<int> weight(moves.size());
      int total = 0;
      for (std::size_t i = 0; i < moves.size(); ++i) {
        auto piece = moves[i].from.board->getPiece(moves[i].from.position);
        int w = 1;
        if (piece->type() == PieceType::Pawn and turn < 12) w = 4;
        if (piece->type() == PieceType::King and moves[i].to.board == moves[i].from.board
            and std::abs(moves[i].to.position.x() - moves[i].from.position.x()) == 2) w = 12; // castling
        weight[i] = w;
        total += w;
      }
      int pick = uni(0, total - 1);
      std::size_t idx = 0;
      while (pick >= weight[idx]) pick -= weight[idx++];
      Played p;
      p.move = moves[idx];
      static const PieceType promos[] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
      p.promotion = promos[uni(0, 3)];
      game.makeMove(p.move, p.promotion);
      turnMoves.push_back(p);
      if (uni(0, 7) == 0) {
        game.undo();
        turnMoves.pop_back();
      }
    }
    if (game.canSubmit()) return true;
    while (game.undoable()) game.undo();
  }
  return false;
}

std::shared_ptr<IGame> makeGame(const std::string& mode) {
  if (mode == "standard") return createGame<StandardGame>();
  if (mode == "emit_bishop") return createGame<CustomGameEmitBishop>();
  if (mode == "emit_knight") return createGame<CustomGameEmitKnight>();
  if (mode == "emit_queen") return createGame<CustomGameEmitQueen>();
  if (mode == "emit_rook") return createGame<CustomGameEmitRook>();
  if (mode == "kvb") return createGame<CustomGameKVB>();
  if (mode == "invasion") return createGame<MiscGameTimeLineInvasion>();
  if (mode == "battle") return createGame<MiscGameTimeLineBattle>();
  if (mode == "fragment") return createGame<MiscGameTimeLineFragment>();
  return nullptr;
}

void printVectors() {
  const char* names[] = {"king", "queen", "rook", "bishop", "knight", "pawn"};
  std::printf("{");
  for (int t = 0; t < 6; ++t) {
    std::printf("%s\"%s\":[", t ? "," : "", names[t]);
    const auto& v = pieceVectors(PieceType(t));
    for (std::size_t i = 0; i < v.size(); ++i) {
      std::printf("%s[%d,%d,%d,%d]", i ? "," : "", v[i][0], v[i][1], v[i][2], v[i][3]);
    }
    std::printf("]");
  }
  std::printf("}\n");
}

} // namespace

int main(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--mode" && i + 1 < argc) args.mode = argv[++i];
    else if (a == "--seed" && i + 1 < argc) args.seed = unsigned(std::stoul(argv[++i]));
    else if (a == "--turns" && i + 1 < argc) args.turns = std::stoi(argv[++i]);
    else if (a == "--search-budget" && i + 1 < argc) args.searchBudget = std::stoi(argv[++i]);
    else if (a == "--vectors") args.vectors = true;
    else {
      std::fprintf(stderr, "usage: refcheck [--mode M] [--seed S] [--turns N] [--search-budget B] | --vectors\n");
      return 2;
    }
  }
  if (args.vectors) {
    printVectors();
    return 0;
  }
  auto gamePtr = makeGame(args.mode);
  if (!gamePtr) {
    std::fprintf(stderr, "unknown mode %s\n", args.mode.c_str());
    return 2;
  }
  IGame& game = *gamePtr;
  std::mt19937 rng(args.seed);
  std::printf("{\"t\":\"init\",\"mode\":\"%s\",\"seed\":%u,\"n\":%d,\"doubleStep\":%d,\"castling\":%d,\"tips\":%s}\n",
              args.mode.c_str(), args.seed, game.dim(), game.rule().pawnCanMakeTwoMoveOnFirstTurn ? 1 : 0,
              game.rule().castling ? 1 : 0, tipsJson(game).c_str());

  for (int turn = 0; turn < args.turns; ++turn) {
    std::vector<Played> turnMoves;
    const std::string before = tipsJson(game);
    const bool built = buildTurn(game, rng, turn, turnMoves);
    while (game.undoable()) game.undo(); // replay from the turn start so that every intermediate state is dumped
    std::printf("%s", stateJson(game, turn, 0, true, args).c_str());
    if (!built) {
      std::printf(",\"noTurn\":1}\n");
      break;
    }
    int index = 0;
    for (const Played& p : turnMoves) {
      std::printf(",\"played\":[%s,%d]}\n", moveJson(p.move, tagOf(game, p.move)).c_str(),
                  promoCode(p.promotion, game.getCurrentTurnColor()));
      game.makeMove(p.move, p.promotion);
      ++index;
      std::printf("%s", stateJson(game, turn, index, false, args).c_str());
    }
    std::printf(",\"submit\":1}\n");
    game.submitTurn();
    game.resolveResult();
    (void)before;
    if (game.result() != GameResult::Ongoing) {
      std::printf("{\"t\":\"end\",\"result\":%d}\n", int(game.result()));
      return 0;
    }
  }
  std::printf("{\"t\":\"end\",\"result\":0}\n");
  return 0;
}
