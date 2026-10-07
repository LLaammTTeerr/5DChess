#include "play/Hud.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <rlgl.h>
#include "App.h"
#include "Input.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"
#include "ui/Audit.h"
#include "ui/TextFit.h"

namespace play {

// A HUD panel in the style of the board view: rounded with a soft shadow (Atlas), a glow (Deep space) or a sharp ink
// rectangle (Blueprint).
void drawPanel(Rectangle r, const BoardStyle& st, float alpha, float roundness) {
  auto fadeBy = [alpha](::Color c) { c.a = static_cast<unsigned char>(c.a * alpha); return c; };
  if (st.hudSquare) {
    DrawRectangleRec(r, fadeBy(st.hudFill));
    DrawRectangleLinesEx(r, 1.0f, fadeBy(st.hudBorder));
    return;
  }
  if (st.card == BoardStyle::Card::Glow) {
    for (int k = 3; k >= 1; --k) {
      const Rectangle g = {r.x - k * 4.0f, r.y - k * 4.0f, r.width + k * 8.0f, r.height + k * 8.0f};
      DrawRectangleRounded(g, roundness, 12, fadeBy(UI::withAlpha(st.hudShadow, static_cast<unsigned char>(st.hudShadow.a * 0.22f / k))));
    }
  } else {
    DrawRectangleRounded({r.x + 2, r.y + 3, r.width, r.height}, roundness, 12, fadeBy(st.hudShadow));
  }
  DrawRectangleRounded(r, roundness, 12, fadeBy(st.hudFill));
  DrawRectangleRoundedLinesEx(r, roundness, 12, 1.0f, fadeBy(st.hudBorder));
}

namespace {

constexpr const char* kDot = "\xC2\xB7"; // middle dot

float textW(::Font font, const std::string& s, float size) { return MeasureTextEx(font, s.c_str(), size, 0).x; }

::Color faded(::Color c, float a) { c.a = static_cast<unsigned char>(c.a * UI::Motion::clamp01(a)); return c; }

// The longest of the candidate controls lines that fits `maxW` (the last one is cut with "..." if even it does not)
std::string controlsLine(bool cameraControls, ::Font font, float maxW) {
  const std::string sep = std::string("  ") + kDot + "  ";
  // Home / Space belong to the camera that reports a state (cameraLabel); until then the keys of the camera as it is
  const std::string candidates[] = {
      cameraControls ? "Overview (Home)" + sep + "Next board (Space)" + sep + "Drag/Wheel" + sep + "H: hide menu"
                     : "Click: select" + sep + "Drag/Wheel: pan/zoom" + sep + "Z: auto-zoom" + sep + "X: fit",
      cameraControls ? "Overview (Home)" + sep + "Next board (Space)" + sep + "Drag/Wheel" : "Click: select" + sep + "Drag/Wheel: pan/zoom",
      cameraControls ? "Overview (Home)" + sep + "Next board (Space)" : "Click: select",
  };

  for (const std::string& c : candidates)
    if (textW(font, c, UI::Font::mono) <= maxW) return c;
  return ui::ellipsized(candidates[2], maxW, [&](const std::string& t) { return textW(font, t, UI::Font::mono); });
}

} // namespace

Rectangle endCardRect(bool buttons) {
  const float screenW = static_cast<float>(GetScreenWidth()), screenH = static_cast<float>(GetScreenHeight());
  const float cardW = buttons ? 580.0f : 460.0f, cardH = buttons ? 238.0f : 220.0f;
  return {std::floor((screenW - cardW) / 2), std::floor((screenH - cardH) / 2), cardW, cardH};
}

void drawHud(const HudData& hud, const BoardStyle& st) {
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float availW = screenW - hud.rightInset; // a side panel at the right is not part of the board view
  const float cx = availW / 2.0f;
  const float margin = UI::Layout::sideInset;
  if (ui::audit::enabled()) ui::audit::rect("HUD zone", {0.0f, 0.0f, availW, UI::Layout::hudBottom}, ui::audit::Kind::HudZone);

  // ---- Top-centre status pill: [chip] "White to move" | "Turn N . K timelines" | hint ----
  const ::Font statusFont = UI::Fonts::button();
  const ::Font monoFont = UI::Fonts::mono();
  const ::Font bodyFont = UI::Fonts::body();
  const std::string status = !hud.title.empty() ? hud.title : hud.whiteToMove ? "White to move" : "Black to move";
  const std::string turnInfo = "Turn " + std::to_string(hud.fullTurn) + " " + kDot + " " + std::to_string(hud.timelineCount) +
                               (hud.timelineCount == 1 ? " timeline" : " timelines");

  // After a turn change the hint segment says whose turn it is for a moment (it replaces the old banner over the ruler)
  float swap = 0.0f;
  if (hud.bannerActive && hud.title.empty()) {
    using namespace UI::Motion;
    const float outDur = exitDuration(fast);
    swap = easeOutCubic(clamp01(hud.bannerClock / fast)) * (1.0f - easeInCubic(clamp01((hud.bannerClock - (1.2f - outDur)) / outDur)));
  }
  const std::string swapText = "Your move"; // (the status segment already names the colour)

  const float pad = UI::Space::md;
  const float chipD = 16.0f;
  const float h = UI::Space::buttonHeight;
  const float statusW = textW(statusFont, status, UI::Font::button);
  const float infoW = textW(monoFont, turnInfo, UI::Font::mono);
  const float gap = UI::Space::md;
  const float dotR = 2.5f, dotsW = 3 * dotR * 2 + 2 * UI::Space::xs + UI::Space::sm;
  // [TurnPanel] "3 / 5 boards": how many of this turn's boards are moved on (only when there are several)
  const std::string boardsText = hud.boardsTotal >= 2 ? std::to_string(hud.boardsDone) + " / " + std::to_string(hud.boardsTotal) + " boards" : std::string();
  const float boardsW = boardsText.empty() ? 0.0f : textW(monoFont, boardsText, UI::Font::mono);
  const float fixedW = pad + chipD + UI::Space::sm + statusW + gap + 1 + gap + infoW + (boardsText.empty() ? 0.0f : gap + 1 + gap + boardsW) + pad;
  // The pill is centred; it never reaches the Back button at the top left (which the game's own screens draw there)
  const float maxPill = std::min(availW - 2 * margin, 2.0f * (cx - UI::Layout::backRight));
  const bool thinkingHint = hud.thinking > 0.003f && !hud.hint.empty();

  // The hint is cut with "..." when the pill would not fit beside the side panel / the window edge
  const float hintRoom = std::max(0.0f, maxPill - fixedW - (gap + 1 + gap) - (thinkingHint ? dotsW : 0.0f));
  auto fitHint = [&](const std::string& t) {
    return ui::ellipsized(t, hintRoom, [&](const std::string& u) { return textW(bodyFont, u, UI::Font::body); });
  };
  const std::string hintShown = fitHint(hud.hint);
  const std::string swapShown = fitHint(swapText);
  const float hintW0 = hintShown.empty() ? 0.0f : textW(bodyFont, hintShown, UI::Font::body);
  const float swapW = textW(bodyFont, swapShown, UI::Font::body);
  const float hintW = UI::Motion::lerp(hintW0, swapW, swap);
  float w = fixedW;
  const bool hasHint = hintW > 0.5f;
  if (hasHint) w += gap + 1 + gap + hintW + (thinkingHint ? dotsW : 0.0f);

  const Rectangle pill = {std::floor(cx - w / 2), UI::Layout::hudPillY, w, h};
  drawPanel(pill, st);
  if (ui::audit::enabled()) ui::audit::rect("status pill", pill, ui::audit::Kind::Pill);
  if (ui::audit::enabled()) ui::audit::within("HUD pill", status + " | " + turnInfo + " | " + hintShown, pill, {margin / 2, 0.0f, availW - margin, screenH});

  float x = pill.x + pad;
  const float cy = pill.y + h / 2;
  {
    // The chip cross-fades between the two colours on a turn change
    const float m = hud.chipWhite;
    const ::Color chip = {
        static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.r, UI::Color::whiteChip.r, m)),
        static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.g, UI::Color::whiteChip.g, m)),
        static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.b, UI::Color::whiteChip.b, m)), 255};
    DrawCircleV({x + chipD / 2, cy}, chipD / 2, chip);
  }
  DrawCircleLinesV({x + chipD / 2, cy}, chipD / 2, st.hudText);
  x += chipD + UI::Space::sm;
  DrawTextEx(statusFont, status.c_str(), {std::floor(x), std::floor(cy - UI::Font::button / 2.0f - 1)}, UI::Font::button, 0,
             st.hudText);
  x += statusW + gap;
  DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), st.hudBorder);
  x += 1 + gap;
  DrawTextEx(monoFont, turnInfo.c_str(), {std::floor(x), std::floor(cy - UI::Font::mono / 2.0f - 1)}, UI::Font::mono, 0,
             st.hudMuted);
  x += infoW;
  if (!boardsText.empty()) {
    x += gap;
    DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), st.hudBorder);
    x += 1 + gap;
    const bool all = hud.boardsDone >= hud.boardsTotal;
    DrawTextEx(monoFont, boardsText.c_str(), {std::floor(x), std::floor(cy - UI::Font::mono / 2.0f - 1)}, UI::Font::mono, 0, all ? st.hudText : st.hudMuted);
    x += boardsW;
  }
  if (hasHint) {
    x += gap;
    DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), st.hudBorder);
    x += 1 + gap;
    const float ty = std::floor(cy - UI::Font::body / 2.0f - 1);
    if (swap < 0.999f) DrawTextEx(bodyFont, hintShown.c_str(), {std::floor(x), ty}, UI::Font::body, 0, faded(st.hudHint, 1.0f - swap));
    if (swap > 0.001f) DrawTextEx(bodyFont, swapShown.c_str(), {std::floor(x), ty}, UI::Font::body, 0, faded(st.hudText, swap));
    if (thinkingHint) {
      // Three dots that swell one after the other (still, at full strength, under Reduce motion), then the bar
      const float a = UI::Motion::clamp01(hud.thinking);
      float dx = x + hintW + UI::Space::sm + dotR;
      for (int i = 0; i < 3; ++i, dx += dotR * 2 + UI::Space::xs) {
        float pulse = 1.0f;
        if (!UI::Motion::reduced()) pulse = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(static_cast<float>(hud.clock) * 3.2f - i * 0.9f));
        DrawCircleV({dx, cy + 5.0f}, dotR, UI::withAlpha(st.hudHint, static_cast<unsigned char>(255.0f * a * pulse)));
      }
    }
  }
  if (thinkingHint && hud.thinkBar) {
    const float a = UI::Motion::clamp01(hud.thinking);
    const float inset = h / 2, barH = 3.0f;
    const Rectangle track = {pill.x + inset, pill.y + h - 7.0f, pill.width - 2 * inset, barH};
    DrawRectangleRounded(track, 1.0f, 4, UI::withAlpha(st.hudBorder, static_cast<unsigned char>(255.0f * a)));
    const float fill = std::max(barH, track.width * UI::Motion::clamp01(hud.thinkFraction)); // never an empty bar: a dot at 0
    DrawRectangleRounded({track.x, track.y, fill, barH}, 1.0f, 4, UI::withAlpha(st.hudHint, static_cast<unsigned char>(255.0f * a)));
  }

  // ---- Bottom controls bar: the camera's state, then the keys (fades after a few idle seconds) ----
  if (hud.controlsAlpha > 0.003f) {
    const float a = hud.controlsAlpha;
    const std::string& cam = hud.cameraLabel;
    const float chipW = cam.empty() ? 0.0f : textW(monoFont, cam, UI::Font::mono) + 2 * UI::Space::sm;
    const float camW = cam.empty() ? 0.0f : chipW + gap;
    const std::string controls = controlsLine(!cam.empty(), monoFont, maxPill - 2 * pad - camW);
    const float cw = textW(monoFont, controls, UI::Font::mono) + camW;
    const Rectangle bar = {std::floor(cx - (cw + 2 * pad) / 2), screenH - UI::Layout::controlsBarMargin - UI::Layout::controlsBarH,
                           cw + 2 * pad, UI::Layout::controlsBarH};
    drawPanel(bar, st, 0.92f * a);
    if (ui::audit::enabled()) ui::audit::rect("controls bar", bar, ui::audit::Kind::Pill);
    if (ui::audit::enabled()) ui::audit::within("controls bar", cam + " " + controls, bar, {0.0f, 0.0f, availW, screenH});
    float bx = bar.x + pad;
    const float by = std::floor(bar.y + (bar.height - UI::Font::mono) / 2 - 1);
    if (!cam.empty()) {
      DrawRectangleRounded({bx, bar.y + 5.0f, chipW, bar.height - 10.0f}, 1.0f, 8, faded(UI::withAlpha(st.accent, 40), a));
      DrawTextEx(monoFont, cam.c_str(), {bx + UI::Space::sm, by}, UI::Font::mono, 0, faded(st.hudText, a));
      const std::string what = cam == "Overview" ? "Every board is in view." : cam == "Focus" ? "Zoomed on one board." : "Your own framing: dragging or the wheel put it here.";
      ui::tooltip({bx, bar.y + 5.0f, chipW, bar.height - 10.0f}, what + " Home shows the whole multiverse.");
      bx += camW;
    }
    DrawTextEx(monoFont, controls.c_str(), {bx, by}, UI::Font::mono, 0, faded(st.hudMuted, a));
  }
  if (hud.controlsAlpha < 0.997f) { // the bar is away: a small chip says where it is (the pointer near the bottom brings it back)
    const float a = 1.0f - hud.controlsAlpha;
    const float w = textW(monoFont, "Controls", UI::Font::mono) + 2 * UI::Space::sm;
    const Rectangle chip = {std::floor(cx - w / 2), screenH - UI::Layout::controlsBarMargin - 22.0f, w, 22.0f};
    DrawRectangleRounded(chip, 1.0f, 8, faded(st.hudFill, 0.8f * a));
    DrawTextEx(monoFont, "Controls", {chip.x + UI::Space::sm, chip.y + 3.0f}, UI::Font::mono, 0, faded(st.hudMuted, a));
  }
}

