#include "ResourceManager.h"
#include <raylib.h>
#include <iostream>
#include <filesystem>

int main() {
    // Initialize the window before using ResourceManager
    InitWindow(1400, 800, "5D Chess Game");
    ResourceManager &resourceManager = ResourceManager::getInstance();

    // while(!WindowShouldClose()) {
    //     BeginDrawing();
    //     ClearBackground(RAYWHITE);

    //     // Example of using a texture
    //     Texture2D chessTexture = resourceManager.getTexture2D("chess");
    //     DrawTexture(chessTexture, 100, 100, WHITE);

    //     EndDrawing();
    // }
}