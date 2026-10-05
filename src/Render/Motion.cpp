#include "Render/Motion.h"
#include "App.h"

namespace UI::Motion {

bool reduced() { return App::current().settings.reduceMotion; }

float Tween::effectiveDuration() const {
    if (!reduced()) return duration;
    return opacityOnly ? (duration < fast ? duration : fast) : 0.0f;
}

void Tween::start(float f, float t, float dur, EaseFn ease, float startDelay, bool fadeOnly) {
    from = f; to = t; duration = dur; easing = ease ? ease : easeLinear; opacityOnly = fadeOnly;
    delay = reduced() ? 0.0f : startDelay;
    elapsed = 0.0f;
    active = true;
    if (effectiveDuration() <= 0.0f) finish();
}

void Tween::update(float dt) {
    if (!active) return;
    elapsed += safeDt(dt);
    if (elapsed >= delay + effectiveDuration()) finish();
}

float Tween::progress() const {
    if (!active) return 1.0f;
    const float d = effectiveDuration();
    if (d <= 0.0f) return 1.0f;
    return easing(clamp01((elapsed - delay) / d));
}

void Spring::update(float dt) {
    if (reduced()) { snap(target); return; }
    dt = safeDt(dt);
    // Semi-implicit Euler in sub-steps keeps stiff springs stable at any frame rate
    const int steps = 4;
    const float h = dt / steps;
    for (int i = 0; i < steps; ++i) {
        const float accel = stiffness * (target - value) - damping * velocity;
        velocity += accel * h;
        value += velocity * h;
    }
    if (settled(0.0005f)) snap(target);
}

}
