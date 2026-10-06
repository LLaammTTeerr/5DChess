#include "App.h"
#include "Input.h"
#include "TestMode.h"
#include "Scene/SceneManager.h"
#include "Render/UITheme.h"
#include "gameState.h"
#include "engine/GameCatalog.h"
#include <raylib.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

namespace {
App* g_current = nullptr;
}

App::Registration::Registration(App* a) { g_current = a; }
App::Registration::~Registration() { g_current = nullptr; }

App& App::current() {
    assert(g_current && "App::current() outside the lifetime of an App");
    return *g_current;
}

App::App() {
    settings.reduceMotion = TestMode::get().reduceMotion;
    audio.init(assets);
    Chess::GameCatalog::setDirectory(assets.root() + "positions"); // one assets root for everything
    gameState = std::make_unique<GameStateModel>();
    scenes = std::make_unique<SceneManager>(gameState.get());
}

App::~App() = default;

void App::frame(const std::function<void()>& beforePresent) {
    // FDCHESS_PERF=1: print average / worst frame time (CPU side, excludes vsync wait) every 300 frames
    static const bool perf = std::getenv("FDCHESS_PERF") != nullptr;
    static int frames = 0; static double sum = 0, worst = 0;
    const double t0 = perf ? GetTime() : 0.0;
    audio.update();
    scenes->update(Input::frameTime());

    BeginDrawing();
    ClearBackground(UI::Color::bg);

    scenes->render();

    const double cpuMs = perf ? (GetTime() - t0) * 1000.0 : 0.0; // before EndDrawing: it sleeps to hold the target FPS
    if (beforePresent) beforePresent();
    EndDrawing();
    TestMode& tm = TestMode::get();
    if (tm.fixedStep) tm.clock += tm.fixedDt;
    if (perf) {
        const double ms = cpuMs;
        sum += ms; worst = std::max(worst, ms);
        if (++frames == 300) {
            std::cout << "[perf] 300 frames: avg " << sum / frames << " ms, worst " << worst << " ms" << std::endl;
            frames = 0; sum = 0; worst = 0;
        }
    }
}
