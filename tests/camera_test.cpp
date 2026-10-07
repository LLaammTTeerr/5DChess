#include <doctest/doctest.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "play/BoardCamera.h"

using play::BoardCamera;
using play::BoardLayout;
using play::Rect;
using play::Vec2;
using State = BoardCamera::State;

namespace {
// A camera on the game's window: 1400x800 with the HUD margins (UITheme::Layout): the free area is x 112..1376, y 160..744
BoardCamera makeCamera() {
  BoardCamera cam;
  cam.setViewport(1400, 800);
  cam.setInsets(160.0f, 24.0f, 56.0f, 112.0f);
  return cam;
}

Rect card(int timeline, int halfTurn) { return BoardLayout::cardRect(BoardLayout::boardRect(timeline, halfTurn)); }

// The union of the cards of two boards
Rect span(int l0, int t0, int l1, int t1) {
  const Rect a = card(l0, t0), b = card(l1, t1);
  const float x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  return {x0, y0, std::max(a.x + a.w, b.x + b.w) - x0, std::max(a.y + a.h, b.y + b.h) - y0};
}

// Run the camera for a while (60 fps)
void run(BoardCamera& cam, float seconds) {
  for (float t = 0; t < seconds; t += 1.0f / 60.0f) cam.update(1.0f / 60.0f);
}

bool near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }
} // namespace

TEST_CASE("BoardCamera: the world <-> screen maps are inverse and centre the target in the free area") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  const Vec2 centre = cam.worldToScreen(cam.target());
  CHECK(near(centre.x, (112.0f + 1376.0f) / 2.0f));
  CHECK(near(centre.y, (160.0f + 744.0f) / 2.0f));
  const Vec2 w = cam.screenToWorld(cam.worldToScreen({123.0f, -45.0f}));
  CHECK(near(w.x, 123.0f, 0.01f));
  CHECK(near(w.y, -45.0f, 0.01f));
  const Rect v = cam.visibleWorld();
  CHECK(near(v.w * cam.zoom(), 1400 - 112 - 24, 0.1f));
  CHECK(near(v.h * cam.zoom(), 800 - 160 - 56, 0.1f));
}

TEST_CASE("BoardCamera: snapZoom picks the nearest fixed step, snapDown the step below") {
  CHECK(BoardCamera::snapZoom(0.3f) == 0.6f);
  CHECK(BoardCamera::snapZoom(0.65f) == 0.6f);
  CHECK(BoardCamera::snapZoom(0.75f) == 0.8f);
  CHECK(BoardCamera::snapZoom(0.95f) == 1.0f);
  CHECK(BoardCamera::snapZoom(1.2f) == 1.25f);
  CHECK(BoardCamera::snapZoom(2.0f) == 2.0f); // above the top step: kept
  CHECK(BoardCamera::snapDown(0.9f) == 0.8f);
  CHECK(BoardCamera::snapDown(1.0f) == 1.0f);
  CHECK(BoardCamera::snapDown(1.24f) == 1.0f);
  CHECK(BoardCamera::snapDown(0.2f) == 0.6f);
}

TEST_CASE("BoardCamera: the first framing jumps; Overview fits everything but never zooms past 1.25") {
  BoardCamera cam = makeCamera();
  CHECK_FALSE(cam.seeded());
  const Rect one = card(0, 0);
  cam.overview(one, one);
  CHECK(cam.seeded());
  CHECK(cam.state() == State::Overview);
  CHECK(cam.zoom() == doctest::Approx(1.25f)); // one board would fit at 1.7
  CHECK_FALSE(cam.moving());
  CHECK(cam.target().x == doctest::Approx(one.centerX()));

  const Rect eight = span(0, 0, 0, 7);
  cam.overview(eight, eight, false, true);
  const Rect v = cam.visibleWorld();
  CHECK(v.x < eight.x);
  CHECK(v.x + v.w > eight.x + eight.w);
  CHECK(cam.zoom() < 0.6f);
  CHECK(cam.zoom() > 0.4f);
}

TEST_CASE("BoardCamera: a field too wide to read falls back to the present column and its neighbours") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 30);
  const Rect present = span(0, 14, 0, 18);
  cam.overview(all, present, false, true);
  CHECK(cam.zoom() >= BoardCamera::kFarOverviewZoom);
  CHECK(cam.target().x == doctest::Approx(present.centerX()));
  // the same call when everything does fit: all of it
  const Rect small = span(0, 0, 0, 3);
  cam.overview(small, present, false, true);
  CHECK(cam.target().x == doctest::Approx(small.centerX()));
}

