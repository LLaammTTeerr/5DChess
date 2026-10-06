// Developer tool: renders a real game through the PlayScreen
// with a chosen piece theme and saves a screenshot, then exits.
//
//   theme_preview <Pixel|Medieval|Bauhaus|Neon|Origami|Ink> <game mode> <output.png> [turns] [options]
//
// Game mode: "Standard", "Battle", "Invasion", "Fragment", or any in-game mode name.
// With turns > 0 (default 2) a few scripted legal moves are played first, preferring
// moves that jump between boards so extra timelines appear in multi-timeline modes.
//
// Options (motion tooling):
//   --demo same|cross   play one animated move through the game screen (select, move, submit) after the
//                       scripted turns; "cross" prefers a board-to-board / time-travel move
//   --dump DIR          write the animated frames as DIR/0000.png ... at 30 fps of simulated time
//   --perf [N]          after the scripted turns render N (default 600) frames with the animation running and
//                       print average / p99 / max frame time (CPU render cost, no vsync)
//   --reduce            enable Reduce motion
//   --view NAME         board view: "Deep space", "Atlas" (default) or "Blueprint"
#include <raylib.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <exception>
#include "chess.h"
#include "App.h"
#include "engine/GameCatalog.h"
#include "PieceTheme.h"
#include "Input.h"
#include "TestMode.h"
#include "Render/UITheme.h"
#include "Screens/PlayScreen.h"
#include "Render/Motion.h"

namespace {

// A catalog id, a catalog title, or one of the short names "Standard", "Battle", "Invasion", "Fragment".
std::shared_ptr<Chess::IGame> makeGame(const std::string& mode) {
  using namespace Chess;
  static const std::pair<const char*, const char*> aliases[] = {
      {"Standard", "standard"}, {"Battle", "timeline-battle"}, {"Invasion", "timeline-invasion"}, {"Fragment", "timeline-fragment"}};
  for (const auto& [alias, id] : aliases)
    if (mode == alias) return GameCatalog::create(id);
  if (const ModeInfo* info = GameCatalog::findByTitle(mode)) return GameCatalog::create(info->id);
  return GameCatalog::create(mode);
}

// Finds a legal move on any moveable board without applying it. Cross-board moves win when preferCross.
bool findScriptedMove(const Chess::IGame& game, bool preferCross, Chess::Core::Move& best) {
  using namespace Chess;
  bool haveBest = false, cross = false;
  auto boards = game.mandatoryBoards();
  if (boards.empty()) boards = game.getMoveableBoards();
  for (const auto& board : boards) {
    for (int y = 0; y < game.dim() && !cross; ++y) {
      for (int x = 0; x < game.dim() && !cross; ++x) {
        const Core::Coord from{static_cast<int8_t>(x), static_cast<int8_t>(y), static_cast<int16_t>(board->halfTurnNumber()),
                               static_cast<int16_t>(board->timeLineId())};
        for (const Core::Move& move : game.legalMovesFrom(from)) {
          const auto target = game.board(move.to.l, move.to.t).at(Position2D(move.to.x, move.to.y));
          if (target && target->type == PieceType::King) continue;  // keep the game running
          const bool sameBoard = move.to.l == from.l && move.to.t == from.t;
          if (!haveBest || !sameBoard) { best = move; haveBest = true; }
          if (preferCross && !sameBoard) { cross = true; break; }
        }
      }
    }
    if (haveBest && (cross || !preferCross)) break;
  }
  return haveBest;
}

// Plays one full turn: every mandatory board gets one move; cross-board moves win.
bool playScriptedTurn(Chess::IGame& game) {
  using namespace Chess;
  for (int guard = 0; !game.mandatoryBoards().empty() && guard < 16; ++guard) {
    Core::Move move;
    if (!findScriptedMove(game, true, move)) break;
    game.makeMove(move);
  }
  if (game.canSubmit()) {
    game.submitTurn();
    game.resolveResult(2000000);  // the result is decided by a search that only runs on demand
    return game.result() == GameResult::Ongoing;
  }
  while (game.undoable()) game.undo();  // an illegal scripted turn: leave the game untouched
  return false;
}

struct FrameClock {
  std::vector<double> ms;
  void add(double v) { ms.push_back(v); }
  void report(const char* label) {
    if (ms.empty()) return;
    std::vector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0; for (double v : ms) sum += v;
    std::cout << label << ": frames=" << ms.size() << " avg=" << sum / ms.size() << " ms  p99="
              << sorted[static_cast<size_t>(sorted.size() * 0.99)] << " ms  max=" << sorted.back() << " ms\n";
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: theme_preview <Pixel|Medieval|Bauhaus|Neon|Origami|Ink> <Standard|Battle|Invasion|Fragment> <out.png> [turns]"
                 " [--demo same|cross] [--dump DIR] [--perf [N]] [--reduce] [--view NAME]\n";
    return 2;
  }
  const std::string theme = argv[1], mode = argv[2];
  std::string out = argv[3];
  int turns = 2;
  std::string demo, dumpDir;
  int perfFrames = 0;
  bool reduce = false;
  std::string view;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--demo") && i + 1 < argc) demo = argv[++i];
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dumpDir = argv[++i];
    else if (!std::strcmp(argv[i], "--perf")) { perfFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? std::atoi(argv[++i]) : 600; }
    else if (!std::strcmp(argv[i], "--reduce")) reduce = true;
    else if (!std::strcmp(argv[i], "--view") && i + 1 < argc) view = argv[++i];
    else if (argv[i][0] != '-') turns = std::atoi(argv[i]);
  }

