#include "LocalBuddyController.h"
#include "MiniVoxelGrid.h"

#include <osg/Geometry>
#include <osg/Geode>
#include <osg/GL>
#include <osg/LineWidth>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/PrimitiveSet>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/Vec3>
#include <osg/Vec4>

#include <cmath>

namespace rc {
namespace standalone {

LocalBuddyController::LocalBuddyController()
    : m_pos(4.0f, 1.4f, 6.0f)
    , m_yaw(0.0f)
    , m_time(0.0f)
    , m_stateTtl(0.0f)
    , m_side(1.0f)
    , m_state(BUDDY_PATROL)
    , m_hasPos(false)
    , m_hidden(false)
    , m_overloadDone(false)
    , m_threatBehind(false)
    , m_hasFetch(false)
    , m_fetchHint(0.0f, 0.0f, 0.0f)
    , m_fetchFlashTtl(0.0f)
{
    buildVisual();
}

void LocalBuddyController::buildVisual()
{
    // 77.2 Cubo naranja 0.45 + aristas LineSegments.
    osg::ref_ptr<osg::Box> cube = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.45f);
    m_drawable = new osg::ShapeDrawable(cube.get());
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_drawable.get());

    m_material = new osg::Material;
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    const float h = 0.45f * 0.5f;
    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
    verts->push_back(osg::Vec3(-h, -h, -h));
    verts->push_back(osg::Vec3( h, -h, -h));
    verts->push_back(osg::Vec3( h,  h, -h));
    verts->push_back(osg::Vec3(-h,  h, -h));
    verts->push_back(osg::Vec3(-h, -h,  h));
    verts->push_back(osg::Vec3( h, -h,  h));
    verts->push_back(osg::Vec3( h,  h,  h));
    verts->push_back(osg::Vec3(-h,  h,  h));
    const unsigned short idx[24] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    osg::ref_ptr<osg::DrawElementsUShort> lines = new osg::DrawElementsUShort(GL_LINES, 24, idx);
    m_lineColor = new osg::Vec4Array;
    m_lineColor->push_back(osg::Vec4(1.00f, 0.95f, 0.55f, 1.0f));
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    geom->setVertexArray(verts.get());
    geom->setColorArray(m_lineColor.get(), osg::Array::BIND_OVERALL);
    geom->addPrimitiveSet(lines.get());
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    osg::ref_ptr<osg::Geode> lineGeode = new osg::Geode;
    lineGeode->addDrawable(geom.get());
    osg::StateSet* ls = lineGeode->getOrCreateStateSet();
    ls->setAttributeAndModes(new osg::LineWidth(1.6f), osg::StateAttribute::ON);
    ls->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ls->setMode(GL_BLEND, osg::StateAttribute::OFF);

    m_node = new osg::MatrixTransform;
    m_node->addChild(geode.get());
    m_node->addChild(lineGeode.get());
    applyTint();
}

void LocalBuddyController::applyTint()
{
    // Base LEAD 0xff6b2d. Rearguard rojo alerta. Burst/Overload = 0xffd24a.
    osg::Vec4 col(1.00f, 0.420f, 0.176f, 1.0f);
    osg::Vec4 emit(0.35f, 0.12f, 0.03f, 1.0f);
    const bool busy = (m_state == BUDDY_OVERLOAD || m_state == BUDDY_BURST);
    if (busy) {
        col.set(1.00f, 0.824f, 0.290f, 1.0f);
        emit.set(1.00f, 0.965f, 0.627f, 1.0f);
    } else if (m_threatBehind) {
        col.set(1.00f, 0.25f, 0.10f, 1.0f);
        emit.set(0.65f, 0.08f, 0.02f, 1.0f);
    } else if (m_state == BUDDY_EXCITE) {
        col.set(1.00f, 0.70f, 0.20f, 1.0f);
        emit.set(0.40f, 0.22f, 0.04f, 1.0f);
    } else if (m_state == BUDDY_SAD) {
        col.set(0.55f, 0.28f, 0.10f, 1.0f);
        emit.set(0.08f, 0.04f, 0.01f, 1.0f);
    }
    if (m_drawable.valid()) {
        m_drawable->setColor(col);
    }
    if (m_material.valid()) {
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, emit);
        if (busy) {
            m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(1.00f, 0.965f, 0.627f, 1.0f));
            m_material->setShininess(osg::Material::FRONT_AND_BACK, 48.0f);
            m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.45f, 0.32f, 0.08f, 1.0f));
        } else if (m_threatBehind) {
            m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(1.00f, 0.35f, 0.12f, 1.0f));
            m_material->setShininess(osg::Material::FRONT_AND_BACK, 28.0f);
            m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.08f, 0.04f, 1.0f));
        } else {
            m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.22f, 0.06f, 1.0f));
            m_material->setShininess(osg::Material::FRONT_AND_BACK, 16.0f);
            m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.25f, 0.12f, 0.04f, 1.0f));
        }
    }
    if (m_lineColor.valid() && !m_lineColor->empty()) {
        if (busy) {
            (*m_lineColor)[0].set(1.00f, 0.965f, 0.627f, 1.0f);
        } else if (m_threatBehind) {
            (*m_lineColor)[0].set(1.00f, 0.45f, 0.20f, 1.0f);
        } else {
            (*m_lineColor)[0].set(1.00f, 0.95f, 0.55f, 1.0f);
        }
        m_lineColor->dirty();
    }
}

