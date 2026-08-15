#ifndef RAYCAST_COMMON_H
#define RAYCAST_COMMON_H

#include "world_defaults.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef ENABLE_TRACE
#include "trace_stats.h"
#endif

namespace rc {

constexpr int TILE_SIZE = 128;
constexpr int TEXTURE_SIZE = 128;
constexpr int ATLAS_GRID_COLUMNS = 16;
constexpr int SKYBOX_WIDTH = 512;
constexpr int SKYBOX_HEIGHT = 128;
constexpr int WORLD_LAYER_COUNT = 6;
constexpr int HIGHEST_CEILING_LEVEL = 3;
constexpr int DESIRED_FPS = 120;
constexpr int UPDATE_INTERVAL_MS = 1000 / DESIRED_FPS;
constexpr float TWO_PI = 6.28318530718f;
constexpr float PLANE_SCALE = 0.66f;
constexpr float MAX_JUMP_DISTANCE = 3.0f * static_cast<float>(TILE_SIZE);
constexpr float HALF_JUMP_DISTANCE = MAX_JUMP_DISTANCE / 2.0f;
constexpr float CAMERA_PROXY_TILES = 2.0f;
constexpr float PLAYER_RADIUS = 0.2f * static_cast<float>(TILE_SIZE);
constexpr float INTERACT_RAY_MAX_TILES = 2.0f;
constexpr float TURN_FACE_EAST = 10.0f;
constexpr float TURN_FACE_NORTH = 11.0f;
constexpr float TURN_FACE_WEST = 12.0f;
constexpr float TURN_FACE_SOUTH = 13.0f;

constexpr int CELL_ID_DOOR_BASE = 1000;
constexpr int CELL_ID_TRANSPARENT_BASE = 1500;
constexpr int CELL_ID_TRANSPARENT_MAX = 1999;
constexpr int SPRITE_DIRECTION_FRAMES = 4;

constexpr int CELL_ID_RAMP_MIN = 4001;
constexpr int CELL_ID_RAMP_MAX = 4005;

constexpr int CONSTRUCTION_ID_MIN = 600;
constexpr int CONSTRUCTION_ID_MAX = 659;
constexpr int CONSTRUCTION_FLOOR_ID_MAX = 619;
constexpr int STRUCTURAL_FALLBACK_ID_MIN = 500;
constexpr int STRUCTURAL_FALLBACK_ID_MAX = 750;

inline bool isConstructionFloorId(int cell)
{
    return cell >= CONSTRUCTION_ID_MIN && cell <= CONSTRUCTION_FLOOR_ID_MAX;
}

inline bool isFallbackStructuralSolid(uint16_t id)
{
    if (id == 0 || isConstructionFloorId(static_cast<int>(id))) {
        return false;
    }
    return id >= STRUCTURAL_FALLBACK_ID_MIN && id <= STRUCTURAL_FALLBACK_ID_MAX;
}

inline bool isConstructionRenderId(int cell)
{
    return cell >= CONSTRUCTION_ID_MIN && cell <= CONSTRUCTION_ID_MAX;
}

inline bool isTerrainSpriteId(int cell)
{
    return cell >= CELL_ID_RAMP_MIN && cell <= CELL_ID_RAMP_MAX;
}

constexpr uint16_t DEFAULT_BRIDGE_PORT = 7171;

// Combat-stabilization: lock authoritative movement to the current editor layer plane.
constexpr bool kStrictFlatMovement = true;

// When true, every catalog tile/wall id blocks movement except construction floors and ramps.
constexpr bool kForceTilesAndWallsSolid = true;

inline bool isPhysicallyExcludedFromSolid(uint16_t id)
{
    return id == 0 || isConstructionFloorId(static_cast<int>(id))
        || isTerrainSpriteId(static_cast<int>(id));
}

inline bool categoryImpliesForcedSolid(const std::string& category)
{
    return category == "tile" || category == "wall" || category == "wall_candidate"
        || category == "door" || category == "window";
}

constexpr unsigned int RAYCAST_COLLISION_MASK = 0x1u;
constexpr unsigned int VISUAL_SCENE_MASK = 0xFFFFFFFFu;

// Extra tiles meshed past each chunk edge to close seams between adjacent chunks.
constexpr int kChunkMeshOverlapTiles = 1;

constexpr int kBuildReachTiles = 3;
constexpr uint16_t kDefaultBuildTileId = 600;

struct BuildBrushPreset {
    uint16_t itemId = kDefaultBuildTileId;
    const char* label = "Suelo";
};

constexpr BuildBrushPreset kBuildBrushPresets[] = {
    {600, "Suelo"},
    {1294, "Muro Piedra"},
    {1533, "Muro Ladrillo"},
    {1498, "Magic Wall"},
    {5637, "Muro Tierra"},
    {1295, "Piedra Alt"},
    {601, "Construccion 601"},
    {602, "Construccion 602"},
    {603, "Construccion 603"},
};

constexpr int kBuildBrushSlotCount = static_cast<int>(sizeof(kBuildBrushPresets) / sizeof(kBuildBrushPresets[0]));

constexpr uint16_t kDefaultChunkTileWidth = 24;
constexpr uint16_t kDefaultChunkTileHeight = 24;

inline bool isDoorCell(int cell)
{
    return cell >= CELL_ID_DOOR_BASE && cell < CELL_ID_TRANSPARENT_BASE;
}

inline bool isTransparentCell(int cell)
{
    return cell >= CELL_ID_TRANSPARENT_BASE && cell <= CELL_ID_TRANSPARENT_MAX;
}

inline bool isOpaqueWallCell(int cell)
{
    return cell > 0 && !isTransparentCell(cell);
}

// 0=front 1=right 2=back 3=left relative to entity facing (GBRaycaster / OT style).
inline int spriteViewFrame(float entityDirX, float entityDirY, float camX, float camY,
                           float entityX, float entityY)
{
    const float toCamX = camX - entityX;
    const float toCamY = camY - entityY;
    const float entityAngle = std::atan2(-entityDirY, entityDirX);
    const float toCamAngle = std::atan2(-toCamY, toCamX);
    float rel = toCamAngle - entityAngle;
    while (rel > 3.14159265359f) rel -= TWO_PI;
    while (rel <= -3.14159265359f) rel += TWO_PI;

    const float quarter = 3.14159265359f / 4.0f;
    if (rel >= -quarter && rel < quarter) return 0;
    if (rel >= quarter && rel < 3.0f * quarter) return 1;
    if (rel < -quarter && rel >= -3.0f * quarter) return 3;
    return 2;
}

enum WorldLayer : uint8_t {
    LAYER_GROUND = 0,
    LAYER_WALL_L1 = 1,
    LAYER_WALL_L2 = 2,
    LAYER_SPRITES = 3,
    LAYER_FLOOR = 4,
    LAYER_CEILING = 5
};

constexpr int DOOR_ANIM_FRAMES = 60;

struct CameraState {
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float dirX = 1.0f;
    float dirY = 0.0f;
    float planeX = 0.0f;
    float planeY = 0.66f;
    float rot = 0.0f;
    uint8_t editorLayer = 0;
    bool editorLayerSynced = false;
};

struct MoveIntent {
    float forward = 0.0f;
    float strafe = 0.0f;
    float turn = 0.0f;
    float jump = 0.0f;
};

struct DoorState {
    int32_t tileX = -1;
    int32_t tileY = -1;
    bool isOpen = false;
};

struct SpawnPoint {
    float tileX = 26.0f;
    float tileY = 6.0f;
    float z = 0.0f;
    float angle = 0.0f;
};

struct WorldFloorSlice {
    std::vector<int32_t> ground;
    std::vector<int32_t> wallL1;
    std::vector<int32_t> wallL2;
    std::vector<int32_t> sprites;
    std::vector<int32_t> floor;
    std::vector<int32_t> ceiling;
};

using ChunkData = WorldFloorSlice;

struct SparseChunkEntry {
    uint16_t originX = 0;
    uint16_t originY = 0;
    uint16_t cw = 0;
    uint16_t ch = 0;
    ChunkData cells;
};

struct ChunkLayerCache {
    uint64_t lastKey = 0;
    const SparseChunkEntry* lastEntry = nullptr;