TEST_CASE("BoardCamera: focusBoard tweens to the board and the framing is Focus") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  const float before = cam.zoom();
  const Rect target = card(0, 4);
  REQUIRE(cam.focusBoard(target, 1.0f));
  CHECK(cam.state() == State::Focus);
  CHECK(cam.moving());
  CHECK(cam.zoom() == doctest::Approx(before)); // nothing moves on the request frame
  run(cam, 0.1f);
  CHECK(cam.moving());
  CHECK(cam.zoom() > before);
  run(cam, 0.3f);
  CHECK_FALSE(cam.moving());
  CHECK(cam.zoom() == doctest::Approx(1.0f));
  CHECK(cam.target().x == doctest::Approx(target.centerX()));
  CHECK(cam.target().y == doctest::Approx(target.centerY()));
  CHECK(cam.isBoardVisible(target, 1.0f));
}

TEST_CASE("BoardCamera: position and log(zoom) share one easing and arrive together") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 7);
  cam.overview(all, all, false, true);
  const Vec2 start = cam.target();
  const float zoom0 = cam.zoom();
  const Rect target = card(0, 6);
  cam.focusBoard(target, 1.0f);
  for (int i = 0; i < 6; ++i) {
    cam.update(1.0f / 60.0f);
    const float px = (cam.target().x - start.x) / (target.centerX() - start.x);
    const float pz = std::log(cam.zoom() / zoom0) / std::log(1.0f / zoom0);
    CHECK(near(px, pz, 0.001f));
  }
  run(cam, 0.5f);
  CHECK(cam.zoom() == doctest::Approx(1.0f));
  CHECK(cam.target().x == doctest::Approx(target.centerX()));
}

TEST_CASE("BoardCamera: Reduce motion is an 80 ms linear move, not a jump") {
  BoardCamera cam = makeCamera();
  cam.setReduceMotion(true);
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  const Rect target = card(0, 5);
  const float x0 = cam.target().x;
  cam.focusBoard(target, 1.0f);
  cam.update(0.04f); // half way
  CHECK(cam.moving());
  CHECK(cam.target().x == doctest::Approx(x0 + (target.centerX() - x0) * 0.5f).epsilon(0.001));
  cam.update(0.05f);
  CHECK_FALSE(cam.moving());
  CHECK(cam.target().x == doctest::Approx(target.centerX()));
}

TEST_CASE("BoardCamera: any player motion interrupts a tween where it is and the framing becomes Free") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  cam.focusBoard(card(0, 5), 1.0f);
  run(cam, 0.1f);
  const Vec2 at = cam.target();
  const float zoom = cam.zoom();
  cam.pan({10.0f, 0.0f});
  CHECK_FALSE(cam.moving());
  CHECK(cam.state() == State::Free);
  CHECK(cam.zoom() == doctest::Approx(zoom));
  CHECK(cam.target().x == doctest::Approx(at.x - 10.0f / zoom));
  run(cam, 1.0f);
  CHECK(cam.target().x == doctest::Approx(at.x - 10.0f / zoom)); // nothing pulls it back: no fly-back, ever

  cam.focusBoard(card(0, 0), 1.0f);
  run(cam, 0.05f);
  cam.wheel({700.0f, 400.0f}, 1.0f);
  CHECK_FALSE(cam.moving());
  CHECK(cam.state() == State::Free);

  cam.focusBoard(card(0, 3), 1.0f);
  run(cam, 0.05f);
  cam.cancel();
  CHECK_FALSE(cam.moving());
  CHECK(cam.state() == State::Free);
}

TEST_CASE("BoardCamera: the wheel zooms around the pointer, multiplicatively, within 0.4..2.5 and with no accumulator") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  cam.focusBoard(card(0, 0), 1.0f);
  cam.finishTween();
  const float z0 = cam.zoom();
  const Vec2 pointer{900.0f, 300.0f};
  const Vec2 under = cam.screenToWorld(pointer);
  cam.wheel(pointer, 1.0f); // one notch counts at once
  CHECK(cam.zoom() == doctest::Approx(z0 * BoardCamera::kWheelStep));
  CHECK(near(cam.screenToWorld(pointer).x, under.x, 0.01f));
  CHECK(near(cam.screenToWorld(pointer).y, under.y, 0.01f));
  cam.wheel(pointer, -3.0f);
  CHECK(cam.zoom() == doctest::Approx(z0 * std::pow(BoardCamera::kWheelStep, -2.0f)).epsilon(1e-4));
  const float low = cam.zoom();
  cam.wheel(pointer, 0.25f); // a trackpad's fractional tick
  CHECK(cam.zoom() > low);
  for (int i = 0; i < 100; ++i) cam.wheel(pointer, 1.0f);
  CHECK(cam.zoom() == doctest::Approx(BoardCamera::kMaxZoom));
  for (int i = 0; i < 100; ++i) cam.wheel(pointer, -1.0f);
  CHECK(cam.zoom() == doctest::Approx(BoardCamera::kMinZoom));
  CHECK(near(cam.screenToWorld(pointer).x, under.x, 0.5f)); // still the same world point under the pointer
}

