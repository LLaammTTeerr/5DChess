#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"

///@brief This is the view class for rendering a single board
/// This class converts model data to view data
/// This class will encapsulate both model and view data



class ChessView;

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
    virtual void render_highlightedPositions(std::vector<Chess::Position2D> positions) const = 0;

    // virtual void setBoard(std::shared_ptr<Chess::Board> board) = 0;
    // virtual std::shared_ptr<Chess::Board> getBoard() const = 0;
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

protected:
  /// Piece position and piece name
  std::vector<std::pair<Chess::Position2D, std::string>> _piecePositions;
public:
  virtual void render_pieces() const = 0;
};
  
class BoardView2D : public BoardView {
private:
  ChessView* _supervisor = nullptr;
  Texture2D* _boardTexture = nullptr;
  Rectangle _area = {0, 0, 0, 0};
  Camera2D* _camera = nullptr;

  bool _isMouseOver = false; // Whether the mouse is over the board
  
public:
  BoardView2D() = default;
  ~BoardView2D() = default;

  // void render() const override {};
  void render() const override;
  void render_pieces() const override;
  void render_highlightBoundaries() const override;
  void render_highlightedPositions(std::vector<Chess::Position2D> positions) const override;
  
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
};

// class BoardView3D : public BoardView {
// private:
//     std::shared_ptr<Chess::Board> _board;
//     Texture2D* _boardTexture;
//     Vector3 _position; // 3D position (e.g., timeline * spacing, 0, turn * spacing)
//     Camera3D* _camera;
//     bool _isActive = false;
//     bool _isMouseOver = false;
//     std::vector<Chess::Position2D> _highlightedPositions;
//     float _boardSize = 8.0f; // Size of board in 3D units
//     Model _boardModel; // For GPU-accelerated rendering
// private:
//     ChessView* _supervisor = nullptr;
// public:
//     BoardView3D(std::shared_ptr<Chess::Board> board, Texture2D* texture, Vector3 position, Camera3D* camera);
//     ~BoardView3D();

//     void update(float deltaTime) override {};
//     void handleInput() override {};
//     void render() const override {};

//     void setBoard(std::shared_ptr<Chess::Board> board) override;
//     std::shared_ptr<Chess::Board> getBoard() const override { return _board; }
//     void setBoardTexture(Texture2D* texture) override;
//     void updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) override;
//     bool is3D() const override;

//     void setPosition(Vector3 position);

//     bool isMouseClickedOnBoard() const override { return 0; };

// private:
//     void updateMouseOverBoard();
    

// public:
//     void setSupervisor(ChessView* supervisor) override;
// };
