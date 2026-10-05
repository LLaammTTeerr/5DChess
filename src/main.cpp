#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include "Scene/SceneManager.h"
#include "gameState.h"
#include "PieceTheme.h"
#include "Render/UITheme.h"
#include "Audio/AudioManager.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {
// File-scope so the per-frame function can be handed to emscripten_set_main_loop.
ResourceManager *g_resourceManager = nullptr;
SceneManager *g_sceneManager = nullptr;

// One iteration of the game loop: the single place for per-frame work.
void UpdateDrawFrame() {
    // FDCHESS_PERF=1: print average / worst frame time (CPU side, excludes vsync wait) every 300 frames
    static const bool perf = std::getenv("FDCHESS_PERF") != nullptr;
    static int frames = 0; static double sum = 0, worst = 0;
    const double t0 = perf ? GetTime() : 0.0;
    AudioManager::instance().update();
    g_sceneManager->update(GetFrameTime());

    BeginDrawing();
    ClearBackground(UI::Color::bg);

    g_sceneManager->render();

    const double cpuMs = perf ? (GetTime() - t0) * 1000.0 : 0.0; // before EndDrawing: it sleeps to hold the target FPS
    EndDrawing();
    if (perf) {
        const double ms = cpuMs;
        sum += ms; worst = std::max(worst, ms);
        if (++frames == 300) {
            std::cout << "[perf] 300 frames: avg " << sum / frames << " ms, worst " << worst << " ms" << std::endl;
            frames = 0; sum = 0; worst = 0;
        }
    }
}
} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    SetExitKey(KEY_NULL); // ESC toggles the in-game menu; don't let raylib close the window
#ifndef __EMSCRIPTEN__
    // Resolve relative "assets/..." paths next to the binary regardless of launch directory
    ChangeDirectory(GetApplicationDirectory());
#endif
    SetTargetFPS(60); // Set the frame rate

    AudioManager::instance().init();

    ResourceManager &resourceManager = ResourceManager::getInstance();
    {  // scope: the SceneManager (render texture, scenes) must be destroyed while the GL context is alive
        GameStateModel gameState;
        SceneManager sceneManager(&gameState);
        ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());
        g_resourceManager = &resourceManager;
        g_sceneManager = &sceneManager;

#ifdef __EMSCRIPTEN__
        // The browser drives the loop; this call never returns (simulate_infinite_loop = 1),
        // so main()'s locals stay alive for the page's lifetime.
        emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
        while(!WindowShouldClose() && !SceneManager::isQuitRequested()) {
            UpdateDrawFrame();
        }
#endif
        g_sceneManager = nullptr;
    }

    // Release GPU resources while the GL context is still alive
    UI::Fonts::unloadAll();
    resourceManager.unloadAll();
    AudioManager::instance().shutdown(); // before CloseWindow
    CloseWindow();
    return 0;
}