TEST_CASE("BoardCamera: an Overview below the wheel range may only be zoomed towards it") {
  BoardCamera cam = makeCamera();
  const Rect all = span(-6, 0, 6, 0); // thirteen timelines: the fit is below 0.4
  cam.overview(all, all, false, true);
  const float z = cam.zoom();
  REQUIRE(z < BoardCamera::kMinZoom);
  cam.wheel({700, 400}, -1.0f);
  CHECK(cam.zoom() == doctest::Approx(z)); // not pushed up to 0.4 by a zoom-out
  cam.wheel({700, 400}, 1.0f);
  CHECK(cam.zoom() == doctest::Approx(z * BoardCamera::kWheelStep));
}

TEST_CASE("BoardCamera: locked, automatic requests are refused and the player's are not") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  cam.lock(true);
  const Vec2 before = cam.target();
  CHECK_FALSE(cam.focusBoard(card(0, 5), 1.0f, true));
  CHECK_FALSE(cam.showBoth(card(0, 0), card(1, 3), 0.8f, true));
  CHECK_FALSE(cam.reveal(card(3, 3), true));
  CHECK_FALSE(cam.peek(card(0, 5), 0.4f, true));
  CHECK_FALSE(cam.fitRegion(card(0, 5), 0.6f, true));
  CHECK_FALSE(cam.overview(all, all, true));
  CHECK_FALSE(cam.moving());
  CHECK(cam.state() == State::Overview);
  CHECK(cam.target().x == before.x);
  CHECK(cam.focusBoard(card(0, 5), 1.0f)); // a click
  CHECK(cam.moving());
  cam.lock(false);
  CHECK(cam.focusBoard(card(0, 4), 1.0f, true));
}

TEST_CASE("BoardCamera: showBoth keeps a time travel's two boards in view, never zooming in") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 3), card(0, 3), false, true);
  cam.focusBoard(card(0, 3), 1.25f);
  cam.finishTween();
  CHECK(cam.zoom() == doctest::Approx(1.25f));
  const Rect from = card(0, 3), to = card(1, 4); // the move landed one timeline up, one half-turn later
  REQUIRE(cam.showBoth(from, to, 0.8f, true));
  cam.finishTween();
  CHECK(cam.zoom() < 1.25f);
  CHECK(cam.zoom() >= 0.8f);
  CHECK(cam.isBoardVisible(from, 1.0f));
  CHECK(cam.isBoardVisible(to, 1.0f));
  // when both already fit at a smaller zoom the zoom is kept
  cam.zoomAt({700, 400}, 0.5f);
  const float z = cam.zoom();
  cam.showBoth(from, to, 0.4f, true);
  cam.finishTween();
  CHECK(cam.zoom() == doctest::Approx(z));
}

TEST_CASE("BoardCamera: showBoth that cannot fit at a readable zoom centres the new board instead") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  const Rect from = card(0, 0), to = card(6, 9);
  cam.showBoth(from, to, 0.8f, true);
  cam.finishTween();
  CHECK(cam.zoom() == doctest::Approx(0.8f));
  CHECK(cam.target().x == doctest::Approx(to.centerX()));
  CHECK(cam.isBoardVisible(to, 1.0f));
}

TEST_CASE("BoardCamera: reveal pans just far enough, keeps the zoom and the label") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  const float z = cam.zoom();
  const Rect above = card(2, 2); // two timelines up: off the top
  REQUIRE_FALSE(cam.isBoardVisible(above, 0.5f));
  cam.reveal(above, true);
  cam.finishTween();
  CHECK(cam.zoom() == doctest::Approx(z));
  CHECK(cam.state() == State::Overview);
  CHECK(cam.isBoardVisible(above, 1.0f));
  const Vec2 t = cam.target();
  cam.reveal(above, true); // already in view: no motion
  CHECK_FALSE(cam.moving());
  CHECK(cam.target().y == t.y);
}

TEST_CASE("BoardCamera: peek slides part of the way, only to a board that is off-screen") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  cam.focusBoard(card(0, 0), 1.0f);
  cam.finishTween();
  cam.zoomAt(cam.worldToScreen({card(0, 0).centerX(), card(0, 0).centerY()}), 2.0f); // the player zoomed in to 2: Free
  REQUIRE(cam.zoom() >= 1.9f);

  const Rect next = card(0, 1);
  REQUIRE_FALSE(cam.isBoardVisible(next, 0.5f));
  const float x0 = cam.target().x;
  REQUIRE(cam.peek(next, 0.4f, true));
  cam.finishTween();
  CHECK(cam.target().x == doctest::Approx(x0 + (next.centerX() - x0) * 0.4f));
  CHECK(cam.state() == State::Free); // the label is kept
  CHECK_FALSE(cam.peek(card(0, 0), 0.4f, true)); // in view: nothing
}

