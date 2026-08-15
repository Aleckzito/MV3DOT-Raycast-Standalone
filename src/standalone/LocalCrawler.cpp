#include "LocalCrawler.h"
#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/GL>
#include <osg/Material>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

LocalCrawler::LocalCrawler()
    : LocalCrawler(osg::Vec3(0.0f, 0.0f, 0.0f))
{
}

LocalCrawler::LocalCrawler(const osg::Vec3& spawnPos)
    : pos(spawnPos)
    , m_alive(true)
    , m_targeted(false)
    , m_speed(1.80f)
    , m_width(0.62f)
    , m_height(0.30f)
    , m_depth(0.78f)
    , m_faceX(0.0f)
    , m_faceZ(1.0f)
    , m_hp(120)
    , m_maxHp(120)
    , m_knock(0.0f, 0.0f, 0.0f)
{
    if (pos.y() < 0.0f) {
        pos.y() = 0.0f;
    }
    buildVisual();
}

void LocalCrawler::buildVisual()
{
    osg::ref_ptr<osg::Box> hull = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), m_width, m_height, m_depth);
    m_bodyDraw = new osg::ShapeDrawable(hull.get());
    m_bodyDraw->setColor(CRAWLER_COLOR);
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_bodyDraw.get());

    osg::ref_ptr<osg::Box> rust = new osg::Box(
        osg::Vec3(0.0f, m_height * 0.28f, m_depth * 0.12f),
        m_width * 0.55f, m_height * 0.22f, m_depth * 0.40f);
    osg::ref_ptr<osg::ShapeDrawable> rustDraw = new osg::ShapeDrawable(rust.get());
    rustDraw->setColor(CRAWLER_RUST);
    geode->addDrawable(rustDraw.get());

    m_material = new osg::Material;
    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    applyTint();
    syncVisual();
}

osg::Node* LocalCrawler::getNode()
{
    return m_pat.get();
}

void LocalCrawler::applyTint()
{
    osg::Vec4 col = CRAWLER_COLOR;
    osg::Vec4 emit(0.06f, 0.05f, 0.04f, 1.0f);
    if (m_targeted) {
        col.set(0.95f, 0.20f, 0.85f, 1.0f);
        emit.set(0.40f, 0.08f, 0.35f, 1.0f);
    }
    if (m_bodyDraw.valid()) {
        m_bodyDraw->setColor(col);
    }
    if (m_material.valid()) {
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.12f, 0.12f, 0.14f, 1.0f));
        m_material->setEmission(osg::Material::FRONT_AND_BACK, emit);
        m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.45f, 0.42f, 0.38f, 1.0f));
        m_material->setShininess(osg::Material::FRONT_AND_BACK, 32.0f);
    }
}

void LocalCrawler::syncVisual()
{
    if (!m_pat.valid()) {
        return;
    }
    m_pat->setPosition(osg::Vec3(pos.x(), pos.y() + m_height * 0.5f, pos.z()));
    const float yaw = std::atan2(m_faceX, m_faceZ);
    m_pat->setAttitude(osg::Quat(yaw, osg::Vec3(0.0f, 1.0f, 0.0f)));
    m_pat->setNodeMask(m_alive ? 0xffffffff : 0);
}

void LocalCrawler::setTargeted(bool targeted)
{
    m_targeted = targeted;
    applyTint();
}

AABB LocalCrawler::makeAabb() const
{
    return makeAabbAt(pos.x(), pos.y(), pos.z());
}

AABB LocalCrawler::makeAabbAt(float x, float y, float z) const
{
    AABB box;
    const float hx = m_width * 0.5f;
    const float hz = m_depth * 0.5f;
    box.minX = x - hx;
    box.maxX = x + hx;
    box.minY = y;
    box.maxY = y + m_height;
    box.minZ = z - hz;
    box.maxZ = z + hz;
    return box;
}

bool LocalCrawler::isFrontalHit(const osg::Vec3& from) const
{
    float dx = from.x() - pos.x();
    float dz = from.z() - pos.z();
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < 0.0001f) {
        return true;
    }
    dx /= len;
    dz /= len;
    return (dx * m_faceX + dz * m_faceZ) > 0.28f;
}

void LocalCrawler::kill()
{
    m_alive = false;
    m_hp = 0;
    m_targeted = false;
    m_knock = osg::Vec3(0.0f, 0.0f, 0.0f);
    syncVisual();
}

void LocalCrawler::applyStats(int maxHp, float speed)
{
    if (maxHp > 0) {
        m_maxHp = maxHp;
        m_hp = maxHp;
    }
    if (speed > 0.0f) {
        m_speed = speed;
    }
}

void LocalCrawler::takeDamage(int amount, const osg::Vec3& origin)
{
    if (!m_alive || amount <= 0) {
        return;
    }
    m_hp -= amount;
    float dx = pos.x() - origin.x();
    float dz = pos.z() - origin.z();
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len > 0.0001f) {
        m_knock = osg::Vec3(dx / len * 6.0f, 0.0f, dz / len * 6.0f);
    }
    if (m_hp <= 0) {
        kill();
    }
}

void LocalCrawler::updateAI(float dt, const osg::Vec3& playerPos)
{
    if (!m_alive || dt <= 0.0f) {
        return;
    }
    pos.x() += m_knock.x() * dt;
    pos.z() += m_knock.z() * dt;
    float damp = 1.0f - 7.0f * dt;
    if (damp < 0.0f) {
        damp = 0.0f;
    }
    m_knock.x() *= damp;
    m_knock.z() *= damp;

    const float dx = playerPos.x() - pos.x();
    const float dz = playerPos.z() - pos.z();
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 0.0001f) {
        return;
    }
    const float inv = 1.0f / dist;
    m_faceX = dx * inv;
    m_faceZ = dz * inv;
    if (dist > 8.0f * TILE_SIZE || dist <= 0.55f) {
        return;
    }
    pos.x() += m_faceX * m_speed * dt;
    pos.z() += m_faceZ * m_speed * dt;
}

} // namespace standalone
} // namespace rc