    void reset()
    {
        lastKey = 0;
        lastEntry = nullptr;
    }
};

struct WorldSnapshot {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t depth = 1;
    uint16_t chunkTileWidth = kDefaultChunkTileWidth;
    uint16_t chunkTileHeight = kDefaultChunkTileHeight;
    int surfaceZ = 7;
    uint8_t surfaceLayer = 0;
    SpawnPoint spawn;
    std::vector<int32_t> ground;
    std::vector<int32_t> wallL1;
    std::vector<int32_t> wallL2;
    std::vector<int32_t> sprites;
    std::vector<int32_t> floor;
    std::vector<int32_t> ceiling;
    std::vector<WorldFloorSlice> floors;
    std::vector<bool> doorsOpen;
    std::unordered_map<uint64_t, SparseChunkEntry> sparseChunks;
};

struct MapSprite {
    float x = 0.0f;
    float y = 0.0f;
    float dirX = 0.0f;
    float dirY = -1.0f;
    int textureId = 0;
    uint8_t floorZ = 0;
};

struct WorldChunkDesc {
    uint16_t cx = 0;
    uint16_t cy = 0;
    uint16_t cw = 0;
    uint16_t ch = 0;
    uint8_t floorZ = 0;
};

struct WorldChunkDelta {
    WorldChunkDesc desc;
    WorldFloorSlice cells;
};

inline void resizeWorldSnapshot(WorldSnapshot& world, uint16_t width, uint16_t height,
                                uint8_t depth = 1)
{
    world.width = width;
    world.height = height;
    world.depth = depth < 1 ? 1 : depth;
    world.surfaceZ = 7;
    world.surfaceLayer = 0;
    world.chunkTileWidth = kDefaultChunkTileWidth;
    world.chunkTileHeight = kDefaultChunkTileHeight;
    world.sparseChunks.clear();
    const size_t cells = static_cast<size_t>(width) * height;
    world.doorsOpen.assign(cells, false);

    if (world.depth <= 1) {
        world.floors.clear();
        world.ground.assign(cells, 0);
        world.wallL1.assign(cells, 0);
        world.wallL2.assign(cells, 0);
        world.sprites.assign(cells, 0);
        world.floor.assign(cells, 0);
        world.ceiling.assign(cells, 0);
        return;
    }

    world.ground.clear();
    world.wallL1.clear();
    world.wallL2.clear();
    world.sprites.clear();
    world.floor.clear();
    world.ceiling.clear();
    world.floors.assign(world.depth, WorldFloorSlice{});
    for (WorldFloorSlice& slice : world.floors) {
        slice.ground.assign(cells, 0);
        slice.wallL1.assign(cells, 0);
        slice.wallL2.assign(cells, 0);
        slice.sprites.assign(cells, 0);
        slice.floor.assign(cells, 0);
        slice.ceiling.assign(cells, 0);
    }
}

// surfaceZ counts down from the top editor layer index.
// surfaceZ=0 places the OTBM surface at layer (depth - 1); higher surfaceZ moves it down.
inline int computeSurfaceLayer(int surfaceZ, int depth)
{
    if (depth <= 1) return 0;
    const int clampedSurface = std::max(0, surfaceZ);
    return std::clamp((depth - 1) - clampedSurface, 0, depth - 1);
}

inline bool isSubterraneanLayer(const WorldSnapshot& world, uint8_t floorZ)
{
    return static_cast<int>(floorZ) < static_cast<int>(world.surfaceLayer);
}

inline bool isSurfaceOrAboveLayer(const WorldSnapshot& world, uint8_t floorZ)
{
    return !isSubterraneanLayer(world, floorZ);
}

inline float surfaceLayerBaseZ(const WorldSnapshot& world, float tileSize)
{
    return static_cast<float>(world.surfaceLayer) * tileSize;
}

inline WorldFloorSlice* mutableFloorSlice(WorldSnapshot& world, uint8_t floorZ)
{
    if (world.depth <= 1 || floorZ >= world.floors.size()) return nullptr;
    return &world.floors[floorZ];
}

inline const WorldFloorSlice* floorSlice(const WorldSnapshot& world, uint8_t floorZ)
{
    if (world.depth <= 1 || floorZ >= world.floors.size()) return nullptr;
    return &world.floors[floorZ];
}

inline void writeLayerRegion(const std::vector<int32_t>& src, uint16_t worldWidth,
                             uint16_t cx, uint16_t cy, uint16_t cw, uint16_t ch,
                             std::vector<int32_t>& dst)
{
    for (uint16_t ty = 0; ty < ch; ++ty) {
        for (uint16_t tx = 0; tx < cw; ++tx) {
            const size_t srcIdx = static_cast<size_t>(ty) * cw + tx;
            const size_t dstIdx =
                static_cast<size_t>(cy + ty) * worldWidth + static_cast<size_t>(cx + tx);
            if (srcIdx < src.size() && dstIdx < dst.size()) {
                dst[dstIdx] = src[srcIdx];
            }
        }
    }
}

inline void clearLayerRegion(uint16_t worldWidth, uint16_t cx, uint16_t cy, uint16_t cw,
                             uint16_t ch, std::vector<int32_t>& dst)
{
    for (uint16_t ty = 0; ty < ch; ++ty) {
        for (uint16_t tx = 0; tx < cw; ++tx) {
            const size_t dstIdx =
                static_cast<size_t>(cy + ty) * worldWidth + static_cast<size_t>(cx + tx);
            if (dstIdx < dst.size()) {
                dst[dstIdx] = 0;
            }
        }
    }
}

inline uint64_t packSparseChunkKey(uint8_t floorZ, uint16_t tileOriginX, uint16_t tileOriginY)
{
    return (static_cast<uint64_t>(floorZ) << 32)
        | (static_cast<uint64_t>(tileOriginX) << 16)
        | static_cast<uint64_t>(tileOriginY);
}

inline int readLayerCellAtIndex(const WorldFloorSlice& slice, WorldLayer layer, size_t idx)
{
    switch (layer) {
    case LAYER_GROUND:
        return idx < slice.ground.size() ? slice.ground[idx] : 0;
    case LAYER_WALL_L1:
        return idx < slice.wallL1.size() ? slice.wallL1[idx] : 0;
    case LAYER_WALL_L2:
        return idx < slice.wallL2.size() ? slice.wallL2[idx] : 0;
    case LAYER_SPRITES:
        return idx < slice.sprites.size() ? slice.sprites[idx] : 0;
    case LAYER_FLOOR:
        return idx < slice.floor.size() ? slice.floor[idx] : 0;
    case LAYER_CEILING:
        return idx < slice.ceiling.size() ? slice.ceiling[idx] : 0;
    default:
        return 0;
    }
}

inline int layerCellLegacy(const WorldSnapshot& world, WorldLayer layer, int x, int y,
                           uint8_t floorZ)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(world.width) ||
        y >= static_cast<int>(world.height)) {
        return 0;
    }

    const size_t idx = static_cast<size_t>(y) * world.width + static_cast<size_t>(x);
    if (world.depth > 1) {
        const WorldFloorSlice* slice = floorSlice(world, floorZ);
        if (slice == nullptr) {
            return 0;
        }
        return readLayerCellAtIndex(*slice, layer, idx);
    }

    switch (layer) {
    case LAYER_GROUND:
        return idx < world.ground.size() ? world.ground[idx] : 0;
    case LAYER_WALL_L1:
        return idx < world.wallL1.size() ? world.wallL1[idx] : 0;
    case LAYER_WALL_L2:
        return idx < world.wallL2.size() ? world.wallL2[idx] : 0;
    case LAYER_SPRITES:
        return idx < world.sprites.size() ? world.sprites[idx] : 0;
    case LAYER_FLOOR:
        return idx < world.floor.size() ? world.floor[idx] : 0;
    case LAYER_CEILING:
        return idx < world.ceiling.size() ? world.ceiling[idx] : 0;
    default:
        return 0;
    }
}