void drawEndCard(const EndCard& end, const EndCardButtons* buttons) {
  using namespace UI::Motion;
  const float fade = end.scrim; // scrim + text fade
  const float pop = end.pop;    // spring: slight overshoot
  // A light scrim: the player wants to look at the final position behind the card
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                UI::withAlpha(UI::Color::scrim, static_cast<unsigned char>(UI::Color::scrim.a * 0.58f * fade)));

  const Rectangle card = endCardRect(end.buttons);
  const float scale = reduced() ? 1.0f : UI::Motion::lerp(0.88f, 1.0f, pop);
  auto fa = [fade](::Color c) { c.a = static_cast<unsigned char>(c.a * clamp01(fade)); return c; };

  rlPushMatrix();
  rlTranslatef(card.x + card.width / 2, card.y + card.height / 2, 0.0f);
  rlScalef(scale, scale, 1.0f);
  rlTranslatef(-(card.x + card.width / 2), -(card.y + card.height / 2), 0.0f);

  DrawRectangleRounded({card.x + 4, card.y + 6, card.width, card.height}, 0.08f, 8, fa(UI::Color::shadow));
  DrawRectangleRounded(card, 0.08f, 8, fa(UI::Color::surface));
  DrawRectangleRoundedLinesEx(card, 0.08f, 8, 1.0f, fa(UI::Color::border));

  const float cx = card.x + card.width / 2;
  UI::drawTextCentered(UI::Fonts::title(), end.title.c_str(), cx, card.y + 30, UI::Font::title, fa(UI::Color::text));
  UI::drawTextCentered(UI::Fonts::body(), end.reason.c_str(), cx, card.y + 90, UI::Font::body, fa(UI::Color::textMuted));
  if (buttons && end.buttons) {
    DrawRectangle(static_cast<int>(card.x + 40), static_cast<int>(card.y + 126), static_cast<int>(card.width - 80), 1, fa(UI::Color::border));
    buttons->draw(clamp01(fade));
  } else {
    DrawRectangle(static_cast<int>(card.x + 40), static_cast<int>(card.y + 146), static_cast<int>(card.width - 80), 1,
                  fa(UI::Color::border));
    UI::drawTextCentered(UI::Fonts::body(), end.footer.c_str(), cx, card.y + 164, UI::Font::body,
                         fa(UI::Color::primary));
  }

  // Pixel theme: the winner's king hops a few times on top of the card
  if (App::current().themes.currentThemeHasBlink() && !end.draw) {
    const char* name = end.whiteWon ? "white_king" : "black_king";
    const PieceTextures& tex = App::current().themes.getPieceTextures(name);
    const float size = 64.0f;
    float hop = 0.0f;
    if (!reduced()) {
      float t = end.clock - 0.30f; // start once the card has popped
      const float period = 0.46f;
      for (int i = 0; i < 3 && t >= 0.0f; ++i, t -= period) {
        if (t < period) { const float u = t / period; hop = (26.0f - i * 8.0f) * 4.0f * u * (1.0f - u); break; }
      }
    }
    const bool closed = blinkClosed(77u, Input::time());
    Texture2D& sprite = (closed && tex.blink) ? *tex.blink : *tex.open;
    const float sx = std::floor(cx - size / 2), sy = std::floor(card.y - size + 6.0f - hop);
    DrawEllipse(static_cast<int>(cx), static_cast<int>(card.y + 1), 20.0f - hop * 0.25f, 4.0f,
                fa(UI::withAlpha(UI::Color::shadow, 70)));
    DrawTexturePro(sprite, {0, 0, (float)sprite.width, (float)sprite.height}, {sx, sy, size, size}, {0, 0}, 0.0f,
                   UI::withAlpha(WHITE, static_cast<unsigned char>(255.0f * clamp01(fade))));
  }
  rlPopMatrix();
}

