#pragma once
#include <array>
#include <optional>
#include <raylib.h>
#include "play/BoardLayout.h"
#include "play/BoardRenderer.h"
#include "play/BoardStyle.h"
#include "play/MultiverseView.h"

namespace play {

/// Everything one frame of the scene needs to know: the look, the multiverse snapshot, the camera.
struct SceneFrame {
  const BoardStyle& style;
  const MultiverseView& view;
  const Camera2D& camera;
  int dim = 8;
  Rectangle safe{};   // screen rectangle the boards are clipped to (between the HUD bars)
  Vector2 toScreen(Vector2 world) const { return GetWorldToScreen2D(world, camera); }
  float zoom() const { return camera.zoom; }
};

/// What surrounds and connects the boards, drawn in the style of the current board view: the background, lanes, the
/// present marker, the turn ruler and lane labels, threads' tails, jump arcs and the check lines. It owns the GPU
/// resources it bakes once (the soft halo, the background, the aurora), so build it after the window exists and drop it
/// before it closes (PlayScreen does both).
class BoardScene {
public:
  BoardScene();
  ~BoardScene();
  BoardScene(const BoardScene&) = delete;
  BoardScene& operator=(const BoardScene&) = delete;

  const SoftBox& soft() const { return _soft; }

  /// World rectangle covering the jump arcs, their badges and labels (nullopt: no jumps); the camera keeps it on screen.
  static std::optional<Rect> jumpBounds(const BoardStyle& style, const MultiverseView& view);

  // --- Screen space, before the boards ---
  /// Background of the view (gradient + stars, paper + dots, or flat), filling the window.
  void drawBackground(const BoardStyle& style);
  /// Lane bands / hairlines and the present marker column; clipped to y >= top.
  void drawLanes(const SceneFrame& f, float top);

  // --- World space (inside BeginMode2D) ---
  void drawTails(const SceneFrame& f) const;
  void drawJumpArcs(const SceneFrame& f) const;
  void drawJumpBadges(const SceneFrame& f) const;
  void drawChecks(const SceneFrame& f) const;

  // --- Screen space, over the boards (inside the board clip) ---
  void drawCardLabels(const SceneFrame& f, const BoardLayout& layout) const;
  void drawJumpLabels(const SceneFrame& f) const;

  // --- Screen space, over everything of the board view ---
  /// The turn ruler (T1 T2 ... with w/b ticks) in the row [y, y + height) and the present marker.
  void drawRuler(const SceneFrame& f, float y, float height) const;
  /// Left labels L0, L+1, L-1 ... with "inactive" tags; `area` is the screen rectangle the labels may use.
  void drawLaneLabels(const SceneFrame& f, Rectangle area) const;

private:
  SoftBox _soft;
  Texture2D _background{}, _aurora{};
  int _backgroundW = 0, _backgroundH = 0;
  BoardView _backgroundView = BoardView::DeepSpace;

  struct Star { float x, y, size, alpha, phase, speed; };
  std::array<Star, 280> _stars;

  void bakeBackground(const BoardStyle& style, int w, int h);
  void bakeAurora();
  void drawStars(int w, int h) const;
  void drawPresentColumn(const SceneFrame& f, float top);
};

/// "L0", "L+1", "L-2": the timeline label (ASCII minus; the UI fonts have no U+2212).
std::string timelineLabel(int id);
/// "T3w" / "T3b": the board label (turns count from 1; w = White to move there).
std::string boardLabel(int halfTurn);

} // namespace play
