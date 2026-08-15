#include "DummyActor.h"
#include "MiniVoxelGrid.h"

#include <osg/Geode>
#include <osg/Material>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>
#include <iostream>

namespace rc {
namespace standalone {

DummyActor::DummyActor()
    : m_x(4.0f)
    , m_y(5.0f)
    , m_z(4.0f)
    , m_velX(0.0f)
    , m_velY(0.0f)
    , m_velZ(0.0f)
    , m_yaw(0.0f)
    , m_pitch(0.20f)
    , m_width(MINI_VOXEL_SIZE)
    , m_height(MINI_VOXEL_SIZE * 2.0f)
    , m_depth(MINI_VOXEL_SIZE)
    , m_hp(100)
    , m_maxHp(100)
    , m_iFrames(0.0f)
    , m_stamina(7200.0f)
    , m_maxStamina(7200.0f)
    , m_dashTimer(0.0f)
    , m_knockVelX(0.0f)
    , m_knockVelZ(0.0f)
    , m_level(1)
    , m_exp(0)
    , m_expToNext(100)
    , m_vocation(0)
{
    const float halfD = m_depth * 0.5f;
    // 73.3 osg::Box = AABB total (ancho, alto, profundidad).
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), m_width, m_height, m_depth);
    osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
    drawable->setColor(osg::Vec4(1.0f, 0.85f, 0.10f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(drawable.get());

    // 17.6 Nariz oscura en +Z local. Tras Quat(yaw,Y): frente = (sin(yaw), 0, cos(yaw)).
    osg::ref_ptr<osg::Box> noseBox = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.05f, 0.05f, 0.05f);
    osg::ref_ptr<osg::ShapeDrawable> noseDraw = new osg::ShapeDrawable(noseBox.get());
    noseDraw->setColor(osg::Vec4(0.08f, 0.08f, 0.10f, 1.0f));
    osg::ref_ptr<osg::Geode> noseGeode = new osg::Geode;
    noseGeode->addDrawable(noseDraw.get());
    osg::ref_ptr<osg::Material> noseMat = new osg::Material;
    noseMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.08f, 0.10f, 1.0f));
    noseMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.04f, 0.04f, 0.05f, 1.0f));
    osg::StateSet* noseState = noseGeode->getOrCreateStateSet();
    noseState->setAttributeAndModes(noseMat.get(), osg::StateAttribute::ON);
    noseState->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    osg::ref_ptr<osg::PositionAttitudeTransform> nosePat = new osg::PositionAttitudeTransform;
    nosePat->setPosition(osg::Vec3(0.0f, 0.04f, halfD + 0.07f));
    nosePat->addChild(noseGeode.get());

    osg::ref_ptr<osg::Material> material = new osg::Material;
    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.35f, 0.25f, 0.02f, 1.0f));
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 0.85f, 0.10f, 1.0f));
    material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.25f, 0.18f, 0.02f, 1.0f));
    material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.20f, 0.18f, 0.05f, 1.0f));
    material->setShininess(osg::Material::FRONT_AND_BACK, 12.0f);

    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());
    m_pat->addChild(nosePat.get());
    syncVisual();
}

osg::Node* DummyActor::getNode()
{
    return m_pat.get();
}

void DummyActor::syncVisual()
{
    // 17.5 Rotacion Y: Quat(yaw, (0,1,0)). Frente mundo = (sin(yaw), 0, cos(yaw)).
    m_pat->setPosition(osg::Vec3(m_x, m_y + m_height * 0.5f, m_z));
    m_pat->setAttitude(osg::Quat(static_cast<double>(m_yaw), osg::Vec3d(0.0, 1.0, 0.0)));
}

void DummyActor::setPosition(float x, float y, float z)
{
    m_x = x;
    m_y = y;
    m_z = z;
}

void DummyActor::teleport(float x, float y, float z)
{
    m_x = x;
    m_y = y;
    m_z = z;
    m_velX = 0.0f;
    m_velY = 0.0f;
    m_velZ = 0.0f;
    m_knockVelX = 0.0f;
    m_knockVelZ = 0.0f;
}

void DummyActor::addYaw(float delta)
{
    m_yaw += delta;
}

