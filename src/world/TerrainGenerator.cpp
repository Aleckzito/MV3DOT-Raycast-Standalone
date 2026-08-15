#include "TerrainGenerator.h"

#include "VoxelMaterials.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace rc {
namespace standalone {

namespace {

// Hash entero determinista. No se usa <random>: sus distribuciones no estan
// garantizadas entre implementaciones y el mapa debe salir identico siempre.
uint32_t hash2(int x, int z, uint32_t seed)
{
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(x) * 0x8da6b343u;
    h ^= static_cast<uint32_t>(z) * 0xd8163841u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

float unitNoise(int x, int z, uint32_t seed)
{
    return static_cast<float>(hash2(x, z, seed) & 0xffffu) / 65535.0f;
}

float smoothStep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// Value noise bilineal. Suficiente para lomas suaves y totalmente reproducible.
float valueNoise(float x, float z, uint32_t seed)
{
    const int xi = static_cast<int>(std::floor(x));
    const int zi = static_cast<int>(std::floor(z));
    const float tx = smoothStep(x - static_cast<float>(xi));
    const float tz = smoothStep(z - static_cast<float>(zi));

    const float n00 = unitNoise(xi, zi, seed);
    const float n10 = unitNoise(xi + 1, zi, seed);
    const float n01 = unitNoise(xi, zi + 1, seed);
    const float n11 = unitNoise(xi + 1, zi + 1, seed);

    const float a = n00 + (n10 - n00) * tx;
    const float b = n01 + (n11 - n01) * tx;
    return a + (b - a) * tz;
}

// Eje del rio: serpentea en X segun avanza en Z.
float riverAxisX(int vz, const ArenaSpec& spec)
{
    const float t = static_cast<float>(vz) / static_cast<float>(spec.sizeZ);
    const float wide = std::sin(t * 3.14159265f * 1.5f) * (spec.sizeX * 0.18f);
    const float wobble = (valueNoise(t * 6.0f, 0.0f, spec.seed ^ 0x9e3779b9u) - 0.5f)
                         * (spec.sizeX * 0.10f);
    return static_cast<float>(spec.sizeX) * 0.5f + wide + wobble;
}

} // namespace

void generateArena(const ArenaSpec& spec, MiniVoxelGrid& grid, ArenaLayout& layout)
{
    grid.clear();
    layout.spawns.clear();

    const float cx = static_cast<float>(spec.sizeX) * 0.5f;
    const float cz = static_cast<float>(spec.sizeZ) * 0.5f;
    const int waterLevel = spec.baseHeight - 1;

    for (int vx = 0; vx < spec.sizeX; ++vx) {
        for (int vz = 0; vz < spec.sizeZ; ++vz) {
            // Relieve suave.
            const float n = valueNoise(static_cast<float>(vx) * 0.06f,
                                       static_cast<float>(vz) * 0.06f, spec.seed);
            int height = spec.baseHeight +
                         static_cast<int>(n * static_cast<float>(spec.hillAmplitude));

            // Safe zone: se aplana para que el jugador caiga en suelo firme.
            const float dx = static_cast<float>(vx) - cx;
            const float dz = static_cast<float>(vz) - cz;
            const float distCenter = std::sqrt(dx * dx + dz * dz);
            const bool safe = distCenter <= static_cast<float>(spec.safeRadius);
            if (safe) {
                height = spec.baseHeight;
            }

            // Rio: la safe zone tiene prioridad, el cauce la rodea.
            const float axis = riverAxisX(vz, spec);
            const float distRiver = std::fabs(static_cast<float>(vx) - axis);
            const bool inRiver = !safe &&
                                 distRiver <= static_cast<float>(spec.riverHalfWidth);
            const bool inBank = !safe && !inRiver &&
                                distRiver <= static_cast<float>(spec.riverHalfWidth +
                                                                spec.bankWidth);

            int surface = height;
            if (inRiver) {
                // Cauce tallado de verdad: el lecho baja bajo el nivel del agua.
                surface = waterLevel - spec.riverDepth;
                if (surface < 0) {
                    surface = 0;
                }
            }

            // Columna solida hasta la superficie.
            for (int vy = 0; vy <= surface; ++vy) {
                uint16_t mat = MAT_EARTH;
                if (vy == surface) {
                    if (inRiver) {
                        mat = MAT_EARTH;      // lecho
                    } else if (inBank) {
                        mat = MAT_SAND;       // ribera
                    } else {
                        mat = MAT_GRASS;
                    }
                }
                grid.setVoxel(vx, vy, vz, mat);
            }

            // Agua desde el lecho hasta la lamina.
            if (inRiver) {
                for (int vy = surface + 1; vy <= waterLevel; ++vy) {
                    grid.setVoxel(vx, vy, vz, MAT_WATER);
                }
            }
        }
    }

    // Jugador en el centro, sobre la meseta de la safe zone.
    layout.playerX = cx * MINI_VOXEL_SIZE;
    layout.playerY = static_cast<float>(spec.baseHeight + 2) * MINI_VOXEL_SIZE;
    layout.playerZ = cz * MINI_VOXEL_SIZE;

    // Anillo perimetral. Solo kinds que spawnSandboxActors ya entiende.
    const char* kinds[3] = { "archer", "bat", "crawler" };
    const int count = 12;
    for (int i = 0; i < count; ++i) {
        const float ang = (6.2831853f * static_cast<float>(i)) / static_cast<float>(count);
        const float rx = cx + std::cos(ang) * static_cast<float>(spec.spawnRingRadius);
        const float rz = cz + std::sin(ang) * static_cast<float>(spec.spawnRingRadius);
        if (rx < 0.0f || rz < 0.0f ||
            rx >= static_cast<float>(spec.sizeX) || rz >= static_cast<float>(spec.sizeZ)) {
            continue;
        }

        ArenaSpawn sp;
        sp.kind = kinds[i % 3];
        sp.x = rx * MINI_VOXEL_SIZE;
        sp.z = rz * MINI_VOXEL_SIZE;
        // Los murcielagos vuelan; el resto va a ras de suelo.
        sp.y = (sp.kind == "bat")
                   ? static_cast<float>(spec.baseHeight + 8) * MINI_VOXEL_SIZE
                   : 0.0f;
        layout.spawns.push_back(sp);
    }
}

bool saveArenaMeta(const ArenaLayout& layout, const std::string& worldRelative,
                   const std::string& metaPath)
{
    nlohmann::json doc;
    doc["schema"] = "otraycast.world.sandbox_meta.v1";
    doc["version"] = 1;
    doc["packId"] = "otr.pack.sandbox";
    doc["voxelWorld"] = worldRelative;

    nlohmann::json player;
    player["x"] = layout.playerX;
    player["y"] = layout.playerY;
    player["z"] = layout.playerZ;
    doc["playerSpawn"] = player;

    nlohmann::json spawns = nlohmann::json::array();
    for (size_t i = 0; i < layout.spawns.size(); ++i) {
        nlohmann::json row;
        row["kind"] = layout.spawns[i].kind;
        row["x"] = layout.spawns[i].x;
        row["y"] = layout.spawns[i].y;
        row["z"] = layout.spawns[i].z;
        spawns.push_back(row);
    }
    doc["spawns"] = spawns;

    const std::filesystem::path out(metaPath);
    if (out.has_parent_path()) {
        std::filesystem::create_directories(out.parent_path());
    }
    std::ofstream file(out, std::ios::binary);
    if (!file) {
        std::cerr << "[arena] no se pudo escribir " << metaPath << "\n";
        return false;
    }
    file << doc.dump(2) << "\n";
    std::cout << "[arena] meta -> " << metaPath
              << " (spawns=" << layout.spawns.size() << ")\n";
    return true;
}

} // namespace standalone
} // namespace rc
