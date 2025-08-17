#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"


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
  std::function<void(std::shared_ptr<BoardView>)> _onMouseBoardClickCallback;

public:
  virtual void handleInput();
  virtual void update(float deltaTime);
  virtual void render() const;

  virtual std::vector<std::shared_ptr<BoardView>> getBoardViews() const;

  virtual void setMouseBoardClickCallback(std::function<void(std::shared_ptr<BoardView>)> callback) { _onMouseBoardClickCallback = callback; };
  
  virtual void queueUpdateInvalidBoardSelection(std::shared_ptr<BoardView> boardView);

private:
  std::vector<TransitionComponent> _transitions;  // List of transitions
  std::vector<std::function<void()>> updateQueue; // Queue for update callbacks

private:
  std::shared_ptr<BoardView> _selectedBoardView = nullptr; // Currently selected board view
  bool _isSelectedBoardViewInvalid = false;
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
  ChessView(Vector3 _worldSize);
  ~ChessView() = default;

  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }

  void addBoardView(std::shared_ptr<BoardView> boardView);
  void removeBoardView(std::shared_ptr<BoardView> boardView);

private:
  void ClampCameraToBounds();
  void setZoom(float zoom);
  void setCameraTarget(Vector2 target); 
  void moveCamera(Vector2 delta); 
  void updateCamera(float deltaTime);
};
