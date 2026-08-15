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
    int dx, dy, dz;              // vecino que la tapa
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

} // namespace

LocalChunkMesher::LocalChunkMesher()
    : m_grid(nullptr)
{
    // 10.3 Holder estable en el grafo. Nunca se saca de m_root.
    m_holder = new osg::Group;
}

void LocalChunkMesher::setGrid(const MiniVoxelGrid* grid)
{
    m_grid = grid;
}

size_t LocalChunkMesher::drawableCount() const
{
    return m_holder.valid() ? m_holder->getNumChildren() : 0;
}

void LocalChunkMesher::rebuildMesh()
{
    // Preserva el X-Ray de los voxels que sigan vivos tras el rebuild.
    std::vector<VoxelKey> wasXray;
    for (auto it = m_xray.begin(); it != m_xray.end(); ++it) {
        wasXray.push_back(it->first);
    }

    m_holder->removeChildren(0, m_holder->getNumChildren());
    m_xray.clear();
    m_slots.clear();

    if (m_grid == nullptr) {
        return;
    }

    buildBatch();

    for (size_t i = 0; i < wasXray.size(); ++i) {
        if (m_slots.find(wasXray[i]) != m_slots.end()) {
            hideInBatch(wasXray[i]);
            addXRayVoxel(wasXray[i]);
        }
    }
}

void LocalChunkMesher::buildBatch()
{
    m_vertices = new osg::Vec3Array;
    m_normals = new osg::Vec3Array;
    m_colors = new osg::Vec4Array;
    m_liquidVertices = new osg::Vec3Array;
    m_liquidNormals = new osg::Vec3Array;
    m_liquidColors = new osg::Vec4Array;
    m_baseVertices.clear();

    const VoxelMap& map = m_grid->voxels();

    for (VoxelMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (!it->second.isActive) {
            continue;
        }
        const VoxelKey& key = it->first;
        const uint16_t mat = it->second.materialId;
        const bool liquid = materialIsLiquid(mat);
        const osg::Vec4 col = colorOf(mat);

        osg::Vec3Array* verts = liquid ? m_liquidVertices.get() : m_vertices.get();
        osg::Vec3Array* norms = liquid ? m_liquidNormals.get() : m_normals.get();
        osg::Vec4Array* cols = liquid ? m_liquidColors.get() : m_colors.get();

        BatchSlot slot;
        slot.first = verts->size();
        slot.count = 0;

        for (int f = 0; f < 6; ++f) {
            const Face& face = kFaces[f];
            const MiniVoxel neighbor =
                m_grid->getVoxel(key.vx + face.dx, key.vy + face.dy, key.vz + face.dz);
            if (neighbor.isActive) {
                // En la frontera agua-solido manda el solido: es quien dibuja la
                // pared del cauce. Si el agua emitiera tambien su cara ahi, las
                // dos quedarian coplanares (z-fighting y blending sobre si mismo).
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
                    (static_cast<float>(key.vx) + local.x()) * MINI_VOXEL_SIZE,
                    (static_cast<float>(key.vy) + local.y()) * MINI_VOXEL_SIZE,
                    (static_cast<float>(key.vz) + local.z()) * MINI_VOXEL_SIZE));
                norms->push_back(face.normal);
                cols->push_back(col);
            }
            slot.count += 4;
        }

        // Solo los solidos entran en slots: el X-Ray no se aplica a liquidos.
        if (slot.count > 0 && !liquid) {
            m_slots[key] = slot;
        }
    }

    m_baseVertices.assign(m_vertices->begin(), m_vertices->end());

    m_batchGeometry = new osg::Geometry;
    m_batchGeometry->setVertexArray(m_vertices.get());
    m_batchGeometry->setNormalArray(m_normals.get(), osg::Array::BIND_PER_VERTEX);
    m_batchGeometry->setColorArray(m_colors.get(), osg::Array::BIND_PER_VERTEX);
    m_batchGeometry->addPrimitiveSet(
        new osg::DrawArrays(GL_QUADS, 0, static_cast<int>(m_vertices->size())));
    // Los vertices se reescriben al entrar y salir del X-Ray.
    m_batchGeometry->setUseDisplayList(false);
    m_batchGeometry->setUseVertexBufferObjects(true);

    m_batchGeode = new osg::Geode;
    m_batchGeode->addDrawable(m_batchGeometry.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 8.0f);

    osg::StateSet* state = m_batchGeode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);
    state->setRenderingHint(osg::StateSet::OPAQUE_BIN);

    m_holder->addChild(m_batchGeode.get());

    // Lote de liquidos, solo si el mapa tiene agua.
    size_t liquidFaces = 0;
    if (!m_liquidVertices->empty()) {
        liquidFaces = m_liquidVertices->size() / 4;

        m_liquidGeometry = new osg::Geometry;
        m_liquidGeometry->setVertexArray(m_liquidVertices.get());
        m_liquidGeometry->setNormalArray(m_liquidNormals.get(), osg::Array::BIND_PER_VERTEX);
        m_liquidGeometry->setColorArray(m_liquidColors.get(), osg::Array::BIND_PER_VERTEX);
        m_liquidGeometry->addPrimitiveSet(
            new osg::DrawArrays(GL_QUADS, 0, static_cast<int>(m_liquidVertices->size())));
        m_liquidGeometry->setUseVertexBufferObjects(true);

        m_liquidGeode = new osg::Geode;
        m_liquidGeode->addDrawable(m_liquidGeometry.get());

        osg::ref_ptr<osg::Material> liquidMat = new osg::Material;
        liquidMat->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
        liquidMat->setSpecular(osg::Material::FRONT_AND_BACK,
                               osg::Vec4(0.20f, 0.24f, 0.26f, 1.0f));
        liquidMat->setShininess(osg::Material::FRONT_AND_BACK, 24.0f);

        osg::StateSet* liquidState = m_liquidGeode->getOrCreateStateSet();
        liquidState->setAttributeAndModes(liquidMat.get(), osg::StateAttribute::ON);
        liquidState->setMode(GL_LIGHTING, osg::StateAttribute::ON);
        liquidState->setMode(GL_BLEND, osg::StateAttribute::ON);
        liquidState->setAttributeAndModes(
            new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON);
        liquidState->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        // Sin escritura de profundidad: el agua no debe tapar lo que hay detras.
        liquidState->setAttributeAndModes(
            new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false), osg::StateAttribute::ON);

        m_holder->addChild(m_liquidGeode.get());
    }

    // Caras internas descartadas: util para ver de un vistazo cuanto ahorra el
    // culling en un mapa dado (6 caras por voxel seria el peor caso).
    const size_t faces = m_vertices->size() / 4;
    std::cout << "[mesh] voxels=" << m_slots.size()
              << " caras=" << faces
              << " (de " << (m_slots.size() * 6) << " sin culling)";
    if (liquidFaces > 0) {
        std::cout << " liquido=" << liquidFaces;
    }
    std::cout << " drawables=" << m_holder->getNumChildren() << "\n";
}

