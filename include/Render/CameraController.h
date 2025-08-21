#pragma once
#include <memory>
#include <vector>
#include <functional>
#include "Render/RenderUtilis.h"
#include "chess.h"
#include "Render/BoardView.h"
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

public:
  Camera2D* getCamera2D() { return &_camera2D; }
  Camera3D* getCamera3D() { return &_camera3D; }
  void focusOnNewestBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews);
  void focusOnBoardWithAdaptiveZoom(const std::vector<std::shared_ptr<BoardView>>& boardViews, std::shared_ptr<BoardView> targetBoard);

  bool isUsing3DRendering() const { return _use3DRendering; }
  void setUsing3DRendering(bool use3D) { _use3DRendering = use3D; }

private:
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
  CameraState getCameraState() const { return _cameraState; }
  void setCameraState(CameraState state) { _cameraState = state; }
  void setCameraTransitionSpeed(float speed) { _cameraTransitionSpeed = speed; }
  void setUserControlTimeout(float timeout) { _userControlTimeout = timeout; }
  void setMaxDistanceFromCenter(float distance) { _maxDistanceFromCenter = distance; }
  Vector2 getAutoCenterPosition() const { return _autoCenterPosition; }
  
  // Focus camera on newest board with appropriate zoom
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

public:
  // UI debug info
  void renderDebugInfo() const;
};
