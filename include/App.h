#pragma once
#include "Audio/AudioManager.h"
#include "Render/PieceTheme.h"
#include "services/Assets.h"
#include "services/Settings.h"
#include <functional>
#include <memory>

class GameStateModel;
class SceneManager;

// The application context: owns every service and the scenes. Create it after InitWindow() and destroy it
// before CloseWindow(). Members are declared in dependency order and destroyed in reverse, so teardown is
// correct by construction: scenes first (they hold textures), then the game state, theme cache and Assets
// (GPU resources, sounds and music streams), and only then the audio device and the settings.
struct App {
private:
  // Makes App::current() valid for the whole life of the members below (set first, cleared last).
  struct Registration { explicit Registration(App* a); ~Registration(); } _registration{this};

public:
  Settings settings;
  AudioManager audio{settings};  // closes the audio device: must outlive `assets`
  Assets assets;
  ThemeManager themes{assets, settings};
  std::unique_ptr<GameStateModel> gameState;
  std::unique_ptr<SceneManager> scenes;
  bool quit = false;             // set by the Exit menu item, honoured by the main loop

  App();
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  // One iteration of the game loop: the single place for per-frame work. Shared by src/main.cpp and the UI
  // test harness so both run the same update/render sequence. `beforePresent` (optional) runs after the
  // scene has been drawn and before EndDrawing(), e.g. to grab a screenshot of the finished frame.
  void frame(const std::function<void()>& beforePresent = {});

  // The running App. Screens, views and commands reach their services through it instead of being handed
  // an App& through every constructor (a follow-up can thread references through instead).
  static App& current();
};
