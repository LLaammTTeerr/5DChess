#pragma once
#include <raylib.h>
#include <iostream>
#include "chess.h"
#include "Render/RenderUtilis.h"
#include "CameraController.h"
#include "Render/BoardView.h"


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


// Timeline arrow structures
struct TimelineArrow {
  Vector2 startPos;
  Vector2 endPos;
  Color color;
  float thickness;
  bool isAnimated;
  float animationOffset; // For animated arrows
  std::string type; // "progression", "branch", "connection"
  
  TimelineArrow() : startPos{0, 0}, endPos{0, 0}, color(BLUE), 
                   thickness(3.0f), isAnimated(true), animationOffset(0.0f), type("progression") {}
};

struct ArrowAnimationState {
  float animationTime;
  float dashOffset;
  float pulsePhase;
  
  ArrowAnimationState() : animationTime(0.0f), dashOffset(0.0f), pulsePhase(0.0f) {}
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
  // Focus camera on newest board
  void focusOnNewestBoard() { _cameraController->focusOnNewestBoard(_boardViews); }
  
  // Adaptive zoom for board selection
  void focusOnBoardWithAdaptiveZoom(std::shared_ptr<BoardView> targetBoard) { 
    _cameraController->focusOnBoardWithAdaptiveZoom(_boardViews, targetBoard); 
  }


public:
  Camera2D* getCamera2D() { return _cameraController->getCamera2D(); }
  Camera3D* getCamera3D() { return _cameraController->getCamera3D(); }

  // Timeline arrow rendering system
  std::vector<TimelineArrow> _timelineArrows;
  ArrowAnimationState _arrowAnimationState;
  std::shared_ptr<Chess::IGame> _gameRef; // Reference to game for timeline data

public:
  /// @brief Set the game reference for timeline data
  virtual void setGameReference(std::shared_ptr<Chess::IGame> game) { _gameRef = game; }

  /// @brief Update timeline arrows based on current game state
  virtual void updateTimelineArrows();

  /// @brief Render timeline arrows behind the boards
  virtual void renderTimelineArrows() const;

private:
  /// @brief Generate arrows for timeline progression
  virtual void generateProgressionArrows();
  
  /// @brief Generate arrows for timeline branching
  virtual void generateBranchingArrows();
  
  /// @brief Calculate arrow position between two board views
  virtual Vector2 calculateArrowPosition(std::shared_ptr<BoardView> boardView, bool isStart) const;
  
  /// @brief Draw a curved arrow between two points
  virtual void drawCurvedArrow(Vector2 start, Vector2 end, Color color, float thickness, float animationOffset) const;
  
  /// @brief Draw an animated dashed line
  virtual void drawAnimatedDashedLine(Vector2 start, Vector2 end, Color color, float thickness, float dashOffset) const;

public:
  ~ChessView() = default;
};
