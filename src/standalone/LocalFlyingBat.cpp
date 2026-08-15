#include "LocalFlyingBat.h"
#include "MiniVoxelGrid.h"

#include <osg/Array>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/GL>
#include <osg/LineWidth>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>
#include <iostream>

namespace rc {
namespace standalone {

namespace {

const float kStalkMin = 5.0f * TILE_SIZE;
const float kStalkMax = 7.0f * TILE_SIZE;
const float kStalkSpeed = 4.80f;
const float kFleeSpeed = 11.0f;
const float kBehindDot = -0.65f;
const float kDiveTtl = 1.35f;
const float kFleeTtl = 2.50f;
const float kStunTtl = 1.50f;

} // namespace

LocalFlyingBat::LocalFlyingBat()
    : LocalFlyingBat(osg::Vec3(2.0f, BAT_CRUISE_ALT, 6.5f))
{
}

LocalFlyingBat::LocalFlyingBat(const osg::Vec3& spawnPos)
    : pos(spawnPos)
    , m_alive(true)
    , m_targeted(false)
    , m_state(BAT_STALK_BEHIND)
    , m_time(0.0f)
    , m_stateTtl(0.0f)
    , m_chirpTtl(0.0f)
    , m_orbit((spawnPos.x() > 4.0f) ? 1.0f : -1.0f)
    , m_hp(100)
    , m_maxHp(100)
    , m_width(0.42f)
    , m_height(0.36f)
    , m_depth(0.42f)
    , m_diveDir(0.0f, 0.0f, -1.0f)
{
    if (pos.y() < BAT_CRUISE_ALT) {
        pos.y() = BAT_CRUISE_ALT;
    }
    buildVisual();
}

void LocalFlyingBat::buildVisual()
{
    // 104.1 Piramide invertida magenta + alas que aletean.
    osg::ref_ptr<osg::Cone> cone = new osg::Cone(osg::Vec3(0.0f, 0.0f, 0.0f), 0.20f, 0.38f);
    m_drawable = new osg::ShapeDrawable(cone.get());
    m_drawable->setColor(BAT_COLOR);
    osg::ref_ptr<osg::Geode> body = new osg::Geode;
    body->addDrawable(m_drawable.get());
    m_material = new osg::Material;
    osg::StateSet* st = body->getOrCreateStateSet();
    st->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    osg::ref_ptr<osg::MatrixTransform> bodySpin = new osg::MatrixTransform;
    bodySpin->setMatrix(osg::Matrix::rotate(1.5707963, osg::Vec3(1.0f, 0.0f, 0.0f)));
    bodySpin->addChild(body.get());

    auto makeWing = [](float side) -> osg::MatrixTransform* {
        osg::ref_ptr<osg::Box> plate = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.38f, 0.025f, 0.16f);
        osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(plate.get());
        draw->setColor(BAT_COLOR);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(draw.get());
        osg::ref_ptr<osg::Material> mat = new osg::Material;
        mat->setDiffuse(osg::Material::FRONT_AND_BACK, BAT_COLOR);
        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.22f, 0.02f, 0.30f, 1.0f));
        geode->getOrCreateStateSet()->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);

        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        const float hx = 0.19f;
        const float hz = 0.08f;
        verts->push_back(osg::Vec3(-hx, 0.0f, -hz));
        verts->push_back(osg::Vec3(hx, 0.0f, -hz));
        verts->push_back(osg::Vec3(hx, 0.0f, hz));
        verts->push_back(osg::Vec3(-hx, 0.0f, hz));
        osg::ref_ptr<osg::Vec4Array> col = new osg::Vec4Array;
        col->push_back(osg::Vec4(0.95f, 0.35f, 1.0f, 1.0f));
        const unsigned short idx[8] = { 0, 1, 1, 2, 2, 3, 3, 0 };
        osg::ref_ptr<osg::DrawElementsUShort> lines = new osg::DrawElementsUShort(GL_LINES, 8, idx);
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setVertexArray(verts.get());
        geom->setColorArray(col.get(), osg::Array::BIND_OVERALL);
        geom->addPrimitiveSet(lines.get());
        osg::ref_ptr<osg::Geode> lineGeode = new osg::Geode;
        lineGeode->addDrawable(geom.get());
        osg::StateSet* ls = lineGeode->getOrCreateStateSet();
        ls->setAttributeAndModes(new osg::LineWidth(1.4f), osg::StateAttribute::ON);
        ls->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        osg::ref_ptr<osg::MatrixTransform> wing = new osg::MatrixTransform;
        wing->addChild(geode.get());
        wing->addChild(lineGeode.get());
        (void)side;
        return wing.release();
    };

    m_wingL = makeWing(-1.0f);
    m_wingR = makeWing(1.0f);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(bodySpin.get());
    m_pat->addChild(m_wingL.get());
    m_pat->addChild(m_wingR.get());
    applyTint();
    syncVisual();
}

