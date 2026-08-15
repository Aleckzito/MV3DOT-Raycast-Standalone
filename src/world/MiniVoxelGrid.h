#ifndef RC_MINI_VOXEL_GRID_H
#define RC_MINI_VOXEL_GRID_H

#include <cstdint>
#include <unordered_map>
#include <vector>

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

    // Rastreo de cambios para el remallado parcial. Se lleva aqui, en setVoxel,
    // y no en cada punto de llamada: hay una decena de sitios que tocan el grid
    // (construir, destruir, bombas, colapsos, rocas, Arquitecto) y cualquiera
    // que se olvidara de marcar dejaria un agujero en la malla.
    const std::vector<VoxelKey>& dirtyVoxels() const { return m_dirty; }
    // clear() invalida el mapa entero: el mesher debe rehacerlo todo.
    bool dirtyAll() const { return m_dirtyAll; }
    void clearDirty();

    // 9.3 Iteracion del mesher sobre celdas activas.
    const VoxelMap& voxels() const { return m_voxels; }

    // Extension real del mapa en indices de voxel. false si esta vacio.
    // El grid es disperso, asi que se calcula recorriendo las celdas activas.
    bool computeBounds(int* minX, int* minY, int* minZ,
                       int* maxX, int* maxY, int* maxZ) const;

    // Y del voxel solido mas alto en (vx, vz). -1 si esa columna esta vacia.
    int topSolidY(int vx, int vz) const;

private:
    VoxelMap m_voxels;
    std::vector<VoxelKey> m_dirty;
    bool m_dirtyAll = true;
};

} // namespace standalone
} // namespace rc

#endif
