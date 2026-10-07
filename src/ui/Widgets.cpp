#include "ui/Widgets.h"
#include "App.h"
#include "Audio/AudioManager.h"
#include "Input.h"
#include "Render/UITheme.h"
#include "ui/Audit.h"
#include "ui/TextFit.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

namespace ui {

namespace {
bool g_consumed = false;
bool g_listDraws = false;  // a clipped list is drawing its rows: its viewport stands for them in the overlap audit

bool contains(Rectangle r, Vector2 p) { return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height; }

Color lerpColor(Color a, Color b, float t) {
  auto ch = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * t); };
  return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}
} // namespace

const Skin& defaultSkin() {
  static const Skin skin = {UI::Color::surface,    UI::Color::surfaceAlt,   UI::Color::border,        UI::Color::accent,
                            UI::Color::text,       UI::Color::primary,      UI::Color::primaryHover,  UI::Color::primaryPressed,
                            UI::Color::onAccent,   UI::Color::accentPressed, UI::Color::onAccent, UI::Color::disabledBg,   UI::Color::disabledText,
                            UI::Space::radius};
  return skin;
}

void beginFrame(bool pointerConsumed) { g_consumed = pointerConsumed; }
bool pointerConsumed() { return g_consumed; }
void consumePointer() { g_consumed = true; }

bool hovered(Rectangle r) { return contains(r, Input::mousePosition()); }

namespace {
struct TipState { size_t id = 0; double since = 0.0, last = -10.0; };
TipState g_tip;
}

namespace {
struct QueuedTip { Rectangle anchor{}; std::string text; float alpha = 0.0f; bool set = false; };
QueuedTip g_queued;
}

void tooltip(Rectangle anchor, const std::string& text) {
  if (text.empty() || !hovered(anchor)) return;
  const size_t id = std::hash<std::string>{}(text) ^ (static_cast<size_t>(anchor.x) * 31u + static_cast<size_t>(anchor.y) * 131071u);
  const double now = Input::time();
  if (g_tip.id != id || now - g_tip.last > 0.25) g_tip = {id, now, now}; // another anchor, or the pointer left for a while: dwell restarts
  g_tip.last = now;
  const double shown = now - g_tip.since - kTooltipDwell;
  if (shown < 0.0) return;
  const float a = UI::Motion::reduced() ? 1.0f : UI::Motion::clamp01(static_cast<float>(shown) / UI::Motion::fast);
  g_queued = {anchor, text, a, true}; // painted by flushTooltip() at the end of the frame, over everything
}

