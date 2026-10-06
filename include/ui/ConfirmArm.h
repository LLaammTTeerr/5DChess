#pragma once

namespace ui {

/// Two-step confirmation for a destructive click (Delete, overwrite a save). The first click on an item arms it; a second
/// click on the same item confirms, but only once kMinDelay has passed (a double-click, or one click read twice, must not
/// confirm: ui::Button fires on the press) and not after kExpire, nor once the pointer has left the item. No graphics
/// dependency, so it is unit tested.
class ConfirmArm {
public:
  static constexpr float kMinDelay = 0.4f; // seconds between arming and the confirming click
  static constexpr float kExpire = 3.0f;   // an armed item disarms after this long

  /// Call once per frame before handling clicks. `hovered`: the item under the pointer, or -1.
  void update(float dt, int hovered) {
    _clock += dt;
    if (_armed >= 0 && (_clock - _armedAt > kExpire || hovered != _armed)) _armed = -1;
  }
  /// A click on item `id`. Returns true when it confirms the action (the arm is then cleared).
  bool click(int id) {
    if (_armed == id) {
      if (_clock - _armedAt < kMinDelay) return false; // too soon: stay armed
      _armed = -1;
      return true;
    }
    _armed = id;
    _armedAt = _clock;
    return false;
  }
  int armed() const { return _armed; }
  void clear() { _armed = -1; }

private:
  float _clock = 0.0f, _armedAt = 0.0f;
  int _armed = -1;
};

} // namespace ui
