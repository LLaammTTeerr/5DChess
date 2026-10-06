#include "play/Hud.h"
#include <cmath>
#include <raylib.h>
#include <rlgl.h>
#include "App.h"
#include "Input.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"

namespace play {

namespace {

// A HUD panel in the style of the board view: rounded with a soft shadow (Atlas), a glow (Deep space) or a sharp ink
// rectangle (Blueprint).
void drawPanel(Rectangle r, const BoardStyle& st, float alpha = 1.0f) {
  auto fadeBy = [alpha](::Color c) { c.a = static_cast<unsigned char>(c.a * alpha); return c; };
  if (st.hudSquare) {
    DrawRectangleRec(r, fadeBy(st.hudFill));
    DrawRectangleLinesEx(r, 1.0f, fadeBy(st.hudBorder));
    return;
  }
  if (st.card == BoardStyle::Card::Glow) {
    for (int k = 3; k >= 1; --k) {
      const Rectangle g = {r.x - k * 4.0f, r.y - k * 4.0f, r.width + k * 8.0f, r.height + k * 8.0f};
      DrawRectangleRounded(g, 1.0f, 12, fadeBy(UI::withAlpha(st.hudShadow, static_cast<unsigned char>(st.hudShadow.a * 0.22f / k))));
    }
  } else {
    DrawRectangleRounded({r.x + 2, r.y + 3, r.width, r.height}, 1.0f, 12, fadeBy(st.hudShadow));
  }
  DrawRectangleRounded(r, 1.0f, 12, fadeBy(st.hudFill));
  DrawRectangleRoundedLinesEx(r, 1.0f, 12, 1.0f, fadeBy(st.hudBorder));
}

// Slim "<Colour> to move" banner: drops in below the action row (fast), leaves faster, auto-dismisses at ~1.2 s.
void drawTurnBanner(const HudData& hud, const BoardStyle& st) {
  if (!hud.bannerActive) return;
  using namespace UI::Motion;
  const float outDur = exitDuration(fast);
  const float in = easeOutCubic(clamp01(hud.bannerClock / fast));
  const float out = easeInCubic(clamp01((hud.bannerClock - (1.2f - outDur)) / outDur));
  const float a = in * (1.0f - out);
  if (a <= 0.003f) return;
  const float slide = reduced() ? 0.0f : (1.0f - in) * -16.0f + out * -8.0f;

  const std::string text = hud.bannerWhite ? "White to move" : "Black to move";
  const ::Font font = UI::Fonts::body();
  const float tw = MeasureTextEx(font, text.c_str(), UI::Font::body, 0).x;
  const float chipD = 10.0f, pad = UI::Space::md, h = 30.0f;
  const float w = pad + chipD + UI::Space::sm + tw + pad;
  const float screenW = static_cast<float>(GetScreenWidth());
  const Rectangle r = {std::floor((screenW - w) / 2), UI::Layout::actionRowBottom + 8.0f + slide, w, h};
  auto fade = [a](::Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
  drawPanel(r, st, a);
  const Vector2 c = {r.x + pad + chipD / 2, r.y + h / 2};
  DrawCircleV(c, chipD / 2, fade(hud.bannerWhite ? UI::Color::whiteChip : UI::Color::blackChip));
  DrawCircleLinesV(c, chipD / 2, fade(st.hudText));
  DrawTextEx(font, text.c_str(), {std::floor(c.x + chipD / 2 + UI::Space::sm), std::floor(r.y + (h - UI::Font::body) / 2 - 1)},
             UI::Font::body, 0, fade(st.hudText));
}

} // namespace

void drawHud(const HudData& hud, const BoardStyle& st) {
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());

  // ---- Top-centre status pill: [chip] "White to move" | "Turn N · K timelines" | hint ----
  const ::Font statusFont = UI::Fonts::button();
  const ::Font monoFont = UI::Fonts::mono();
  const ::Font bodyFont = UI::Fonts::body();
  const std::string status = hud.whiteToMove ? "White to move" : "Black to move";
  const std::string turnInfo = "Turn " + std::to_string(hud.fullTurn) + " \xC2\xB7 " + std::to_string(hud.timelineCount) +
                               (hud.timelineCount == 1 ? " timeline" : " timelines");

  const float pad = UI::Space::md;
  const float chipD = 16.0f;
  const float h = UI::Space::buttonHeight;
  const float statusW = MeasureTextEx(statusFont, status.c_str(), UI::Font::button, 0).x;
  const float infoW = MeasureTextEx(monoFont, turnInfo.c_str(), UI::Font::mono, 0).x;
  const float hintW = hud.hint.empty() ? 0.0f : MeasureTextEx(bodyFont, hud.hint.c_str(), UI::Font::body, 0).x;
  const float gap = UI::Space::md;
  float w = pad + chipD + UI::Space::sm + statusW + gap + 1 + gap + infoW + pad;
  if (hintW > 0) w += gap + 1 + gap + hintW;

