#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>

#include "Menu/MenuStructure.h"

int main() {
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    SetTargetFPS(60); // Set the frame rate
    // ResourceManager &resourceManager = ResourceManager::getInstance();

    TestApp app;
    app.init();
    // app.update();
    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        app.update();
        app.render();
    //     // Example of using a texture
    //     Texture2D chessTexture = resourceManager.getTexture2D("chess");
    //     DrawTexture(chessTexture, 100, 100, WHITE);

        EndDrawing();
    }
}