void LocalChunkMesher::hideInBatch(const VoxelKey& key)
{
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash>::iterator it = m_slots.find(key);
    if (it == m_slots.end() || it->second.hidden || !m_vertices.valid()) {
        return;
    }
    // Colapsar los vertices degenera los quads: dejan de pintarse sin tocar
    // los indices ni reconstruir el lote. Es O(caras del voxel).
    const osg::Vec3 collapse = (*m_vertices)[it->second.first];
    for (size_t i = 0; i < it->second.count; ++i) {
        (*m_vertices)[it->second.first + i] = collapse;
    }
    it->second.hidden = true;
    m_vertices->dirty();
    m_batchGeometry->dirtyBound();
}

void LocalChunkMesher::showInBatch(const VoxelKey& key)
{
    std::unordered_map<VoxelKey, BatchSlot, VoxelKeyHash>::iterator it = m_slots.find(key);
    if (it == m_slots.end() || !it->second.hidden || !m_vertices.valid()) {
        return;
    }
    for (size_t i = 0; i < it->second.count; ++i) {
        (*m_vertices)[it->second.first + i] = m_baseVertices[it->second.first + i];
    }
    it->second.hidden = false;
    m_vertices->dirty();
    m_batchGeometry->dirtyBound();
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

osg::Node* LocalChunkMesher::getNode()
{
    return m_holder.get();
}

} // namespace standalone
} // namespace rc