osg::Node* LocalBuddyController::getNode()
{
    return m_node.get();
}

const char* LocalBuddyController::stateName() const
{
    if (m_state == BUDDY_EXCITE) {
        return "Excite";
    }
    if (m_state == BUDDY_SAD) {
        return "Sad";
    }
    if (m_state == BUDDY_OVERLOAD) {
        return "Overload";
    }
    if (m_state == BUDDY_BURST) {
        return "Burst";
    }
    if (m_threatBehind) {
        return "Rearguard";
    }
    return "Patrol";
}

void LocalBuddyController::notifyPlace()
{
    if (m_state == BUDDY_BURST || m_state == BUDDY_OVERLOAD) {
        return;
    }
    m_state = BUDDY_EXCITE;
    m_stateTtl = 1.20f;
    applyTint();
}

void LocalBuddyController::notifyDestroy()
{
    if (m_state == BUDDY_BURST || m_state == BUDDY_OVERLOAD) {
        return;
    }
    m_state = BUDDY_SAD;
    m_stateTtl = 1.80f;
    applyTint();
}

void LocalBuddyController::notifyFetch()
{
    if (m_state == BUDDY_BURST || m_state == BUDDY_OVERLOAD) {
        m_fetchFlashTtl = 0.35f;
        return;
    }
    m_state = BUDDY_EXCITE;
    m_stateTtl = 0.55f;
    m_fetchFlashTtl = 0.35f;
    applyTint();
}

void LocalBuddyController::setFetchHint(const osg::Vec3& pos)
{
    m_fetchHint = pos;
    m_hasFetch = true;
}

void LocalBuddyController::clearFetchHint()
{
    m_hasFetch = false;
}

bool LocalBuddyController::inFetchRange(const osg::Vec3& dropPos) const
{
    const osg::Vec3 d = dropPos - m_pos;
    return d.length() <= 2.50f;
}

void LocalBuddyController::setOverload(bool on)
{
    if (on) {
        startOverload(4.0f);
    } else if (m_state == BUDDY_OVERLOAD) {
        m_state = BUDDY_PATROL;
        m_stateTtl = 0.0f;
        applyTint();
    }
}

void LocalBuddyController::startBurst(float seconds)
{
    m_state = BUDDY_BURST;
    m_stateTtl = seconds;
    applyTint();
}

void LocalBuddyController::startOverload(float seconds)
{
    m_state = BUDDY_OVERLOAD;
    m_stateTtl = seconds;
    m_overloadDone = false;
    applyTint();
}

bool LocalBuddyController::consumeOverloadDone()
{
    const bool v = m_overloadDone;
    m_overloadDone = false;
    return v;
}

void LocalBuddyController::setHidden(bool hidden)
{
    m_hidden = hidden;
    if (m_node.valid()) {
        m_node->setNodeMask(hidden ? 0 : 0xffffffff);
    }
}

osg::Vec3 LocalBuddyController::goalForState(const osg::Vec3& playerPos, const osg::Vec3& facing) const
{
    osg::Vec3 right(-facing.z(), 0.0f, facing.x());
    const float rlen = right.length();
    if (rlen > 0.0001f) {
        right = right * (1.0f / rlen);
    } else {
        right.set(1.0f, 0.0f, 0.0f);
    }

    const bool busy = (m_state == BUDDY_OVERLOAD || m_state == BUDDY_BURST);
    osg::Vec3 goal = playerPos;

    if (!busy && m_threatBehind) {
        // 86.1 REARGUARD: -2.0 frente, weave invertido.
        goal = playerPos - facing * 2.0f + right * (m_side * 1.5f);
        goal.y() += 0.95f + std::sin(m_time * 12.0f) * 0.22f;
        return goal;
    }

    if (m_state == BUDDY_PATROL || m_state == BUDDY_BURST) {
        // 86.2 LEAD_WEAVE: +2.15 frente.
        const float mul = (m_state == BUDDY_BURST) ? 5.5f : 1.0f;
        goal = playerPos + facing * 2.15f + right * (m_side * 1.85f);
        goal.y() += 0.95f + std::sin(m_time * 3.1f * mul) * 0.08f;
    } else if (m_state == BUDDY_EXCITE) {
        const float ang = m_time * 8.0f;
        goal.x() += std::cos(ang) * 1.10f;
        goal.y() += 1.35f + std::fabs(std::sin(m_time * 10.0f)) * 0.45f;
        goal.z() += std::sin(ang) * 1.10f;
    } else if (m_state == BUDDY_SAD) {
        goal = playerPos + facing * 0.70f;
        goal.y() += 0.35f;
    } else {
        goal = playerPos + facing * 1.10f;
        goal.y() += 1.20f;
    }
    return goal;
}