inline void upsertSparseChunk(WorldSnapshot& world, const WorldChunkDesc& chunk,
                              const ChunkData& cells)
{
    if (world.chunkTileWidth == 0) {
        world.chunkTileWidth = kDefaultChunkTileWidth;
    }
    if (world.chunkTileHeight == 0) {
        world.chunkTileHeight = kDefaultChunkTileHeight;
    }

    SparseChunkEntry entry;
    entry.originX = chunk.cx;
    entry.originY = chunk.cy;
    entry.cw = chunk.cw;
    entry.ch = chunk.ch;
    entry.cells = cells;
    world.sparseChunks[packSparseChunkKey(chunk.floorZ, chunk.cx, chunk.cy)] = std::move(entry);
}

inline void eraseSparseChunk(WorldSnapshot& world, const WorldChunkDesc& chunk)
{
    world.sparseChunks.erase(packSparseChunkKey(chunk.floorZ, chunk.cx, chunk.cy));
}

inline void clearWorldChunkRegion(WorldSnapshot& world, const WorldChunkDesc& chunk)
{
    if (world.width == 0 || world.height == 0) return;
    if (chunk.cx + chunk.cw > world.width || chunk.cy + chunk.ch > world.height) return;

    if (world.depth > 1) {
        WorldFloorSlice* slice = mutableFloorSlice(world, chunk.floorZ);
        if (!slice) return;
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->ground);
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->wallL1);
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->wallL2);
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->sprites);
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->floor);
        clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, slice->ceiling);
        eraseSparseChunk(world, chunk);
        return;
    }

    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.ground);
    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.wallL1);
    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.wallL2);
    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.sprites);
    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.floor);
    clearLayerRegion(world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch, world.ceiling);
    eraseSparseChunk(world, chunk);
}

