#include "LocalChunkMesher.h"

#include "VoxelMaterials.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/GL>
#include <osg/Group>
#include <osg/PolygonOffset>
#include <osg/PrimitiveSet>
#include <osg/Shape>
#include <osg/StateSet>

#include <chrono>
#include <iostream>

namespace rc {
namespace standalone {

namespace {

const float kXRayAlpha = 0.35f;

osg::Vec4 colorOf(uint16_t materialId)
{
    return materialColor(materialId);
}

osg::Vec4 ambientOf(uint16_t materialId)
{
    return materialAmbient(materialId);
}

// Las 6 caras: normal y los 4 vertices en orden antihorario visto desde fuera,
// en coordenadas locales [0,1] dentro de la celda.
struct Face {
    int dx, dy, dz;
    osg::Vec3 normal;
    osg::Vec3 corner[4];
};

const Face kFaces[6] = {
    { 0, 0, 1, osg::Vec3(0, 0, 1),
      { osg::Vec3(0, 0, 1), osg::Vec3(1, 0, 1), osg::Vec3(1, 1, 1), osg::Vec3(0, 1, 1) } },
    { 0, 0, -1, osg::Vec3(0, 0, -1),
      { osg::Vec3(1, 0, 0), osg::Vec3(0, 0, 0), osg::Vec3(0, 1, 0), osg::Vec3(1, 1, 0) } },
    { 1, 0, 0, osg::Vec3(1, 0, 0),
      { osg::Vec3(1, 0, 1), osg::Vec3(1, 0, 0), osg::Vec3(1, 1, 0), osg::Vec3(1, 1, 1) } },
    { -1, 0, 0, osg::Vec3(-1, 0, 0),
      { osg::Vec3(0, 0, 0), osg::Vec3(0, 0, 1), osg::Vec3(0, 1, 1), osg::Vec3(0, 1, 0) } },
    { 0, 1, 0, osg::Vec3(0, 1, 0),
      { osg::Vec3(0, 1, 1), osg::Vec3(1, 1, 1), osg::Vec3(1, 1, 0), osg::Vec3(0, 1, 0) } },
    { 0, -1, 0, osg::Vec3(0, -1, 0),
      { osg::Vec3(0, 0, 0), osg::Vec3(1, 0, 0), osg::Vec3(1, 0, 1), osg::Vec3(0, 0, 1) } }
};

void resetArray(osg::ref_ptr<osg::Vec3Array>& arr)
{
    if (!arr.valid()) {
        arr = new osg::Vec3Array;
    } else {
        arr->clear();
    }
}

void resetArray(osg::ref_ptr<osg::Vec4Array>& arr)
{
    if (!arr.valid()) {
        arr = new osg::Vec4Array;
    } else {
        arr->clear();
    }
}

} // namespace

LocalChunkMesher::LocalChunkMesher()
    : m_grid(nullptr)
{
    // 10.3 Holder estable en el grafo. Nunca se saca de m_root.
    m_holder = new osg::Group;
    m_terrainRoot = new osg::Group;
    m_holder->addChild(m_terrainRoot.get());
}

void LocalChunkMesher::setGrid(const MiniVoxelGrid* grid)
{
    m_grid = grid;
}

size_t LocalChunkMesher::drawableCount() const
{
    return m_holder.valid() ? m_holder->getNumChildren() : 0;
}

ChunkCoord LocalChunkMesher::chunkOf(int vx, int vy, int vz)
{
    ChunkCoord c;
    c.cx = floorDivChunk(vx);
    c.cy = floorDivChunk(vy);
    c.cz = floorDivChunk(vz);
    return c;
}

void LocalChunkMesher::collectDirty(
    std::unordered_set<ChunkCoord, ChunkCoordHash>& out) const
{
    const std::vector<VoxelKey>& dirty = m_grid->dirtyVoxels();
    for (size_t i = 0; i < dirty.size(); ++i) {
        const VoxelKey& k = dirty[i];
        out.insert(chunkOf(k.vx, k.vy, k.vz));

        // Frontera: el vecino tenia una cara oculta contra este voxel (o al
        // reves), asi que su malla tambien deja de ser valida.
        const int lx = localInChunk(k.vx);
        const int ly = localInChunk(k.vy);
        const int lz = localInChunk(k.vz);
        const int last = CHUNK_SIZE - 1;
        if (lx == 0) out.insert(chunkOf(k.vx - 1, k.vy, k.vz));
        if (lx == last) out.insert(chunkOf(k.vx + 1, k.vy, k.vz));
        if (ly == 0) out.insert(chunkOf(k.vx, k.vy - 1, k.vz));
        if (ly == last) out.insert(chunkOf(k.vx, k.vy + 1, k.vz));
        if (lz == 0) out.insert(chunkOf(k.vx, k.vy, k.vz - 1));
        if (lz == last) out.insert(chunkOf(k.vx, k.vy, k.vz + 1));
    }
}

void LocalChunkMesher::rebuildMesh()
{
    if (m_grid == nullptr) {
        return;
    }
    const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

    const size_t dirtyVoxels = m_grid->dirtyVoxels().size();
    size_t rebuilt = 0;
    bool full = false;
    if (m_grid->dirtyAll()) {
        rebuildAll();
        rebuilt = m_chunks.size();
        full = true;
    } else {
        if (m_grid->dirtyVoxels().empty()) {
            return;  // nada que rehacer: ni un voxel cambio
        }
        std::unordered_set<ChunkCoord, ChunkCoordHash> dirty;
        collectDirty(dirty);
        for (std::unordered_set<ChunkCoord, ChunkCoordHash>::const_iterator it = dirty.begin();
             it != dirty.end(); ++it) {
            rebuildChunk(*it);
        }
        rebuilt = dirty.size();
    }

    m_lastRebuiltChunks = rebuilt;
    // El grid ya esta al dia con la malla.
    const_cast<MiniVoxelGrid*>(m_grid)->clearDirty();

    m_lastRebuildMs = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();

    size_t faces = 0;
    size_t liquidFaces = 0;
    for (std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>::const_iterator it = m_chunks.begin();
         it != m_chunks.end(); ++it) {
        if (it->second.vertices.valid()) faces += it->second.vertices->size() / 4;
        if (it->second.liquidVertices.valid()) liquidFaces += it->second.liquidVertices->size() / 4;
    }

    std::cout << "[mesh] dirty=" << (full ? m_chunks.size() : dirtyVoxels)
              << " rebuilt=" << rebuilt << "/" << m_chunks.size()
              << " ms=" << m_lastRebuildMs
              << (full ? " (full)" : " (parcial)")
              << " voxels=" << m_slots.size()
              << " caras=" << faces;
    if (liquidFaces > 0) {
        std::cout << " liquido=" << liquidFaces;
    }
    std::cout << "\n";
}

void LocalChunkMesher::rebuildAll()
{
    // Se conserva el X-Ray de los voxels que sigan vivos.
    std::vector<VoxelKey> wasXray;
    for (std::unordered_map<VoxelKey, XRayVisual, VoxelKeyHash>::const_iterator it = m_xray.begin();
         it != m_xray.end(); ++it) {
        wasXray.push_back(it->first);
    }
    for (size_t i = 0; i < wasXray.size(); ++i) {
        removeXRayVoxel(wasXray[i]);
    }

    m_terrainRoot->removeChildren(0, m_terrainRoot->getNumChildren());
    m_chunks.clear();
    m_slots.clear();

    // Un pase sobre el grid para saber que chunks existen.
    std::unordered_set<ChunkCoord, ChunkCoordHash> present;
    const VoxelMap& map = m_grid->voxels();
    for (VoxelMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (!it->second.isActive) {
            continue;
        }
        present.insert(chunkOf(it->first.vx, it->first.vy, it->first.vz));
    }
    for (std::unordered_set<ChunkCoord, ChunkCoordHash>::const_iterator it = present.begin();
         it != present.end(); ++it) {
        rebuildChunk(*it);
    }

    for (size_t i = 0; i < wasXray.size(); ++i) {
        if (m_slots.find(wasXray[i]) != m_slots.end()) {
            hideInBatch(wasXray[i]);
            addXRayVoxel(wasXray[i]);
        }
    }
}

LocalChunkMesher::Chunk& LocalChunkMesher::ensureChunk(const ChunkCoord& coord)
{
    Chunk& chunk = m_chunks[coord];
    if (chunk.geode.valid()) {
        return chunk;
    }

    chunk.geode = new osg::Geode;
    chunk.solid = new osg::Geometry;
    chunk.liquid = new osg::Geometry;
    chunk.solid->setUseDisplayList(false);
    chunk.solid->setUseVertexBufferObjects(true);
    chunk.liquid->setUseDisplayList(false);
    chunk.liquid->setUseVertexBufferObjects(true);

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 8.0f);

