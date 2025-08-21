#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"

class BoardView;

// Camera state management
enum class CameraState {
    AUTO_CENTERING,     // Camera automatically centers on board views
    AUTO_ZOOMING,       // Camera automatically adjusts zoom level
    USER_CONTROLLED,    // User is manually controlling camera
    TRANSITIONING       // Camera is smoothly transitioning to target
};

// Camera controller class to encapsulate all camera-related functionality
class CameraController {
private:
  Camera2D _camera2D;
  Camera3D _camera3D;
  Vector3 _worldSize;
  bool _use3DRendering = false;
  
  // Camera state tracking
  CameraState _cameraState = CameraState::AUTO_CENTERING;
  Vector2 _targetCameraPosition;
  Vector2 _autoCenterPosition;
  float _cameraTransitionSpeed = 2.0f;
  float _userControlTimeout = 2.0f; // Time before returning to auto-centering
  float _timeSinceUserInput = 0.0f;
  float _maxDistanceFromCenter = 500.0f; // Max distance before auto-centering kicks in

  // Auto-zoom properties
  float _targetZoom = 1.0f; // Target zoom level for auto-zoom
  float _minAutoZoom = 0.5f; // Minimum auto-zoom level (increased from 0.3f)
  float _maxAutoZoom = 1.5f; // Maximum auto-zoom level (decreased from 2.0f)
  float _zoomTransitionSpeed = 1.5f; // Speed of zoom transitions
  bool _autoZoomEnabled = true; // Whether auto-zoom is enabled

public:
  CameraController(Vector3 worldSize);
  
  // Core camera update methods
  void update(float deltaTime, const std::vector<std::shared_ptr<BoardView>>& boardViews);
  void handleUserInput();
  
  // Camera manipulation methods
  void clampToBounds();
  void setZoom(float zoom);
  void setCameraTarget(Vector2 target);
  void moveCamera(Vector2 delta);
  
  // Camera state methods
  void calculateAutoCenterPosition(const std::vector<std::shared_ptr<BoardView>>& boardViews);
  void calculateOptimalZoom(const std::vector<std::shared_ptr<BoardView>>& boardViews);
  void updateCameraState(float deltaTime);
  void smoothTransitionToTarget(float deltaTime);
  void smoothZoomToTarget(float deltaTime);
  
  // Getters and setters
  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }
  bool isUsing3DRendering() const { return _use3DRendering; }
  void setUsing3DRendering(bool use3D) { _use3DRendering = use3D; }
  
  CameraState getCameraState() const { return _cameraState; }
  void setCameraState(CameraState state) { _cameraState = state; }
  void setCameraTransitionSpeed(float speed) { _cameraTransitionSpeed = speed; }
  void setUserControlTimeout(float timeout) { _userControlTimeout = timeout; }
  void setMaxDistanceFromCenter(float distance) { _maxDistanceFromCenter = distance; }
  Vector2 getAutoCenterPosition() const { return _autoCenterPosition; }
  
  // Focus camera on newest board with appropriate zoom
  void focusOnNewestBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews);
  void calculateOptimalZoomForNewestBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews, std::shared_ptr<BoardView> newestBoard);
  
  // Auto-zoom control methods
  void setAutoZoomEnabled(bool enabled) { _autoZoomEnabled = enabled; }
  bool isAutoZoomEnabled() const { return _autoZoomEnabled; }
  void toggleAutoZoom() { _autoZoomEnabled = !_autoZoomEnabled; }
  void setZoomTransitionSpeed(float speed) { _zoomTransitionSpeed = speed; }
  void setAutoZoomRange(float minZoom, float maxZoom) { 
    _minAutoZoom = minZoom; 
    _maxAutoZoom = maxZoom; 
  }
  float getTargetZoom() const { return _targetZoom; }
  
  // UI debug info
  void renderDebugInfo() const;
};


struct TransitionComponent {
  bool isActive;  // Is animation running?
  float duration; // Duration of the transition in seconds
  float elapsedTime; // Time elapsed since the start of the transition
  std::function<void()> onStart;  // Callback when animation starts
  std::function<void()> onComplete; // Callback when animation completes
  std::string name; // Name of the transition for identification  