// ---------------------------------------------------------------------------------------------------------------------

EndCardButtons::EndCardButtons() {
  layout();
  for (ui::Button* b : {&_rematch, &_review, &_menu}) b->enterAfter(0.12f);
}

void EndCardButtons::setRematch(bool available) {
  if (available == _hasRematch) return;
  _hasRematch = available;
  layout();
}

void EndCardButtons::layout() {
  const Rectangle card = endCardRect(true);
  const float bw = 168.0f, gap = UI::Space::sm + 4.0f, y = card.y + card.height - UI::Space::lg - UI::Space::buttonHeight;
  const int n = _hasRematch ? 3 : 2;
  const auto slots = ui::row({card.x, y, card.width, UI::Space::buttonHeight}, n, bw, gap);
  size_t i = 0;
  if (_hasRematch) _rematch.rect = slots[i++];
  _review.rect = slots[i++];
  _menu.rect = slots[i];
}

EndCardButtons::Action EndCardButtons::update(float dt, bool reachable) {
  Action action = Action::None;
  if (_hasRematch && _rematch.update(dt, reachable)) action = Action::Rematch;
  if (_review.update(dt, reachable) && action == Action::None) action = Action::Review;
  if (_menu.update(dt, reachable) && action == Action::None) action = Action::Menu;
  if (ui::hovered(endCardRect(true))) ui::consumePointer(); // the card is modal: no click goes through to the board
  return action;
}