    osg::StateSet* state = chunk.geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::OPAQUE_BIN);

    // El liquido lleva su propio StateSet: bin transparente y sin escritura de
    // profundidad, para no tapar lo que hay detras.
    osg::StateSet* liquidState = chunk.liquid->getOrCreateStateSet();
    liquidState->setMode(GL_BLEND, osg::StateAttribute::ON);
    liquidState->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    liquidState->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    liquidState->setAttributeAndModes(
        new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false), osg::StateAttribute::ON);

    chunk.geode->addDrawable(chunk.solid.get());
    chunk.geode->addDrawable(chunk.liquid.get());
    m_terrainRoot->addChild(chunk.geode.get());
    return chunk;
}

void LocalChunkMesher::rebuildChunk(const ChunkCoord& coord)
{
    Chunk& chunk = ensureChunk(coord);

    resetArray(chunk.vertices);
    resetArray(chunk.normals);
    resetArray(chunk.colors);
    resetArray(chunk.liquidVertices);
    resetArray(chunk.liquidNormals);
    resetArray(chunk.liquidColors);
    chunk.baseVertices.clear();

    // Multiplicacion, no shift: cx puede ser negativo y << seria UB.
    const int baseX = coord.cx * CHUNK_SIZE;
    const int baseY = coord.cy * CHUNK_SIZE;
    const int baseZ = coord.cz * CHUNK_SIZE;

    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                const int vx = baseX + lx;
                const int vy = baseY + ly;
                const int vz = baseZ + lz;

                VoxelKey key;
                key.vx = vx;
                key.vy = vy;
                key.vz = vz;

                // Se purga el slot antes de decidir: si el voxel se borro, su
                // slot apuntaria a vertices que ahora son de otro voxel.
                m_slots.erase(key);

                const MiniVoxel voxel = m_grid->getVoxel(vx, vy, vz);
                if (!voxel.isActive) {
                    continue;
                }

                const bool liquid = materialIsLiquid(voxel.materialId);
                const osg::Vec4 col = colorOf(voxel.materialId);
                osg::Vec3Array* verts = liquid ? chunk.liquidVertices.get() : chunk.vertices.get();
                osg::Vec3Array* norms = liquid ? chunk.liquidNormals.get() : chunk.normals.get();
                osg::Vec4Array* cols = liquid ? chunk.liquidColors.get() : chunk.colors.get();

                BatchSlot slot;
                slot.chunk = coord;
                slot.first = verts->size();
                slot.count = 0;

                for (int f = 0; f < 6; ++f) {
                    const Face& face = kFaces[f];
                    // El vecino puede estar en otro chunk: se consulta al grid,
                    // que es la fuente de verdad, no a la malla del chunk.
                    const MiniVoxel neighbor =
                        m_grid->getVoxel(vx + face.dx, vy + face.dy, vz + face.dz);
                    if (neighbor.isActive) {
                        // En la frontera agua-solido manda el solido: si el agua
                        // emitiera tambien su cara, quedarian coplanares.
                        if (liquid) {
                            continue;
                        }
                        if (!materialIsLiquid(neighbor.materialId)) {
                            continue;
                        }
                    }
                    for (int c = 0; c < 4; ++c) {
                        const osg::Vec3 local = face.corner[c];
                        verts->push_back(osg::Vec3(
                            (static_cast<float>(vx) + local.x()) * MINI_VOXEL_SIZE,
                            (static_cast<float>(vy) + local.y()) * MINI_VOXEL_SIZE,
                            (static_cast<float>(vz) + local.z()) * MINI_VOXEL_SIZE));
                        norms->push_back(face.normal);
                        cols->push_back(col);
                    }
                    slot.count += 4;
                }

                // Solo los solidos entran en slots: el X-Ray no toca liquidos.
                if (slot.count > 0 && !liquid) {
                    m_slots[key] = slot;
                }
            }
        }
    }

    chunk.baseVertices.assign(chunk.vertices->begin(), chunk.vertices->end());
    applyChunkGeometry(chunk);

    // Reconciliar el X-Ray de este chunk. Se recogen las claves primero porque
    // removeXRayVoxel modifica m_xray.
    std::vector<VoxelKey> xrayHere;
    for (std::unordered_map<VoxelKey, XRayVisual, VoxelKeyHash>::const_iterator it = m_xray.begin();
         it != m_xray.end(); ++it) {
        if (chunkOf(it->first.vx, it->first.vy, it->first.vz) == coord) {
            xrayHere.push_back(it->first);
        }
    }
    for (size_t i = 0; i < xrayHere.size(); ++i) {
        const VoxelKey& key = xrayHere[i];
        if (m_slots.find(key) == m_slots.end()) {
            // El voxel se borro o dejo de ser solido (el agua no lleva slot).
            // Sin esto quedaria su Geode translucido flotando, con el color del
            // material anterior.
            removeXRayVoxel(key);
            continue;
        }
        // Sigue siendo solido: el remallado lo dejo visible, hay que volver a
        // colapsarlo o se dibuja dos veces, opaco y translucido.
        hideInBatch(key);
    }

    // Un chunk sin geometria no debe quedarse en el grafo: editar en x=0
    // invalida el chunk -1, que puede no existir, y acumularia Geodes vacios.
    if (chunk.vertices->empty() && chunk.liquidVertices->empty()) {
        if (chunk.geode.valid()) {
            m_terrainRoot->removeChild(chunk.geode.get());
        }
        m_chunks.erase(coord);
    }
}