void DummyActor::addPitch(float delta)
{
    m_pitch += delta;
    if (m_pitch > 1.20f) {
        m_pitch = 1.20f;
    }
    if (m_pitch < -0.85f) {
        m_pitch = -0.85f;
    }
}

void DummyActor::tickIFrames(float dt)
{
    if (m_iFrames > 0.0f) {
        m_iFrames -= dt;
        if (m_iFrames < 0.0f) {
            m_iFrames = 0.0f;
        }
    }

    // 52.2 Drain pasivo. Tope 0. Dash requiere SP > 0.
    m_stamina -= dt;
    if (m_stamina < 0.0f) {
        m_stamina = 0.0f;
    }
}

void DummyActor::tickDash(float dt)
{
    if (m_dashTimer <= 0.0f) {
        m_dashTimer = 0.0f;
        return;
    }
    m_dashTimer -= dt;
    if (m_dashTimer < 0.0f) {
        m_dashTimer = 0.0f;
    }
}

bool DummyActor::activateDash()
{
    // 44.2 Coste 35 SP. 0.25s haste + iFrames.
    if (m_stamina < 35.0f || m_dashTimer > 0.0f) {
        return false;
    }
    m_stamina -= 35.0f;
    if (m_stamina < 0.0f) {
        m_stamina = 0.0f;
    }
    m_dashTimer = 0.25f;
    m_iFrames = 0.25f;
    return true;
}

void DummyActor::applyDamage(int amount)
{
    if (amount <= 0) {
        return;
    }
    m_hp -= amount;
    if (m_hp < 0) {
        m_hp = 0;
    }
    m_iFrames = 0.45f;
}

void DummyActor::takeDamage(int amount)
{
    // 29.1 / 29.3 iFrames bloquean daño continuo por frame.
    if (m_iFrames > 0.0f || amount <= 0) {
        return;
    }
    m_hp -= amount;
    if (m_hp < 0) {
        m_hp = 0;
    }
    m_iFrames = 1.0f;
}

void DummyActor::heal(int amount)
{
    if (amount <= 0) {
        return;
    }
    m_hp += amount;
    if (m_hp > m_maxHp) {
        m_hp = m_maxHp;
    }
}

void DummyActor::restoreHp()
{
    m_hp = m_maxHp;
}

void DummyActor::addExp(int amount)
{
    if (amount <= 0) {
        return;
    }
    m_exp += amount;
    while (m_exp >= m_expToNext) {
        m_exp -= m_expToNext;
        m_level += 1;
        m_expToNext = static_cast<int>(m_expToNext * 1.5f);
        if (m_expToNext < 1) {
            m_expToNext = 1;
        }
        m_maxHp += 20;
        m_maxStamina += 10.0f;
        m_hp = m_maxHp;
        m_stamina = m_maxStamina;
        std::cout << "[progress] LEVEL UP! Level: " << m_level << "\n";
    }
}

void DummyActor::addStamina(float amount)
{
    if (amount <= 0.0f) {
        return;
    }
    m_stamina += amount;
    if (m_stamina > m_maxStamina) {
        m_stamina = m_maxStamina;
    }
}

void DummyActor::applyKnockback(float velX, float velZ)
{
    m_knockVelX = velX;
    m_knockVelZ = velZ;
}

void DummyActor::tickKnockback(float dt)
{
    float damp = 1.0f - 8.0f * dt;
    if (damp < 0.0f) {
        damp = 0.0f;
    }
    m_knockVelX *= damp;
    m_knockVelZ *= damp;
    if (m_knockVelX * m_knockVelX + m_knockVelZ * m_knockVelZ < 0.04f) {
        m_knockVelX = 0.0f;
        m_knockVelZ = 0.0f;
    }
}

AABB DummyActor::makeAabb() const
{
    return makeAabbAt(m_x, m_y, m_z);
}

AABB DummyActor::makeAabbAtY(float y) const
{
    return makeAabbAt(m_x, y, m_z);
}

AABB DummyActor::makeAabbAt(float x, float y, float z) const
{
    AABB box;
    const float halfW = m_width * 0.5f;
    const float halfD = m_depth * 0.5f;
    box.minX = x - halfW;
    box.maxX = x + halfW;
    box.minY = y;
    box.maxY = y + m_height;
    box.minZ = z - halfD;
    box.maxZ = z + halfD;
    return box;
}

} // namespace standalone
} // namespace rc
