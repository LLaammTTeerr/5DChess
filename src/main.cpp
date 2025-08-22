#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>
#include "Scene/SceneManager.h"
#include "gameState.h"
#include "PieceTheme.h"
// #include "Menu/MenuStructure.h"

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    SetTargetFPS(60); // Set the frame rate

    ResourceManager &resourceManager = ResourceManager::getInstance();
    GameStateModel gameState;
    SceneManager sceneManager(&gameState);
    ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());

    // TestApp app;
    // app.init();
    // app.update();
    while(!WindowShouldClose()) {
        

        // app.update();
        // app.render();
    //     // Example of using a texture
    //     Texture2D chessTexture = resourceManager.getTexture2D("chess");
    //     DrawTexture(chessTexture, 100, 100, WHITE);

        sceneManager.update(GetFrameTime());
       
        BeginDrawing();
        ClearBackground(RAYWHITE);
       
        sceneManager.render();

        EndDrawing();
    }
}