  // Make the output paths absolute before we change into the asset directory.
  auto absolute = [](std::string p) {
    if (!p.empty() && p[0] != '/' && (p.size() < 2 || p[1] != ':')) p = std::string(GetWorkingDirectory()) + "/" + p;
    return p;
  };
  out = absolute(out);
  if (!dumpDir.empty()) dumpDir = absolute(dumpDir);

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1400, 800, "theme_preview");
  ChangeDirectory(GetApplicationDirectory());  // assets are copied next to the binary
  TestMode::get().audioDisabled = true;  // a screenshot tool: never open an audio device
  TestMode::get().reduceMotion = reduce;
  TestMode::get().active = true;  // scripted (idle) input: the real pointer must not hover a square in the shots
  Input::scripted().position = {-100000.0f, -100000.0f};  // ... nor the scripted one

  int rc = 0;
  {
    App app;
    if (const PieceTheme* t = Themes::byName(theme)) app.themes.setTheme(*t);
    else { std::cerr << "unknown theme: " << theme << "\n"; rc = 2; }
    if (!view.empty() && !boardview::fromName(view, app.settings.boardView)) { std::cerr << "unknown board view: " << view << "\n"; rc = 2; }

    auto game = rc == 0 ? makeGame(mode) : nullptr;
    if (rc == 0 && !game) { std::cerr << "unknown game mode: " << mode << "\n"; rc = 2; }

    if (rc == 0) {
      for (int i = 0; i < turns; ++i)
        if (!playScriptedTurn(*game)) break;
      std::cout << "timelines: " << game->timeLineCount() << ", half-turn: " << game->presentHalfTurn() << "\n";

      PlayScreen screen(game);
      const float dt = 1.0f / 60.0f;
      double updateMs = 0, renderMs = 0; int splitFrames = 0;
      auto frame = [&](bool dump, int index) {
        const auto a0 = std::chrono::steady_clock::now();
        screen.update(app, dt);
        const auto a1 = std::chrono::steady_clock::now();
        BeginDrawing();
        ClearBackground(UI::Color::bg);
        screen.draw(app);
        const auto a2 = std::chrono::steady_clock::now();
        EndDrawing();
        updateMs += std::chrono::duration<double, std::milli>(a1 - a0).count();
        renderMs += std::chrono::duration<double, std::milli>(a2 - a1).count();
        ++splitFrames;
        if (dump) {
          Image shot = LoadImageFromScreen();
          char name[64];
          std::snprintf(name, sizeof name, "/%04d.png", index);
          ExportImage(shot, (dumpDir + name).c_str());
          UnloadImage(shot);
        }
      };
      // Let the camera settle exactly like it would in the running game.
      for (int i = 0; i < 400; ++i) frame(false, 0);

      if (!demo.empty() || perfFrames > 0) {
        using namespace Chess;
        Core::Move move;
        const bool wantCross = demo != "same";
        if (!findScriptedMove(*game, wantCross, move)) { std::cerr << "no legal move for the demo\n"; rc = 1; }
        else {
          const bool cross = move.from.l != move.to.l || move.from.t != move.to.t;
          std::cout << "demo move: timeline " << move.from.l << " -> " << move.to.l
                    << (cross ? " (cross-board)" : " (same board)") << "\n";
          FrameClock idle, anim;
          int n = 0;
          const bool dumping = !dumpDir.empty();
          auto run = [&](float seconds, FrameClock& clock) {
            const int frames = static_cast<int>(seconds * 60.0f);
            for (int i = 0; i < frames; ++i, ++n) {
              const auto t0 = std::chrono::steady_clock::now();
              frame(dumping && (n % 2 == 0), n / 2);
              clock.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
          };
          run(0.8f, idle);                              // idle
          screen.click(move.from);                      // pick up the piece: lift + dots pop
          run(1.3f, anim);
          screen.click(move.to);                        // move: piece travels, new board(s) grow, arrow draws
          run(1.8f, anim);
          // Finish the turn with scripted moves on any other boards, then submit: turn banner
          for (int guard = 0; !game->mandatoryBoards().empty() && guard < 16; ++guard) {
            Core::Move next;
            if (!findScriptedMove(*game, wantCross, next)) break;
            screen.click(next.from);
            run(0.5f, anim);
            screen.click(next.to);
            run(0.9f, anim);
          }
          if (game->mandatoryBoards().empty() && game->canSubmit()) screen.submit();
          else std::cerr << "demo: could not submit the turn (mandatory boards left or turn illegal)\n";
          run(1.6f, anim);
          if (perfFrames > 0) {
            // A busy stretch: keep re-triggering selection + moves is out of scope, so just measure the steady state
            FrameClock steady;
            for (int i = 0; i < perfFrames; ++i) {
              const auto t0 = std::chrono::steady_clock::now();
              frame(false, 0);
              steady.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
            steady.report("steady ");
            std::cout << "cpu split (all frames): update " << updateMs / splitFrames << " ms, render calls " << renderMs / splitFrames << " ms\n";
          }
          idle.report("idle   ");
          anim.report("animated");
        }
      }
      // TakeScreenshot() writes under the (platform dependent) base path, so export explicitly.
      Image shot = LoadImageFromScreen();
      if (!ExportImage(shot, out.c_str())) { std::cerr << "failed to write " << out << "\n"; rc = 1; }
      else std::cout << "wrote " << out << "\n";
      UnloadImage(shot);
    }
  }
  CloseWindow();
  return rc;
}
