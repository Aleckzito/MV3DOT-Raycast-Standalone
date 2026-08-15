#include "LocalEnemy.h"
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

const float kAggroRadius = 8.0f * TILE_SIZE;
const float kAttackRadius = 0.5f * TILE_SIZE;
const float kBossStompRange = 5.0f * TILE_SIZE;
const float kBossStompPeriod = 4.0f;
const float kBossStompTtl = 0.30f;
const float kArcherFar = 10.0f * TILE_SIZE;
const float kArcherNear = 4.0f * TILE_SIZE;
const float kArcherFireCd = 2.0f;

} // namespace

LocalEnemy::LocalEnemy()
    : LocalEnemy(osg::Vec3(0.0f, 0.0f, 0.0f), ENEMY_GRUNT)
{
}

LocalEnemy::LocalEnemy(const osg::Vec3& spawnPos)
    : LocalEnemy(spawnPos, ENEMY_GRUNT)
{
}

LocalEnemy::LocalEnemy(const osg::Vec3& spawnPos, bool isBoss)
    : LocalEnemy(spawnPos, isBoss ? ENEMY_BOSS : ENEMY_GRUNT)
{
}

LocalEnemy::LocalEnemy(const osg::Vec3& spawnPos, EnemyKind kind)
    : pos(spawnPos)
    , isAlive(true)
    , m_speed(2.5f)
    , m_velY(0.0f)
    , m_width(MINI_VOXEL_SIZE)
    , m_height(MINI_VOXEL_SIZE * 2.0f)
    , m_depth(MINI_VOXEL_SIZE)
    , m_hp(30)
    , m_maxHp(30)
    , m_targeted(false)
    , m_kind(kind)
    , m_abilityTimer(0.0f)
    , m_stompTtl(0.0f)
    , m_stompPulse(false)
    , m_arrowShot(false)
    , m_knockbackVel(0.0f, 0.0f, 0.0f)
{
    if (kind == ENEMY_BOSS) {
        m_speed = 2.1f;
        m_width = MINI_VOXEL_SIZE * 1.5f;
        m_height = MINI_VOXEL_SIZE * 2.0f * 1.5f;
        m_depth = MINI_VOXEL_SIZE * 1.5f;
        m_hp = 150;
        m_maxHp = 150;
    } else if (kind == ENEMY_ARCHER) {
        m_speed = 1.35f;
        m_width = MINI_VOXEL_SIZE;
        m_height = MINI_VOXEL_SIZE * 2.5f;
        m_depth = MINI_VOXEL_SIZE;
        m_hp = 25;
        m_maxHp = 25;
        m_abilityTimer = 1.0f;
    }
    buildVisual();
}

void LocalEnemy::applyStats(int maxHp, float speed)
{
    if (maxHp > 0) {
        m_maxHp = maxHp;
        m_hp = maxHp;
    }
    if (speed > 0.0f) {
        m_speed = speed;
    }
}

void LocalEnemy::buildVisual()
{
    const float halfH = m_height * 0.5f;
    // 73.3 osg::Box = AABB total. Boss usa m_width/height/depth x1.5.
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), m_width, m_height, m_depth);
    m_drawable = new osg::ShapeDrawable(box.get());

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_drawable.get());

    m_material = new osg::Material;
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geode.get());

    // 54.3 Anillo de stomp. Cilindro plano en XZ.
    osg::ref_ptr<osg::Cylinder> cyl = new osg::Cylinder(osg::Vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.04f);
    osg::ref_ptr<osg::ShapeDrawable> ringDraw = new osg::ShapeDrawable(cyl.get());
    ringDraw->setColor(osg::Vec4(1.0f, 0.75f, 0.15f, 0.40f));
    osg::ref_ptr<osg::Geode> ringGeode = new osg::Geode;
    ringGeode->addDrawable(ringDraw.get());
    osg::ref_ptr<osg::Material> ringMat = new osg::Material;
    ringMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 0.75f, 0.15f, 0.40f));
    ringMat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.35f, 0.05f, 0.40f));
    ringMat->setAlpha(osg::Material::FRONT_AND_BACK, 0.40f);
    osg::StateSet* ringState = ringGeode->getOrCreateStateSet();
    ringState->setAttributeAndModes(ringMat.get(), osg::StateAttribute::ON);
    ringState->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    ringState->setMode(GL_BLEND, osg::StateAttribute::ON);
    ringState->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    ringState->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                    osg::StateAttribute::ON);

    osg::ref_ptr<osg::PositionAttitudeTransform> ringSpin = new osg::PositionAttitudeTransform;
    ringSpin->setAttitude(osg::Quat(1.5707963, osg::Vec3d(1.0, 0.0, 0.0)));
    ringSpin->addChild(ringGeode.get());

    m_stompPat = new osg::PositionAttitudeTransform;
    m_stompPat->setPosition(osg::Vec3(0.0f, -halfH + 0.04f, 0.0f));
    m_stompPat->addChild(ringSpin.get());
    m_stompPat->setNodeMask(0);
    m_pat->addChild(m_stompPat.get());

    applyTargetTint();
    syncVisual();
}

