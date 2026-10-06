#pragma once
#include <string>
#include <vector>
#include "ui/Screen.h"

// Title screen: animated timeline-board field, hero title, idling creatures and the navigation column.
class MainMenuScreen : public Screen {
public:
  MainMenuScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  enum class Item { Continue, Versus, Load, Puzzles, Guide, Settings, Exit };
  std::vector<Item> _items;  // what each button of _nav does
  ui::ButtonList _nav;       // [Continue,] Versus, Load game, Puzzles, Guide, Settings, Exit
  std::string _notice;       // "This save can't be loaded" after a failed Continue
  float _time = 0.0f;        // drives the field drift, bob and blink (frozen under Reduce motion)
  float _enterClock = 0.0f;  // seconds since entering (creature stagger)
  float _noticeClock = 0.0f;
  UI::Motion::Tween _enter;  // title / subtitle entrance
};
