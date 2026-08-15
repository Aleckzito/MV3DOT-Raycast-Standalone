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

bool MiniVoxelGrid::computeBounds(int* minX, int* minY, int* minZ,
                                  int* maxX, int* maxY, int* maxZ) const
{
    bool any = false;
    int lox = 0, loy = 0, loz = 0, hix = 0, hiy = 0, hiz = 0;
    for (VoxelMap::const_iterator it = m_voxels.begin(); it != m_voxels.end(); ++it) {
        if (!it->second.isActive) {
            continue;
        }
        const VoxelKey& k = it->first;
        if (!any) {
            lox = hix = k.vx;
            loy = hiy = k.vy;
            loz = hiz = k.vz;
            any = true;
            continue;
        }
        if (k.vx < lox) lox = k.vx;
        if (k.vy < loy) loy = k.vy;
        if (k.vz < loz) loz = k.vz;
        if (k.vx > hix) hix = k.vx;
        if (k.vy > hiy) hiy = k.vy;
        if (k.vz > hiz) hiz = k.vz;
    }
    if (!any) {
        return false;
    }
    if (minX != nullptr) *minX = lox;
    if (minY != nullptr) *minY = loy;
    if (minZ != nullptr) *minZ = loz;
    if (maxX != nullptr) *maxX = hix;
    if (maxY != nullptr) *maxY = hiy;
    if (maxZ != nullptr) *maxZ = hiz;
    return true;
}

int MiniVoxelGrid::topSolidY(int vx, int vz) const
{
    int top = -1;
    for (VoxelMap::const_iterator it = m_voxels.begin(); it != m_voxels.end(); ++it) {
        if (!it->second.isActive) {
            continue;
        }
        const VoxelKey& k = it->first;
        if (k.vx == vx && k.vz == vz && k.vy > top) {
            top = k.vy;
        }
    }
    return top;
}

} // namespace standalone
} // namespace rc