void EndCardButtons::draw(float alpha) const {
  if (_hasRematch) _rematch.draw(alpha);
  _review.draw(alpha);
  _menu.draw(alpha);
}

// ---------------------------------------------------------------------------------------------------------------------

HudMotion::HudMotion() { _endPop.init(0.0f, 260.0f, 0.62f); }

void HudMotion::update(float dt) {
  _clock += dt;
  if (_thinking) {
    if (UI::Motion::reduced()) _thinkShown = _thinkTarget;
    else _thinkShown = UI::Motion::smoothDamp(_thinkShown, _thinkTarget, _thinkVelocity, 0.25f, dt);
  }
  _thinkFade.update(dt);
  if (_bannerActive) {
    _bannerClock += dt;
    if (_bannerClock >= 1.2f) _bannerActive = false;
  }
  _chip.update(dt);
  if (_endActive) {
    _endClock += dt;
    _endScrim.update(dt);
    _endPop.update(dt);
  }

  // The controls bar steps aside after a few seconds without pointer input, and returns when the pointer moves or nears it
  const Vector2 delta = Input::mouseDelta();
  const bool nearBottom = Input::mousePosition().y > static_cast<float>(GetScreenHeight()) - 90.0f;
  bool key = false;
  for (int k : {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_SPACE, KEY_TAB, KEY_ENTER, KEY_HOME, KEY_H, KEY_U, KEY_Z, KEY_X, KEY_C, KEY_Q, KEY_R, KEY_B, KEY_N})
    key = key || Input::keyPressed(k);
  if (delta.x != 0.0f || delta.y != 0.0f || Input::mousePressed(MOUSE_BUTTON_LEFT) || Input::mouseWheel() != 0.0f || nearBottom || key) _idle = 0.0f;
  else _idle += dt;
  // (a new player is not left without the controls: they step aside only once a turn has been handed over)
  const float target = _idle < kControlsIdle || _turnChanges < 1 ? 1.0f : 0.0f;
  if (UI::Motion::reduced()) _controls = target;
  else _controls = target > _controls ? std::min(target, _controls + dt / UI::Motion::fast) : std::max(target, _controls - dt / UI::Motion::base);
}

