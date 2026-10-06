#pragma once
#include "ui/Screen.h"

// Title screen: animated timeline-board field, hero title, idling creatures and the navigation column.
class MainMenuScreen : public Screen {
public:
  MainMenuScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  ui::ButtonList _nav;       // Versus, Puzzles, Guide, Settings, Exit
  float _time = 0.0f;        // drives the field drift, bob and blink (frozen under Reduce motion)
  float _enterClock = 0.0f;  // seconds since entering (creature stagger)
  UI::Motion::Tween _enter;  // title / subtitle entrance
};