void LocalChunkMesher::applyChunkGeometry(Chunk& chunk)
{
    chunk.solid->setVertexArray(chunk.vertices.get());
    chunk.solid->setNormalArray(chunk.normals.get(), osg::Array::BIND_PER_VERTEX);
    chunk.solid->setColorArray(chunk.colors.get(), osg::Array::BIND_PER_VERTEX);
    chunk.solid->removePrimitiveSet(0, chunk.solid->getNumPrimitiveSets());
    if (!chunk.vertices->empty()) {
        chunk.solid->addPrimitiveSet(
            new osg::DrawArrays(GL_QUADS, 0, static_cast<int>(chunk.vertices->size())));
    }
    chunk.solid->dirtyBound();

    chunk.liquid->setVertexArray(chunk.liquidVertices.get());
    chunk.liquid->setNormalArray(chunk.liquidNormals.get(), osg::Array::BIND_PER_VERTEX);
    chunk.liquid->setColorArray(chunk.liquidColors.get(), osg::Array::BIND_PER_VERTEX);
    chunk.liquid->removePrimitiveSet(0, chunk.liquid->getNumPrimitiveSets());
    if (!chunk.liquidVertices->empty()) {
        chunk.liquid->addPrimitiveSet(
            new osg::DrawArrays(GL_QUADS, 0, static_cast<int>(chunk.liquidVertices->size())));
    }
    chunk.liquid->dirtyBound();
}