  std::shared_ptr<BoardView> fromBoardView; // Board view where the move starts
  Chess::Position2D fromPosition; // Position on the fromBoardView where the move starts
  std::shared_ptr<BoardView> toBoardView; // Board view where the move ends
  Chess::Position2D toPosition; // Position on the toBoardView where the move ends

  TransitionComponent()
      : isActive(false), duration(0.0f), elapsedTime(0.0f),
        onStart(nullptr), onComplete(nullptr), name(""),
        fromBoardView(nullptr), fromPosition{-1, -1},
        toBoardView(nullptr), toPosition{-1, -1} {}

  virtual void update(float deltaTime) {
    if (!isActive) return;

    elapsedTime += deltaTime;
    if (elapsedTime >= duration) {
      isActive = false;
      if (onComplete) onComplete();
    }
  }
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
  virtual void render() const;

private:
  std::vector<std::shared_ptr<BoardView>> _highlightedBoards;
  std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> _highlightedPositions;
public:
  virtual void handleMouseSelection(); 
  virtual void render_highlightBoard() const;
  virtual void render_highlightedPositions() const;
  virtual void render_boardViews() const; 
public:
  virtual void update_highlightedBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews);
  virtual void update_highlightedPositions(const std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>>& positions);

private:
  std::vector<TransitionComponent> _transitions;  // List of transitions
public:
  /// @brief Start a transition for adding a new board view
  /// @param fromBoardView The board view to transition from
  virtual void startAddBoardViewTransition(std::shared_ptr<BoardView> fromBoardView) {};

  /// @brief Start a transition for moving a piece from one board view to another
  /// @param fromBoardView The board view to move from
  /// @param fromPosition The position on the fromBoardView where the move starts
  /// @param toBoardView The board view to move to
  /// @param toPosition The position on the toBoardView where the move ends
  /// @param duration The duration of the transition in seconds
  /// @param onComplete callback to update model and view after the transition completes
  /// @note This method will be called by the controller to start the transition
  virtual void startMoveTransition(
      std::shared_ptr<BoardView> fromBoardView,
      Chess::Position2D fromPosition,
      std::shared_ptr<BoardView> toBoardView,
      Chess::Position2D toPosition,
      float duration,
      std::function<void()> onComplete = nullptr
  );
private:
  std::vector<std::function<void()>> updateQueue; // Queue for update callbacks
public:



private:
  std::vector<std::shared_ptr<BoardView>> _boardViews; // List of board views
  std::unique_ptr<CameraController> _cameraController; // Camera management

public:
  // virtual void update
  virtual void clearBoardViews();
  virtual void addBoardView(std::shared_ptr<BoardView> boardView);
  virtual void removeBoardView(std::shared_ptr<BoardView> boardView);
  virtual std::vector<std::shared_ptr<BoardView>> getBoardViews() const;

public:
  // Camera control methods (delegate to CameraController)
  void setCameraState(CameraState state) { _cameraController->setCameraState(state); }
  CameraState getCameraState() const { return _cameraController->getCameraState(); }
  void setCameraTransitionSpeed(float speed) { _cameraController->setCameraTransitionSpeed(speed); }
  void setUserControlTimeout(float timeout) { _cameraController->setUserControlTimeout(timeout); }
  void setMaxDistanceFromCenter(float distance) { _cameraController->setMaxDistanceFromCenter(distance); }
  Vector2 getAutoCenterPosition() const { return _cameraController->getAutoCenterPosition(); }
  
  // Focus camera on newest board
  void focusOnNewestBoard() { _cameraController->focusOnNewestBoard(_boardViews); }

  // Auto-zoom control methods (delegate to CameraController)
  void setAutoZoomEnabled(bool enabled) { _cameraController->setAutoZoomEnabled(enabled); }
  bool isAutoZoomEnabled() const { return _cameraController->isAutoZoomEnabled(); }
  void setZoomTransitionSpeed(float speed) { _cameraController->setZoomTransitionSpeed(speed); }
  void setAutoZoomRange(float minZoom, float maxZoom) { 
    _cameraController->setAutoZoomRange(minZoom, maxZoom); 
  }
  float getTargetZoom() const { return _cameraController->getTargetZoom(); }

public:
  Camera2D* getCamera2D() { return _cameraController->getCamera2D(); }
  Camera3D* getCamera3D() { return _cameraController->getCamera3D(); }

  ~ChessView() = default;
};
