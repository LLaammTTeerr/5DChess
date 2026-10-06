#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Screens/PlayScreen.h"
#include "ai/Search.h"
#include "puzzles/Puzzle.h"
#include "puzzles/Solver.h"
#include "ui/Screen.h"

// One puzzle: the normal multiverse board (an embedded PlayScreen, as in the Guide) with a panel at the right holding the goal,
// the hint and Hint / Reset / Show solution. The player submits a turn; the engine judges it:
//   mate in 1   the result of the submitted turn: checkmate solves it, anything else is "Not quite".
//   mate in 2   after the first turn the DefenceProver proves that every defence loses (a first turn that does not win by force is
//               "Not quite"); then ai::Search (Hard, capped) picks Black's reply, which is played on the board, and the player's
//               second turn is judged like a mate in 1.
// Solving marks the puzzle in puzzles::progress(); nothing here touches the autosave.
class PuzzleScreen : public Screen {
public:
  explicit PuzzleScreen(int index); // index into puzzles::all()
  void update(App& app, float dt) override;
  void draw(App& app) const override;

  /// Developer tools: the game screen (to click squares by name).
  PlayScreen* board() const { return _board.get(); }
  int index() const { return _index; }

private:
  enum class Phase { Playing, Judging, Proving, Searching, Replying, Wrong, Solved, Solution, SolutionDone };
  enum class Tone { Info, Good, Bad };

  /// One step of a scripted line (the opponent's reply, or Show solution): a move, or Submit; `after` seconds pass before the next.
  struct Step {
    bool submit = false;
    Chess::Core::Move move{};
    float after = 0.0f;
  };

  struct Lines {
    std::vector<std::string> lines;
    float y = 0, height = 0;
  };

  int _index = 0;
  const puzzles::Puzzle* _puzzle = nullptr;
  std::unique_ptr<PlayScreen> _board;
  Phase _phase = Phase::Playing;
  int _stage = 0;            // 0: White's first turn, 1: the second turn of a mate in 2 (after Black's reply)
  size_t _turnsSeen = 0;     // submitted turns of the board that were already judged
  int _hintLevel = 0;        // 0: nothing, 1: the hint text, 2: the piece to move is ringed
  std::string _message;
  Tone _tone = Tone::Info;
  float _clock = 0.0f;       // seconds in the current phase
  float _solvedClock = 0.0f; // seconds since the card appeared (the flourish)

  std::unique_ptr<puzzles::DefenceProver> _prover;
  std::unique_ptr<Chess::ai::Search> _search;
  puzzles::Turn _defence;
  std::vector<Step> _script;
  size_t _scriptPos = 0;
  float _scriptClock = 0.0f;

  Rectangle _panel{}, _card{};
  ui::Button _back = backButton();
  ui::Button _hint, _reset, _solution;
  ui::Button _next, _list; // on the success card
  Lines _step, _title, _banner, _status, _hintText;
  std::string _wrapped[5];   // the texts the Lines were wrapped from (step, title, banner, status, hint)

  void openBoard();
  void reset();
  void layout();
  void rewrap();
  std::string bannerText() const;
  std::string statusText() const;
  std::string hintTextShown() const;
  std::optional<Chess::Core::Coord> hintSquare() const;
  void applyHint();
  void judge();
  void wrong(const std::string& why);
  void solve();
  void startReply();
  void startSolution();
  void runScript(float dt);
  bool scriptDone() const { return _scriptPos >= _script.size(); }
  void setPhase(Phase phase) {
    _phase = phase;
    _clock = 0.0f;
  }
  void say(const std::string& text, Tone tone = Tone::Info) {
    _message = text;
    _tone = tone;
  }
  bool hasNext() const;
};