void flushTooltip() {
  if (!g_queued.set) return;
  const Rectangle anchor = g_queued.anchor;
  const std::string text = g_queued.text;
  const float a = g_queued.alpha;
  g_queued.set = false;

  // Wrap to the width of a short paragraph; explicit '\n' starts a new line
  const ::Font font = UI::Fonts::body();
  constexpr float size = 16.0f, maxW = 320.0f, pad = 10.0f, lineH = 22.0f;
  std::vector<std::string> lines;
  std::istringstream paragraphs(text);
  for (std::string para; std::getline(paragraphs, para);) {
    std::string line;
    std::istringstream words(para);
    for (std::string word; words >> word;) {
      const std::string trial = line.empty() ? word : line + " " + word;
      if (!line.empty() && MeasureTextEx(font, trial.c_str(), size, 0).x > maxW) { lines.push_back(line); line = word; }
      else line = trial;
    }
    lines.push_back(line);
  }
  float w = 0.0f;
  for (const std::string& l : lines) w = std::max(w, MeasureTextEx(font, l.c_str(), size, 0).x);
  const float h = lineH * static_cast<float>(lines.size()) + 2 * pad - 4.0f;
  const float W = static_cast<float>(GetScreenWidth()), H = static_cast<float>(GetScreenHeight());
  float x = anchor.x + anchor.width / 2 - (w + 2 * pad) / 2;
  float y = anchor.y + anchor.height + 8.0f;
  if (y + h > H - 6.0f) y = anchor.y - h - 8.0f;
  // Under an anchor above the turn ruler the bubble would cover the ruler: open beside the anchor instead (right, else left)
  if (anchor.y < UI::Layout::rulerY && y + h > UI::Layout::rulerY - 2.0f) {
    const float bw = w + 2 * pad;
    y = anchor.y + anchor.height / 2 - h / 2;
    if (anchor.x + anchor.width + 8.0f + bw <= W - 6.0f) x = anchor.x + anchor.width + 8.0f;
    else if (anchor.x - 8.0f - bw >= 6.0f) x = anchor.x - 8.0f - bw;
    else y = anchor.y - h - 8.0f;
  }
  x = std::clamp(x, 6.0f, std::max(6.0f, W - w - 2 * pad - 6.0f));
  const Rectangle box = {std::floor(x), std::floor(y), w + 2 * pad, h};
  auto fade = [a](Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
  DrawRectangleRounded({box.x + 1, box.y + 2, box.width, box.height}, 0.2f, 8, fade(UI::Color::shadow));
  DrawRectangleRounded(box, 0.2f, 8, fade({31, 30, 29, 238}));
  for (size_t i = 0; i < lines.size(); ++i)
    DrawTextEx(font, lines[i].c_str(), {box.x + pad, std::floor(box.y + pad - 2.0f + static_cast<float>(i) * lineH)}, size, 0, fade({250, 249, 245, 255}));
}

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
  over_ = over && !g_consumed;
  hot_ = over && enabled && !g_consumed;
  if (over) consumePointer();

  const bool press = hot_ && Input::mousePressed(MOUSE_BUTTON_LEFT);
  if (press) pressStartedHere_ = true;
  if (!Input::mouseDown(MOUSE_BUTTON_LEFT)) pressStartedHere_ = false;
  pressed_ = hot_ && pressStartedHere_;

  const float step = dt / 0.15f;
  hover_ = hot_ ? std::fmin(1.0f, hover_ + step) : std::fmax(0.0f, hover_ - step);
  if (entering_) enter_.update(dt);

  if (press && enabled) { press_.value = 0.98f; press_.velocity = 0.0f; press_.target = 1.0f; }
  press_.update(dt);
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
  if (alpha <= 0.003f) return;
  if (hot_) UI::Cursor::requestHand();
  auto fade = [alpha](Color c) { c.a = static_cast<unsigned char>(c.a * alpha); return c; };
  Rectangle r = {rect.x, rect.y + rise, rect.width, rect.height};
  if (!UI::Motion::reduced() && std::fabs(press_.value - 1.0f) > 0.0005f) { // a press dips to 0.98x about the centre and springs back
    const float s = press_.value;
    r = {r.x + r.width * (1.0f - s) / 2.0f, r.y + r.height * (1.0f - s) / 2.0f, r.width * s, r.height * s};
  }

  if (ui::audit::enabled() && !g_listDraws) ui::audit::rect("button '" + label + "'", rect, ui::audit::Kind::Button);
  const Skin& k = skin ? *skin : defaultSkin();
  Color bg, border, text;
  float thickness = 1.0f;
  if (!enabled) {
    bg = border = k.disabledBg;
    text = k.disabledText;
  } else if (pressed_) {
    bg = border = primary ? k.primaryPressed : k.pressed;
    text = primary ? k.onPrimary : k.pressedText;
  } else if (primary) {
    bg = border = lerpColor(k.primary, k.primaryHover, hover_);
    text = k.onPrimary;
  } else {
    bg = lerpColor(k.surface, k.surfaceHover, hover_);
    border = lerpColor(k.border, k.borderHover, hover_);
    text = k.text;
    thickness = 1.0f + hover_;
  }
  if (k.roundness > 0.0f) {
    DrawRectangleRounded(r, k.roundness, 8, fade(bg));
    DrawRectangleRoundedLinesEx(r, k.roundness, 8, thickness, fade(border));
  } else {
    DrawRectangleRec(r, fade(bg));
    DrawRectangleLinesEx(r, thickness, fade(border));
  }

  if (icon) {
    const float side = std::fmin(r.width, r.height) - 10.0f;
    DrawTexturePro(*icon, {0, 0, static_cast<float>(icon->width), static_cast<float>(icon->height)},
                   {std::floor(r.x + (r.width - side) / 2), std::floor(r.y + (r.height - side) / 2), side, side}, {0, 0}, 0.0f,
                   fade(enabled ? WHITE : Color{255, 255, 255, 110}));
    if (!tip.empty() && over_) ui::tooltip(rect, tip);
    return;
  }
  const ::Font font = UI::Fonts::button();
  const float nominal = static_cast<float>(UI::Font::button);
  float size = nominal;
  auto widthOf = [&](const std::string& t, float sz) { return MeasureTextEx(font, t.c_str(), sz, 0.0f).x; };
  const float pad = UI::Space::md;
  const float maxW = r.width - 2 * pad;
  const float textH = MeasureTextEx(font, "Ag", nominal, 0.0f).y;
  const float ty = std::floor(r.y + (r.height - textH) / 2);
  if (!detail.empty() || leftAligned) {
    // A row of a list: the title at the left, the muted detail at the right; only the title is cut
    const ::Font small = UI::Fonts::body();
    const float dw = detail.empty() ? 0.0f : MeasureTextEx(small, detail.c_str(), UI::Font::body, 0.0f).x;
    const float room = maxW - (detail.empty() ? 0.0f : dw + pad);
    const std::string shown = ellipsize ? ui::ellipsized(label, room, [&](const std::string& t) { return widthOf(t, nominal); }) : label;
    const float shownW = widthOf(shown, nominal);
    if (ui::audit::enabled()) ui::audit::fit(detail.empty() ? "row label" : "row label + detail", shown, shownW + (detail.empty() ? 0.0f : dw + pad), textH,
                   {0, 0, maxW, r.height});
    DrawTextEx(font, shown.c_str(), {std::floor(r.x + pad), ty}, nominal, 0.0f, fade(text));
    if (!detail.empty()) {
      const Color muted = lerpColor(text, bg, 0.38f);
      DrawTextEx(small, detail.c_str(), {std::floor(r.x + r.width - pad - dw), std::floor(r.y + (r.height - UI::Font::body) / 2)},
                 UI::Font::body, 0.0f, fade(enabled ? muted : text));
    }
    if (!tip.empty() && over_) ui::tooltip(rect, tip);
    return;
  }
  float w = widthOf(label, size);
  std::string shown = label;
  if (w > maxW) {
    if (ellipsize) {
      shown = ui::ellipsized(label, maxW, [&](const std::string& t) { return widthOf(t, nominal); });
    } else {
      size = std::fmax(static_cast<float>(UI::Font::minimum), size * maxW / w); // shrink a long label (min 14 px) instead of overflowing
      if (ui::audit::enabled()) ui::audit::shrunk("button label", label, size, nominal);
    }
  }
  const Vector2 ts = MeasureTextEx(font, shown.c_str(), size, 0.0f);
  if (ui::audit::enabled()) ui::audit::fit("button label", shown, ts.x, ts.y, {0, 0, r.width - 8.0f, r.height});
  DrawTextEx(font, shown.c_str(), {std::floor(r.x + (r.width - ts.x) / 2), std::floor(r.y + (r.height - ts.y) / 2)}, size, 0.0f,
             fade(text));
  if (!tip.empty() && over_) ui::tooltip(rect, tip);
}