inline void applyWorldChunkDelta(WorldSnapshot& world, const WorldChunkDelta& delta)
{
    const WorldChunkDesc& chunk = delta.desc;
    if (world.width == 0 || world.height == 0) return;
    if (chunk.cx + chunk.cw > world.width || chunk.cy + chunk.ch > world.height) return;

    if (world.depth > 1) {
        WorldFloorSlice* slice = mutableFloorSlice(world, chunk.floorZ);
        if (!slice) return;
        writeLayerRegion(delta.cells.ground, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->ground);
        writeLayerRegion(delta.cells.wallL1, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->wallL1);
        writeLayerRegion(delta.cells.wallL2, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->wallL2);
        writeLayerRegion(delta.cells.sprites, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->sprites);
        writeLayerRegion(delta.cells.floor, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->floor);
        writeLayerRegion(delta.cells.ceiling, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                         slice->ceiling);
        upsertSparseChunk(world, chunk, delta.cells);
        return;
    }

    writeLayerRegion(delta.cells.ground, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.ground);
    writeLayerRegion(delta.cells.wallL1, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.wallL1);
    writeLayerRegion(delta.cells.wallL2, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.wallL2);
    writeLayerRegion(delta.cells.sprites, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.sprites);
    writeLayerRegion(delta.cells.floor, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.floor);
    writeLayerRegion(delta.cells.ceiling, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                     world.ceiling);
    upsertSparseChunk(world, chunk, delta.cells);
}