osg::Node* LocalFlyingBat::getNode()
{
    return m_pat.get();
}

void LocalFlyingBat::applyTint()
{
    osg::Vec4 col = BAT_COLOR;
    osg::Vec4 emit(0.22f, 0.02f, 0.30f, 1.0f);
    if (m_chirpTtl > 0.0f) {
        col.set(1.0f, 0.12f, 0.10f, 1.0f);
        emit.set(0.95f, 0.08f, 0.05f, 1.0f);
    } else if (m_targeted) {
        col.set(0.95f, 0.35f, 1.0f, 1.0f);
        emit.set(0.55f, 0.12f, 0.70f, 1.0f);
    } else if (m_state == BAT_STUNNED) {
        col.set(0.45f, 0.08f, 0.55f, 1.0f);
        emit.set(0.08f, 0.0f, 0.12f, 1.0f);
    }
    if (m_drawable.valid()) {
        m_drawable->setColor(col);
    }
    if (m_material.valid()) {
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.18f, 0.02f, 0.22f, 1.0f));
        m_material->setEmission(osg::Material::FRONT_AND_BACK, emit);
        m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.20f, 0.70f, 1.0f));
        m_material->setShininess(osg::Material::FRONT_AND_BACK, 28.0f);
    }
}

void LocalFlyingBat::syncVisual()
{
    if (!m_pat.valid()) {
        return;
    }
    m_pat->setPosition(pos);
    m_pat->setNodeMask(m_alive ? 0xffffffff : 0);
    if (!m_alive) {
        return;
    }

    osg::Vec3 look = m_diveDir;
    look.y() = 0.0f;
    if (look.length2() < 1.0e-6f) {
        look.set(0.0f, 0.0f, 1.0f);
    } else {
        look.normalize();
    }
    const float yaw = std::atan2(look.x(), look.z());
    m_pat->setAttitude(osg::Quat(yaw, osg::Vec3(0.0f, 1.0f, 0.0f)));

    const float flap = std::sin(m_time * 14.0f) * 0.42f;
    const float stunWobble = (m_state == BAT_STUNNED) ? std::sin(m_time * 22.0f) * 0.18f : 0.0f;
    if (m_wingL.valid()) {
        osg::Matrix tr = osg::Matrix::translate(-0.22f, 0.04f + stunWobble, 0.0f);
        osg::Matrix rot = osg::Matrix::rotate(-0.35f + flap, osg::Vec3(0.0f, 0.0f, 1.0f));
        m_wingL->setMatrix(rot * tr);
    }
    if (m_wingR.valid()) {
        osg::Matrix tr = osg::Matrix::translate(0.22f, 0.04f + stunWobble, 0.0f);
        osg::Matrix rot = osg::Matrix::rotate(0.35f - flap, osg::Vec3(0.0f, 0.0f, 1.0f));
        m_wingR->setMatrix(rot * tr);
    }
    applyTint();
}

void LocalFlyingBat::setTargeted(bool targeted)
{
    m_targeted = targeted;
    applyTint();
}

