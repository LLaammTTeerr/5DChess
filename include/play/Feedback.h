#pragma once
#include "play/Selection.h"

/// The numbers and curves of the board view's feedback motion (illegal-click shake, hover preview, lane unfold, check pulse, turn
/// hand-over). Pure functions of time, no graphics and no clock, so they are unit tested; MoveAnimator keeps the state and
/// BoardRenderer draws it. Durations follow include/Render/Motion.h (fast 0.12, base 0.22, slow 0.38).
namespace play::feedback {

inline constexpr float kShakeSeconds = 0.24f;      // illegal click: three half-cycles
inline constexpr float kShakePixels = 3.0f;        // peak sideways offset on screen
inline constexpr float kRejectFlash = 0.35f;       // peak alpha of the red square flash
inline constexpr float kRejectFlashSeconds = 0.30f;
inline constexpr float kHintAlertSeconds = 0.6f;   // the HUD reason stays red this long
inline constexpr float kHoverDwell = 0.12f;        // pointer rest before the legal-move preview starts
inline constexpr float kPreviewAlpha = 0.40f;
inline constexpr float kArcGrowSeconds = 0.18f;    // live time-travel arc
inline constexpr float kMaxDotStagger = 0.15f;     // the legal dots of a long-range piece all exist after this
inline constexpr float kLaneUnfoldSeconds = 0.32f;
inline constexpr float kFlightDelayNewTimeline = 0.12f;
inline constexpr float kCheckSeconds = 0.50f;      // two pulses of the checked king's square
inline constexpr float kCheckDrawOnSeconds = 0.22f;
inline constexpr float kHandOverSeconds = 0.26f;   // present column slide
inline constexpr float kLiftPixels = 4.0f;         // boards of the side to move lift this much...
inline constexpr float kLiftStagger = 0.04f;       // ...one after the other
inline constexpr float kSourceHaloSeconds = 1.0f;  // the board a time-travel move came from
inline constexpr float kChromeLiftPixels = 2.0f;   // hovered card frame

/// What the HUD says for a rejected click.
const char* reasonText(Intent::Reason reason);

/// Sideways offset (pixels) of a shaking card `t` (0..1) into the shake: three half-cycles that decay with easeOutCubic.
float shakeOffset(float t, float amplitude = kShakePixels);

/// Delay before the legal-move dot `ring` squares away from the picked-up piece pops in: 30 ms per ring, never beyond 150 ms.
float dotDelay(float ring);

/// 0..1 intensity of the checked king's square `t` (0..1) into the check animation: two pulses.
float checkPulse(float t);

/// Upward offset (0..kLiftPixels) of a board `elapsed` seconds into its lift (a rise and a settle).
float liftBump(float elapsed, float duration = kHandOverSeconds);

/// The visible part of a lane band [y0, y0 + h) that unfolds from the parent lane's side (`fromTop`: the parent lane is above)
/// at eased progress `p` (0..1).
struct Span { float y0 = 0, y1 = 0; };
Span unfoldedSpan(float y0, float h, bool fromTop, float p);

} // namespace play::feedback
