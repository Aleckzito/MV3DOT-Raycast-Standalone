#ifndef RC_TERRAIN_GENERATOR_H
#define RC_TERRAIN_GENERATOR_H

#include "MiniVoxelGrid.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rc {
namespace standalone {

// Arena procedural determinista. Misma semilla => mismo mapa, siempre.
//
// No toca el pipeline: produce un MiniVoxelGrid que se vuelca a JSON con el
// saveWorld de siempre, y el motor lo carga por la ruta normal (argv[1] o
// voxelWorld del meta). El generador no se ejecuta en tiempo de juego.
struct ArenaSpec {
    int sizeX = 120;            // mini-voxels (120 = 40 tiles)
    int sizeZ = 120;
    uint32_t seed = 1337u;

    int baseHeight = 3;         // altura del terreno en mini-voxels
    int hillAmplitude = 4;      // relieve sobre la base

    int riverHalfWidth = 5;     // semiancho del cauce
    int riverDepth = 3;         // profundidad tallada bajo la orilla
    int bankWidth = 3;          // ribera de arena a cada lado

    int safeRadius = 20;        // radio aplanado alrededor del centro
    int spawnRingRadius = 48;   // anillo de enemigos, en mini-voxels
};

struct ArenaSpawn {
    std::string kind;           // archer | bat | crawler
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ArenaLayout {
    float playerX = 0.0f;       // coordenadas de mundo, no de voxel
    float playerY = 0.0f;
    float playerZ = 0.0f;
    std::vector<ArenaSpawn> spawns;
};

// Rellena grid y layout. El grid se limpia antes de generar.
void generateArena(const ArenaSpec& spec, MiniVoxelGrid& grid, ArenaLayout& layout);

// Escribe el meta del pack junto al mapa: voxelWorld, playerSpawn y spawns.
bool saveArenaMeta(const ArenaLayout& layout, const std::string& worldRelative,
                   const std::string& metaPath);

} // namespace standalone
} // namespace rc

#endif