  Rectangle pill = {std::floor((screenW - w) / 2), UI::Layout::hudPillY, w, h};
  drawPanel(pill, st);

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
  if (hintW > 0) {
    x += gap;
    DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), st.hudBorder);
    x += 1 + gap;
    DrawTextEx(bodyFont, hud.hint.c_str(), {std::floor(x), std::floor(cy - UI::Font::body / 2.0f - 1)}, UI::Font::body, 0,
               st.hudHint);
  }

  // ---- Bottom controls bar (only real controls: see BoardCamera) ----
  const char* controls = "Click: select  \xC2\xB7  Drag/Wheel: pan/zoom  \xC2\xB7  Z: auto-zoom  \xC2\xB7  X: fit  \xC2\xB7  Esc: menu";
  const float cw = MeasureTextEx(monoFont, controls, UI::Font::mono, 0).x;
  Rectangle bar = {std::floor((screenW - (cw + 2 * pad)) / 2),
                   screenH - UI::Layout::controlsBarMargin - UI::Layout::controlsBarH, cw + 2 * pad, UI::Layout::controlsBarH};
  drawPanel(bar, st, 0.92f);
  DrawTextEx(monoFont, controls, {bar.x + pad, std::floor(bar.y + (bar.height - UI::Font::mono) / 2 - 1)}, UI::Font::mono, 0,
             st.hudMuted);

  drawTurnBanner(hud, st);
}

void drawEndCard(const EndCard& end) {
  using namespace UI::Motion;
  const float screenW = static_cast<float>(GetScreenWidth());
  const float screenH = static_cast<float>(GetScreenHeight());
  const float fade = end.scrim; // scrim + text fade
  const float pop = end.pop;    // spring: slight overshoot
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                UI::withAlpha(UI::Color::scrim, static_cast<unsigned char>(UI::Color::scrim.a * fade)));

  const float cardW = 460.0f, cardH = 220.0f;
  Rectangle card = {std::floor((screenW - cardW) / 2), std::floor((screenH - cardH) / 2), cardW, cardH};
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
  UI::drawTextCentered(UI::Fonts::title(), end.title.c_str(), cx, card.y + 34, UI::Font::title, fa(UI::Color::text));
  UI::drawTextCentered(UI::Fonts::body(), end.reason.c_str(), cx, card.y + 102, UI::Font::body, fa(UI::Color::textMuted));
  DrawRectangle(static_cast<int>(card.x + 40), static_cast<int>(card.y + 146), static_cast<int>(card.width - 80), 1,
                fa(UI::Color::border));
  UI::drawTextCentered(UI::Fonts::body(), end.footer.c_str(), cx, card.y + 164, UI::Font::body,
                       fa(UI::Color::primary));

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

HudMotion::HudMotion() { _endPop.init(0.0f, 260.0f, 0.62f); }

void HudMotion::update(float dt) {
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
}

void HudMotion::setTurn(bool whiteToMove) {
  if (_seeded && whiteToMove != _white) {
    // Turn change: a slim banner drops in below the action row; the HUD chip cross-fades
    _bannerActive = true;
    _bannerClock = 0.0f;
    _chip.start(_chip.value(), whiteToMove ? 1.0f : 0.0f, UI::Motion::fast, UI::Motion::easeOutCubic, 0.0f, true);
  } else if (!_seeded) {
    _chip.start(whiteToMove ? 1.0f : 0.0f, whiteToMove ? 1.0f : 0.0f, 0.0f);
  }
  _seeded = true;
  _white = whiteToMove;
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

ActionRow::ActionRow() {
  // Just below the HUD pill
  const auto slots = ui::row({0.0f, UI::Layout::actionRowY, static_cast<float>(GetScreenWidth()), UI::Space::buttonHeight}, 3,
                             UI::Space::actionButtonWidth, UI::Space::buttonSpacing);
  _undo.rect = slots[0];
  _deselect.rect = slots[1];
  _submit.rect = slots[2];
}

ActionRow::Action ActionRow::update(float dt) {
  Action action = Action::None;
  if (_undo.update(dt)) action = Action::Undo;
  if (_deselect.update(dt) && action == Action::None) action = Action::Deselect;
  if (_submit.update(dt) && action == Action::None) action = Action::Submit;
  return action;
}

void ActionRow::draw() const {
  for (const ui::Button* b : {&_undo, &_deselect, &_submit}) b->draw();
}

} // namespace play
