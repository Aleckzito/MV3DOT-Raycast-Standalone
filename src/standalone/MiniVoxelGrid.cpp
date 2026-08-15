#include "MiniVoxelGrid.h"

namespace rc {
namespace standalone {

MiniVoxelGrid::MiniVoxelGrid()
{
}

void MiniVoxelGrid::setVoxel(int vx, int vy, int vz, uint16_t matId)
{
    // 4.5 matId 0 borra la celda. Cualquier otro material activa el mini-voxel.
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;

    if (matId == 0) {
        m_voxels.erase(key);
        return;
    }

    MiniVoxel voxel;
    voxel.materialId = matId;
    voxel.isActive = true;
    m_voxels[key] = voxel;
}

MiniVoxel MiniVoxelGrid::getVoxel(int vx, int vy, int vz) const
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;

    const std::unordered_map<VoxelKey, MiniVoxel, VoxelKeyHash>::const_iterator it = m_voxels.find(key);
    if (it == m_voxels.end()) {
        MiniVoxel empty;
        empty.materialId = 0;
        empty.isActive = false;
        return empty;
    }
    return it->second;
}

void MiniVoxelGrid::clear()
{
    m_voxels.clear();
}

} // namespace standalone
} // namespace rc
