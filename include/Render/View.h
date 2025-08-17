#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"


const extern int BOARD_SIZE; // Assuming BOARD_SIZE is defined somewhere in the project
const extern int HORIZONTAL_SPACING; // Assuming HORIZONTAL_SPACING is defined somewhere in the project
const extern int VERTICAL_SPACING; // Assuming VERTICAL_SPACING is defined somewhere in the project

struct TransitionComponent {
  bool isActive;  // Is animation running?
  float duration; // Duration of the transition in seconds
  float elapsedTime; // Time elapsed since the start of the transition
  std::function<void()> onStart;  // Callback when animation starts
  std::function<void()> onComplete; // Callback when animation completes
  std::string name; // Name of the transition for identification
};



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


class ChessView {
public:
  virtual ~ChessView() = default;

  // workflow : update -> handleInput -> render
  virtual void update(float deltaTime) = 0;
  virtual void handleInput() = 0;
  virtual void render() const = 0;

  /// @brief  get information from the view, this is used to get information about the board
  /// used to update selected board, selected position, etc.
  virtual void getInformation() = 0;
  // Callbacks for Controller to handle inputs
  virtual void setMousePositionCallback(std::function<void(Chess::Position2D pos)> callback) = 0;
  virtual void setMouseClickCallback(std::function<void(Vector2 pos)> callback) = 0;
  // virtual void setKeyPressCallback(std::function<void(int key)> callback) = 0;
  // virtual void setBoardSelectCallback(std::function<void(std::shared_ptr<Chess::Board> board)> callback) = 0;
  // virtual void setPositionHighlightCallback(std::function<void(Chess::Position2D pos)> callback) = 0;
};

/// This is the view class for rendering a single board

// this class convert model data to view data


class GameWorld : public ChessView {
private:
  std::function<void(Chess::Position2D pos)> _onMousePositionCallback;
  std::function<void(Vector2 pos)> _onMouseClickCallback;
  std::function<void(int key)> _onKeyPressCallback;
  std::function<void(std::shared_ptr<Chess::Board> board)> _onBoardSelectCallback;
  std::function<void(Chess::Position2D pos)> _onPositionHighlightCallback;

// inherited methods
public:
  virtual void handleInput() override;
  virtual void update(float deltaTime) override;
  virtual void render() const override;
  virtual void getInformation() override;
  // Callbacks for Controller to handle inputs
  virtual void setMousePositionCallback(std::function<void(Chess::Position2D pos)> callback) override { _onMousePositionCallback = callback; }; // Not applicable for GameWorld
  virtual void setMouseClickCallback(std::function<void(Vector2 pos)> callback) override { _onMouseClickCallback = callback; };
  // virtual void setOnKeyPressCallback(std::function<void(int key)> callback) override { _onKeyPressCallback = callback; };
  // virtual void setOnBoardSelectCallback(std::function<void(std::shared_ptr<Chess::Board> board)> callback) override { _onBoardSelectCallback = callback; };
  // virtual void setOnPositionHighlightCallback(std::function<void(Chess::Position2D pos)> callback) override { _onPositionHighlightCallback = callback; };

private:
  std::shared_ptr<BoardView> _selectedBoardView = nullptr; // Currently selected board view
  Chess::Position2D _selectedPosition = Chess::Position2D(-1, -1); // Currently selected position on the board
// Own attributes
private: 
  Vector3 _worldSize;
  std::vector<std::shared_ptr<BoardView>> _boardViews; // List of board views
  std::shared_ptr<BoardView> _selectedBoardView = nullptr; // Currently selected board view
  Camera2D _camera2D;
  Camera3D _camera3D;
  bool _use3DRendering = false;
// Own methods
public:
  GameWorld(Vector3 _worldSize);
  ~GameWorld() = default;

  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }

  void addBoardView(std::shared_ptr<BoardView> boardView);
  void removeBoardView(std::shared_ptr<BoardView> boardView);
  void clearBoardViews();

private:
  void ClampCameraToBounds();
  void setZoom(float zoom);
  void setCameraTarget(Vector2 target); 
  void moveCamera(Vector2 delta); 
  void updateCamera(float deltaTime);

// transition methods
private:
  TransitionComponent _tryBoardTransition; // Transition for trying to select a board
public:
  void startTryBoardTransition
};
