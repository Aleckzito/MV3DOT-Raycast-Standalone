#include "LocalDebris.h"

#include <osg/Geode>
#include <osg/Material>
#include <osg/StateSet>

namespace rc {
namespace standalone {

namespace {

const float kDebrisEdge = 0.08f;

} // namespace

LocalDebris::LocalDebris()
    : pos(0.0f, 0.0f, 0.0f)
    , vel(0.0f, 0.0f, 0.0f)
    , ttl(0.0f)
    , isActive(false)
{
    buildVisual();
}

LocalDebris::LocalDebris(const osg::Vec3& spawnPos, const osg::Vec3& vel, const osg::Vec4& color, float ttl)
    : pos(spawnPos)
    , vel(vel)
    , ttl(ttl)
    , isActive(true)
{
    buildVisual();
    applyColor(color);
}

void LocalDebris::buildVisual()
{
    // 73.4 Particula: arista total 0.08.
    osg::ref_ptr<osg::Box> box = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), kDebrisEdge, kDebrisEdge, kDebrisEdge);
    m_drawable = new osg::ShapeDrawable(box.get());
    m_drawable->setColor(osg::Vec4(0.72f, 0.74f, 0.78f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_drawable.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.28f, 0.30f, 0.32f, 1.0f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.72f, 0.74f, 0.78f, 1.0f));
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    syncVisual();
}

void LocalDebris::applyColor(const osg::Vec4& color)
{
    if (m_drawable.valid()) {
        m_drawable->setColor(color);
    }
}

osg::Node* LocalDebris::getNode()
{
    return m_pat.get();
}

void LocalDebris::syncVisual()
{
    if (m_pat.valid()) {
        m_pat->setPosition(pos);
        m_pat->setNodeMask(isActive ? 0xffffffff : 0);
    }
}

void LocalDebris::activate(const osg::Vec3& spawnPos, const osg::Vec3& velocity, const osg::Vec4& color, float life)
{
    pos = spawnPos;
    vel = velocity;
    ttl = life;
    isActive = true;
    applyColor(color);
    if (m_pat.valid() && m_pat->getNumParents() == 0) {
        m_pat->setNodeMask(0xffffffff);
    }
    syncVisual();
}

void LocalDebris::deactivate()
{
    isActive = false;
    ttl = 0.0f;
    vel = osg::Vec3(0.0f, 0.0f, 0.0f);
    syncVisual();
}

} // namespace standalone
} // namespace rc
