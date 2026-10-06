#include <raylib.h>
#include "App.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {
// File-scope so the per-frame function can be handed to emscripten_set_main_loop.
App* g_app = nullptr;

void UpdateDrawFrame() { g_app->frame(); }
} // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1400, 800, "5D Chess Game");
    SetExitKey(KEY_NULL); // ESC toggles the in-game menu; don't let raylib close the window
#ifndef __EMSCRIPTEN__
    // Resolve relative "assets/..." paths next to the binary regardless of launch directory
    ChangeDirectory(GetApplicationDirectory());
#endif
    SetTargetFPS(60); // Set the frame rate

    {   // scope: the App (assets, audio device, scenes) must be destroyed while the window and GL context are alive
        App app;
        g_app = &app;
#ifdef __EMSCRIPTEN__
        // The browser drives the loop; this call never returns (simulate_infinite_loop = 1),
        // so main()'s locals stay alive for the page's lifetime.
        emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
        while (!WindowShouldClose() && !app.quit) {
            UpdateDrawFrame();
        }
#endif
        g_app = nullptr;
    }
    CloseWindow();
    return 0;
}
