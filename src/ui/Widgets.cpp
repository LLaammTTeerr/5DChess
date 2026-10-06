#include "ui/Widgets.h"
#include "App.h"
#include "Audio/AudioManager.h"
#include "Input.h"
#include "Render/UITheme.h"
#include <algorithm>
#include <cmath>

namespace ui {

namespace {
bool g_consumed = false;

bool contains(Rectangle r, Vector2 p) { return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height; }

Color lerpColor(Color a, Color b, float t) {
  auto ch = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * t); };
  return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}
float clamp(float v, float lo, float hi) { return std::fmax(lo, std::fmin(hi, v)); }
} // namespace

void beginFrame(bool pointerConsumed) { g_consumed = pointerConsumed; }
bool pointerConsumed() { return g_consumed; }
void consumePointer() { g_consumed = true; }

std::vector<Rectangle> column(Rectangle area, int n, float itemH, float gap, Align align) {
  const float total = n * itemH + (n - 1) * gap;
  const float y0 = align == Align::Center ? area.y + (area.height - total) / 2 : area.y;
  std::vector<Rectangle> out;
  for (int i = 0; i < n; ++i) out.push_back({area.x, y0 + i * (itemH + gap), area.width, itemH});
  return out;
}

std::vector<Rectangle> row(Rectangle area, int n, float itemW, float gap) {
  const float x0 = area.x + (area.width - (n * itemW + (n - 1) * gap)) / 2;
  std::vector<Rectangle> out;
  for (int i = 0; i < n; ++i) out.push_back({x0 + i * (itemW + gap), area.y, itemW, area.height});
  return out;
}

// ---- Button ---------------------------------------------------------------------------------------------

void Button::enterAfter(float delay) {
  entering_ = true;
  enter_.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, delay, true);
}

bool Button::update(float dt, bool reachable) {
  const bool over = reachable && contains(rect, Input::mousePosition());
  hot_ = over && enabled && !g_consumed;
  if (over) consumePointer();

  const bool press = hot_ && Input::mousePressed(MOUSE_BUTTON_LEFT);
  if (press) pressStartedHere_ = true;
  if (!Input::mouseDown(MOUSE_BUTTON_LEFT)) pressStartedHere_ = false;
  pressed_ = hot_ && pressStartedHere_;

  const float step = dt / 0.15f;
  hover_ = hot_ ? std::fmin(1.0f, hover_ + step) : std::fmax(0.0f, hover_ - step);
  if (entering_) enter_.update(dt);

  if (press) App::current().audio.playSfx(Sfx::Click);
  return press;
}

void Button::draw(float alpha) const {
  float rise = 0.0f;
  if (entering_) {
    const float p = enter_.progress();
    alpha *= p;
    if (!UI::Motion::reduced()) rise = (1.0f - p) * 8.0f;
  }
  if (hot_) UI::Cursor::requestHand();
  if (alpha <= 0.003f) return;
  auto fade = [alpha](Color c) { c.a = static_cast<unsigned char>(c.a * alpha); return c; };
  const Rectangle r = {rect.x, rect.y + rise, rect.width, rect.height};

  Color bg, border, text;
  float thickness = 1.0f;
  if (!enabled) {
    bg = border = UI::Color::disabledBg;
    text = UI::Color::disabledText;
  } else if (pressed_) {
    bg = border = primary ? UI::Color::primaryPressed : UI::Color::accentPressed;
    text = UI::Color::onAccent;
  } else if (primary) {
    bg = border = lerpColor(UI::Color::primary, UI::Color::primaryHover, hover_);
    text = UI::Color::onAccent;
  } else {
    bg = lerpColor(UI::Color::surface, UI::Color::surfaceAlt, hover_);
    border = lerpColor(UI::Color::border, UI::Color::accent, hover_);
    text = UI::Color::text;
    thickness = 1.0f + hover_;
  }
  DrawRectangleRounded(r, UI::Space::radius, 8, fade(bg));
  DrawRectangleRoundedLinesEx(r, UI::Space::radius, 8, thickness, fade(border));

  const ::Font font = UI::Fonts::button();
  float size = UI::Font::button;
  Vector2 ts = MeasureTextEx(font, label.c_str(), size, 0.0f);
  const float maxW = r.width - 2 * UI::Space::md;  // shrink a long label (min 14 px) instead of overflowing
  if (ts.x > maxW) {
    size = std::fmax(static_cast<float>(UI::Font::minimum), size * maxW / ts.x);
    ts = MeasureTextEx(font, label.c_str(), size, 0.0f);
  }
  DrawTextEx(font, label.c_str(), {std::floor(r.x + (r.width - ts.x) / 2), std::floor(r.y + (r.height - ts.y) / 2)},
             size, 0.0f, fade(text));
}

Toggle::Toggle(Rectangle r, bool& value, const char* onLabel, const char* offLabel)
    : Button(value ? onLabel : offLabel, r), value_(value), on_(onLabel), off_(offLabel) {}

bool Toggle::update(float dt) {
  if (!Button::update(dt)) return false;
  value_ = !value_;
  label = value_ ? on_ : off_;
  return true;
}