Toggle::Toggle(Rectangle r, bool& value, const char* onLabel, const char* offLabel)
    : Button(value ? onLabel : offLabel, r), value_(value), on_(onLabel), off_(offLabel) {}

bool Toggle::update(float dt) {
  if (!Button::update(dt)) return false;
  value_ = !value_;
  label = value_ ? on_ : off_;
  return true;
}

Cycle::Cycle(Rectangle r, std::string prefix, std::vector<std::string> options, int value)
    : Button(prefix + options.at(static_cast<size_t>(value)), r), prefix_(std::move(prefix)), options_(std::move(options)), value_(value) {}

bool Cycle::update(float dt) {
  if (!Button::update(dt)) return false;
  value_ = (value_ + 1) % static_cast<int>(options_.size());
  label = prefix_ + options_[static_cast<size_t>(value_)];
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
  if (const float wheel = Input::mouseWheel(); !g_consumed && wheel != 0 && contains(view_, mouse))
    scroll_ = std::clamp(scroll_ - wheel * 30.0f, 0.0f, maxScroll_);
  if (!g_consumed && Input::mousePressed(MOUSE_BUTTON_LEFT) && contains(scrollbar(), mouse)) dragging_ = true;
  if (!Input::mouseDown(MOUSE_BUTTON_LEFT)) dragging_ = false;
  if (dragging_ && scrollbar().height > 0)  // the thumb follows the pointer along the track
    scroll_ = std::clamp((mouse.y - scrollbar().y) / scrollbar().height * maxScroll_, 0.0f, maxScroll_);
}

