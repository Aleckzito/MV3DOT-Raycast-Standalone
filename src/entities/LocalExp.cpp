#include "LocalExp.h"

#include <osg/Geode>
#include <osg/GL>
#include <osg/Material>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

LocalExp::LocalExp()
    : pos(0.0f, 0.0f, 0.0f)
    , velocity(0.0f, 0.0f, 0.0f)
    , isActive(false)
    , isGrounded(false)
    , m_expValue(0)
    , m_spin(0.0f)
{
    buildVisual();
}

LocalExp::LocalExp(const osg::Vec3& spawnPos)
    : LocalExp(spawnPos, 0)
{
}

LocalExp::LocalExp(const osg::Vec3& spawnPos, int expValue)
    : pos(spawnPos)
    , velocity(0.0f, 0.0f, 0.0f)
    , isActive(true)
    , isGrounded(false)
    , m_expValue(expValue)
    , m_spin(0.0f)
{
    buildVisual();
}

void LocalExp::buildVisual()
{
    const float edge = halfExtent() * 2.0f;
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), edge, edge, edge);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(0.0f, 0.5f, 1.0f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.02f, 0.12f, 0.35f, 1.0f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0f, 0.5f, 1.0f, 1.0f));
    material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.20f, 0.55f, 1.0f));

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    syncVisual();
}

osg::Node* LocalExp::getNode()
{
    return m_pat.get();
}

void LocalExp::syncVisual()
{
    if (!m_pat.valid()) {
        return;
    }
    const float bob = isGrounded ? (0.08f * std::sin(m_spin * 2.2f)) : 0.0f;
    m_pat->setPosition(osg::Vec3(pos.x(), pos.y() + halfExtent() + bob, pos.z()));
    m_pat->setAttitude(osg::Quat(m_spin, osg::Vec3(0.0f, 1.0f, 0.0f)));
    m_pat->setNodeMask(isActive ? 0xffffffff : 0);
}

void LocalExp::tick(float dt)
{
    if (!isActive || dt <= 0.0f) {
        return;
    }
    m_spin += dt * 4.50f;
    syncVisual();
}

void LocalExp::activate(const osg::Vec3& spawnPos, int expValue)
{
    pos = spawnPos;
    velocity.set(0.0f, 0.0f, 0.0f);
    m_expValue = expValue;
    m_spin = 0.0f;
    isActive = true;
    isGrounded = false;
    syncVisual();
}

void LocalExp::deactivate()
{
    isActive = false;
    syncVisual();
}

} // namespace standalone
} // namespace rc
