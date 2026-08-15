#ifndef RC_MINI_VOXEL_GRID_H
#define RC_MINI_VOXEL_GRID_H

#include <cstdint>
#include <unordered_map>

namespace rc {
namespace standalone {

// 4.2 Escala estricta: 1 tile = 3 mini-voxels por eje.
const float TILE_SIZE = 1.0f;
const float MINI_VOXEL_SIZE = 1.0f / 3.0f;

// 4.3 Celda sub-tile. materialId 0 = vacio.
struct MiniVoxel {
    uint16_t materialId;
    bool isActive;
};

struct VoxelKey {
    int vx;
    int vy;
    int vz;
};

inline bool operator==(const VoxelKey& a, const VoxelKey& b)
{
    return a.vx == b.vx && a.vy == b.vy && a.vz == b.vz;
}

struct VoxelKeyHash {
    std::size_t operator()(const VoxelKey& key) const
    {
        const unsigned int x = static_cast<unsigned int>(key.vx);
        const unsigned int y = static_cast<unsigned int>(key.vy);
        const unsigned int z = static_cast<unsigned int>(key.vz);
        return static_cast<std::size_t>(x * 73856093u ^ y * 19349663u ^ z * 83492791u);
    }
};

typedef std::unordered_map<VoxelKey, MiniVoxel, VoxelKeyHash> VoxelMap;

// 4.4 Mapa disperso (vx,vy,vz) en pasos de 1/3 de tile.
class MiniVoxelGrid {
public:
    MiniVoxelGrid();

    void setVoxel(int vx, int vy, int vz, uint16_t matId);
    MiniVoxel getVoxel(int vx, int vy, int vz) const;
    void clear();

    // 9.3 Iteracion del mesher sobre celdas activas.
    const VoxelMap& voxels() const { return m_voxels; }

private:
    VoxelMap m_voxels;
};

} // namespace standalone
} // namespace rc

#endif