int ButtonList::update(float dt, bool interactive) {
  if (interactive && clipped() && scrollbarW_ > 0) scrollInput();
  const bool inArea = interactive && (!clipped() || contains(itemsArea(), Input::mousePosition()));
  int clicked = -1;
  for (size_t i = 0; i < items_.size(); ++i) {
    items_[i].rect = {slots_[i].x, slots_[i].y - scroll_, slots_[i].width, slots_[i].height};
    items_[i].skin = skin;
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
  if (clipped() && ui::audit::enabled())
    ui::audit::rect("list " + std::to_string(static_cast<int>(view_.x)) + "," + std::to_string(static_cast<int>(view_.y)), view_, ui::audit::Kind::List);
  g_listDraws = clipped();
  if (clipped())
    BeginScissorMode(static_cast<int>(view_.x), static_cast<int>(view_.y), static_cast<int>(view_.width - scrollbarW_),
                     static_cast<int>(view_.height));
  for (const Button& b : items_)
    if (!clipped() || (b.rect.y + b.rect.height >= view_.y && b.rect.y <= view_.y + view_.height)) b.draw(alpha);

  const float a = std::clamp(alpha_.value, 0.0f, 1.0f) * alpha;
  if (indicatorInit_ && a > 0.01f) {
    const Rectangle r = {x_.value, y_.value, w_.value, h_.value};
    auto fade = [a](Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
    DrawRectangleRoundedLinesEx(r, UI::Space::radius, 8, UI::Space::outline, fade(UI::Color::selected));
    const float barH = r.height * 0.5f;
    DrawRectangleRounded({r.x + 8, r.y + (r.height - barH) / 2, 4, barH}, 1.0f, 4, fade(UI::Color::selected));
  }
  if (clipped() && maxScroll_ > 0.0f) {
    // Rows that continue past the viewport fade out instead of being cut through their text
    const float fh = 18.0f, w = view_.width - scrollbarW_;
    const Color bg = fadeTo, clear = {bg.r, bg.g, bg.b, 0}, solid = {bg.r, bg.g, bg.b, static_cast<unsigned char>(255.0f * alpha)};
    if (scroll_ < maxScroll_ - 0.5f) DrawRectangleGradientV(static_cast<int>(view_.x), static_cast<int>(view_.y + view_.height - fh), static_cast<int>(w), static_cast<int>(fh), clear, solid);
    if (scroll_ > 0.5f) DrawRectangleGradientV(static_cast<int>(view_.x), static_cast<int>(view_.y), static_cast<int>(w), static_cast<int>(fh), solid, clear);
  }
  if (clipped()) EndScissorMode();
  g_listDraws = false;

  if (maxScroll_ > 0) {
    DrawRectangleRounded(scrollbar(), 1.0f, 6, UI::Color::surfaceAlt);
    DrawRectangleRounded(handle(), 1.0f, 6,
                         contains(handle(), Input::mousePosition()) ? UI::Color::textMuted : UI::Color::border);
  }
}

}
