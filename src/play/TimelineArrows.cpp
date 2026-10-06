#include "play/TimelineArrows.h"
#include <algorithm>
#include <cmath>
#include "Render/UITheme.h"

namespace play {

std::vector<Arrow> timelineArrows(const Chess::IGame& game) {
  std::vector<Arrow> arrows;
  const auto timeLines = game.getTimeLines();

  // Progression: between consecutive boards of a timeline
  for (const auto& timeLine : timeLines) {
    const auto boards = timeLine->getBoards();
    for (size_t i = 0; i + 1 < boards.size(); ++i) {
      const Chess::Board& a = *boards[i];
      const Chess::Board& b = *boards[i + 1];
      arrows.push_back({BoardLayout::boardRect(a.timeLineId(), a.halfTurnNumber()),
                        BoardLayout::boardRect(b.timeLineId(), b.halfTurnNumber()), b.timeLineId(), b.halfTurnNumber(), false});
    }
  }

  // Branches: from the board a timeline was forked from to its first board
  for (const auto& timeLine : timeLines) {
    if (!timeLine->hasParent() || timeLine->getBoards().empty()) continue;
    if (!game.boardExists(timeLine->parentId(), timeLine->forkAt())) continue;
    const Chess::Board& parent = game.board(timeLine->parentId(), timeLine->forkAt());
    const Chess::Board& first = *timeLine->getBoards().front();
    arrows.push_back({BoardLayout::boardRect(parent.timeLineId(), parent.halfTurnNumber()),
                      BoardLayout::boardRect(first.timeLineId(), first.halfTurnNumber()), first.timeLineId(),
                      first.halfTurnNumber(), true});
  }
  return arrows;
}

void TimelineArrows::update(float dt) {
  _dashOffset += dt * 50.0f;
  _pulsePhase += dt * 2.0f;
  for (auto& a : _active) a.t.update(dt);
  _active.erase(std::remove_if(_active.begin(), _active.end(), [](const Anim& a) { return a.t.done(); }), _active.end());
}

float TimelineArrows::progressOf(const Key& key) const {
  for (const auto& a : _active)
    if (a.key.branch == key.branch && a.key.timeline == key.timeline && a.key.halfTurn == key.halfTurn) return a.t.progress();
  return 1.0f;
}

void TimelineArrows::set(const std::vector<Arrow>& arrows) {
  _lines.clear();
  for (const Arrow& arrow : arrows) {
    // From the right edge of the earlier board to the left edge of the later one
    const float fromSize = std::fmin(arrow.from.w, arrow.from.h), toSize = std::fmin(arrow.to.w, arrow.to.h);
    Line line;
    line.start = {arrow.from.centerX() + fromSize * 0.6f, arrow.from.centerY()};
    line.end = {arrow.to.centerX() - toSize * 0.6f, arrow.to.centerY()};
    line.color = arrow.branch ? UI::Color::branchArrow : UI::Color::timelineArrow;
    line.thickness = arrow.branch ? 5.0f : 4.0f;
    line.key = {arrow.branch, arrow.timeline, arrow.halfTurn};
    _lines.push_back(line);
  }

  // Arrows that are new since the last call start drawing in (never on the first call)
  _scratch.clear();
  for (const auto& line : _lines) _scratch.push_back(line.key);
  std::sort(_scratch.begin(), _scratch.end());
  if (_seeded) {
    for (const auto& k : _scratch) {
      if (std::binary_search(_known.begin(), _known.end(), k)) continue;
      Anim anim{k, {}};
      // Branch arrows draw slowly after the new board has started to grow; progression arrows quickly
      anim.t.start(0.0f, 1.0f, k.branch ? UI::Motion::slow : UI::Motion::base, UI::Motion::easeOutCubic,
                   k.branch ? 0.10f : 0.05f);
      if (!anim.t.done()) _active.push_back(anim);
    }
  }
  _known.swap(_scratch);
  _seeded = true;
}

void TimelineArrows::draw() const {
  for (const auto& line : _lines) {
    const float progress = progressOf(line.key);
    if (progress <= 0.0f) continue;
    if (line.key.branch) drawCurved(line, progress);
    else drawDashed(line, progress);
  }
}

void TimelineArrows::drawCurved(const Line& line, float progress) const {
  const Vector2 start = line.start, end = line.end;
  const Vector2 midPoint = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
  const Vector2 direction = {end.x - start.x, end.y - start.y};
  Vector2 perpendicular = {-direction.y, direction.x};
  const float length = sqrtf(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y);
  if (length > 0) {
    perpendicular.x /= length;
    perpendicular.y /= length;
  }
  const float curvature = 50.0f;
  const Vector2 control = {midPoint.x + perpendicular.x * curvature, midPoint.y + perpendicular.y * curvature};

  // A quadratic Bezier drawn as 20 segments, with a pulsing brightness
  const int segments = 20;
  for (int i = 0; i < segments; ++i) {
    const float t1 = (float)i / segments;
    float t2 = (float)(i + 1) / segments;
    if (t1 >= progress) break; // progressive draw: stop where the arrow has not reached yet
    if (t2 > progress) t2 = progress;

    const Vector2 p1 = {(1 - t1) * (1 - t1) * start.x + 2 * (1 - t1) * t1 * control.x + t1 * t1 * end.x,
                        (1 - t1) * (1 - t1) * start.y + 2 * (1 - t1) * t1 * control.y + t1 * t1 * end.y};
    const Vector2 p2 = {(1 - t2) * (1 - t2) * start.x + 2 * (1 - t2) * t2 * control.x + t2 * t2 * end.x,
                        (1 - t2) * (1 - t2) * start.y + 2 * (1 - t2) * t2 * control.y + t2 * t2 * end.y};
    const float pulse = 1.0f + 0.3f * sinf(_pulsePhase + t1 * 3.14159f);
    const Color animated = {(unsigned char)fminf(255.0f, line.color.r * pulse), (unsigned char)fminf(255.0f, line.color.g * pulse),
                            (unsigned char)fminf(255.0f, line.color.b * pulse), line.color.a};
    DrawLineEx(p1, p2, line.thickness, animated);
  }

  // Arrowhead at the end (grows in over the last 12 % of the draw)
  const float headScale = std::fmin(1.0f, std::fmax(0.0f, (progress - 0.88f) / 0.12f));
  Vector2 dir = {end.x - control.x, end.y - control.y};
  const float dirLength = sqrtf(dir.x * dir.x + dir.y * dir.y);
  if (dirLength > 0 && headScale > 0.0f) {
    dir.x /= dirLength;
    dir.y /= dirLength;
    const Vector2 side = {-dir.y, dir.x};
    const float size = line.thickness * 2 * headScale;
    const Vector2 p1 = {end.x - dir.x * size + side.x * size * 0.5f, end.y - dir.y * size + side.y * size * 0.5f};
    const Vector2 p2 = {end.x - dir.x * size - side.x * size * 0.5f, end.y - dir.y * size - side.y * size * 0.5f};
    DrawTriangle(end, p2, p1, line.color); // raylib needs counter-clockwise order (y down) or the triangle is culled
  }
}

void TimelineArrows::drawDashed(const Line& line, float progress) const {
  const Vector2 start = line.start, end = line.end;
  Vector2 direction = {end.x - start.x, end.y - start.y};
  float totalLength = sqrtf(direction.x * direction.x + direction.y * direction.y);
  if (totalLength == 0) return;
  direction.x /= totalLength; // unit vector from the FULL length
  direction.y /= totalLength;
  totalLength *= progress; // progressive draw: only the length shrinks, dashes keep theirs

  const float dashLength = 10.0f, gapLength = 5.0f, segmentLength = dashLength + gapLength;
  const float offset = fmodf(_dashOffset, segmentLength); // marching dashes
  for (float distance = -offset; distance < totalLength; distance += segmentLength) {
    const float segmentStart = fmaxf(0, distance);
    const float segmentEnd = fminf(totalLength, distance + dashLength);
    if (segmentStart < segmentEnd) {
      DrawLineEx({start.x + direction.x * segmentStart, start.y + direction.y * segmentStart},
                 {start.x + direction.x * segmentEnd, start.y + direction.y * segmentEnd}, line.thickness, line.color);
    }
  }

  // Arrowhead at the end once the line has arrived
  if (progress < 0.98f) return;
  const float t = line.thickness;
  const Vector2 p1 = {end.x - direction.x * t * 2 + direction.y * t, end.y - direction.y * t * 2 - direction.x * t};
  const Vector2 p2 = {end.x - direction.x * t * 2 - direction.y * t, end.y - direction.y * t * 2 + direction.x * t};
  DrawTriangle(end, p1, p2, line.color);
}

} // namespace play