// ---- ButtonList -----------------------------------------------------------------------------------------

ButtonList::ButtonList(const std::vector<std::string>& labels, std::vector<Rectangle> slots, Rectangle viewport,
                       float scrollbarW)
    : slots_(std::move(slots)), view_(viewport), scrollbarW_(scrollbarW) {
  for (size_t i = 0; i < labels.size(); ++i) {
    items_.emplace_back(labels[i], slots_[i]);
    items_.back().enterAfter(static_cast<float>(i) * UI::Motion::stagger);
  }
  if (clipped() && !slots_.empty())
    maxScroll_ = std::fmax(0.0f, slots_.back().y + slots_.back().height - (view_.y + view_.height));
}

Rectangle ButtonList::handle() const {
  const Rectangle bar = scrollbar();
  const float h = maxScroll_ <= 0 ? bar.height : std::fmax(20.0f, view_.height / (view_.height + maxScroll_) * bar.height);
  return {bar.x, bar.y + (maxScroll_ <= 0 ? 0.0f : scroll_ / maxScroll_ * (bar.height - h)), bar.width, h};
}

void ButtonList::scrollInput() {
  const Vector2 mouse = Input::mousePosition();
  if (const float wheel = Input::mouseWheel(); wheel != 0 && contains(view_, mouse))
    scroll_ = clamp(scroll_ - wheel * 30.0f, 0.0f, maxScroll_);
  if (Input::mousePressed(MOUSE_BUTTON_LEFT) && contains(scrollbar(), mouse)) dragging_ = true;
  if (!Input::mouseDown(MOUSE_BUTTON_LEFT)) dragging_ = false;
  if (dragging_ && scrollbar().height > 0)  // the thumb follows the pointer along the track
    scroll_ = clamp((mouse.y - scrollbar().y) / scrollbar().height * maxScroll_, 0.0f, maxScroll_);
}

int ButtonList::update(float dt) {
  if (clipped() && scrollbarW_ > 0) scrollInput();
  const bool inArea = !clipped() || contains(itemsArea(), Input::mousePosition());
  int clicked = -1;
  for (size_t i = 0; i < items_.size(); ++i) {
    items_[i].rect = {slots_[i].x, slots_[i].y - scroll_, slots_[i].width, slots_[i].height};
    if (items_[i].update(dt, inArea) && clicked < 0) clicked = static_cast<int>(i);
  }
  if (clicked >= 0 && selectable) selected = clicked;

  // Indicator: spring toward the selected item while it is (partly) in view, fade out otherwise
  const bool have = selected >= 0 && selected < static_cast<int>(items_.size()) && items_[selected].enabled &&
                    (!clipped() || (items_[selected].rect.y + items_[selected].rect.height >= view_.y &&
                                    items_[selected].rect.y <= view_.y + view_.height));
  if (have) {
    const Rectangle t = items_[selected].rect;
    if (!indicatorInit_) {
      indicatorInit_ = true;
      x_.init(t.x, 420.0f, 0.9f); y_.init(t.y, 420.0f, 0.9f);
      w_.init(t.width, 420.0f, 0.9f); h_.init(t.height, 420.0f, 0.9f);
      alpha_.init(0.0f, 600.0f, 1.0f);
    }
    x_.setTarget(t.x); y_.setTarget(t.y); w_.setTarget(t.width); h_.setTarget(t.height);
  }
  alpha_.setTarget(have ? 1.0f : 0.0f);
  for (UI::Motion::Spring* s : {&x_, &y_, &w_, &h_, &alpha_}) s->update(dt);
  return clicked;
}

void ButtonList::draw(float alpha) const {
  if (clipped())
    BeginScissorMode(static_cast<int>(view_.x), static_cast<int>(view_.y), static_cast<int>(view_.width - scrollbarW_),
                     static_cast<int>(view_.height));
  for (const Button& b : items_)
    if (!clipped() || (b.rect.y + b.rect.height >= view_.y && b.rect.y <= view_.y + view_.height)) b.draw(alpha);

  const float a = clamp(alpha_.value, 0.0f, 1.0f) * alpha;
  if (indicatorInit_ && a > 0.01f) {
    const Rectangle r = {x_.value, y_.value, w_.value, h_.value};
    auto fade = [a](Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
    DrawRectangleRoundedLinesEx(r, UI::Space::radius, 8, UI::Space::outline, fade(UI::Color::selected));
    const float barH = r.height * 0.5f;
    DrawRectangleRounded({r.x + 8, r.y + (r.height - barH) / 2, 4, barH}, 1.0f, 4, fade(UI::Color::selected));
  }
  if (clipped()) EndScissorMode();

  if (maxScroll_ > 0) {
    DrawRectangleRounded(scrollbar(), 1.0f, 6, UI::Color::surfaceAlt);
    DrawRectangleRounded(handle(), 1.0f, 6,
                         contains(handle(), Input::mousePosition()) ? UI::Color::textMuted : UI::Color::border);
  }
}

}
