// Developer tool: renders a real game through ChessModel/ChessView/ChessController
// with a chosen piece theme and saves a screenshot, then exits.
//
//   theme_preview <Classic|Modern|Fantasy|Pixel> <game mode> <output.png> [turns]
//
// Game mode: "Standard", "Battle", "Invasion", "Fragment", or any in-game mode name.
// With turns > 0 (default 4) a few scripted legal moves are played first, preferring
// moves that jump between boards so extra timelines appear in multi-timeline modes.
#include <raylib.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <exception>
#include "chess.h"
#include "PieceTheme.h"
#include "ResourceManager.h"
#include "Render/View.h"
#include "Render/UITheme.h"
#include "Render/Controller.h"
#include "Render/RenModel.h"

namespace {

std::shared_ptr<Chess::IGame> makeGame(const std::string& mode) {
  using namespace Chess;
  if (mode == "Standard" || mode == NameOfGame<StandardGame>::value) return createGame<StandardGame>();
  if (mode == "Battle" || mode == NameOfGame<MiscGameTimeLineBattle>::value) return createGame<MiscGameTimeLineBattle>();
  if (mode == "Invasion" || mode == NameOfGame<MiscGameTimeLineInvasion>::value) return createGame<MiscGameTimeLineInvasion>();
  if (mode == "Fragment" || mode == NameOfGame<MiscGameTimeLineFragment>::value) return createGame<MiscGameTimeLineFragment>();
  return nullptr;
}

// Plays one full turn: every moveable board gets one move; cross-board moves win.
bool playScriptedTurn(Chess::IGame& game) {
  using namespace Chess;
  for (int guard = 0; !game.getMoveableBoards().empty() && guard < 16; ++guard) {
    SelectedPosition bestFrom, bestTo;
    bool haveBest = false, cross = false;
    for (const auto& board : game.getMoveableBoards()) {
      for (int y = 0; y < game.dim() && !cross; ++y) {
        for (int x = 0; x < game.dim() && !cross; ++x) {
          SelectedPosition from(board, Position2D(x, y));
          std::vector<SelectedPosition> targets;
          try { targets = game.getMoveablePositions(from); } catch (const std::exception&) { continue; }
          for (const auto& to : targets) {
            if (std::dynamic_pointer_cast<King>(to.board->getPiece(to.position))) continue;  // keep the game running
            if (!haveBest || to.board != board) { bestFrom = from; bestTo = to; haveBest = true; }
            if (to.board != board) { cross = true; break; }
          }
        }
      }
      if (haveBest) break;
    }
    if (!haveBest) break;
    game.makeMove({bestFrom, bestTo});
    if (game.gameEnd()) { game.undo(); break; }  // safety net: never end the game in a preview
  }
  if (game.undoable()) game.submitTurn();
  return !game.gameEnd();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: theme_preview <Classic|Modern|Fantasy|Pixel> <Standard|Battle|Invasion|Fragment> <out.png> [turns]\n";
    return 2;
  }
  const std::string theme = argv[1], mode = argv[2];
  std::string out = argv[3];
  const int turns = argc > 4 ? std::atoi(argv[4]) : 2;

  // Make the output path absolute before we change into the asset directory.
  if (!IsPathFile(GetWorkingDirectory()) && !out.empty() && out[0] != '/' && (out.size() < 2 || out[1] != ':'))
    out = std::string(GetWorkingDirectory()) + "/" + out;

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1400, 800, "theme_preview");
  ChangeDirectory(GetApplicationDirectory());  // assets are copied next to the binary

  int rc = 0;
  {
    ResourceManager& resources = ResourceManager::getInstance();
    ThemeManager& themes = ThemeManager::getInstance();
    if (theme == "Classic") themes.setTheme(std::make_unique<ClassicTheme>());
    else if (theme == "Modern") themes.setTheme(std::make_unique<ModernTheme>());
    else if (theme == "Fantasy") themes.setTheme(std::make_unique<Modern2Theme>());
    else if (theme == "Pixel") themes.setTheme(std::make_unique<PixelTheme>());
    else { std::cerr << "unknown theme: " << theme << "\n"; rc = 2; }

    auto game = rc == 0 ? makeGame(mode) : nullptr;
    if (rc == 0 && !game) { std::cerr << "unknown game mode: " << mode << "\n"; rc = 2; }

    if (rc == 0) {
      for (int i = 0; i < turns; ++i)
        if (!playScriptedTurn(*game)) break;
      std::cout << "timelines: " << game->getTimeLines().size() << ", half-turn: " << game->presentHalfTurn() << "\n";

      ChessModel model(game);
      ChessView view(Vector3{5000, 5000, 1});
      ChessController controller(model, view);
      controller.update(0.0f);
      // Let the camera settle exactly like it would in the running game.
      for (int frame = 0; frame < 400; ++frame) {
        controller.update(1.0f / 60.0f);
        view.update(1.0f / 60.0f);
        BeginDrawing();
        ClearBackground(UI::Color::bg);
        controller.render();
        EndDrawing();
      }
      // TakeScreenshot() writes under the (platform dependent) base path, so export explicitly.
      Image shot = LoadImageFromScreen();
      if (!ExportImage(shot, out.c_str())) { std::cerr << "failed to write " << out << "\n"; rc = 1; }
      else std::cout << "wrote " << out << "\n";
      UnloadImage(shot);
    }
    UI::Fonts::unloadAll();
    resources.unloadAll();
  }
  CloseWindow();
  return rc;
}
