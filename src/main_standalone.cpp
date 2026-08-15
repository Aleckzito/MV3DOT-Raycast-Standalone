#include "StandaloneEngine.h"

#include <SDL.h>

#include <chrono>
#include <iostream>
#include <string>

// 1.3 Punto de entrada local. SDL2 + OSG. Cero sockets.
int main(int argc, char** argv)
{
    // 11. argv[1] opcional = mapa de voxeles. Sin argumento manda el meta del pack.
    std::string worldJsonPath;
    if (argc > 1 && argv[1] != nullptr) {
        worldJsonPath = argv[1];
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[standalone] SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    rc::standalone::StandaloneEngine engine;
    if (!engine.initialize(worldJsonPath)) {
        std::cerr << "[standalone] initialize failed\n";
        SDL_Quit();
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();

    while (!engine.shouldQuit()) {
        // 2.3 Poll SDL solo para QUIT. Input de camara/editor no cableado.
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                engine.requestQuit();
            }
        }

        const auto now = Clock::now();
        const float deltaTime = std::chrono::duration<float>(now - last).count();
        last = now;

        engine.update(deltaTime);
        engine.render();
    }

    engine.shutdown();
    SDL_Quit();
    return 0;
}
