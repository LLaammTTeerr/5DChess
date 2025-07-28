#include <array>
#include <optional>
#include <vector>

enum class PieceType : int {
  Pawn = 0,
  Knight = 1,
  Bishop = 2,
  Rook = 3,
  Queen = 4,
  King = 5,
};

enum class PieceColor : int{
  White = 0,
  Black = 1,
};

class Position {
public:
  Position() = default;
  Position(int, int, int, int);

  /**
   * Returns the x-coordinate of the position.
   * @return x-coordinate
   */
  int x(void) const;

  /**
   * Returns the y-coordinate of the position.
   * @return y-coordinate
   */
  int y(void) const;

  /**
   * Returns the z-coordinate of the position.
   * @return z-coordinate
   */
  int z(void) const;

  /**
   * Returns the t-coordinate of the position.
   * @return t-coordinate
   */
  int t(void) const;
private:
  std::array<int, 4> _coordinates;
};

class Piece {
public:
  Piece(PieceType, PieceColor);

  /**
   * Returns the type of the piece.
   * @return PieceType
   */
  PieceType type(void) const;

  /**
   * Returns the color of the piece.
   * @return PieceColor
   */
  PieceColor color(void) const;
private:
  PieceType _type;
  PieceColor _color;
};

class Board {
public:
  Board(void);

  std::array<std::optional<Piece>, 8>& operator [](int x);
  const std::array<std::optional<Piece>, 8>& operator [](int x) const;
private:
  std::array<std::array<std::optional<Piece>, 8>, 8> _board;
};

class Move {
public:
  Move(Position, Position, Piece);

  /**
   * Returns the starting position of the move.
   * @return Position from which the piece is moved.
   */
  Position from(void) const;

  /**
   * Returns the ending position of the move.
   * @return Position to which the piece is moved.
   */
  Position to(void) const;

  /**
   * Returns the piece that is being moved.
   * @return Piece being moved.
   */
  Piece piece(void) const;
private:
  Position _from;
  Position _to;
  Piece _piece;
};

class Game {
public:
  Game(void);

  /**
   * Make a move in the game.
   * Notice that this will not check if the move is valid.
   * It is the responsibility of the caller to ensure that the move is valid.
   * You can use the function generateMoves() to get a list of valid moves.
   * @param move The move to make.
   */
  void make_move(const Move&);

  /**
   * Get piece on the board at the specified position.
   * @param Position The position to get.
   * @return Piece placed on the board at the specified position.
   *         If no piece is placed at the position, it returns an empty optional.
   */
  std::optional<Piece> get_piece(Position) const;

  /**
   * Generates a list of valid moves for the piece at the specified position.
   * This function does not check if the moves are valid in the context of the game.
   * @param pos The position of the piece to generate moves for.
   * @return A vector of valid moves for the piece at the specified position.
   */
  std::vector<Move> generate_moves(Position pos) const;

  /**
   * Returns the current timeline index.
   * @return Current timeline index.
   */
  int current_timeline_index(void);

  /**
   * Returns the number of timelines in the game.
   * @return Number of timelines.
   */
  int timeline_count(void) const;

  /**
   * Returns the parent timeline index of the current timeline.
   * @return Parent timeline index.
   */
  int parent_timeline_index(void) const;

  /**
   * Returns the timeline offset for the current timeline.
   * @return Timeline offset.
   */
  int timeline_offset(void) const;

  /**
   * Returns the person who created the specific timeline.
   * @param timelineIndex The index of the timeline.
   * @return The color of the piece that created the timeline, or std::nullopt if the timeline is the original one.
   */
  std::optional<PieceColor> created_by(int timelineIndex) const;
private:
  using Timeline = std::vector<Board>;
  std::vector<Timeline> _timelines;
  std::vector<int> _timelineOffsets;
  std::vector<int> _parent;
  std::vector<std::optional<PieceColor>> _createdBy;
  std::vector<Move> _moves;
  int _presentTimelineIndex;
};