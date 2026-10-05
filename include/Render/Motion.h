#pragma once
#include <cmath>

// Shared motion tokens, easing functions and the two animation primitives (Tween, Spring).
//
// Rules of thumb (see README "Motion"): entering = easeOutCubic, leaving = easeInCubic and ~65 % of the enter
// duration; stagger 30-50 ms per item; animations never delay game state or input.
namespace UI::Motion {

// ---- Duration tokens (seconds) ----
inline constexpr float fast = 0.12f;
inline constexpr float base = 0.22f;
inline constexpr float slow = 0.38f;
inline constexpr float exitFactor = 0.65f;     // leaving takes this fraction of the enter duration
inline constexpr float stagger = 0.04f;        // default per-item delay of staggered lists
inline constexpr float staggerDots = 0.03f;    // legal-target dots
inline constexpr float maxStep = 1.0f / 15.0f; // longest dt we integrate (tab switch / debugger pause)

inline constexpr float exitDuration(float enter) { return enter * exitFactor; }

// ---- Global "Reduce motion" setting: everything becomes instant or a short cross-fade ----
bool reduced();
void setReduced(bool on);

// ---- Easing (t in [0,1]) ----
inline float clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float easeLinear(float t) { return t; }
inline float easeOutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
inline float easeInCubic(float t) { return t * t * t; }
inline float easeInOutCubic(float t) { return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f; }
// Slight overshoot, for "pop"
inline float easeOutBack(float t) {
    constexpr float c1 = 1.4f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
using EaseFn = float (*)(float);

// Clamp a frame time so a stalled frame never explodes a spring or skips an animation.
inline float safeDt(float dt) { return dt < 0.0f ? 0.0f : (dt > maxStep ? maxStep : dt); }

// A value animated from `from` to `to` over `duration` seconds (after an optional `delay`).
// opacityOnly tweens (fades) still run under Reduce motion, shortened to `fast`; every other tween jumps to its end.
struct Tween {
    float from = 0.0f, to = 0.0f;
    float duration = 0.0f;
    float delay = 0.0f;
    float elapsed = 0.0f;
    EaseFn easing = easeOutCubic;
    bool opacityOnly = false;
    bool active = false;

    void start(float f, float t, float dur, EaseFn ease = easeOutCubic, float startDelay = 0.0f, bool fadeOnly = false);
    void update(float dt);
    void finish() { elapsed = delay + effectiveDuration(); active = false; }
    // 0..1 eased progress (0 before the delay has passed)
    float progress() const;
    float value() const { return lerp(from, to, progress()); }
    bool done() const { return !active; }
    // Never started or finished: value() == to
    float effectiveDuration() const;
};

// Damped spring toward `target`. Defaults are critically damped (damping = 2*sqrt(stiffness)).
// Use a smaller damping ratio for a slight overshoot ("pop").
struct Spring {
    float value = 0.0f, velocity = 0.0f, target = 0.0f;
    float stiffness = 380.0f;
    float damping = 2.0f * 19.4936f;

    Spring() = default;
    Spring(float v, float stiff, float dampingRatio) { init(v, stiff, dampingRatio); }
    void init(float v, float stiff, float dampingRatio) {
        value = target = v; velocity = 0.0f; stiffness = stiff; damping = 2.0f * dampingRatio * std::sqrt(stiff);
    }
    void setTarget(float t) { target = t; }
    void snap(float v) { value = target = v; velocity = 0.0f; }
    void update(float dt);
    bool settled(float eps = 0.002f) const { return std::fabs(value - target) < eps && std::fabs(velocity) < eps * 4.0f; }
};

}
