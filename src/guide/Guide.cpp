#include "guide/Guide.h"
#include "engine/GameCatalog.h"
#include "engine/Position.h"
#include <stdexcept>

namespace guide {

namespace {

using Chess::Core::PlayedMove;

std::string g_directory = "assets/guide";

/// Every move made on the game so far: the submitted turns, then the pending one.
std::vector<PlayedMove> played(const Chess::IGame& game) {
  std::vector<PlayedMove> all;
  for (const auto& turn : game.history()) all.insert(all.end(), turn.moves.begin(), turn.moves.end());
  all.insert(all.end(), game.pendingMoves().begin(), game.pendingMoves().end());
  return all;
}

/// The piece that stood on the move's source square (boards are immutable, so this is still the mover).
std::optional<Chess::Piece> mover(const Chess::IGame& game, const PlayedMove& m) {
  return game.board(m.move.from.l, m.move.from.t).at(Chess::Position2D(m.move.from.x, m.move.from.y));
}

/// The piece on the target square before the move (a capture when it is an enemy).
std::optional<Chess::Piece> victim(const Chess::IGame& game, const PlayedMove& m) {
  return game.board(m.move.to.l, m.move.to.t).at(Chess::Position2D(m.move.to.x, m.move.to.y));
}

bool turnSubmitted(const Chess::IGame& game) { return !game.history().empty(); }

bool knightThroughTime(const Chess::IGame& game) {
  if (game.timeLineCount() < 2) return false;
  for (const auto& m : played(game)) {
    const auto piece = mover(game, m);
    if (piece && piece->type == Chess::PieceType::Knight && m.move.to.t < m.move.from.t) return true;
  }
  return false;
}

bool blackTimeline(const Chess::IGame& game) { return game.hasTimeLine(-1); }

bool changedBoard(const Chess::IGame& game) {
  for (const auto& m : played(game))
    if (m.move.from.l != m.move.to.l || m.move.from.t != m.move.to.t) return true;
  return false;
}

bool pawnCapture(const Chess::IGame& game) {
  for (const auto& m : played(game)) {
    const auto piece = mover(game, m), taken = victim(game, m);
    if (piece && taken && piece->type == Chess::PieceType::Pawn && taken->color != piece->color) return true;
  }
  return false;
}

bool checkmate(const Chess::IGame& game) { return game.result() == Chess::GameResult::WhiteWins; }

bool promoted(const Chess::IGame& game) {
  for (const auto& m : played(game))
    if (m.promotes) return true;
  return false;
}

const Goal kFirstTurn = {"Make a move, then press Submit.",
                         "Click a piece, click one of the dots, then press Submit at the top.",
                         "Turn submitted: a new board appeared and now it is Black's move.", turnSubmitted};
const Goal kJump = {"Move the knight back in time.",
                    "Select the knight on the newest board, then click a dot on an older board to its left.",
                    "A new timeline appeared. Your knight lives there now; the old board carries on without it.",
                    knightThroughTime};
const Goal kBlackTimeline = {"Make a timeline for Black.",
                             "Select Black's knight on the last board of L0, then jump to the earlier Black board of L0 (T1b).",
                             "L-1 appeared below L0: Black's timelines grow downwards.", blackTimeline};
const Goal kMandatory = {"Move on both marked boards (L0 and L+1), then Submit.",
                         "Make one move on L0 and one on L+1. Submit lights up once no marked board is left.",
                         "Turn submitted. L+2 was optional, so it could stay as it was.", turnSubmitted};
const Goal kAxes = {"Make a move that lands on a different board.",
                    "Select a piece and click a dot on another board: an earlier one, or one on another timeline.",
                    "That piece changed boards. Time and timelines are just more directions to move in.", changedBoard};
const Goal kCapture = {"Capture a black pawn with a white pawn.",
                       "Pick a pawn that has a black pawn diagonally ahead of it, then click that pawn.",
                       "Captured. On a board, pawns take diagonally forward.", pawnCapture};
const Goal kEscape = {"Get out of check, then press Submit.",
                      "Move the king on L0 off the attacked square, and make a move on L+1 too.",
                      "Submitted: after your turn no king of yours can be captured.", turnSubmitted};
const Goal kMate = {"Checkmate Black in one move.",
                    "Black's king is boxed in by its own pawns. Which of your pieces can reach the back rank?",
                    "Checkmate! Black has no turn that escapes check.", checkmate};
const Goal kPromote = {"Promote the pawn on b7.",
                       "Select the pawn on b7, click b8, then choose a piece from the picker.",
                       "Promoted. Undo takes a move back if you want to try castling or en passant too.", promoted};

std::vector<Page> build() {
  std::vector<Page> p;
  p.push_back({"Boards and time",
               "5D chess is played on many boards at once. Every half-turn adds a new board to the right: first one where "
               "White is to move, then one where Black is. The ruler at the top counts the turns (T1w, T1b, T2w ...) and "
               "marks the present.",
               "", "01-boards-and-time.5dp", "", &kFirstTurn, false});
  p.push_back({"Moving through time",
               "A piece may move to an earlier board, not only to the newest one. That does not rewrite the past: it splits "
               "off a new timeline that continues from that earlier board. The board you left carries on in its own "
               "timeline, without the piece.",
               "Time is an axis like file and rank. A knight goes two along one axis and one along another.",
               "02-moving-through-time.5dp", "", &kJump, false});
  p.push_back({"Timelines and who owns them",
               "Timelines are numbered. The game starts with L0. A timeline that White creates gets the next number above "
               "(L+1, L+2 ...); one that Black creates gets the next number below (L-1, L-2 ...). White's branches grow "
               "upwards and Black's downwards.",
               "", "03-timeline-owners.5dp", "", &kBlackTimeline, false});
  p.push_back({"The present and active timelines",
               "The present is the earliest board among the active timelines. You must move on every board of an active "
               "timeline that lies on the present before you can submit; boards elsewhere are optional. A timeline is inactive when its creator "
               "is more than one timeline ahead of the opponent: L+2 here is dimmed and never holds the present back.",
               "", "04-present-and-active.5dp", "", &kMandatory, false});
  p.push_back({"Moving in four axes",
               "Every piece except the pawn moves along four axes: file, rank, time and timeline. A rook slides along one axis, a bishop "
               "along two at once, a queen along any mix, and the king takes one step of the same kind. The knight "
               "jumps two along one axis and one along another. Select the queen to see how far it reaches.",
               "Other variants add unicorns and dragons, which slide diagonally across three or four axes at once. "
               "This game does not use them.",
               "05-four-axes.5dp", "", &kAxes, false});
  p.push_back({"Pawns",
               "A pawn steps forward one rank (two from its first square). It can also step forward along the timeline "
               "axis: White towards lower timeline numbers, Black towards higher. It captures diagonally forward, or one "
               "timeline forward and one turn earlier or later.",
               "", "06-pawns.5dp", "", &kCapture, false});
  p.push_back({"Check across timelines",
               "A king is in check when an enemy piece could capture it next move, on any board, through time and across "
               "timelines. Here the black rook on L+1 can slide down one timeline onto the white king on L0. A turn may "
               "only be submitted when no king of yours can be captured, so Submit stays grey until you are safe.",
               "", "07-check.5dp", "", &kEscape, false});
  p.push_back({"Checkmate and stalemate",
               "If you are in check and no turn of yours escapes it, that is checkmate and the other side wins. If you are "
               "not in check but have no legal turn, it is stalemate and the game is a draw. The game searches the moves of "
               "every board and timeline before it decides.",
               "", "08-checkmate.5dp", "", &kMate, false});
  p.push_back({"Special moves",
               "Castling: the king moves two files towards an unmoved rook and the rook hops over it to the square the king "
               "crossed. Neither may have moved, the squares between must be empty, and the king may not be in check or "
               "pass through an attacked square on that board. En passant: a pawn that has just advanced two squares can "
               "be taken as if it had moved one. Promotion: a pawn that reaches the last rank becomes a queen, rook, bishop "
               "or knight, which you pick.",
               "Castling and en passant work within a single board. Try castling and e5xd6 on this one; Undo takes a move back.",
               "09-special-moves.5dp", "", &kPromote, false});
  p.push_back({"Your first game",
               "You know the rules now. Before each turn, glance at the ruler to see which boards need a move, and watch your "
               "king on every timeline, not only the one you are playing on. Branch into the past when it helps you and not "
               "just because you can.",
               "The Guide stays in the main menu whenever you want to come back.",
               "", "standard", nullptr, true});
  return p;
}

} // namespace

const std::vector<Page>& pages() {
  static const std::vector<Page> all = build();
  return all;
}

void setDirectory(std::string directory) { g_directory = std::move(directory); }
const std::string& directory() { return g_directory; }

std::shared_ptr<Chess::IGame> load(const Page& page) {
  if (*page.position) return Chess::Core::loadPositionFile(g_directory + "/" + page.position).makeGame();
  auto game = Chess::GameCatalog::create(page.mode);
  if (!game) throw std::runtime_error(std::string("guide: unknown mode ") + page.mode);
  return game;
}

bool anyMoveMade(const Chess::IGame& game) { return !game.history().empty() || !game.pendingMoves().empty(); }

} // namespace guide
