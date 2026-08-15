#include "LocalFary.h"
#include "MiniVoxelGrid.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Material>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

namespace {

const float kLeadDist = 2.0f * TILE_SIZE;
const float kRearDist = 2.0f * TILE_SIZE;
const float kLeadUp = 0.70f;
const float kOscSpeed = 2.6f;
const float kOscAmp = 0.85f * TILE_SIZE;
const float kFollowLerp = 4.5f;

} // namespace

LocalFary::LocalFary()
    : m_pos(4.0f, 1.2f, 6.0f)
    , m_hasPos(false)
    , m_time(0.0f)
{
    // 73. osg::Box ctor = arista total 0.10 (nodo oculto; buddy es el visible).
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.10f, 0.10f, 0.10f);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(0.15f, 1.0f, 1.0f, 0.55f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.35f, 0.35f, 0.55f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.15f, 1.0f, 1.0f, 0.55f));
    material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.45f, 0.50f, 0.55f));
    material->setAlpha(osg::Material::FRONT_AND_BACK, 0.55f);

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                osg::StateAttribute::ON);

    osg::ref_ptr<osg::PositionAttitudeTransform> diamond = new osg::PositionAttitudeTransform;
    diamond->setAttitude(
        osg::Quat(0.78539816, osg::Vec3d(1.0, 0.0, 0.0)) *
        osg::Quat(0.78539816, osg::Vec3d(0.0, 0.0, 1.0)));
    diamond->addChild(geode.get());

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(diamond.get());
    m_pat->setPosition(m_pos);
}

osg::Node* LocalFary::getNode()
{
    return m_pat.get();
}

void LocalFary::update(float dt, const osg::Vec3& targetPos, float targetYaw, bool threatBehind)
{
    // 26. Frente Dummy = (sin,0,cos). Perp weave = (cos,0,-sin).
    m_time += dt;
    const float fx = std::sin(targetYaw);
    const float fz = std::cos(targetYaw);
    const float px = std::cos(targetYaw);
    const float pz = -std::sin(targetYaw);
    const float weave = std::sin(m_time * kOscSpeed) * kOscAmp;

    float along = kLeadDist;
    float sideSign = 1.0f;
    if (threatBehind) {
        // 26.2 REARGUARD: 2 tiles atras. Weave invertido (alerta).
        along = -kRearDist;
        sideSign = -1.0f;
    }

    const osg::Vec3 ideal(
        targetPos.x() + fx * along + px * weave * sideSign,
        targetPos.y() + kLeadUp,
        targetPos.z() + fz * along + pz * weave * sideSign);

    if (!m_hasPos) {
        m_pos = ideal;
        m_hasPos = true;
    } else {
        float t = dt * kFollowLerp;
        if (t > 1.0f) {
            t = 1.0f;
        }
        m_pos = m_pos + (ideal - m_pos) * t;
    }

    m_pat->setPosition(m_pos);
    m_pat->setAttitude(osg::Quat(static_cast<double>(targetYaw), osg::Vec3d(0.0, 1.0, 0.0)));
}

} // namespace standalone
} // namespace rc