inline void readLayerRegion(const std::vector<int32_t>& src, uint16_t worldWidth,
                            uint16_t cx, uint16_t cy, uint16_t cw, uint16_t ch,
                            std::vector<int32_t>& dst)
{
    dst.assign(static_cast<size_t>(cw) * ch, 0);
    for (uint16_t ty = 0; ty < ch; ++ty) {
        for (uint16_t tx = 0; tx < cw; ++tx) {
            const size_t srcIdx =
                static_cast<size_t>(cy + ty) * worldWidth + static_cast<size_t>(cx + tx);
            const size_t dstIdx = static_cast<size_t>(ty) * cw + tx;
            if (srcIdx < src.size() && dstIdx < dst.size()) {
                dst[dstIdx] = src[srcIdx];
            }
        }
    }
}

inline bool extractWorldChunkDelta(const WorldSnapshot& world, const WorldChunkDesc& chunk,
                                   WorldChunkDelta& outDelta)
{
    if (world.width == 0 || world.height == 0) return false;
    if (chunk.cx + chunk.cw > world.width || chunk.cy + chunk.ch > world.height) return false;

    outDelta.desc = chunk;
    if (world.depth > 1) {
        const WorldFloorSlice* slice = floorSlice(world, chunk.floorZ);
        if (!slice) return false;
        readLayerRegion(slice->ground, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.ground);
        readLayerRegion(slice->wallL1, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.wallL1);
        readLayerRegion(slice->wallL2, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.wallL2);
        readLayerRegion(slice->sprites, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.sprites);
        readLayerRegion(slice->floor, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.floor);
        readLayerRegion(slice->ceiling, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                        outDelta.cells.ceiling);
        return true;
    }

    readLayerRegion(world.ground, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.ground);
    readLayerRegion(world.wallL1, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.wallL1);
    readLayerRegion(world.wallL2, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.wallL2);
    readLayerRegion(world.sprites, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.sprites);
    readLayerRegion(world.floor, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.floor);
    readLayerRegion(world.ceiling, world.width, chunk.cx, chunk.cy, chunk.cw, chunk.ch,
                    outDelta.cells.ceiling);
    return true;
}

