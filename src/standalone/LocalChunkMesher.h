#ifndef RC_LOCAL_CHUNK_MESHER_H
#define RC_LOCAL_CHUNK_MESHER_H

#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/Group>
#include <osg/Material>
#include <osg/Node>
#include <osg/ShapeDrawable>
#include <osg/ref_ptr>

#include <unordered_map>

namespace rc {
namespace standalone {

// 9 / 57. Mesher local: un Geode por mini-voxel para X-Ray. Cero packets.
class LocalChunkMesher {
public:
    LocalChunkMesher();

    void setGrid(const MiniVoxelGrid* grid);
    void rebuildMesh();
    void setXRay(int vx, int vy, int vz, bool enabled);
    osg::Node* getNode();

private:
    struct VoxelVisual {
        osg::ref_ptr<osg::Geode> geode;
        osg::ref_ptr<osg::Material> material;
        osg::ref_ptr<osg::ShapeDrawable> drawable;
        osg::Vec4 baseColor;
        osg::Vec4 baseAmbient;
        bool xray;
    };

    void applyXRayState(VoxelVisual& vis);

    const MiniVoxelGrid* m_grid;
    osg::ref_ptr<osg::Group> m_holder;
    std::unordered_map<VoxelKey, VoxelVisual, VoxelKeyHash> m_visuals;
};

} // namespace standalone
} // namespace rc

#endif