void LocalChunkMesher::hideInBatch(const VoxelKey& key)
{
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash>::iterator it = m_slots.find(key);
    if (it == m_slots.end() || it->second.hidden) {
        return;
    }
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>::iterator ch =
        m_chunks.find(it->second.chunk);
    if (ch == m_chunks.end() || !ch->second.vertices.valid()) {
        return;
    }
    osg::Vec3Array& verts = *ch->second.vertices;
    if (it->second.first + it->second.count > verts.size()) {
        return;
    }
    // Colapsar los vertices degenera los quads: dejan de pintarse sin tocar los
    // indices ni remallar el chunk. Es O(caras del voxel).
    const osg::Vec3 collapse = verts[it->second.first];
    for (size_t i = 0; i < it->second.count; ++i) {
        verts[it->second.first + i] = collapse;
    }
    it->second.hidden = true;
    verts.dirty();
    ch->second.solid->dirtyBound();
}

void LocalChunkMesher::showInBatch(const VoxelKey& key)
{
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash>::iterator it = m_slots.find(key);
    if (it == m_slots.end() || !it->second.hidden) {
        return;
    }
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>::iterator ch =
        m_chunks.find(it->second.chunk);
    if (ch == m_chunks.end() || !ch->second.vertices.valid()) {
        return;
    }
    osg::Vec3Array& verts = *ch->second.vertices;
    if (it->second.first + it->second.count > verts.size() ||
        it->second.first + it->second.count > ch->second.baseVertices.size()) {
        return;
    }
    for (size_t i = 0; i < it->second.count; ++i) {
        verts[it->second.first + i] = ch->second.baseVertices[it->second.first + i];
    }
    it->second.hidden = false;
    verts.dirty();
    ch->second.solid->dirtyBound();
}

