#include "StandaloneEngine.h"

#include "DataRoot.h"
#include "MiniVoxelGrid.h"
#include "StandaloneWorldIO.h"
#include "TerrainGenerator.h"

#include <SDL.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// Genera la arena y sale. Sin SDL ni ventana: es una herramienta, no una partida.
// El motor cargara el JSON despues, por la ruta de siempre.
int generateArena(int argc, char** argv)
{
    rc::standalone::ArenaSpec spec;
    std::string worldRelative = "data/worlds/arena_120.json";

    if (argc > 2 && argv[2] != nullptr) {
        worldRelative = argv[2];
    }
    if (argc > 3 && argv[3] != nullptr) {
        spec.seed = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
    }

    rc::standalone::MiniVoxelGrid grid;
    rc::standalone::ArenaLayout layout;
    rc::standalone::generateArena(spec, grid, layout);

    const std::string worldPath = rc::standalone::dataPath(worldRelative);
    if (!rc::standalone::saveWorld(&grid, worldPath)) {
        std::cerr << "[arena] no se pudo escribir " << worldPath << "\n";
        return 1;
    }

    std::string metaRelative = worldRelative;
    const size_t dot = metaRelative.rfind('.');
    if (dot != std::string::npos) {
        metaRelative = metaRelative.substr(0, dot);
    }
    metaRelative += ".meta.json";

    if (!rc::standalone::saveArenaMeta(layout, worldRelative,
                                       rc::standalone::dataPath(metaRelative))) {
        return 1;
    }

    std::cout << "[arena] semilla=" << spec.seed
              << " " << spec.sizeX << "x" << spec.sizeZ << " mini-voxels"
              << " (" << (spec.sizeX / 3) << "x" << (spec.sizeZ / 3) << " tiles)\n";
    return 0;
}

} // namespace

// 1.3 Punto de entrada local. SDL2 + OSG. Cero sockets.
int main(int argc, char** argv)
{
    // Modo herramienta: --gen-arena [ruta relativa] [semilla]
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--gen-arena") {
        return generateArena(argc, argv);
    }

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