inline void patchWorldGroundTile(WorldSnapshot& world, uint16_t tileX, uint16_t tileY,
                                 uint8_t tileZ, uint16_t groundId)
{
    if (tileX >= world.width || tileY >= world.height) {
        return;
    }
    const size_t idx = static_cast<size_t>(tileY) * world.width + static_cast<size_t>(tileX);
    const int32_t cellId = static_cast<int32_t>(groundId);
    if (world.depth > 1 && tileZ < world.floors.size()) {
        WorldFloorSlice& slice = world.floors[tileZ];
        if (idx < slice.ground.size()) {
            slice.ground[idx] = cellId;
        }
        return;
    }
    if (idx < world.ground.size()) {
        world.ground[idx] = cellId;
    }
}

inline uint64_t packChunkIndexKey(uint8_t floorZ, uint16_t chunkX, uint16_t chunkY)
{
    return (static_cast<uint64_t>(floorZ) << 32)
        | (static_cast<uint64_t>(chunkX) << 16)
        | static_cast<uint64_t>(chunkY);
}

inline void chunkIndexToTileOrigin(uint16_t chunkX, uint16_t chunkY, uint16_t chunkTileWidth,
                                   uint16_t chunkTileHeight, uint16_t& tileX, uint16_t& tileY)
{
    tileX = static_cast<uint16_t>(chunkX * chunkTileWidth);
    tileY = static_cast<uint16_t>(chunkY * chunkTileHeight);
}

inline WorldChunkDesc chunkDescFromIndex(uint16_t chunkX, uint16_t chunkY, uint8_t floorZ,
                                         uint16_t chunkTileWidth, uint16_t chunkTileHeight,
                                         uint16_t mapWidth, uint16_t mapHeight)
{
    WorldChunkDesc chunk {};
    chunkIndexToTileOrigin(chunkX, chunkY, chunkTileWidth, chunkTileHeight, chunk.cx, chunk.cy);
    chunk.cw = static_cast<uint16_t>(
        std::min<int>(chunkTileWidth, static_cast<int>(mapWidth) - chunk.cx));
    chunk.ch = static_cast<uint16_t>(
        std::min<int>(chunkTileHeight, static_cast<int>(mapHeight) - chunk.cy));
    chunk.floorZ = floorZ;
    return chunk;
}

