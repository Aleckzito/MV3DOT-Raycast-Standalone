#include "StandaloneEngine.h"

#include "DataRoot.h"
#include "LocalChunkMesher.h"
#include "MiniVoxelGrid.h"
#include "StandaloneWorldIO.h"
#include "TerrainGenerator.h"
#include "VoxelMaterials.h"

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

// Comprueba la invalidacion por chunks con coordenadas exactas: dentro, en
// frontera y en esquina no se pueden provocar disparando a ojo. Sin ventana:
// el mesher construye nodos OSG, que no necesitan contexto GL para existir.
int selfTestChunks()
{
    using namespace rc::standalone;

    MiniVoxelGrid grid;
    LocalChunkMesher mesher;
    mesher.setGrid(&grid);

    // Bloque solido de 48x4x48: cubre varios chunks de 16.
    for (int x = 0; x < 48; ++x) {
        for (int z = 0; z < 48; ++z) {
            for (int y = 0; y < 4; ++y) {
                grid.setVoxel(x, y, z, MAT_STONE);
            }
        }
    }
    mesher.rebuildMesh();
    const size_t chunksTotal = mesher.chunkCount();
    std::cout << "[selftest] build inicial: chunks=" << chunksTotal
              << " rebuilt=" << mesher.lastRebuiltChunks()
              << " caras=" << mesher.solidFaceCount() << "\n";

    int failures = 0;
    struct Case {
        const char* name;
        int vx, vy, vz;
        size_t expected;
    };
    // lx/lz = 8 esta en el interior; 0 toca la frontera anterior en ese eje.
    const Case cases[3] = {
        { "interior       ", 24, 1, 24, 1 },
        { "frontera X     ", 32, 1, 24, 2 },
        { "esquina X/Z    ", 32, 1, 32, 3 }
    };
    for (int i = 0; i < 3; ++i) {
        const Case& c = cases[i];
        grid.setVoxel(c.vx, c.vy, c.vz, 0);  // destruir
        mesher.rebuildMesh();
        const size_t got = mesher.lastRebuiltChunks();
        const bool ok = (got == c.expected);
        if (!ok) {
            failures += 1;
        }
        std::cout << "[selftest] " << c.name
                  << " (" << c.vx << "," << c.vy << "," << c.vz << ")"
                  << " rebuilt=" << got << " esperado=" << c.expected
                  << (ok ? "  OK" : "  FALLO") << "\n";
    }

    // Bomba: varios setVoxel y UNA sola reconstruccion.
    const size_t facesBefore = mesher.solidFaceCount();
    int removed = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            grid.setVoxel(20 + dx, 3, 20 + dz, 0);
            removed += 1;
        }
    }
    const size_t dirtyVoxels = grid.dirtyVoxels().size();
    mesher.rebuildMesh();
    std::cout << "[selftest] bomba 3x3: setVoxel=" << removed
              << " dirty=" << dirtyVoxels
              << " rebuilt=" << mesher.lastRebuiltChunks()
              << " (una sola llamada)\n";
    if (dirtyVoxels != static_cast<size_t>(removed)) {
        std::cout << "[selftest] FALLO: los cambios no se acumularon\n";
        failures += 1;
    }

    // Al quitar voxels de la capa superior se exponen caras laterales de los
    // vecinos: el total de caras opacas debe subir, no bajar.
    const size_t facesAfter = mesher.solidFaceCount();
    const bool exposed = facesAfter > facesBefore;
    std::cout << "[selftest] caras expuestas: antes=" << facesBefore
              << " despues=" << facesAfter
              << (exposed ? "  OK" : "  FALLO") << "\n";
    if (!exposed) {
        failures += 1;
    }

    std::cout << "[selftest] " << (failures == 0 ? "TODO OK" : "FALLOS") << "\n";
    return failures == 0 ? 0 : 1;
}

} // namespace

// 1.3 Punto de entrada local. SDL2 + OSG. Cero sockets.
int main(int argc, char** argv)
{
    // Modo herramienta: --gen-arena [ruta relativa] [semilla]
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--gen-arena") {
        return generateArena(argc, argv);
    }
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--selftest-chunks") {
        return selfTestChunks();
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