AABB LocalFlyingBat::makeAabb() const
{
    AABB box;
    const float hx = m_width * 0.5f;
    const float hy = m_height * 0.5f;
    const float hz = m_depth * 0.5f;
    box.minX = pos.x() - hx;
    box.maxX = pos.x() + hx;
    box.minY = pos.y() - hy;
    box.maxY = pos.y() + hy;
    box.minZ = pos.z() - hz;
    box.maxZ = pos.z() + hz;
    return box;
}

void LocalFlyingBat::kill()
{
    m_alive = false;
    m_hp = 0;
    m_targeted = false;
    syncVisual();
}

void LocalFlyingBat::applyStats(int maxHp)
{
    if (maxHp > 0) {
        m_maxHp = maxHp;
        m_hp = maxHp;
    }
}

void LocalFlyingBat::takeDamage(int amount, const osg::Vec3& origin)
{
    if (!m_alive || amount <= 0) {
        return;
    }
    m_hp -= amount;
    osg::Vec3 away = pos - origin;
    away.y() = 0.0f;
    if (away.length2() > 1.0e-6f) {
        away.normalize();
        pos = pos + away * 0.35f;
    }
    if (m_hp <= 0) {
        kill();
        return;
    }
    if (m_state == BAT_DIVE_STRIKE) {
        beginFlee(away);
    }
}

void LocalFlyingBat::flyToward(const osg::Vec3& goal, float speed, float dt)
{
    osg::Vec3 d = goal - pos;
    const float dist = d.length();
    if (dist < 0.12f) {
        pos = goal;
        return;
    }
    const float step = speed * dt;
    if (step >= dist) {
        pos = goal;
        return;
    }
    pos = pos + d * (step / dist);
    d.y() = 0.0f;
    if (d.length2() > 1.0e-6f) {
        d.normalize();
        m_diveDir = d;
    }
}

void LocalFlyingBat::beginDive(const osg::Vec3& target)
{
    m_state = BAT_DIVE_STRIKE;
    m_stateTtl = kDiveTtl;
    m_chirpTtl = 0.40f;
    osg::Vec3 dir = target - pos;
    if (dir.length2() < 1.0e-6f) {
        dir.set(0.0f, -1.0f, 0.0f);
    } else {
        dir.normalize();
    }
    m_diveDir = dir;
    std::cout << "[bat] DIVE_STRIKE\n";
}

void LocalFlyingBat::beginFlee(const osg::Vec3& away)
{
    m_state = BAT_DISENGAGE_FLEE;
    m_stateTtl = kFleeTtl;
    m_chirpTtl = 0.0f;
    osg::Vec3 dir = away;
    dir.y() = 0.0f;
    if (dir.length2() < 1.0e-6f) {
        dir = m_diveDir;
        dir.y() = 0.0f;
    }
    if (dir.length2() < 1.0e-6f) {
        dir.set(0.0f, 0.0f, 1.0f);
    } else {
        dir.normalize();
    }
    m_diveDir = dir;
    std::cout << "[bat] DISENGAGE_FLEE\n";
}

void LocalFlyingBat::notifyHit()
{
    osg::Vec3 away = m_diveDir;
    away.y() = 0.0f;
    if (away.length2() < 1.0e-6f) {
        away.set(0.0f, 0.0f, 1.0f);
    } else {
        away.normalize();
    }
    beginFlee(away);
}

void LocalFlyingBat::notifyMiss()
{
    osg::Vec3 away = m_diveDir;
    away.y() = 0.0f;
    if (away.length2() < 1.0e-6f) {
        away.set(0.0f, 0.0f, 1.0f);
    } else {
        away.normalize();
    }
    beginFlee(away);
}

void LocalFlyingBat::notifyWall()
{
    m_state = BAT_STUNNED;
    m_stateTtl = kStunTtl;
    m_chirpTtl = 0.0f;
    std::cout << "[bat] STUNNED\n";
}