void HudMotion::setTurn(bool whiteToMove) {
  if (_seeded && whiteToMove != _white) {
    // Turn change: the hint segment of the pill says whose turn it is for a moment; the chip cross-fades
    ++_turnChanges;
    _bannerActive = true;
    _bannerClock = 0.0f;
    _chip.start(_chip.value(), whiteToMove ? 1.0f : 0.0f, UI::Motion::fast, UI::Motion::easeOutCubic, 0.0f, true);
  } else if (!_seeded) {
    _chip.start(whiteToMove ? 1.0f : 0.0f, whiteToMove ? 1.0f : 0.0f, 0.0f);
  }
  _seeded = true;
  _white = whiteToMove;
}

void HudMotion::setThinking(bool thinking, float fraction) {
  _thinkBar = fraction >= 0.0f;
  _thinkTarget = UI::Motion::clamp01(fraction);
  if (thinking == _thinking) return;
  _thinking = thinking;
  if (thinking) _thinkShown = 0.0f, _thinkVelocity = 0.0f;
  _thinkFade.start(_thinkFade.value(), thinking ? 1.0f : 0.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
}

void HudMotion::setEnded(bool ended, bool whiteWon, bool draw) {
  if (ended && !_endActive) {
    _endActive = true;
    _endWhiteWon = whiteWon;
    _endDraw = draw;
    _endClock = 0.0f;
    _endScrim.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
    _endPop.init(0.0f, 260.0f, 0.62f);
    _endPop.setTarget(1.0f);
  } else if (!ended && _endActive) {
    _endActive = false;
    _endPop.snap(0.0f);
  }
}

void HudMotion::apply(HudData& hud) const {
  hud.chipWhite = _chip.value();
  hud.bannerActive = _bannerActive;
  hud.bannerWhite = _white;
  hud.bannerClock = _bannerClock;
  hud.thinking = _thinkFade.value();
  hud.thinkFraction = _thinkShown;
  hud.thinkBar = _thinkBar;
  hud.clock = _clock;
  hud.controlsAlpha = _controls;
}

EndCard HudMotion::endCard() const {
  EndCard card;
  card.whiteWon = _endWhiteWon;
  card.draw = _endDraw;
  card.scrim = _endActive ? _endScrim.progress() : 1.0f;
  card.pop = _endActive ? _endPop.value : 1.0f;
  card.clock = _endClock;
  return card;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace {
constexpr float kOverviewW = 124.0f, kNextW = 140.0f, kClusterGap = UI::Space::sm + 4.0f;
}

ActionRow::ActionRow() {
  _deselect.tip = "Put the piece down";
  layout(static_cast<float>(GetScreenWidth()));
}

void ActionRow::layout(float availableWidth) {
  _availableW = availableWidth;
  const float y = UI::Layout::actionRowY, h = UI::Space::buttonHeight;
  _overview.rect = {availableWidth - UI::Layout::sideInset - kOverviewW, y, kOverviewW, h};
  // Beside a side panel there is no room for the long label: "To move" (the tooltip and the controls bar say the rest)
  const bool roomy = availableWidth >= 1100.0f;
  const float nextW = roomy ? kNextW : 90.0f;
  _next.label = roomy ? "Next board" : "To move";
  _next.rect = {_overview.rect.x - kClusterGap - nextW, y, nextW, h};
  auto slots = ui::row({0.0f, y, availableWidth, h}, 3, UI::Space::actionButtonWidth, UI::Space::buttonSpacing);
  // The side panel leaves little room: slide the centred trio left before it would run into Next board
  const float clusterLeft = _nextShown ? _next.rect.x : _overviewShown ? _overview.rect.x : availableWidth;
  const float over = slots[2].x + slots[2].width + UI::Space::md - clusterLeft;
  if (over > 0.0f) {
    const float shift = std::min(over, std::max(0.0f, slots[0].x - UI::Layout::sideInset - 200.0f));
    for (Rectangle& s : slots) s.x -= shift;
  }
  _undo.rect = slots[0];
  _deselect.rect = slots[1];
  _submit.rect = slots[2];
}

void ActionRow::sync(const HudData& hud) {
  const bool overview = !hud.cameraLabel.empty();
  _overviewShown = overview;
  _overview.tip = "Fit every board on screen (Home)";
  _undo.label = hud.undoLabel.empty() ? "Undo" : hud.undoLabel;
  _undoCount = hud.undoCount;
  // The keys of the new key map (U, Esc) are named once the screen reports a camera state, i.e. has that map
  const char* u = overview ? " (U)" : "";
  _undo.tip = !_undo.enabled ? "Nothing to take back yet"
              : std::string(hud.undoLabel == "Undo turn" ? "Take back your last turn and the reply" : "Take back the last move of this turn") + u;
  _deselect.tip = overview ? "Put the piece down (Esc)" : "Put the piece down";
  _submit.tip = hud.submitTip;
  _next.tip = hud.nextBoardLabel.empty() ? std::string() : hud.nextBoardLabel + " (Space)";
  const bool shown = !hud.nextBoardLabel.empty();
  if (shown != _nextShown || overview != _overviewLaidOut) {
    _nextShown = shown;
    _overviewLaidOut = overview;
    layout(_availableW);
  }
}

ActionRow::Action ActionRow::update(float dt) {
  Action action = Action::None;
  if (_undo.update(dt)) action = Action::Undo;
  if (_deselect.update(dt) && action == Action::None) action = Action::Deselect;
  if (_submit.update(dt) && action == Action::None) action = Action::Submit;
  if (_overviewShown && _overview.update(dt) && action == Action::None) action = Action::Overview;
  if (_nextShown && _next.update(dt) && action == Action::None) action = Action::NextBoard;
  return action;
}

void ActionRow::draw() const {
  for (const ui::Button* b : {&_undo, &_deselect, &_submit}) b->draw();
  if (_overviewShown) _overview.draw();
  if (_nextShown) _next.draw();
  if (_undoCount > 1) {
    // A badge on Undo: how many moves of this turn it would take back one by one
    const Vector2 c = {_undo.rect.x + _undo.rect.width - 6.0f, _undo.rect.y + 6.0f};
    const std::string n = std::to_string(std::min(_undoCount, 99));
    const ui::Skin& skin = _undo.skin ? *_undo.skin : ui::defaultSkin();
    if (ui::audit::enabled()) ui::audit::rect("undo count badge", {c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f}, ui::audit::Kind::Badge);
    DrawCircleV(c, 10.0f, skin.primary);
    const ::Font font = UI::Fonts::mono();
    const Vector2 ts = MeasureTextEx(font, n.c_str(), UI::Font::minimum, 0);
    DrawTextEx(font, n.c_str(), {std::floor(c.x - ts.x / 2), std::floor(c.y - ts.y / 2)}, UI::Font::minimum, 0, skin.onPrimary);
  }
}

} // namespace play