inline void chunkIndexFromTileOrigin(uint16_t tileX, uint16_t tileY, uint16_t chunkTileWidth,
                                     uint16_t chunkTileHeight, uint16_t& chunkX, uint16_t& chunkY)
{
    chunkX = chunkTileWidth > 0 ? static_cast<uint16_t>(tileX / chunkTileWidth) : 0;
    chunkY = chunkTileHeight > 0 ? static_cast<uint16_t>(tileY / chunkTileHeight) : 0;
}

inline const SparseChunkEntry* lookupSparseChunk(const WorldSnapshot& world, uint8_t floorZ,
                                                 uint16_t tileX, uint16_t tileY,
                                                 ChunkLayerCache& cache)
{
    if (world.sparseChunks.empty() || world.chunkTileWidth == 0 || world.chunkTileHeight == 0) {
        return nullptr;
    }

    uint16_t chunkX = 0;
    uint16_t chunkY = 0;
    chunkIndexFromTileOrigin(tileX, tileY, world.chunkTileWidth, world.chunkTileHeight, chunkX,
                             chunkY);
    uint16_t originX = 0;
    uint16_t originY = 0;
    chunkIndexToTileOrigin(chunkX, chunkY, world.chunkTileWidth, world.chunkTileHeight, originX,
                           originY);
    const uint64_t key = packSparseChunkKey(floorZ, originX, originY);
    if (cache.lastKey == key) {
#ifdef ENABLE_TRACE
        traceStats().noteChunkCacheHit();
#endif
        return cache.lastEntry;
    }

#ifdef ENABLE_TRACE
    traceStats().noteChunkCacheMiss();
#endif
    cache.lastKey = key;
    const auto it = world.sparseChunks.find(key);
    cache.lastEntry = it != world.sparseChunks.end() ? &it->second : nullptr;
    return cache.lastEntry;
}

inline int layerCellSparse(const WorldSnapshot& world, WorldLayer layer, int x, int y,
                           uint8_t floorZ, ChunkLayerCache& cache)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(world.width) ||
        y >= static_cast<int>(world.height)) {
        return 0;
    }

    const SparseChunkEntry* entry =
        lookupSparseChunk(world, floorZ, static_cast<uint16_t>(x), static_cast<uint16_t>(y), cache);
    if (entry != nullptr) {
        const int localX = x - static_cast<int>(entry->originX);
        const int localY = y - static_cast<int>(entry->originY);
        if (localX >= 0 && localY >= 0 && localX < static_cast<int>(entry->cw) &&
            localY < static_cast<int>(entry->ch)) {
            const size_t idx =
                static_cast<size_t>(localY) * static_cast<size_t>(entry->cw)
                + static_cast<size_t>(localX);
            return readLayerCellAtIndex(entry->cells, layer, idx);
        }
    }

    return layerCellLegacy(world, layer, x, y, floorZ);
}

inline void cameraFromRotation(CameraState& cam, float rot)
{
    cam.rot = rot;
    cam.dirX = std::cos(rot);
    cam.dirY = -std::sin(rot);
    cam.planeX = -cam.dirY * PLANE_SCALE;
    cam.planeY = cam.dirX * PLANE_SCALE;
}

inline void rotationFromCamera(CameraState& cam)
{
    cam.rot = std::atan2(-cam.dirY, cam.dirX);
}

inline CameraState cameraProxy(const CameraState& cam, float tilesBack = CAMERA_PROXY_TILES)
{
    CameraState proxy = cam;
    proxy.posX -= cam.dirX * tilesBack * static_cast<float>(TILE_SIZE);
    proxy.posY -= cam.dirY * tilesBack * static_cast<float>(TILE_SIZE);
    return proxy;
}

} // namespace rc

#endif
