#pragma once
#include <array>
#include <string>
#include "ui/ConfirmArm.h"
#include "ui/Screen.h"

// "Load Game": the three save slots (what each holds; empty ones are disabled), a Delete button beside each
// (asks once more before it deletes) and, on desktop, "Paste record" to start from a record copied as text.
// A save that cannot be loaded says so here and stays where it is.
class LoadScreen : public Screen {
public:
  LoadScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  ui::ButtonList _slots;
  std::array<ui::Button, 3> _delete;
  ui::Button _back, _paste;
  std::array<bool, 3> _used{};
  ui::ConfirmArm _confirm; // Delete asks twice
  std::string _message;  // "This save can't be loaded"

  void build(App& app);
  void layoutNavRow();
};
