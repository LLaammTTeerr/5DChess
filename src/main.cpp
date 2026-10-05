#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>
#include "Scene/SceneManager.h"
#include "gameState.h"
#include "PieceTheme.h"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    SetExitKey(KEY_NULL); // ESC toggles the in-game menu; don't let raylib close the window
    // Resolve relative "assets/..." paths next to the binary regardless of launch directory
    ChangeDirectory(GetApplicationDirectory());
    SetTargetFPS(60); // Set the frame rate

    ResourceManager &resourceManager = ResourceManager::getInstance();
    GameStateModel gameState;
    SceneManager sceneManager(&gameState);
    ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());

    while(!WindowShouldClose() && !SceneManager::isQuitRequested()) {
        sceneManager.update(GetFrameTime());
       
        BeginDrawing();
        ClearBackground(RAYWHITE);
       
        sceneManager.render();

        EndDrawing();
    }

    // Release GPU resources while the GL context is still alive
    resourceManager.unloadAll();
    CloseWindow();
    return 0;
}
