#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>
#include "Scene/SceneManager.h"
#include "gameState.h"
#include "PieceTheme.h"
#include "Render/UITheme.h"
#include "Audio/AudioManager.h"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    SetExitKey(KEY_NULL); // ESC toggles the in-game menu; don't let raylib close the window
    // Resolve relative "assets/..." paths next to the binary regardless of launch directory
    ChangeDirectory(GetApplicationDirectory());
    SetTargetFPS(60); // Set the frame rate

    AudioManager::instance().init();

    ResourceManager &resourceManager = ResourceManager::getInstance();
    GameStateModel gameState;
    SceneManager sceneManager(&gameState);
    ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());

    while(!WindowShouldClose() && !SceneManager::isQuitRequested()) {
        AudioManager::instance().update();
        sceneManager.update(GetFrameTime());
       
        BeginDrawing();
        ClearBackground(UI::Color::bg);
       
        sceneManager.render();

        EndDrawing();
    }

    // Release GPU resources while the GL context is still alive
    UI::Fonts::unloadAll();
    resourceManager.unloadAll();
    AudioManager::instance().shutdown(); // before CloseWindow
    CloseWindow();
    return 0;
}
