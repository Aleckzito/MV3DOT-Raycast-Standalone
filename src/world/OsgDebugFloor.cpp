#include "OsgDebugFloor.h"
#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/Group>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

namespace rc {
namespace standalone {

OsgDebugFloor::OsgDebugFloor()
{
    // 7.1 Flujo: caja 8x8 tiles, cara superior en y=0, origen en (0,0,0).
    const int tiles = 8;
    const float world = static_cast<float>(tiles) * TILE_SIZE;
    const float halfXz = world * 0.5f;
    const float thickness = 0.08f;
    const float halfY = thickness * 0.5f;

    osg::ref_ptr<osg::Box> box = new osg::Box(
        osg::Vec3(halfXz, -halfY, halfXz),
        world, thickness, world);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(0.22f, 0.22f, 0.24f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.10f, 0.10f, 0.12f, 1.0f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.22f, 0.22f, 0.24f, 1.0f));
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.05f, 0.05f, 1.0f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 4.0f);

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    osg::ref_ptr<osg::Group> group = new osg::Group;
    group->addChild(geode.get());
    m_root = group;
}

osg::Node* OsgDebugFloor::getNode()
{
    return m_root.get();
}

} // namespace standalone
} // namespace rc