void LocalBuddyController::update(float dt, const osg::Vec3& playerPos, const osg::Vec3& facing,
                                 float bodyYaw, bool threatBehind)
{
    (void)bodyYaw;
    m_time += dt;
    if (m_state != BUDDY_PATROL) {
        m_stateTtl -= dt;
        if (m_stateTtl <= 0.0f) {
            if (m_state == BUDDY_OVERLOAD) {
                m_overloadDone = true;
            }
            m_state = BUDDY_PATROL;
            m_stateTtl = 0.0f;
            applyTint();
        }
    }

    const bool busy = (m_state == BUDDY_OVERLOAD || m_state == BUDDY_BURST);
    const bool alert = threatBehind && !busy;
    if (alert) {
        m_side = -std::sin(m_time * 8.0f);
    } else {
        m_side = std::sin(m_time * 2.4f);
    }
    if (alert != m_threatBehind) {
        m_threatBehind = alert;
        applyTint();
    } else {
        m_threatBehind = alert;
    }

    if (alert && m_material.valid() && m_drawable.valid()) {
        const float pulse = 0.55f + 0.45f * (0.5f + 0.5f * std::sin(m_time * 14.0f));
        osg::Vec4 col(1.00f * pulse, 0.25f * pulse, 0.10f, 1.0f);
        osg::Vec4 emit(0.65f * pulse, 0.08f, 0.02f, 1.0f);
        m_drawable->setColor(col);
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, emit);
    }

    const osg::Vec3 goal = goalForState(playerPos, facing);
    osg::Vec3 want = goal;
    float lerp = 5.5f;
    if (m_state == BUDDY_SAD) {
        lerp = 1.8f;
    } else if (m_state == BUDDY_EXCITE) {
        lerp = 9.0f;
    } else if (m_state == BUDDY_BURST) {
        lerp = 5.5f * 5.5f;
    } else if (alert) {
        lerp = 8.0f;
    }
    if (m_hasFetch && !busy && !alert) {
        want = m_fetchHint;
        want.y() += 0.15f;
        lerp = 11.0f;
    }
    if (!m_hasPos) {
        m_pos = want;
        m_hasPos = true;
    }
    float t = lerp * dt;
    if (t > 1.0f) {
        t = 1.0f;
    }
    m_pos = m_pos + (want - m_pos) * t;

    const float tx = playerPos.x() - m_pos.x();
    const float tz = playerPos.z() - m_pos.z();
    const float wantYaw = std::atan2(tx, tz);
    float dyaw = wantYaw - m_yaw;
    while (dyaw > 3.14159265f) {
        dyaw -= 6.2831853f;
    }
    while (dyaw < -3.14159265f) {
        dyaw += 6.2831853f;
    }
    float spin = 6.0f;
    if (m_state == BUDDY_SAD) {
        spin = 2.0f;
    } else if (m_state == BUDDY_BURST) {
        spin = 18.0f;
    } else if (alert) {
        spin = 8.0f;
    }
    m_yaw += dyaw * spin * dt;

    if (m_fetchFlashTtl > 0.0f) {
        m_fetchFlashTtl -= dt;
        if (m_fetchFlashTtl < 0.0f) {
            m_fetchFlashTtl = 0.0f;
        }
        if (m_drawable.valid() && m_material.valid()) {
            const float pulse = 0.55f + 0.45f * (m_fetchFlashTtl / 0.35f);
            osg::Vec4 col(0.35f * pulse, 0.85f * pulse, 1.00f, 1.0f);
            m_drawable->setColor(col);
            m_material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
            m_material->setEmission(osg::Material::FRONT_AND_BACK,
                                   osg::Vec4(0.20f * pulse, 0.55f * pulse, 0.95f * pulse, 1.0f));
        }
    }

    if (m_node.valid()) {
        osg::Matrix rot = osg::Matrix::rotate(m_yaw, osg::Vec3(0.0f, 1.0f, 0.0f));
        osg::Matrix tr = osg::Matrix::translate(m_pos);
        m_node->setMatrix(rot * tr);
        m_node->setNodeMask(m_hidden ? 0 : 0xffffffff);
    }
}

} // namespace standalone
} // namespace rc
