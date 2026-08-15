#include "LocalProjectile.h"

#include <osg/BlendFunc>
#include <osg/GL>
#include <osg/Geode>
#include <osg/Material>
#include <osg/Quat>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

LocalProjectile::LocalProjectile()
    : m_pos(0.0f, 0.0f, 0.0f)
    , m_vel(0.0f, 0.0f, 0.0f)
    , m_ttl(0.0f)
    , m_alive(false)
    , m_hostile(false)
{
}

LocalProjectile::LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl)
    : LocalProjectile(pos, vel, ttl, false)
{
}

LocalProjectile::LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl, bool hostile)
    : m_pos(pos)
    , m_vel(vel)
    , m_ttl(ttl)
    , m_alive(true)
    , m_hostile(hostile)
{
    buildVisual();
}

void LocalProjectile::buildVisual()
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    if (m_hostile) {
        osg::ref_ptr<osg::Box> shaft = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.045f, 0.045f, 0.28f);
        osg::ref_ptr<osg::ShapeDrawable> body = new osg::ShapeDrawable(shaft.get());
        body->setColor(osg::Vec4(0.25f, 1.00f, 0.35f, 1.0f));
        geode->addDrawable(body.get());

        osg::ref_ptr<osg::Box> trailBox = new osg::Box(osg::Vec3(0.0f, 0.0f, -0.22f), 0.03f, 0.03f, 0.18f);
        osg::ref_ptr<osg::ShapeDrawable> trail = new osg::ShapeDrawable(trailBox.get());
        trail->setColor(osg::Vec4(0.10f, 0.85f, 0.22f, 0.45f));
        geode->addDrawable(trail.get());

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.04f, 0.30f, 0.08f, 1.0f));
        material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.25f, 1.00f, 0.35f, 1.0f));
        material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.15f, 0.85f, 0.20f, 1.0f));
        osg::StateSet* state = geode->getOrCreateStateSet();
        state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
        state->setAttributeAndModes(
            new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON);
        state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
        state->setMode(GL_BLEND, osg::StateAttribute::ON);
    } else {
        osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.07f, 0.07f, 0.14f);
        osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
        drawable->setColor(osg::Vec4(1.00f, 0.92f, 0.15f, 1.0f));
        geode->addDrawable(drawable.get());

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.32f, 0.04f, 1.0f));
        material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.00f, 0.92f, 0.15f, 1.0f));
        material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.95f, 0.80f, 0.08f, 1.0f));
        osg::StateSet* state = geode->getOrCreateStateSet();
        state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
        state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
        state->setMode(GL_BLEND, osg::StateAttribute::OFF);
    }

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    syncVisual();
}

osg::Node* LocalProjectile::getNode()
{
    return m_pat.get();
}

void LocalProjectile::syncVisual()
{
    if (!m_pat.valid()) {
        return;
    }
    m_pat->setPosition(m_pos);
    osg::Vec3 dir = m_vel;
    const float len = dir.length();
    if (len > 0.0001f) {
        dir = dir * (1.0f / len);
        osg::Quat q;
        q.makeRotate(osg::Vec3(0.0f, 0.0f, 1.0f), dir);
        m_pat->setAttitude(q);
    }
}

} // namespace standalone
} // namespace rc
