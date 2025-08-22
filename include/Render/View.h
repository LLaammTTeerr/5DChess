#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"
#include "CameraController.h"
#include "Render/BoardView.h"
#include "Render/TimelineArrowRenderer.h"


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
  std::function<void(std::pair<std::shared_ptr<BoardView>, Chess::Position2D>)> _onMouseOverPositionCallback;

public:
  virtual void setSelectedPositionCallback(std::function<void(std::pair<std::shared_ptr<BoardView>, Chess::Position2D>)> callback) { _onSelectedPositionCallback = callback; };
  virtual void setMouseOverPositionCallback(std::function<void(std::pair<std::shared_ptr<BoardView>, Chess::Position2D>)> callback) { _onMouseOverPositionCallback = callback; };
public:
  virtual void update(float deltaTime);
  virtual void handleInput();
  virtual void render() const;

private:
  std::pair<std::shared_ptr<BoardView>, Chess::Position2D> _fromPosition = {nullptr, {-1, -1}}; // use to Highlight piece at fromPosition
  std::vector<std::shared_ptr<BoardView>> _highlightedBoards;
  std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> _highlightedPositions;
public:
  virtual void handleMouseSelection(); 
  virtual void handleMouseOver();
  virtual void render_highlightBoard() const;
  virtual void render_highlightedPositions() const;
  virtual void render_highlightPiece(std::pair<std::shared_ptr<BoardView>, Chess::Position2D> piecePosition) const;
  virtual void render_boardViews() const; 
public:
  virtual void update_FromPosition(std::pair<std::shared_ptr<BoardView>, Chess::Position2D> fromPosition); 
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
  std::unique_ptr<TimelineArrowRenderer> _arrowRenderer; // Timeline arrow rendering

public:
  // virtual void update
  virtual void clearBoardViews();
  virtual void addBoardView(std::shared_ptr<BoardView> boardView);
  virtual void removeBoardView(std::shared_ptr<BoardView> boardView);
  virtual std::vector<std::shared_ptr<BoardView>> getBoardViews() const;

public:  
  // Focus camera on newest board
  void focusOnNewestBoard() { _cameraController->focusOnNewestBoard(_boardViews); }
  
  // Adaptive zoom for board selection
  void focusOnBoardWithAdaptiveZoom(std::shared_ptr<BoardView> targetBoard) { 
    _cameraController->focusOnBoardWithAdaptiveZoom(_boardViews, targetBoard); 
  }


public:
  Camera2D* getCamera2D() { return _cameraController->getCamera2D(); }
  Camera3D* getCamera3D() { return _cameraController->getCamera3D(); }

  // Timeline arrow rendering system - delegated to TimelineArrowRenderer
public:
  /// @brief Update timeline arrows from Controller-provided data
  virtual void updateTimelineArrows(const std::vector<TimelineArrowData>& arrowData);

  /// @brief Render timeline arrows behind the boards
  virtual void renderTimelineArrows() const;

public:
  ~ChessView() = default;
};