void LocalFlyingBat::updateAI(float dt, const osg::Vec3& playerPos, const osg::Vec3& facing,
                              float headY)
{
    if (!m_alive || dt <= 0.0f) {
        return;
    }
    m_time += dt;
    if (m_chirpTtl > 0.0f) {
        m_chirpTtl -= dt;
        if (m_chirpTtl < 0.0f) {
            m_chirpTtl = 0.0f;
        }
    }

    osg::Vec3 face = facing;
    face.y() = 0.0f;
    if (face.length2() < 1.0e-6f) {
        face.set(0.0f, 0.0f, 1.0f);
    } else {
        face.normalize();
    }
    const osg::Vec3 right(-face.z(), 0.0f, face.x());

    if (m_state == BAT_STUNNED) {
        m_stateTtl -= dt;
        pos.y() += std::sin(m_time * 18.0f) * 0.015f;
        if (m_stateTtl <= 0.0f) {
            m_state = BAT_STALK_BEHIND;
            std::cout << "[bat] STALK_BEHIND\n";
        }
        syncVisual();
        return;
    }

    if (m_state == BAT_DISENGAGE_FLEE) {
        m_stateTtl -= dt;
        osg::Vec3 goal = pos + m_diveDir * 8.0f;
        goal.y() = BAT_FLEE_ALT;
        flyToward(goal, kFleeSpeed, dt);
        if (pos.y() < BAT_FLEE_ALT) {
            pos.y() += (BAT_FLEE_ALT - pos.y()) * 6.0f * dt;
        }
        if (m_stateTtl <= 0.0f) {
            m_state = BAT_STALK_BEHIND;
            std::cout << "[bat] STALK_BEHIND\n";
        }
        syncVisual();
        return;
    }

    if (m_state == BAT_DIVE_STRIKE) {
        m_stateTtl -= dt;
        osg::Vec3 target(playerPos.x() - face.x() * 0.20f, headY, playerPos.z() - face.z() * 0.20f);
        osg::Vec3 dir = target - pos;
        if (dir.length2() > 1.0e-6f) {
            dir.normalize();
            m_diveDir = dir;
        }
        pos = pos + m_diveDir * (BAT_DIVE_SPEED * dt);
        if (m_stateTtl <= 0.0f) {
            osg::Vec3 away = pos - playerPos;
            away.y() = 0.0f;
            beginFlee(away);
        }
        syncVisual();
        return;
    }

    // 105.1 STALK_BEHIND: orbitar a 5-7 tiles, dot < -0.65.
    osg::Vec3 toBat(pos.x() - playerPos.x(), 0.0f, pos.z() - playerPos.z());
    float dist = toBat.length();
    if (dist < 0.001f) {
        toBat = -face;
        dist = 1.0f;
    } else {
        toBat = toBat * (1.0f / dist);
    }
    const float toDot = face.x() * toBat.x() + face.z() * toBat.z();
    const float stalkDist = 0.5f * (kStalkMin + kStalkMax);
    osg::Vec3 goal = playerPos - face * stalkDist + right * (m_orbit * 0.85f);
    goal.y() = BAT_CRUISE_ALT;
    if (toDot > kBehindDot) {
        const float side = (toBat.x() * right.x() + toBat.z() * right.z() >= 0.0f) ? 1.0f : -1.0f;
        m_orbit = side;
        goal = playerPos - face * stalkDist + right * (side * 3.20f);
        goal.y() = BAT_CRUISE_ALT;
    }
    goal.x() += right.x() * std::sin(m_time * 1.35f) * 0.55f;
    goal.z() += right.z() * std::sin(m_time * 1.35f) * 0.55f;
    flyToward(goal, kStalkSpeed, dt);
    pos.y() += (BAT_CRUISE_ALT - pos.y()) * 3.5f * dt;

    osg::Vec3 now(pos.x() - playerPos.x(), 0.0f, pos.z() - playerPos.z());
    const float nd = now.length();
    float ndot = 0.0f;
    if (nd > 0.001f) {
        ndot = (face.x() * now.x() + face.z() * now.z()) / nd;
    }
    if (ndot < kBehindDot && nd >= kStalkMin && nd <= kStalkMax + 0.75f) {
        osg::Vec3 target(playerPos.x() - face.x() * 0.20f, headY, playerPos.z() - face.z() * 0.20f);
        beginDive(target);
    }
    syncVisual();
}

} // namespace standalone
} // namespace rc
