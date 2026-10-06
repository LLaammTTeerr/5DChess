#pragma once
#include <vector>
#include "ui/Screen.h"

// "Puzzles": the puzzles of assets/puzzles in one column per tier, each row with its title, its goal and a check mark when
// the player solved it (puzzles/Progress.h). A click opens the puzzle (Screens/PuzzleScreen).
class PuzzleListScreen : public Screen {
public:
  PuzzleListScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  struct Row {
    ui::Button button;
    int puzzle = 0; // index into puzzles::all()
  };
  std::vector<Row> _rows;
  ui::Button _back;
  Rectangle _columns[3]{};       // where each tier's column starts (x, y of the header, width)
};
