#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "ai/Search.h"
#include "chess.h"

// "Play vs Computer": the pure logic around it (no graphics, unit tested): the player's side and the AI level with their
// names in the settings file and the save files, the search seed, the one-line record metadata and rebuilding a game
// a few turns back (the engine can only undo the unsubmitted moves of the current turn).
namespace play {

enum class SideChoice { White, Black, Random };
using AiLevel = Chess::ai::Level;

/// A game against the computer: which side the player has (already resolved: never "random"), the level, and the seed
/// of this game. The AI's decision of a turn depends only on the position, the level and searchSeed(seed, turns played).
struct VsAi {
  Chess::PieceColor human = Chess::PieceColor::PIECEWHITE;
  AiLevel level = AiLevel::Normal;
  std::uint64_t seed = 1;

  Chess::PieceColor aiColor() const {
    return human == Chess::PieceColor::PIECEWHITE ? Chess::PieceColor::PIECEBLACK : Chess::PieceColor::PIECEWHITE;
  }
  bool operator==(const VsAi&) const = default;
};

// Names as stored in the settings and in records ("white", "easy"); fromName leaves `out` alone and returns false for an unknown one.
const char* name(SideChoice side);
const char* name(AiLevel level);
bool fromName(std::string_view text, SideChoice& out);
bool fromName(std::string_view text, AiLevel& out);
/// What the mode screen shows: "White", "Easy"...
const char* title(SideChoice side);
const char* title(AiLevel level);
/// One line under the level buttons.
const char* describe(AiLevel level);

/// The side the player gets: White and Black as chosen; Random from the game's seed (so a record's seed tells it too).
Chess::PieceColor humanSide(SideChoice choice, std::uint64_t seed);
VsAi makeVsAi(SideChoice choice, AiLevel level, std::uint64_t seed);
/// The seed of the search of the turn that follows `turnsPlayed` submitted turns.
std::uint64_t searchSeed(std::uint64_t gameSeed, std::size_t turnsPlayed);

/// The record comment line: `# vs-computer: you=white level=normal seed=123` (no newline). A record reader skips comments, so
/// older versions load such a record as an ordinary game.
std::string formatMeta(const VsAi& vs);
/// Parses that line; nullopt if it is not one or a field is missing, repeated or unknown.
std::optional<VsAi> parseMeta(std::string_view line);
/// The metadata of record text: the first such line among the comments before the header; nullopt for a plain record.
std::optional<VsAi> findMeta(std::string_view record);
/// `record` with the metadata line inserted after its first line (the magic).
std::string withMeta(const std::string& record, const VsAi& vs);

/// What vs-Computer Undo takes back right now (0: nothing; 1: the player's last turn while the computer thinks; 2: the player's last turn
/// and the computer's reply). `toMove` is the side to move, `historySize` the submitted turns, `pending` whether the current turn has
/// moves, `computerMoving` that the computer is part-way through playing its turn. Unsubmitted moves of the player's own turn are
/// undone one by one first (0 here); playing Black the computer's opening turn is never taken back.
int takeBackCount(const VsAi& vs, Chess::PieceColor toMove, std::size_t historySize, bool pending, bool computerMoving);

/// The game as it was after its first `turns` submitted turns, replayed from the start position (the result search of the last
/// one is left pending, like after a submit). Null if the game has no start position or `turns` exceeds its history.
std::shared_ptr<Chess::IGame> replayPrefix(const Chess::IGame& game, std::size_t turns);

} // namespace play
