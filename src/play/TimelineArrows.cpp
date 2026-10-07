#include "play/TimelineArrows.h"
#include <algorithm>
#include <cmath>
#include "play/BoardRenderer.h"
#include "play/Paths.h"

namespace play {

std::vector<Arrow> timelineArrows(const Chess::IGame& game) {
  std::vector<Arrow> arrows;
  const auto timeLines = game.getTimeLines();

  // Threads: between consecutive boards of a timeline
  for (const auto& timeLine : timeLines) {
    const auto boards = timeLine->getBoards();
    const bool inactive = !game.isTimeLineActive(timeLine->ID());
    for (size_t i = 0; i + 1 < boards.size(); ++i) {
      const Chess::Board& a = *boards[i];
      const Chess::Board& b = *boards[i + 1];
      Arrow arrow;
      arrow.from = BoardLayout::boardRect(a.timeLineId(), a.halfTurnNumber());
      arrow.to = BoardLayout::boardRect(b.timeLineId(), b.halfTurnNumber());
      arrow.timeline = b.timeLineId();
      arrow.halfTurn = b.halfTurnNumber();
      arrow.inactive = inactive;
      arrows.push_back(arrow);
    }
  }

  // Branches: from the board a timeline was forked from to its first board
  for (const auto& timeLine : timeLines) {
    if (!timeLine->hasParent() || timeLine->getBoards().empty()) continue;
    if (!game.boardExists(timeLine->parentId(), timeLine->forkAt())) continue;
    const Chess::Board& parent = game.board(timeLine->parentId(), timeLine->forkAt());
    const Chess::Board& first = *timeLine->getBoards().front();
    Arrow arrow;
    arrow.from = BoardLayout::boardRect(parent.timeLineId(), parent.halfTurnNumber());
    arrow.to = BoardLayout::boardRect(first.timeLineId(), first.halfTurnNumber());
    arrow.timeline = first.timeLineId();
    arrow.halfTurn = first.halfTurnNumber();
    arrow.branch = true;
    arrow.byWhite = timeLine->ID() > 0;
    arrow.inactive = !game.isTimeLineActive(timeLine->ID());
    arrows.push_back(arrow);
  }
  return arrows;
}

void TimelineArrows::update(float dt) {
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
    // From the right edge of the earlier card to the left edge of the later one, at the height of the boards' middle
    Line line;
    line.start = {arrow.from.x + arrow.from.w + BoardLayout::kCardPad, arrow.from.centerY()};
    line.end = {arrow.to.x - BoardLayout::kCardPad, arrow.to.centerY()};
    line.byWhite = arrow.byWhite;
    line.inactive = arrow.inactive;
    line.key = {arrow.branch, arrow.timeline, arrow.halfTurn};
    _lines.push_back(line);
  }

  // Links that are new since the last call start drawing in (never on the first call)
  _scratch.clear();
  for (const auto& line : _lines) _scratch.push_back(line.key);
  std::sort(_scratch.begin(), _scratch.end());
  if (_seeded) {
    for (const auto& k : _scratch) {
      if (std::binary_search(_known.begin(), _known.end(), k)) continue;
      Anim anim{k, {}};
      // Branches draw slowly, together with the new timeline's lane unfolding (100 ms earlier than the boards used to); threads quickly
      anim.t.start(0.0f, 1.0f, k.branch ? UI::Motion::slow : UI::Motion::base, UI::Motion::easeOutCubic,
                   k.branch ? 0.0f : 0.05f);
      if (!anim.t.done()) _active.push_back(anim);
    }
  }
  _known.swap(_scratch);
  _seeded = true;
}

namespace {

void drawNode(Vector2 at, Color color, bool filled, const BoardStyle& style, float px, float alpha) {
  switch (style.connector) {
    case BoardStyle::Connector::Curve:
      DrawCircleV(at, 6.0f * px, fade(style.cardWhite, alpha));
      DrawRing(at, 4.5f * px, 7.5f * px, 0, 360, 24, fade(color, alpha));
      break;
    case BoardStyle::Connector::Luminous:
      DrawCircleV(at, 10.0f * px, fade(color, 0.28f * alpha));
      DrawCircleV(at, 5.5f * px, fade(WHITE, alpha));
      DrawRing(at, 4.0f * px, 7.0f * px, 0, 360, 24, fade(color, alpha));
      break;
    case BoardStyle::Connector::Elbow:
      DrawCircleV(at, 6.0f * px, fade(filled ? style.ink : WHITE, alpha));
      DrawRing(at, 4.0f * px, 7.0f * px, 0, 360, 24, fade(style.ink, alpha));
      break;
  }
}

} // namespace

void TimelineArrows::draw(const BoardStyle& style, float zoom) const {
  const float px = 1.0f / std::max(zoom, 0.05f); // one screen pixel in world units
  const BoardStyle::Connector kind = style.connector;
  path::Poly poly;
  for (const auto& line : _lines) {
    const float progress = progressOf(line.key);
    if (progress <= 0.0f) continue;
    const float alpha = line.inactive ? 0.45f : 1.0f;
    const Vector2 a = line.start, b = line.end;

    if (!line.key.branch) {
      // The thread of a timeline through its boards
      poly.n = 0;
      poly.add(a);
      poly.add(b);
      switch (kind) {
        case BoardStyle::Connector::Curve:
          path::stroke(poly, progress, fade(style.thread, alpha), 2.5f * px);
          break;
        case BoardStyle::Connector::Luminous:
          if (line.inactive) {
            path::stroke(poly, progress, fade(style.thread, 0.9f), 2.4f * px, 3.0f * px, 5.0f * px);
          } else {
            path::stroke(poly, progress, fade(style.thread, 0.22f), 7.0f * px);
            path::stroke(poly, progress, style.thread, 2.0f * px);
          }
          break;
        case BoardStyle::Connector::Elbow:
          path::stroke(poly, progress, fade(style.thread, alpha), 1.5f * px);
          break;
      }
      continue;
    }

    // A branch: a curve (or an elbow) from the parent board to the first board of the new timeline
    const Color color = line.byWhite ? style.whiteBranch : style.blackBranch;
    switch (kind) {
      case BoardStyle::Connector::Curve: {
        const float k = (b.x - a.x) * 0.5f;
        path::bezier(poly, a, {a.x + k, a.y}, {b.x - k, b.y}, b);
        path::stroke(poly, progress, fade(color, alpha), 4.0f * px);
        break;
      }
      case BoardStyle::Connector::Luminous: {
        const float k = (b.x - a.x) * 0.55f;
        path::bezier(poly, a, {a.x + k, a.y}, {b.x - k, b.y}, b);
        path::stroke(poly, progress, fade(color, 0.30f * alpha), 11.0f * px);
        path::stroke(poly, progress, fade(color, alpha), 3.2f * px);
        path::stroke(poly, progress, fade(WHITE, 0.75f * alpha), 1.0f * px);
        break;
      }
      case BoardStyle::Connector::Elbow:
        path::elbow(poly, a, b, (a.x + b.x) / 2.0f, 12.0f);
        path::stroke(poly, progress, fade(style.ink, alpha), 2.0f * px);
        break;
    }
    drawNode(a, color, !line.byWhite, style, px, alpha);
    if (progress > 0.95f) drawNode(b, color, !line.byWhite, style, px, alpha);
  }
}

} // namespace play
