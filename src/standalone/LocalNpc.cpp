#include "LocalNpc.h"
#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/Material>
#include <osg/Shape>

namespace rc {
namespace standalone {

LocalNpc::LocalNpc()
    : LocalNpc(osg::Vec3(0.0f, 0.0f, 0.0f), "", "NPC", "", osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f))
{
}

LocalNpc::LocalNpc(const osg::Vec3& spawnPos, const std::string& id, const std::string& display,
                   const std::string& talk, const osg::Vec4& color)
    : pos(spawnPos)
    , contentId(id)
    , name(display)
    , talkId(talk)
    , greetCd(0.0f)
{
    buildVisual(color);
}

void LocalNpc::buildVisual(const osg::Vec4& color)
{
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f),
                                              MINI_VOXEL_SIZE, MINI_VOXEL_SIZE * 2.2f,
                                              MINI_VOXEL_SIZE);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(box.get());
    draw->setColor(color);
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());
    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, color);
    mat->setEmission(osg::Material::FRONT_AND_BACK,
                     osg::Vec4(color.x() * 0.25f, color.y() * 0.25f, color.z() * 0.25f, 1.0f));
    geode->getOrCreateStateSet()->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    syncVisual();
}

osg::Node* LocalNpc::getNode()
{
    return m_pat.get();
}

void LocalNpc::syncVisual()
{
    if (m_pat.valid()) {
        m_pat->setPosition(osg::Vec3(pos.x(), pos.y() + MINI_VOXEL_SIZE * 1.1f, pos.z()));
    }
}

AABB LocalNpc::makeAabb() const
{
    const float h = MINI_VOXEL_SIZE * 2.2f;
    const float w = MINI_VOXEL_SIZE * 0.5f;
    AABB box;
    box.minX = pos.x() - w;
    box.maxX = pos.x() + w;
    box.minY = pos.y();
    box.maxY = pos.y() + h;
    box.minZ = pos.z() - w;
    box.maxZ = pos.z() + w;
    return box;
}

} // namespace standalone
} // namespace rc
