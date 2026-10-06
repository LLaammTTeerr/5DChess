#pragma once
#include <memory>
#include <string>
#include <vector>
#include "chess.h"

// The content of the interactive Guide (Screens/GuideScreen): a sequence of short pages, one rule each. A page is a title,
// a few sentences, a small live position (assets/guide/*.5dp, or a catalog mode) and optionally a "Try it" goal. Nothing
// here draws anything, so the pages and their goals are unit-tested without a window (tests/guide_test.cpp).
namespace guide {

/// "Try it": something for the player to do on the page's position. `met` looks only at the game (what was played so far:
/// the submitted turns and the pending one), so it answers the same whether the player is still in the turn or has
/// submitted it. The screen latches the first time it is true.
struct Goal {
  const char* prompt;  ///< "Move the knight back in time."
  const char* hint;    ///< what to do, shown after a move that did not reach the goal
  const char* success; ///< shown with the check mark
  bool (*met)(const Chess::IGame& game);
};

struct Page {
  const char* title;
  const char* text;          ///< two to four sentences of plain English, ASCII only
  const char* note;          ///< optional smaller aside under the text ("" for none)
  const char* position;      ///< file name under the guide directory; empty: use `mode`
  const char* mode;          ///< catalog id (Chess::GameCatalog) when there is no position file
  const Goal* goal;          ///< nullptr: nothing to try
  bool startsGame;           ///< the last page: offers a button that starts a Standard game
};

/// All pages in order.
const std::vector<Page>& pages();

/// Where the position files are looked up (default "assets/guide", relative to the working directory).
void setDirectory(std::string directory);
const std::string& directory();

/// A fresh game for the page (throws Chess::Core::ParseError when its file is bad, std::runtime_error for an unknown mode).
std::shared_ptr<Chess::IGame> load(const Page& page);

/// Whether the player has made any move on `game` (pending or submitted): the screen then says "not quite" instead of
/// only showing the prompt, when the goal is not met.
bool anyMoveMade(const Chess::IGame& game);

} // namespace guide