TEST_CASE("BoardCamera: isBoardVisible measures the visible fraction of the card") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  cam.focusBoard(card(0, 0), 1.0f);
  cam.finishTween();
  const Rect c = card(0, 0);
  CHECK(cam.isBoardVisible(c, 1.0f));
  CHECK_FALSE(cam.isBoardVisible(card(0, 4), 0.5f));
  const Rect v = cam.visibleWorld();
  const Rect halfOut = {v.x + v.w - c.w / 2.0f, c.y, c.w, c.h}; // half of it past the right edge
  CHECK(cam.isBoardVisible(halfOut, 0.49f));
  CHECK_FALSE(cam.isBoardVisible(halfOut, 0.6f));
}

TEST_CASE("BoardCamera: resizing re-applies the Overview or the Focus; a Free view stays") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  const float z = cam.zoom();
  cam.setViewport(1000, 700);
  CHECK(cam.state() == State::Overview);
  CHECK(cam.zoom() < z);
  const Rect v = cam.visibleWorld();
  CHECK(v.x < all.x);
  CHECK(v.x + v.w > all.x + all.w);
  CHECK_FALSE(cam.moving());

  cam.focusBoard(card(0, 2), 1.0f);
  cam.finishTween();
  cam.setViewport(1400, 800);
  CHECK(cam.state() == State::Focus);
  CHECK(cam.target().x == doctest::Approx(card(0, 2).centerX()));

  cam.pan({40, 0});
  const Vec2 t = cam.target();
  cam.setViewport(1200, 800);
  CHECK(cam.state() == State::Free);
  CHECK(cam.target().x == t.x);
}

TEST_CASE("BoardCamera: the player may not pan beyond the boards plus a board of margin") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 3);
  cam.setWorldBounds(all);
  cam.overview(all, all, false, true);
  cam.pan({-100000.0f, 0.0f}); // far to the right
  CHECK(cam.target().x == doctest::Approx(all.x + all.w + BoardLayout::kBoardSize));
  cam.pan({100000.0f, 100000.0f});
  CHECK(cam.target().x == doctest::Approx(all.x - BoardLayout::kBoardSize));
  CHECK(cam.target().y == doctest::Approx(all.y - BoardLayout::kBoardSize));
}

TEST_CASE("BoardCamera: nothing moves by itself: updates leave an idle camera exactly where it is") {
  BoardCamera cam = makeCamera();
  const Rect all = span(0, 0, 0, 5);
  cam.overview(all, all, false, true);
  cam.pan({25.0f, -10.0f}); // Free
  const Vec2 t = cam.target();
  const float z = cam.zoom();
  run(cam, 30.0f); // longer than the old 10 s fly-back
  CHECK(cam.target().x == t.x);
  CHECK(cam.target().y == t.y);
  CHECK(cam.zoom() == z);
  CHECK(std::string(cam.stateLabel()) == "Free");
}

TEST_CASE("BoardCamera: fitRegion is an Overview framing of a region, at least minZoom") {
  BoardCamera cam = makeCamera();
  cam.overview(card(0, 0), card(0, 0), false, true);
  const Rect column = span(-3, 6, 3, 6); // seven boards tall: it fits below 0.6
  REQUIRE(cam.fitRegion(column, 0.6f, true));
  cam.finishTween();
  CHECK(cam.zoom() == doctest::Approx(0.6f));
  CHECK(cam.state() == State::Overview);
  CHECK(cam.target().y == doctest::Approx(column.centerY()));
  CHECK(std::string(cam.stateLabel()) == "Overview");
}

TEST_CASE("BoardCamera: fling coasts for a moment and stops, and not under Reduce motion") {
  BoardCamera cam = makeCamera();
  cam.overview(span(0, 0, 0, 5), span(0, 0, 0, 5), false, true);
  cam.pan({1, 0});
  const float x0 = cam.target().x;
  cam.fling({600.0f, 0.0f});
  for (int i = 0; i < 5; ++i) { // it keeps moving over the first frames
    const float before = cam.target().x;
    cam.update(1.0f / 60.0f);
    CHECK(cam.target().x < before);
  }
  run(cam, 1.0f);
  const float x1 = cam.target().x;
  CHECK(x0 - x1 > 30.0f); // dragged right: the world moved right under the view
  run(cam, 1.0f);
  CHECK(cam.target().x == x1);

  BoardCamera reduced = makeCamera();
  reduced.setReduceMotion(true);
  reduced.overview(card(0, 0), card(0, 0), false, true);
  reduced.pan({1, 0});
  const float r0 = reduced.target().x;
  reduced.fling({600.0f, 0.0f});
  run(reduced, 1.0f);
  CHECK(reduced.target().x == r0);
}
