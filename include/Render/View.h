#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"

class BoardView;

const extern float BOARD_WORLD_SIZE; // Assuming BOARD_SIZE is defined somewhere in the project
const extern float HORIZONTAL_SPACING; // Assuming HORIZONTAL_SPACING is defined somewhere in the project
const extern float VERTICAL_SPACING; // Assuming VERTICAL_SPACING is defined somewhere in the project

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
  std::function<void(std::pair<std::shared_ptr<BoardView>, Chess::Position2D>)> _onSelectedPositionCallback;
public:
  virtual void setSelectedPositionCallback(std::function<void(std::pair<std::shared_ptr<BoardView>, Chess::Position2D>)> callback) { _onSelectedPositionCallback = callback; };
public:
  virtual void update(float deltaTime);
  virtual void handleInput();
  virtual void render(std::vector<std::shared_ptr<Chess::Board>> boards) const;

private:
  std::vector<std::shared_ptr<BoardView>> _highlightedBoards;

protected:
  virtual void handleMouseSelection(); 
  virtual void render_highlightBoard() const;
public:
  virtual void update_highlightedBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews);


private:
  std::vector<TransitionComponent> _transitions;  // List of transitions
public:

private:
  std::vector<std::function<void()>> updateQueue; // Queue for update callbacks
public:
  // virtual void queueUpdateInvalidBoardSelection();
  // virtual void queueUpdateMoveState(const RenderMoveState& rmoveState);

private:
  std::vector<std::shared_ptr<BoardView>> _boardViews; // List of board views


public:
  // virtual void update
  virtual void clearBoardViews();
  virtual void addBoardView(std::shared_ptr<BoardView> boardView);
  virtual void removeBoardView(std::shared_ptr<BoardView> boardView);
  virtual std::vector<std::shared_ptr<BoardView>> getBoardViews() const;


private:
  Camera2D _camera2D;
  Camera3D _camera3D;
  void ClampCameraToBounds();
  void setZoom(float zoom);
  void setCameraTarget(Vector2 target); 
  void moveCamera(Vector2 delta); 
  void updateCamera(float deltaTime); // set camera target based on active board view
  bool _use3DRendering = false; 

public:
  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }
  ~ChessView() = default;
  
};
