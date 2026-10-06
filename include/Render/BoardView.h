#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include <cmath>

///@brief This is the view class for rendering a single board
/// This class converts model data to view data
/// This class will encapsulate both model and view data

const extern float BOARD_WORLD_SIZE; // Assuming BOARD_SIZE is defined somewhere in the project
const extern float HORIZONTAL_SPACING; // Assuming HORIZONTAL_SPACING is defined somewhere in the project
const extern float VERTICAL_SPACING; // Assuming VERTICAL_SPACING is defined somewhere in the project
const extern int STANDARD_BOARD_DIM;

class ChessView;

/// Stable identity of a board: (timeline ID, half turn). Board views are rebuilt every frame, so persistent
/// view-side animation state is keyed by this instead of by object.
using BoardKey = std::pair<int, int>;
inline BoardKey boardKeyOf(const Chess::Board& b) { return {b.timeLineId(), b.halfTurnNumber()}; }
/// World-space area of a board (same layout the controller uses when it builds board views)
Rectangle boardWorldArea(BoardKey key);
/// World-space rectangle of an engine square on a board with the given area (White at the bottom, file a on the left)
Rectangle squareRectFor(const Rectangle& area, int dim, Chess::Position2D pos);

// interface for board view, this is the interface that converting model data to view data
// This interface is used to render a single board
// It is used to render the board and handle input events
// It is also used to highlight positions on the board
// It is used to render the board texture
// It is used to render the board pieces
class BoardView {
public:
    virtual ~BoardView() = default;

    virtual void render() const = 0;
    virtual void render_highlightBoundaries() const = 0;
    /// Legal-target marker (dot, or capture ring on an occupied square); scale animates the pop-in
    virtual void render_legalTarget(Chess::Position2D position, float scale) const = 0;
    virtual void render_hoverSquare(Chess::Position2D position, float alpha = 1.0f) const = 0;
    /// Selected piece drawn raised (lift 0..1) with a small shadow; the board skips it in render_pieces()
    virtual void render_liftedPiece(Chess::Position2D position, float lift) const = 0;
    virtual Rectangle squareWorldRect(Chess::Position2D position) const = 0;
    /// Name of the piece on a square ("white_pawn") or nullptr
    virtual const std::string* pieceNameAt(Chess::Position2D position) const = 0;

    virtual void setBoardTexture(Texture2D* texture) = 0;

    virtual bool is3D() const = 0;

    virtual bool isMouseClickedOnBoard() const = 0;
    virtual bool isMouseOverBoard() const = 0;

    virtual Chess::Position2D getMouseOverPosition() const = 0;
    virtual Chess::Position2D getMouseClickedPosition() const = 0;

    virtual void setSupervisor(ChessView* supervisor) = 0;

    virtual void setRenderArea(Rectangle area) = 0;
    virtual Rectangle getArea() const = 0;

    virtual void setCamera2D(Camera2D* camera) = 0;
    virtual void setCamera3D(Camera3D* camera) = 0;

    virtual void setPiecePositions(const std::vector<std::pair<Chess::Position2D, std::string>>& piecePositions) = 0;

    // Timeline arrow support methods
    virtual void setBoard(std::shared_ptr<Chess::Board> board) = 0;
    virtual std::shared_ptr<Chess::Board> getBoard() const = 0;
    virtual Vector2 getBoardCenter() const = 0;
    virtual float getBoardSize() const = 0;

protected:
  /// Piece position and piece name
  std::vector<std::pair<Chess::Position2D, std::string>> _piecePositions;
  int _boardDim = 8;
  static constexpr int kMaxHidden = 3;
  Chess::Position2D _hidden[kMaxHidden] = {{-1, -1}, {-1, -1}, {-1, -1}};
  int _hiddenCount = 0;
  float _enter = 1.0f;
  bool _blinkEnabled = false;
  unsigned _blinkSeed = 0;
  bool isHiddenSquare(Chess::Position2D p) const {
    for (int i = 0; i < _hiddenCount; ++i) if (_hidden[i] == p) return true;
    return false;
  }

public:
  virtual void render_pieces() const = 0;
  virtual void render_highlightPiece(Chess::Position2D piecePosition) const = 0;
  virtual void setBoardDim(int dim) { _boardDim = dim; }
  int boardDim() const { return _boardDim; }

