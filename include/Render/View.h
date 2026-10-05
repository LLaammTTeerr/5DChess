#pragma once
#include <raylib.h>
#include <iostream>
#include <string>
#include "chess.h"
#include "Render/RenderUtilis.h"
#include "CameraController.h"
#include "Render/BoardView.h"
#include "Render/TimelineArrowRenderer.h"
#include "Render/PresentLineRenderer.h"


/// @brief Screen-space HUD content supplied by the controller
struct HudData {
  bool whiteToMove = true;
  int fullTurn = 1;        // 1-based full-turn number shown to the player
  int timelineCount = 1;
  std::string hint;        // e.g. "Select a board"
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
  /// @param pointerBlocked true when the mouse is over UI drawn on top (no board selection/hover)
  virtual void handleInput(bool pointerBlocked = false);
  void clearHover() { _hoverPosition = {nullptr, {-1, -1}}; }
  virtual void render() const;

private:
  std::pair<std::shared_ptr<BoardView>, Chess::Position2D> _fromPosition = {nullptr, {-1, -1}}; // use to Highlight piece at fromPosition
  std::vector<std::shared_ptr<BoardView>> _highlightedBoards;
  std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> _highlightedPositions;
  std::pair<std::shared_ptr<BoardView>, Chess::Position2D> _hoverPosition = {nullptr, {-1, -1}};
  HudData _hud;
public:
  virtual void render_hoverSquare() const;
  virtual void updateHud(const HudData& hud) { _hud = hud; }
  /// @brief Top-centre status pill and bottom controls bar (screen space, unaffected by the camera)
  virtual void renderHud() const;
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
  std::vector<std::shared_ptr<BoardView>> _boardViews; // List of board views
  std::unique_ptr<CameraController> _cameraController; // Camera management
  std::unique_ptr<TimelineArrowRenderer> _arrowRenderer; // Timeline arrow rendering
  std::unique_ptr<PresentLineRenderer> _presentLineRenderer; // Present line rendering

public:
  virtual void clearBoardViews();
  virtual void addBoardView(std::shared_ptr<BoardView> boardView);

public:  
  // Focus camera on newest board
  void focusOnNewestBoard(std::shared_ptr<BoardView> newestBoardView) { 
    _cameraController->focusOnNewestBoard(_boardViews, newestBoardView); 
  }

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

  // Present line rendering system - delegated to PresentLineRenderer
public:
  /// @brief Update present line from Controller-provided data
  virtual void updatePresentLine(const PresentLineData& lineData);

  /// @brief Render present line behind all boards and arrows
  virtual void renderPresentLine() const;
  /// @param winnerText e.g. "White wins!" - drawn on a scrim with a centred card
  virtual void renderEndGameScreen(std::string winnerText) const;
public:
  ~ChessView() = default;
};
