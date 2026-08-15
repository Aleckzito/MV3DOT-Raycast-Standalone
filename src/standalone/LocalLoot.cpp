#include "LocalLoot.h"
#include "MiniVoxelGrid.h"

#include <osg/BlendFunc>
#include <osg/Geode>
#include <osg/GL>
#include <osg/Material>
#include <osg/Quat>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

LocalLoot::LocalLoot()
    : pos(0.0f, 0.0f, 0.0f)
    , velocity(0.0f, 0.0f, 0.0f)
    , isActive(false)
    , isGrounded(false)
    , m_size(SMALL_HP)
    , m_spin(0.0f)
{
    buildVisual();
}

LocalLoot::LocalLoot(const osg::Vec3& spawnPos, LootSize size)
    : pos(spawnPos)
    , velocity(0.0f, 0.0f, 0.0f)
    , isActive(true)
    , isGrounded(false)
    , m_size(size)
    , m_spin(0.0f)
{
    buildVisual();
}

float LocalLoot::halfExtent() const
{
    if (m_size == ENERGY_CELL) {
        return 0.10f;
    }
    if (m_size == LARGE_HP) {
        return MINI_VOXEL_SIZE;
    }
    return 0.15f;
}

void LocalLoot::applySize()
{
    const float half = halfExtent();
    if (m_box.valid()) {
        m_box->setHalfLengths(osg::Vec3(half, half, half));
    }
    if (m_drawable.valid()) {
        m_drawable->dirtyBound();
        m_drawable->dirtyDisplayList();
    }
    const bool energy = (m_size == ENERGY_CELL);
    const osg::Vec4 col = energy
        ? osg::Vec4(0.18f, 0.72f, 1.00f, 1.0f)
        : osg::Vec4(0.20f, 0.95f, 0.28f, 1.0f);
    const osg::Vec4 emit = energy
        ? osg::Vec4(0.12f, 0.55f, 0.95f, 1.0f)
        : osg::Vec4(0.08f, 0.45f, 0.12f, 1.0f);
    if (m_drawable.valid()) {
        m_drawable->setColor(col);
    }
    if (m_material.valid()) {
        m_material->setAmbient(osg::Material::FRONT_AND_BACK,
                               energy ? osg::Vec4(0.04f, 0.18f, 0.35f, 1.0f)
                                      : osg::Vec4(0.05f, 0.30f, 0.08f, 1.0f));
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, emit);
    }
    if (m_haloDraw.valid()) {
        m_haloDraw->setColor(osg::Vec4(0.20f, 0.80f, 1.00f, energy ? 0.28f : 0.0f));
    }
}

void LocalLoot::buildVisual()
{
    const float half = halfExtent();
    const float edge = half * 2.0f;
    m_box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), edge, edge, edge);
    m_drawable = new osg::ShapeDrawable(m_box.get());
    m_drawable->setColor(osg::Vec4(0.20f, 0.95f, 0.28f, 1.0f));

    osg::ref_ptr<osg::Box> haloBox = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), edge * 1.85f, edge * 1.85f, edge * 1.85f);
    m_haloDraw = new osg::ShapeDrawable(haloBox.get());
    m_haloDraw->setColor(osg::Vec4(0.20f, 0.80f, 1.00f, 0.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_drawable.get());
    geode->addDrawable(m_haloDraw.get());

    m_material = new osg::Material;
    m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.30f, 0.08f, 1.0f));
    m_material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.20f, 0.95f, 0.28f, 1.0f));
    m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.45f, 0.12f, 1.0f));

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    applySize();
    syncVisual();
}

osg::Node* LocalLoot::getNode()
{
    return m_pat.get();
}

void LocalLoot::syncVisual()
{
    if (!m_pat.valid()) {
        return;
    }
    const float bob = isGrounded ? (0.08f * std::sin(m_spin * 2.2f)) : 0.0f;
    m_pat->setPosition(osg::Vec3(pos.x(), pos.y() + halfExtent() + bob, pos.z()));
    m_pat->setAttitude(osg::Quat(m_spin, osg::Vec3(0.0f, 1.0f, 0.0f)));
    m_pat->setNodeMask(isActive ? 0xffffffff : 0);
}

void LocalLoot::tick(float dt)
{
    if (!isActive || dt <= 0.0f) {
        return;
    }
    m_spin += dt * 4.50f;
    syncVisual();
}

void LocalLoot::activate(const osg::Vec3& spawnPos, LootSize size)
{
    pos = spawnPos;
    velocity.set(0.0f, 0.0f, 0.0f);
    m_size = size;
    m_spin = 0.0f;
    isActive = true;
    isGrounded = false;
    applySize();
    syncVisual();
}

void LocalLoot::deactivate()
{
    isActive = false;
    syncVisual();
}

} // namespace standalone
} // namespace rc
