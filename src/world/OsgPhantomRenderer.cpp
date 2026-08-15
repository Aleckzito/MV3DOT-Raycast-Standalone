#include "OsgPhantomRenderer.h"
#include "MiniVoxelGrid.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Material>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

namespace rc {
namespace standalone {

OsgPhantomRenderer::OsgPhantomRenderer()
{
    // 73.1 osg::Box(dx,dy,dz) = arista TOTAL. Centro del cubo = min-corner + size/2.
    osg::ref_ptr<osg::Box> box = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), MINI_VOXEL_SIZE, MINI_VOXEL_SIZE, MINI_VOXEL_SIZE);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(0.0f, 1.0f, 1.0f, 0.5f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    // 6.4 Material cyan + blending (rayos X).
    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.35f, 0.35f, 0.5f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 1.0f, 1.0f, 0.5f));
    material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.45f, 0.45f, 0.5f));
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.2f, 0.6f, 0.6f, 0.5f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 16.0f);
    material->setAlpha(osg::Material::FRONT_AND_BACK, 0.5f);

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    // 6.5 Sin escritura Z: translucido no pisa la profundidad del suelo.
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                osg::StateAttribute::ON);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    m_pat->setPosition(osg::Vec3(MINI_VOXEL_SIZE * 0.5f, MINI_VOXEL_SIZE * 0.5f, MINI_VOXEL_SIZE * 0.5f));
}

osg::Node* OsgPhantomRenderer::getNode()
{
    return m_pat.get();
}

void OsgPhantomRenderer::setPosition(const SnappedPosition& pose)
{
    // Pose es min-corner. PAT mueve el centro del cubo.
    const float half = MINI_VOXEL_SIZE * 0.5f;
    m_pat->setPosition(osg::Vec3(pose.x + half, pose.y + half, pose.z + half));
}

} // namespace standalone
} // namespace rc
