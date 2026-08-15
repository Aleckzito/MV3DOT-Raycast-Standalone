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
#include <unordered_set>
#include <vector>

namespace rc {
namespace standalone {

// 16x16x16 mini-voxels por chunk.
//
// NO se usan >> y << para el mapeo: con coordenadas negativas el shift a la
// derecha de un entero con signo es implementation-defined en C++17, y el shift
// a la izquierda de un negativo es directamente UB. El jugador puede construir
// en x o z negativos, asi que se usa division con redondeo a -infinito y
// multiplicacion, que si estan definidas.
const int CHUNK_SIZE = 16;

// Division entera redondeando hacia -infinito (no hacia cero, como hace /).
inline int floorDivChunk(int value)
{
    int q = value / CHUNK_SIZE;
    if (value % CHUNK_SIZE != 0 && value < 0) {
        q -= 1;
    }
    return q;
}

// Offset dentro del chunk, siempre en [0, CHUNK_SIZE).
inline int localInChunk(int value)
{
    return value - floorDivChunk(value) * CHUNK_SIZE;
}

struct ChunkCoord {
    int cx = 0;
    int cy = 0;
    int cz = 0;
};

inline bool operator==(const ChunkCoord& a, const ChunkCoord& b)
{
    return a.cx == b.cx && a.cy == b.cy && a.cz == b.cz;
}

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const
    {
        // Mismo esquema que VoxelKeyHash: primos grandes y mezcla, en vez de
        // XOR de hashes desplazados, que colisiona en cuanto los ejes se cruzan.
        const unsigned int x = static_cast<unsigned int>(c.cx);
        const unsigned int y = static_cast<unsigned int>(c.cy);
        const unsigned int z = static_cast<unsigned int>(c.cz);
        return static_cast<std::size_t>(x * 73856093u ^ y * 19349663u ^ z * 83492791u);
    }
};

// 9 / 57. Mesher local por chunks. Cero packets.
//
// El terreno se parte en chunks de 16^3, cada uno con su geometria opaca y su
// geometria de liquido. Al cambiar un voxel solo se remalla su chunk (y el
// vecino si toca frontera), en vez del mapa entero: el rebuild global de la
// arena costaba ~24 ms, mas que un frame completo a 60 fps.
//
// El X-Ray sigue siendo por voxel: los pocos que ocluyen la camara se colapsan
// dentro del chunk al que pertenecen y se dibujan como Geode translucido propio.
class LocalChunkMesher {
public:
    LocalChunkMesher();

    void setGrid(const MiniVoxelGrid* grid);
    // Remalla lo que haga falta: todo si el grid se limpio, o solo los chunks
    // tocados desde la ultima llamada. Los puntos de llamada no cambian.
    void rebuildMesh();
    void setXRay(int vx, int vy, int vz, bool enabled);
    osg::Node* getNode();

    size_t drawableCount() const;
    double lastRebuildMs() const { return m_lastRebuildMs; }
    size_t chunkCount() const { return m_chunks.size(); }
    // Chunks procesados en la ultima llamada a rebuildMesh.
    size_t lastRebuiltChunks() const { return m_lastRebuiltChunks; }
    // Caras opacas emitidas en total, para comprobar que al destruir un voxel
    // aparecen las caras del vecino que estaban ocultas.
    size_t solidFaceCount() const;
    // Diagnostico para el self-test.
    bool hasSlot(int vx, int vy, int vz) const;
    bool slotHidden(int vx, int vy, int vz) const;
    bool isXRay(int vx, int vy, int vz) const;
    // Material con el que se construyo la visual X-Ray. 0 si no esta en X-Ray.
    uint16_t xrayMaterial(int vx, int vy, int vz) const;

private:
    // Rango de vertices de un voxel dentro de la geometria de su chunk.
    struct BatchSlot {
        ChunkCoord chunk;
        size_t first = 0;
        size_t count = 0;
        bool hidden = false;
    };

    struct Chunk {
        osg::ref_ptr<osg::Geode> geode;
        osg::ref_ptr<osg::Geometry> solid;
        osg::ref_ptr<osg::Geometry> liquid;
        osg::ref_ptr<osg::Vec3Array> vertices;
        osg::ref_ptr<osg::Vec3Array> normals;
        osg::ref_ptr<osg::Vec4Array> colors;
        osg::ref_ptr<osg::Vec3Array> liquidVertices;
        osg::ref_ptr<osg::Vec3Array> liquidNormals;
        osg::ref_ptr<osg::Vec4Array> liquidColors;
        std::vector<osg::Vec3> baseVertices;
    };

    struct XRayVisual {
        osg::ref_ptr<osg::Geode> geode;
        osg::ref_ptr<osg::Material> material;
        osg::ref_ptr<osg::ShapeDrawable> drawable;
        osg::Vec4 baseColor;
        osg::Vec4 baseAmbient;
        // Material con el que se construyo: si el voxel cambia de material hay
        // que rehacer la visual, o mostraria el color antiguo.
        uint16_t materialId = 0;
    };

    static ChunkCoord chunkOf(int vx, int vy, int vz);

    void rebuildAll();
    void rebuildChunk(const ChunkCoord& coord);
    void collectDirty(std::unordered_set<ChunkCoord, ChunkCoordHash>& out) const;
    Chunk& ensureChunk(const ChunkCoord& coord);
    void applyChunkGeometry(Chunk& chunk);

    void hideInBatch(const VoxelKey& key);
    void showInBatch(const VoxelKey& key);
    void addXRayVoxel(const VoxelKey& key);
    void removeXRayVoxel(const VoxelKey& key);

    const MiniVoxelGrid* m_grid;
    osg::ref_ptr<osg::Group> m_holder;
    osg::ref_ptr<osg::Group> m_terrainRoot;
    double m_lastRebuildMs = 0.0;
    size_t m_lastRebuiltChunks = 0;

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_chunks;
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash> m_slots;
    std::unordered_map<VoxelKey, XRayVisual, VoxelKeyHash> m_xray;
};

} // namespace standalone
} // namespace rc

#endif