void LocalChunkMesher::addXRayVoxel(const VoxelKey& key)
{
    if (m_grid == nullptr || m_xray.find(key) != m_xray.end()) {
        return;
    }
    const MiniVoxel voxel = m_grid->getVoxel(key.vx, key.vy, key.vz);
    if (!voxel.isActive) {
        return;
    }

    const float cx = (static_cast<float>(key.vx) + 0.5f) * MINI_VOXEL_SIZE;
    const float cy = (static_cast<float>(key.vy) + 0.5f) * MINI_VOXEL_SIZE;
    const float cz = (static_cast<float>(key.vz) + 0.5f) * MINI_VOXEL_SIZE;
    // 73.1 osg::Box(dx,dy,dz) = arista TOTAL. Adyacentes colindan sin rendija.
    const float edge = MINI_VOXEL_SIZE * 0.996f;

    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(cx, cy, cz), edge, edge, edge);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());

    XRayVisual vis;
    vis.baseColor = colorOf(voxel.materialId);
    vis.baseAmbient = ambientOf(voxel.materialId);

    osg::Vec4 col = vis.baseColor;
    col.a() = kXRayAlpha;
    drawable->setColor(col);

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK,
                         osg::Vec4(vis.baseAmbient.x(), vis.baseAmbient.y(),
                                   vis.baseAmbient.z(), kXRayAlpha));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 8.0f);
    material->setAlpha(osg::Material::FRONT_AND_BACK, kXRayAlpha);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(
        new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false), osg::StateAttribute::ON);

    vis.geode = geode;
    vis.material = material;
    vis.drawable = drawable;

    m_holder->addChild(geode.get());
    m_xray[key] = vis;
}

void LocalChunkMesher::removeXRayVoxel(const VoxelKey& key)
{
    std::unordered_map<VoxelKey, XRayVisual, VoxelKeyHash>::iterator it = m_xray.find(key);
    if (it == m_xray.end()) {
        return;
    }
    if (it->second.geode.valid()) {
        m_holder->removeChild(it->second.geode.get());
    }
    m_xray.erase(it);
}

void LocalChunkMesher::setXRay(int vx, int vy, int vz, bool enabled)
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;

    if (m_slots.find(key) == m_slots.end()) {
        return;
    }
    const bool already = (m_xray.find(key) != m_xray.end());
    if (already == enabled) {
        return;
    }

    if (enabled) {
        hideInBatch(key);
        addXRayVoxel(key);
    } else {
        removeXRayVoxel(key);
        showInBatch(key);
    }
}

size_t LocalChunkMesher::solidFaceCount() const
{
    size_t faces = 0;
    for (std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>::const_iterator it = m_chunks.begin();
         it != m_chunks.end(); ++it) {
        if (it->second.vertices.valid()) {
            faces += it->second.vertices->size() / 4;
        }
    }
    return faces;
}

bool LocalChunkMesher::hasSlot(int vx, int vy, int vz) const
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;
    return m_slots.find(key) != m_slots.end();
}

bool LocalChunkMesher::slotHidden(int vx, int vy, int vz) const
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;
    const std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash>::const_iterator it =
        m_slots.find(key);
    return it != m_slots.end() && it->second.hidden;
}

bool LocalChunkMesher::isXRay(int vx, int vy, int vz) const
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;
    return m_xray.find(key) != m_xray.end();
}

osg::Node* LocalChunkMesher::getNode()
{
    return m_holder.get();
}

} // namespace standalone
} // namespace rc
