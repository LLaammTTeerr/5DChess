#pragma once

// Move notation and game records (full description and grammar in docs/NOTATION.md).
//
// A move is written  (L0T3)e2>(L1T2)e4  or, for a promotion,  (L0T3)e7>(L0T3)e8=Q :
//   (L<timeline>T<turn>)<file><rank>  for the source and for the target, joined by '>', then an optional =Q / =R / =B / =N.
// <turn> is the full turn number of the board, counted from 1 (half-turn / 2 + 1, so Black's boards of turn 3 are T3 as well:
// the side that makes the move tells White's from Black's half-turn), <file> is 'a' + x (a = the left file seen from White,
// the king stands on e1), <rank> is y + 1, <timeline> may be negative or written with a leading '+' when read: (L-1T2)a1.
//
// A record is the starting position plus one line per submitted turn:
//
//   5dchess-record 1
//   mode: standard                       (or an embedded `position:` ... `end-position` block)
//   T1w: (L0T1)e2>(L0T1)e4
//   T1b: (L0T1)e7>(L0T1)e5
//   T2w: (L0T2)b1>(L0T2)c3               (several moves of one turn go on one line, separated by spaces)
//
// Everything that reads text is bounded and fails with Core::ParseError, never with undefined behaviour.

#include "chess.h"
#include "engine/Position.h"

#include <memory>
#include <string>
#include <string_view>

namespace Chess {

/**
 * The text of a move. A promotion suffix (=Q, =R, =B, =N) is written when `promotes` is true or `move.promotion` is not
 * the default Queen; Core::Move does not know whether the move promotes, the game does (IGame::history()).
 */
std::string toNotation(const Core::Move& move, bool promotes = false);

/**
 * Parses one move made by `mover` (needed because the text names full turns, not half-turns). No suffix means
 * promotion = Queen. Throws Core::ParseError on malformed text or numbers out of range; whether the move is legal
 * is not checked here (loadRecord does).
 */
Core::Move parseMove(std::string_view text, PieceColor mover);

/**
 * The record of the submitted turns of `game`. The moves of an unfinished turn are NOT part of it; if `droppedPending` is
 * given it is set to whether there were such moves, so that a save function can warn the player. The header names the
 * catalog mode if the game came from GameCatalog::create, otherwise it embeds the game's starting position. Throws
 * std::logic_error for a game that was not built from a Position (IGame::startPosition() is empty), e.g. test sandboxes.
 */
std::string writeRecord(const IGame& game, bool* droppedPending = nullptr);

/**
 * A game replayed from a record. Every move is checked against the engine (a legal move of the side to move, each
 * turn complete and submittable, the turn label matching the present, no moves after the game was decided); the first
 * violation throws Core::ParseError ("line N: ..."). The result of the replayed game is resolved before returning
 * (a bounded search; a position it cannot decide in time is simply left Ongoing).
 */
std::shared_ptr<IGame> loadRecord(std::string_view text);

} // namespace Chess
