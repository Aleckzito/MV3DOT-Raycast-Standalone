#include "LocalChunkMesher.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/GL>
#include <osg/Group>
#include <osg/PolygonOffset>
#include <osg/Shape>
#include <osg/StateSet>

namespace rc {
namespace standalone {

namespace {

const osg::Vec4 kVoxelOpaque(0.72f, 0.74f, 0.78f, 1.0f);
const osg::Vec4 kVoxelBrick(0.78f, 0.22f, 0.10f, 1.0f);

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

void LocalChunkMesher::applyXRayState(VoxelVisual& vis)
{
    if (!vis.geode.valid() || !vis.material.valid() || !vis.drawable.valid()) {
        return;
    }
    osg::StateSet* state = vis.geode->getOrCreateStateSet();
    if (vis.xray) {
        osg::Vec4 col = vis.baseColor;
        col.a() = 0.35f;
        vis.drawable->setColor(col);
        vis.material->setAmbient(osg::Material::FRONT_AND_BACK,
                                 osg::Vec4(vis.baseAmbient.x(), vis.baseAmbient.y(),
                                           vis.baseAmbient.z(), 0.35f));
        vis.material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        vis.material->setAlpha(osg::Material::FRONT_AND_BACK, 0.35f);
        state->setMode(GL_BLEND, osg::StateAttribute::ON);
        state->setAttributeAndModes(
            new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON);
        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        state->setAttributeAndModes(
            new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
            osg::StateAttribute::ON);
        return;
    }

    vis.drawable->setColor(vis.baseColor);
    vis.material->setAmbient(osg::Material::FRONT_AND_BACK, vis.baseAmbient);
    vis.material->setDiffuse(osg::Material::FRONT_AND_BACK, vis.baseColor);
    vis.material->setAlpha(osg::Material::FRONT_AND_BACK, 1.0f);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);
    state->setRenderingHint(osg::StateSet::OPAQUE_BIN);
    state->setAttributeAndModes(
        new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, true),
        osg::StateAttribute::ON);
}

void LocalChunkMesher::rebuildMesh()
{
    // 9.3 / 57. Un Geode por celda. Preserva flags X-Ray si el voxel sigue vivo.
    std::unordered_map<VoxelKey, bool, VoxelKeyHash> wasXray;
    std::unordered_map<VoxelKey, VoxelVisual, VoxelKeyHash>::const_iterator oldIt = m_visuals.begin();
    while (oldIt != m_visuals.end()) {
        if (oldIt->second.xray) {
            wasXray[oldIt->first] = true;
        }
        ++oldIt;
    }

    m_holder->removeChildren(0, m_holder->getNumChildren());
    m_visuals.clear();

    if (m_grid == nullptr) {
        return;
    }

    const VoxelMap& map = m_grid->voxels();
    // 73.1 osg::Box(dx,dy,dz) = arista TOTAL. Adyacentes colindan sin rendija.
    VoxelMap::const_iterator it = map.begin();
    while (it != map.end()) {
        if (!it->second.isActive) {
            ++it;
            continue;
        }

        const float cx = (static_cast<float>(it->first.vx) + 0.5f) * MINI_VOXEL_SIZE;
        const float cy = (static_cast<float>(it->first.vy) + 0.5f) * MINI_VOXEL_SIZE;
        const float cz = (static_cast<float>(it->first.vz) + 0.5f) * MINI_VOXEL_SIZE;

        const float edge = MINI_VOXEL_SIZE * 0.996f;
        osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(cx, cy, cz), edge, edge, edge);
        osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
        const bool brick = (it->second.materialId >= 2);
        const osg::Vec4 col = brick ? kVoxelBrick : kVoxelOpaque;
        const osg::Vec4 amb = brick ? osg::Vec4(0.32f, 0.08f, 0.04f, 1.0f)
                                    : osg::Vec4(0.28f, 0.30f, 0.32f, 1.0f);
        drawable->setColor(col);

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(drawable.get());

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setAmbient(osg::Material::FRONT_AND_BACK, amb);
        material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));
        material->setShininess(osg::Material::FRONT_AND_BACK, 8.0f);
        material->setAlpha(osg::Material::FRONT_AND_BACK, 1.0f);

        osg::StateSet* state = geode->getOrCreateStateSet();
        state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
        state->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f), osg::StateAttribute::ON);
        state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
        state->setMode(GL_BLEND, osg::StateAttribute::OFF);
        state->setRenderingHint(osg::StateSet::OPAQUE_BIN);

        VoxelVisual vis;
        vis.geode = geode;
        vis.material = material;
        vis.drawable = drawable;
        vis.baseColor = col;
        vis.baseAmbient = amb;
        vis.xray = (wasXray.find(it->first) != wasXray.end());
        if (vis.xray) {
            applyXRayState(vis);
        }

        m_holder->addChild(geode.get());
        m_visuals[it->first] = vis;
        ++it;
    }
}

void LocalChunkMesher::setXRay(int vx, int vy, int vz, bool enabled)
{
    VoxelKey key;
    key.vx = vx;
    key.vy = vy;
    key.vz = vz;
    std::unordered_map<VoxelKey, VoxelVisual, VoxelKeyHash>::iterator it = m_visuals.find(key);
    if (it == m_visuals.end()) {
        return;
    }
    if (it->second.xray == enabled) {
        return;
    }
    it->second.xray = enabled;
    applyXRayState(it->second);
}

osg::Node* LocalChunkMesher::getNode()
{
    return m_holder.get();
}

} // namespace standalone
} // namespace rc