osg::Node* LocalEnemy::getNode()
{
    return m_pat.get();
}

void LocalEnemy::syncVisual()
{
    if (m_pat.valid()) {
        m_pat->setPosition(osg::Vec3(pos.x(), pos.y() + m_height * 0.5f, pos.z()));
        m_pat->setNodeMask(isAlive ? 0xffffffff : 0);
    }
    tickStompVisual();
}

void LocalEnemy::tickStompVisual()
{
    if (!m_stompPat.valid()) {
        return;
    }
    if (m_stompTtl <= 0.0f || m_kind != ENEMY_BOSS) {
        m_stompPat->setNodeMask(0);
        return;
    }
    const float t = 1.0f - (m_stompTtl / kBossStompTtl);
    const float s = 0.25f + t * 2.75f;
    m_stompPat->setScale(osg::Vec3(s, 1.0f, s));
    m_stompPat->setNodeMask(0xffffffff);
}

void LocalEnemy::setTargeted(bool targeted)
{
    m_targeted = targeted;
    applyTargetTint();
}

void LocalEnemy::applyTargetTint()
{
    if (!m_drawable.valid() || !m_material.valid()) {
        return;
    }
    if (m_targeted) {
        const osg::Vec4 mag(0.95f, 0.20f, 0.85f, 1.0f);
        m_drawable->setColor(mag);
        m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.35f, 0.05f, 0.30f, 1.0f));
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, mag);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.45f, 0.08f, 0.40f, 1.0f));
        return;
    }
    if (m_kind == ENEMY_BOSS) {
        const osg::Vec4 gold(0.80f, 0.60f, 0.00f, 1.0f);
        m_drawable->setColor(gold);
        m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.30f, 0.20f, 0.00f, 1.0f));
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, gold);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.35f, 0.22f, 0.00f, 1.0f));
        return;
    }
    if (m_kind == ENEMY_ARCHER) {
        const osg::Vec4 green(0.10f, 0.90f, 0.30f, 1.0f);
        m_drawable->setColor(green);
        m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.03f, 0.28f, 0.08f, 1.0f));
        m_material->setDiffuse(osg::Material::FRONT_AND_BACK, green);
        m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.04f, 0.35f, 0.10f, 1.0f));
        return;
    }
    const osg::Vec4 red(0.55f, 0.08f, 0.08f, 1.0f);
    m_drawable->setColor(red);
    m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.20f, 0.02f, 0.02f, 1.0f));
    m_material->setDiffuse(osg::Material::FRONT_AND_BACK, red);
    m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.12f, 0.01f, 0.01f, 1.0f));
}

bool LocalEnemy::consumeStompPulse()
{
    const bool pulse = m_stompPulse;
    m_stompPulse = false;
    return pulse;
}

bool LocalEnemy::consumeArrowShot()
{
    const bool shot = m_arrowShot;
    m_arrowShot = false;
    return shot;
}

osg::Vec3 LocalEnemy::muzzle() const
{
    return osg::Vec3(pos.x(), pos.y() + m_height * 0.78f, pos.z());
}

void LocalEnemy::kill()
{
    setTargeted(false);
    isAlive = false;
    m_hp = 0;
    m_velY = 0.0f;
    m_stompTtl = 0.0f;
    m_stompPulse = false;
    m_arrowShot = false;
    m_knockbackVel = osg::Vec3(0.0f, 0.0f, 0.0f);
    syncVisual();
}

void LocalEnemy::takeDamage(int amount, const osg::Vec3& origin)
{
    if (!isAlive || amount <= 0) {
        return;
    }
    m_hp -= amount;

    float dx = pos.x() - origin.x();
    float dz = pos.z() - origin.z();
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len > 0.0001f) {
        const float inv = 1.0f / len;
        const float force = 12.0f;
        m_knockbackVel = osg::Vec3(dx * inv * force, 0.0f, dz * inv * force);
    }

    if (m_hp <= 0) {
        kill();
    }
}

