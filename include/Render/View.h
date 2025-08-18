#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"

class BoardView;

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


class ChessView {
private:
  Vector3 _worldSize;
public:
  ChessView(Vector3 _worldSize);

private:
  std::function<void(std::shared_ptr<BoardView>)> _onMouseBoardClickCallback;
  std::function<void(Chess::Position2D)> _onPositionClickCallback;
public:
  virtual void setMouseBoardClickCallback(std::function<void(std::shared_ptr<BoardView>)> callback) { _onMouseBoardClickCallback = callback; };
  virtual void setPositionClickCallback(std::function<void(Chess::Position2D)> callback) { _onPositionClickCallback = callback; };

public:
  virtual void update(float deltaTime);
  virtual void handleInput();
  virtual void render() const;
  

private:
  std::vector<TransitionComponent> _transitions;  // List of transitions
public:

private:
  std::vector<std::function<void()>> updateQueue; // Queue for update callbacks
public:
  virtual void queueUpdateInvalidBoardSelection();
  virtual void queueUpdateMoveState(const RenderMoveState& rmoveState);

private:
  std::vector<std::shared_ptr<BoardView>> _boardViews; // List of board views
  std::shared_ptr<BoardView> _selectedBoardView = nullptr; // Currently selected board view
  Chess::Position2D _selectedPosition = Chess::Position2D(-1, -1); // Currently selected position on the board
  bool _isSelectedBoardViewInvalid = false;


public:
  virtual void addBoardView(std::shared_ptr<BoardView> boardView);
  virtual void removeBoardView(std::shared_ptr<BoardView> boardView);
  virtual std::vector<std::shared_ptr<BoardView>> getBoardViews() const;
  virtual void updateSelectedBoardView();
  virtual void updateSelectedPosition();


private:
  Camera2D _camera2D;
  Camera3D _camera3D;
  void ClampCameraToBounds();
  void setZoom(float zoom);
  void setCameraTarget(Vector2 target); 
  void moveCamera(Vector2 delta); 
  void updateCamera(float deltaTime);
  bool _use3DRendering = false;

public:
  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }
  ~ChessView() = default;
  
};
