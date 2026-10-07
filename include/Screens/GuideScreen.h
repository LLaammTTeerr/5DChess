#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Screens/PlayScreen.h"
#include "ui/Screen.h"

// The interactive Guide: one rule per page (content in guide/Guide.h), each with a small live position drawn and played by
// an embedded PlayScreen in the board view the player chose, and a panel at the right with the text, the "Try it" goal and
// Prev / Next. Left / Right arrow keys turn the pages too.
class GuideScreen : public Screen {
public:
  explicit GuideScreen(int page = 0);
  void update(App& app, float dt) override;
  void draw(App& app) const override;
  bool escape(App& app) override; // Esc cancels on the embedded board first
  void back(App& app) override;   // then does what the Back button does

  /// Developer tools: the game screen of the current page (to click squares by name), and the page number (0-based).
  PlayScreen* board() const { return _board.get(); }
  int page() const { return _page; }

private:
  int _page = 0;
  std::unique_ptr<PlayScreen> _board;
  bool _done = false; // the page's goal was met (latched until the page is reset or left)

  Rectangle _panel{};
  ui::Button _back = backButton();
  ui::Button _prev, _next, _reset, _start;

  struct Lines {
    std::vector<std::string> lines;
    float y = 0, height = 0;
  };
  Lines _step, _title, _text, _note, _prompt, _status;
  Rectangle _card{};          // the "Try it" card (empty when the page has no goal)
  std::string _statusText;    // what _status was wrapped from
  Rectangle _dots{};

  void open(int page);
  void go(int page);
  void layout();
  std::string statusText() const;
  void wrapStatus();
  float cardHeight() const;
};