void LocalEnemy::updateAI(float dt, const osg::Vec3& playerPos)
{
    if (!isAlive || dt <= 0.0f) {
        return;
    }

    pos.x() += m_knockbackVel.x() * dt;
    pos.z() += m_knockbackVel.z() * dt;
    float damp = 1.0f - 8.0f * dt;
    if (damp < 0.0f) {
        damp = 0.0f;
    }
    m_knockbackVel.x() *= damp;
    m_knockbackVel.z() *= damp;
    if (m_knockbackVel.x() * m_knockbackVel.x() + m_knockbackVel.z() * m_knockbackVel.z() < 0.04f) {
        m_knockbackVel.x() = 0.0f;
        m_knockbackVel.z() = 0.0f;
    }

    const float dx = playerPos.x() - pos.x();
    const float dz = playerPos.z() - pos.z();
    const float dist = std::sqrt(dx * dx + dz * dz);

    if (m_kind == ENEMY_BOSS) {
        if (m_stompTtl > 0.0f) {
            m_stompTtl -= dt;
            if (m_stompTtl < 0.0f) {
                m_stompTtl = 0.0f;
            }
        }
        if (dist <= kBossStompRange) {
            m_abilityTimer += dt;
            if (m_abilityTimer >= kBossStompPeriod) {
                m_abilityTimer = 0.0f;
                m_stompTtl = kBossStompTtl;
                m_stompPulse = true;
            }
        }
    }

    if (m_kind == ENEMY_ARCHER) {
        return;
    }

    if (dist > kAggroRadius || dist <= kAttackRadius) {
        return;
    }
    if (dist < 0.0001f) {
        return;
    }

    const float inv = 1.0f / dist;
    pos.x() += dx * inv * m_speed * dt;
    pos.z() += dz * inv * m_speed * dt;
}

void LocalEnemy::updateArcherAI(float dt, const osg::Vec3& playerPos, bool hasLos, const osg::Vec3& coverPos)
{
    if (!isAlive || dt <= 0.0f || m_kind != ENEMY_ARCHER) {
        return;
    }

    pos.x() += m_knockbackVel.x() * dt;
    pos.z() += m_knockbackVel.z() * dt;
    float damp = 1.0f - 8.0f * dt;
    if (damp < 0.0f) {
        damp = 0.0f;
    }
    m_knockbackVel.x() *= damp;
    m_knockbackVel.z() *= damp;

    const float dx = playerPos.x() - pos.x();
    const float dz = playerPos.z() - pos.z();
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (m_abilityTimer > 0.0f) {
        m_abilityTimer -= dt;
    }

    auto moveToward = [&](float tx, float tz, float spd) {
        const float mx = tx - pos.x();
        const float mz = tz - pos.z();
        const float md = std::sqrt(mx * mx + mz * mz);
        if (md < 0.08f) {
            return;
        }
        const float inv = 1.0f / md;
        pos.x() += mx * inv * spd * dt;
        pos.z() += mz * inv * spd * dt;
    };

    if (dist < kArcherNear) {
        // FASE 3: kiting a cobertura (pilar entre jugador y arquero).
        float hx = coverPos.x() - playerPos.x();
        float hz = coverPos.z() - playerPos.z();
        const float hl = std::sqrt(hx * hx + hz * hz);
        osg::Vec3 hide = coverPos;
        if (hl > 0.0001f) {
            const float inv = 1.0f / hl;
            hide.x() = coverPos.x() + hx * inv * 1.45f;
            hide.z() = coverPos.z() + hz * inv * 1.45f;
        }
        moveToward(hide.x(), hide.z(), 3.20f);
        return;
    }

    if (dist > kArcherFar) {
        // FASE 1: avance lento a la arena / jugador. Sin disparo.
        if (dist > 0.0001f) {
            moveToward(playerPos.x(), playerPos.z(), 1.10f);
        }
        return;
    }

    // FASE 2: 4..10 tiles. LOS = parar y disparar cada 2s. Sin LOS = peek.
    if (hasLos) {
        if (m_abilityTimer <= 0.0f) {
            m_abilityTimer = kArcherFireCd;
            m_arrowShot = true;
        }
        return;
    }
    if (dist > 0.0001f) {
        moveToward(playerPos.x(), playerPos.z(), 1.35f);
    }
}

AABB LocalEnemy::makeAabb() const
{
    return makeAabbAt(pos.x(), pos.y(), pos.z());
}

AABB LocalEnemy::makeAabbAt(float x, float y, float z) const
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