  // ---- Motion state, (re)applied by ChessView every frame ----
  /// 0..1: the board grows from 0.92x and fades in while < 1
  void setEnterProgress(float p) { _enter = p; }
  /// Squares whose piece is drawn by an overlay instead (moving or lifted pieces)
  void clearHiddenSquares() { _hiddenCount = 0; }
  void hideSquare(Chess::Position2D pos) { if (_hiddenCount < kMaxHidden) _hidden[_hiddenCount++] = pos; }
  /// Pixel theme: swap in the eyes-closed frame at random intervals (seeded per board)
  void setBlink(bool enabled, unsigned seed) { _blinkEnabled = enabled; _blinkSeed = seed; }

  /// True when the current player may still move from this board (drawn with an accent border)
  virtual void setMoveable(bool moveable) { _moveable = moveable; }
  virtual bool isMoveable() const { return _moveable; }
protected:
  bool _moveable = false;
};

class BoardView2D : public BoardView {
private:
  ChessView* _supervisor = nullptr;
  Texture2D* _boardTexture = nullptr;
  Rectangle _area = {0, 0, 0, 0};
  Camera2D* _camera = nullptr;
  std::shared_ptr<Chess::Board> _board = nullptr; // Board reference for timeline arrows

  bool _isMouseOver = false; // Whether the mouse is over the board

  /// World-space thickness that appears as `px` screen pixels at the current zoom
  float worldThickness(float px) const;

  /// Engine file/row <-> on-screen column/row (0 = left/top); columns are the files (a on the left), rows are flipped (White at the bottom)
  int colToScreen(int x) const;
  int screenToCol(int screenCol) const;
  int rowToScreen(int y) const;
  int screenToRow(int screenRow) const;
  /// Screen-space (world) rectangle of an engine square
  Rectangle squareRect(Chess::Position2D pos) const;
  Chess::Position2D worldToPosition(Vector2 world) const;
  Chess::Position2D mouseToPosition() const;

public:
  BoardView2D() = default;
  ~BoardView2D() = default;

  void render() const override;
  void render_pieces() const override;
  void render_highlightPiece(Chess::Position2D piecePosition) const override;
  void render_highlightBoundaries() const override;
  void render_legalTarget(Chess::Position2D position, float scale) const override;
  void render_hoverSquare(Chess::Position2D position, float alpha = 1.0f) const override;
  void render_liftedPiece(Chess::Position2D position, float lift) const override;
  Rectangle squareWorldRect(Chess::Position2D position) const override { return squareRect(position); }
  const std::string* pieceNameAt(Chess::Position2D position) const override;

  void setPiecePositions(const std::vector<std::pair<Chess::Position2D, std::string>>& piecePositions) override {
    _piecePositions = piecePositions;
  }

  bool is3D() const override { return false; } // This is a 2D view

  bool isMouseClickedOnBoard() const override;
  bool isMouseOverBoard() const override;

  Chess::Position2D getMouseOverPosition() const override;
  Chess::Position2D getMouseClickedPosition() const override;

  void setBoardTexture(Texture2D* texture) override { _boardTexture = texture; }

  void setRenderArea(Rectangle area) override { _area = area; }
  Rectangle getArea() const override { return _area; }

  void setSupervisor(ChessView* supervisor) override;

  void setCamera2D(Camera2D* camera) override { _camera = camera; }
  void setCamera3D(Camera3D* camera) override {  }

  // Timeline arrow support methods implementation
  void setBoard(std::shared_ptr<Chess::Board> board) override { _board = board; }
  std::shared_ptr<Chess::Board> getBoard() const override { return _board; }

  Vector2 getBoardCenter() const override {
    return Vector2{_area.x + _area.width * 0.5f, _area.y + _area.height * 0.5f};
  }

  float getBoardSize() const override {
    return fminf(_area.width, _area.height);
  }
};
