#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"

///@brief This is the view class for rendering a single board
/// This class converts model data to view data
/// This class will encapsulate both model and view data



class ChessView

// interface for board view, this is the interface that converting model data to view data
// This interface is used to render a single board
// It is used to render the board and handle input events
// It is also used to highlight positions on the board
// It is used to render the board texture
// It is used to render the board pieces
class BoardView {
public:
    virtual ~BoardView() = default;
    virtual void update(float deltaTime) = 0;
    virtual void handleInput() = 0;
    virtual void render() const = 0;

    virtual void setBoard(std::shared_ptr<Chess::Board> board) = 0;
    virtual std::shared_ptr<Chess::Board> getBoard() const = 0;
    
    virtual void setBoardTexture(Texture2D* texture) = 0;
    virtual void setActive(bool active) = 0;
    virtual bool isActive() const = 0;
    virtual void updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) = 0;
    virtual bool is3D() const = 0;

    virtual bool isMouseClickedOnBoard() const = 0;
    virtual Chess::Position2D getSelectedPosition() const = 0; 
    virtual void setSupervisor(ChessView* supervisor); 
};

class BoardView2D : public BoardView {
private:
  std::shared_ptr<Chess::Board> _board;
  Texture2D* _boardTexture;
  
  // Area where the board will be rendered
  Rectangle _area; 

  // Camera for rendering
  // This can be used to zoom or pan the board view
  Camera2D* _camera;


  bool _isActive = false; // Whether the board is active
  bool _isMouseOver = false; // Whether the mouse is over the board


  std::vector<Chess::Position2D> _highlightedPositions; // Positions to highlight

private:
  ChessView* _supervisor = nullptr;

public:
  BoardView2D(std::shared_ptr<Chess::Board> board, Texture2D* texture);
  ~BoardView2D() = default;
  
  void update(float deltaTime) override;
  void handleInput() override;
  void render() const override;

  void setBoard(std::shared_ptr<Chess::Board> board) override { _board = board; }
  std::shared_ptr<Chess::Board> getBoard() const override { return _board; }

  void setBoardTexture(Texture2D* texture) override { _boardTexture = texture; }

  void setActive(bool active) override { _isActive = active; }
  bool isActive() const override { return _isActive; }

  void updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) override { _highlightedPositions = positions; }
  bool is3D() const override { return false; } // This is a 2D view

  bool isMouseClickedOnBoard() const override;

  void setRenderArea(Rectangle area) { _area = area; }
  Rectangle getArea() const { return _area; }
  void setCamera(Camera2D* camera) { _camera = camera; }


private:
  void updateMouseOverBoard(); // update helper
  void drawSelectedBoundaries() const;
  void drawMouseOverBoundaries() const;

public:
  void setSupervisor(ChessView* supervisor) override; 
};

class BoardView3D : public BoardView {
private:
    std::shared_ptr<Chess::Board> _board;
    Texture2D* _boardTexture;
    Vector3 _position; // 3D position (e.g., timeline * spacing, 0, turn * spacing)
    Camera3D* _camera;
    bool _isActive = false;
    bool _isMouseOver = false;
    std::vector<Chess::Position2D> _highlightedPositions;
    float _boardSize = 8.0f; // Size of board in 3D units
    Model _boardModel; // For GPU-accelerated rendering
private:
    ChessView* _supervisor = nullptr;
public:
    BoardView3D(std::shared_ptr<Chess::Board> board, Texture2D* texture, Vector3 position, Camera3D* camera);
    ~BoardView3D();

    void update(float deltaTime) override {};
    void handleInput() override {};
    void render() const override {};

    void setBoard(std::shared_ptr<Chess::Board> board) override;
    std::shared_ptr<Chess::Board> getBoard() const override { return _board; }
    void setBoardTexture(Texture2D* texture) override;
    void setActive(bool active) override;
    bool isActive() const override;
    void updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) override;
    bool is3D() const override;

    void setPosition(Vector3 position);

    bool isMouseClickedOnBoard() const override { return 0; };

private:
    void updateMouseOverBoard();
    

public:
    void setSupervisor(ChessView* supervisor) override;
};
