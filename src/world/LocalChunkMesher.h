#ifndef RC_LOCAL_CHUNK_MESHER_H
#define RC_LOCAL_CHUNK_MESHER_H

#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include <osg/Node>
#include <osg/ref_ptr>

#include <unordered_map>
#include <vector>

namespace rc {
namespace standalone {

// 9 / 57. Mesher local. Cero packets.
//
// El terreno estatico va en UNA sola geometria fusionada, sin caras internas
// entre voxels adyacentes. Antes era un Geode por mini-voxel, lo que a 14k
// voxels saturaba dos nucleos con el suelo vacio.
//
// El X-Ray sigue siendo por voxel, que es lo que exigia aquel diseño: los pocos
// voxels que ocluyen la camara salen del lote (sus vertices se colapsan, que es
// una escritura O(1)) y se dibujan como Geode translucido propio. Al salir del
// X-Ray vuelven al lote.
class LocalChunkMesher {
public:
    LocalChunkMesher();

    void setGrid(const MiniVoxelGrid* grid);
    void rebuildMesh();
    void setXRay(int vx, int vy, int vz, bool enabled);
    osg::Node* getNode();

    // Diagnostico: nodos que cuelgan del holder (1 lote + N voxels en X-Ray).
    size_t drawableCount() const;

private:
    // Rango de vertices que ocupa un voxel dentro de la geometria fusionada.
    struct BatchSlot {
        size_t first = 0;   // indice del primer vertice
        size_t count = 0;   // vertices emitidos (4 por cara visible)
        bool hidden = false;
    };

    struct XRayVisual {
        osg::ref_ptr<osg::Geode> geode;
        osg::ref_ptr<osg::Material> material;
        osg::ref_ptr<osg::ShapeDrawable> drawable;
        osg::Vec4 baseColor;
        osg::Vec4 baseAmbient;
    };

    void buildBatch();
    void hideInBatch(const VoxelKey& key);
    void showInBatch(const VoxelKey& key);
    void addXRayVoxel(const VoxelKey& key);
    void removeXRayVoxel(const VoxelKey& key);

    const MiniVoxelGrid* m_grid;
    osg::ref_ptr<osg::Group> m_holder;

    // Lote estatico.
    osg::ref_ptr<osg::Geode> m_batchGeode;
    osg::ref_ptr<osg::Geometry> m_batchGeometry;
    osg::ref_ptr<osg::Vec3Array> m_vertices;
    osg::ref_ptr<osg::Vec3Array> m_normals;
    osg::ref_ptr<osg::Vec4Array> m_colors;
    std::vector<osg::Vec3> m_baseVertices;  // copia para restaurar tras X-Ray
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash> m_slots;

    // Voxels actualmente en X-Ray, fuera del lote.
    std::unordered_map<VoxelKey, XRayVisual, VoxelKeyHash> m_xray;
};

} // namespace standalone
} // namespace rc

#endif
