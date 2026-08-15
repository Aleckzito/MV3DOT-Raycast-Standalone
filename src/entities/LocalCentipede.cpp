#include "LocalCentipede.h"

#include "LocalBoxWorld.h"
#include "LocalBoulder.h"

#include <osg/Geode>
#include <osg/GL>
#include <osg/PolygonMode>
#include <osg/Quat>
#include <osg/Shape>
#include <osg/StateSet>

#include <cmath>

namespace rc {
namespace standalone {

namespace {

int worldToVoxelIndex(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

bool aabbOverlap(const AABB& a, const AABB& b)
{
    return a.minX <= b.maxX && a.maxX >= b.minX &&
           a.minY <= b.maxY && a.maxY >= b.minY &&
           a.minZ <= b.maxZ && a.maxZ >= b.minZ;
}

} // namespace

LocalCentipede::LocalCentipede()
    : m_alive(false)
    , m_orbit(false)
    , m_dirX(1.0f)
    , m_rowSign(1.0f)
    , m_speed(CENTIPEDE_SPEED)
    , m_stunTtl(0.0f)
    , m_orbitAng(0.0f)
    , m_size(MINI_VOXEL_SIZE * 0.92f)
    , m_lockedSeg(-1)
    , m_lastDeadPos(0.0f, 0.0f, 0.0f)
{
    m_root = new osg::Group;
}

LocalCentipede::LocalCentipede(const osg::Vec3& headPos, int nSeg, float dirX)
    : LocalCentipede()
{
    spawnChain(headPos, nSeg, dirX);
}

void LocalCentipede::spawnChain(const osg::Vec3& headPos, int nSeg, float dirX)
{
    int n = nSeg;
    if (n < CENTIPEDE_SEG_MIN) {
        n = CENTIPEDE_SEG_MIN;
    }
    if (n > CENTIPEDE_SEG_MAX) {
        n = CENTIPEDE_SEG_MAX;
    }
    std::vector<osg::Vec3> pos;
    std::vector<int> hp;
    const float dx = (dirX >= 0.0f) ? 1.0f : -1.0f;
    for (int i = 0; i < n; ++i) {
        pos.push_back(osg::Vec3(
            headPos.x() - dx * CENTIPEDE_SPACING * static_cast<float>(i),
            headPos.y(),
            headPos.z()));
        hp.push_back(CENTIPEDE_SEG_HP);
    }
    buildFrom(pos, hp, dx);
}

void LocalCentipede::buildFrom(const std::vector<osg::Vec3>& positions,
                              const std::vector<int>& hps, float dirX)
{
    if (m_root.valid()) {
        m_root->removeChildren(0, m_root->getNumChildren());
    } else {
        m_root = new osg::Group;
    }
    m_segments.clear();
    m_dirX = (dirX >= 0.0f) ? 1.0f : -1.0f;
    m_rowSign = 1.0f;
    m_orbit = false;
    m_stunTtl = 0.0f;
    m_lockedSeg = -1;
    m_alive = !positions.empty();
    for (size_t i = 0; i < positions.size(); ++i) {
        CentipedeSegment seg;
        seg.pos = positions[i];
        seg.isHead = (i == 0);
        seg.alive = true;
        seg.hp = (i < hps.size()) ? hps[i] : CENTIPEDE_SEG_HP;
        seg.maxHp = CENTIPEDE_SEG_HP;
        if (seg.hp < 1) {
            seg.hp = CENTIPEDE_SEG_HP;
        }
        buildSegmentVisual(seg);
        m_segments.push_back(seg);
        if (m_root.valid() && m_segments.back().pat.valid()) {
            m_root->addChild(m_segments.back().pat.get());
        }
    }
    syncVisual();
}

void LocalCentipede::buildSegmentVisual(CentipedeSegment& seg)
{
    const float edge = m_size;
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), edge, edge, edge);
    seg.bodyDraw = new osg::ShapeDrawable(box.get());
    seg.bodyDraw->setColor(CENTIPEDE_COLOR);
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(seg.bodyDraw.get());

    if (seg.isHead) {
        osg::ref_ptr<osg::Box> eyeL = new osg::Box(
            osg::Vec3(edge * 0.22f, edge * 0.18f, edge * 0.38f),
            edge * 0.18f, edge * 0.18f, edge * 0.16f);
        osg::ref_ptr<osg::Box> eyeR = new osg::Box(
            osg::Vec3(-edge * 0.22f, edge * 0.18f, edge * 0.38f),
            edge * 0.18f, edge * 0.18f, edge * 0.16f);
        osg::ref_ptr<osg::ShapeDrawable> el = new osg::ShapeDrawable(eyeL.get());
        osg::ref_ptr<osg::ShapeDrawable> er = new osg::ShapeDrawable(eyeR.get());
        el->setColor(CENTIPEDE_EYE);
        er->setColor(CENTIPEDE_EYE);
        geode->addDrawable(el.get());
        geode->addDrawable(er.get());
    }

    seg.material = new osg::Material;
    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(seg.material.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    osg::ref_ptr<osg::Box> frame = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), edge * 1.18f, edge * 1.18f, edge * 1.18f);
    osg::ref_ptr<osg::ShapeDrawable> frameDraw = new osg::ShapeDrawable(frame.get());
    frameDraw->setColor(CENTIPEDE_LOCK);
    osg::ref_ptr<osg::Geode> frameGeode = new osg::Geode;
    frameGeode->addDrawable(frameDraw.get());
    osg::StateSet* fs = frameGeode->getOrCreateStateSet();
    fs->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK,
                                                  osg::PolygonMode::LINE),
                             osg::StateAttribute::ON);
    fs->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    frameGeode->setNodeMask(0);
    seg.lockBox = frameGeode.get();

    seg.pat = new osg::PositionAttitudeTransform;
    seg.pat->addChild(geode.get());
    seg.pat->addChild(seg.lockBox.get());
    applyTint(seg, false);
}

void LocalCentipede::applyTint(CentipedeSegment& seg, bool locked)
{
    osg::Vec4 col = CENTIPEDE_COLOR;
    osg::Vec4 emit(0.18f, 0.28f, 0.02f, 1.0f);
    if (locked) {
        col = CENTIPEDE_LOCK;
        emit.set(0.45f, 0.35f, 0.04f, 1.0f);
    }
    if (seg.bodyDraw.valid()) {
        seg.bodyDraw->setColor(col);
    }
    if (seg.material.valid()) {
        seg.material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
        seg.material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.18f, 0.22f, 0.04f, 1.0f));
        seg.material->setEmission(osg::Material::FRONT_AND_BACK, emit);
        seg.material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.25f, 0.30f, 0.08f, 1.0f));
        seg.material->setShininess(osg::Material::FRONT_AND_BACK, 18.0f);
    }
    if (seg.lockBox.valid()) {
        seg.lockBox->setNodeMask(locked ? 0xffffffff : 0);
    }
}

osg::Node* LocalCentipede::getNode()
{
    return m_root.get();
}

void LocalCentipede::syncVisual()
{
    for (size_t i = 0; i < m_segments.size(); ++i) {
        CentipedeSegment& seg = m_segments[i];
        if (!seg.pat.valid()) {
            continue;
        }
        seg.pat->setPosition(seg.pos);
        const float yaw = (m_dirX >= 0.0f) ? 1.5707963f : -1.5707963f;
        seg.pat->setAttitude(osg::Quat(yaw, osg::Vec3(0.0f, 1.0f, 0.0f)));
        seg.pat->setNodeMask((m_alive && seg.alive) ? 0xffffffff : 0);
        applyTint(seg, static_cast<int>(i) == m_lockedSeg);
    }
}

void LocalCentipede::setLockedSegment(int index)
{
    m_lockedSeg = index;
    syncVisual();
}

AABB LocalCentipede::makeAabb(int i) const
{
    AABB box;
    box.minX = box.maxX = box.minY = box.maxY = box.minZ = box.maxZ = 0.0f;
    if (i < 0 || i >= static_cast<int>(m_segments.size())) {
        return box;
    }
    const osg::Vec3& p = m_segments[static_cast<size_t>(i)].pos;
    const float h = m_size * 0.5f;
    box.minX = p.x() - h;
    box.maxX = p.x() + h;
    box.minY = p.y() - h;
    box.maxY = p.y() + h;
    box.minZ = p.z() - h;
    box.maxZ = p.z() + h;
    return box;
}

int LocalCentipede::totalHp() const
{
    int hp = 0;
    for (size_t i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].alive) {
            hp += m_segments[i].hp;
        }
    }
    return hp;
}

int LocalCentipede::totalMaxHp() const
{
    return static_cast<int>(m_segments.size()) * CENTIPEDE_SEG_HP;
}

void LocalCentipede::kill()
{
    m_alive = false;
    m_lockedSeg = -1;
    for (size_t i = 0; i < m_segments.size(); ++i) {
        m_segments[i].alive = false;
        m_segments[i].hp = 0;
    }
    if (m_root.valid()) {
        m_root->setNodeMask(0);
    }
    syncVisual();
}

void LocalCentipede::promoteHead(int index)
{
    if (index < 0 || index >= static_cast<int>(m_segments.size())) {
        return;
    }
    if (m_root.valid() && m_segments[static_cast<size_t>(index)].pat.valid()) {
        m_root->removeChild(m_segments[static_cast<size_t>(index)].pat.get());
    }
    m_segments[static_cast<size_t>(index)].isHead = true;
    buildSegmentVisual(m_segments[static_cast<size_t>(index)]);
    if (m_root.valid() && m_segments[static_cast<size_t>(index)].pat.valid()) {
        m_root->addChild(m_segments[static_cast<size_t>(index)].pat.get());
    }
}

bool LocalCentipede::probeBlocked(const osg::Vec3& probe, const MiniVoxelGrid& grid,
                                  const LocalBoxWorld& boxes, const LocalBoulderWorld& boulders) const
{
    if (probe.x() < 0.22f || probe.x() > 7.78f || probe.z() < 0.18f || probe.z() > 7.78f) {
        return true;
    }
    const int vx = worldToVoxelIndex(probe.x());
    const int vy = worldToVoxelIndex(probe.y());
    const int vz = worldToVoxelIndex(probe.z());
    if (vy >= 0 && grid.getVoxel(vx, vy, vz).isActive) {
        return true;
    }
    float hitT = 0.0f;
    if (!m_segments.empty() && boxes.rayHits(m_segments[0].pos, probe, &hitT)) {
        return true;
    }
    const float h = m_size * 0.5f;
    AABB probeBox;
    probeBox.minX = probe.x() - h;
    probeBox.maxX = probe.x() + h;
    probeBox.minY = probe.y() - h;
    probeBox.maxY = probe.y() + h;
    probeBox.minZ = probe.z() - h;
    probeBox.maxZ = probe.z() + h;
    const int nB = boulders.boulderCount();
    for (int b = 0; b < nB; ++b) {
        if (!boulders.boulderAlive(b)) {
            continue;
        }
        if (aabbOverlap(probeBox, boulders.makeAabb(b))) {
            return true;
        }
    }
    return false;
}

void LocalCentipede::followBody()
{
    for (size_t i = 1; i < m_segments.size(); ++i) {
        osg::Vec3 delta = m_segments[i - 1].pos - m_segments[i].pos;
        const float len = delta.length();
        if (len < 1.0e-5f) {
            m_segments[i].pos.x() = m_segments[i - 1].pos.x() - m_dirX * CENTIPEDE_SPACING;
            continue;
        }
        delta = delta * (1.0f / len);
        m_segments[i].pos = m_segments[i - 1].pos - delta * CENTIPEDE_SPACING;
    }
}

void LocalCentipede::updateMovement(float dt, const MiniVoxelGrid& grid, const LocalBoxWorld& boxes,
                                    const LocalBoulderWorld& boulders, const osg::Vec3& playerPos)
{
    if (!m_alive || m_segments.empty() || dt <= 0.0f) {
        return;
    }
    if (m_stunTtl > 0.0f) {
        m_stunTtl -= dt;
        const float wiggle = std::sin(m_stunTtl * 28.0f) * 0.04f;
        m_segments[0].pos.x() += wiggle;
        followBody();
        syncVisual();
        return;
    }

    CentipedeSegment& head = m_segments[0];
    const float dxp = playerPos.x() - head.pos.x();
    const float dzp = playerPos.z() - head.pos.z();
    const float distP = std::sqrt(dxp * dxp + dzp * dzp);
    if (head.pos.y() <= 0.42f && distP < 7.50f) {
        m_orbit = true;
    }

    if (m_orbit) {
        m_orbitAng += dt * 2.35f;
        const float r = 2.35f;
        const osg::Vec3 goal(
            playerPos.x() + std::cos(m_orbitAng) * r,
            0.12f,
            playerPos.z() + std::sin(m_orbitAng) * r);
        osg::Vec3 d = goal - head.pos;
        const float len = d.length();
        if (len > 0.0001f) {
            float step = m_speed * dt;
            if (step > len) {
                step = len;
            }
            head.pos = head.pos + d * (step / len);
            m_dirX = (d.x() >= 0.0f) ? 1.0f : -1.0f;
        }
    } else {
        const float look = m_size * 0.70f + 0.06f;
        const osg::Vec3 probe(head.pos.x() + m_dirX * look, head.pos.y(), head.pos.z());
        if (probeBlocked(probe, grid, boxes, boulders)) {
            m_dirX = -m_dirX;
            head.pos.z() += m_rowSign * MINI_VOXEL_SIZE;
            if (head.pos.y() > 0.20f) {
                head.pos.y() -= MINI_VOXEL_SIZE;
                if (head.pos.y() < 0.12f) {
                    head.pos.y() = 0.12f;
                }
            }
            if (head.pos.z() > 7.55f) {
                head.pos.z() = 7.55f;
                m_rowSign = -1.0f;
            } else if (head.pos.z() < 0.22f) {
                head.pos.z() = 0.22f;
                m_rowSign = 1.0f;
            }
        } else {
            head.pos.x() += m_dirX * m_speed * dt;
        }
    }
    followBody();
    syncVisual();
}

CentipedeHit LocalCentipede::takeDamageAtSegment(int segmentIndex, int dmg, LocalCentipede* outSplit)
{
    if (!m_alive || dmg <= 0 || segmentIndex < 0 ||
        segmentIndex >= static_cast<int>(m_segments.size())) {
        return CENTI_HIT_NONE;
    }
    CentipedeSegment& hit = m_segments[static_cast<size_t>(segmentIndex)];
    hit.hp -= dmg;
    if (hit.hp > 0) {
        return CENTI_HIT_ALIVE;
    }

    m_lastDeadPos = hit.pos;
    const int n = static_cast<int>(m_segments.size());
    const bool wasHead = (segmentIndex == 0);
    const bool intermediate = (segmentIndex >= 1 && segmentIndex <= n - 2);

    if (wasHead) {
        if (m_root.valid() && hit.pat.valid()) {
            m_root->removeChild(hit.pat.get());
        }
        m_segments.erase(m_segments.begin());
        if (m_lockedSeg == 0) {
            m_lockedSeg = -1;
        } else if (m_lockedSeg > 0) {
            m_lockedSeg -= 1;
        }
        if (m_segments.empty()) {
            kill();
            return CENTI_HIT_DEAD;
        }
        promoteHead(0);
        m_stunTtl = 0.85f;
        m_orbit = false;
        syncVisual();
        return CENTI_HIT_TRIM;
    }

    if (intermediate && outSplit != nullptr) {
        std::vector<osg::Vec3> tailPos;
        std::vector<int> tailHp;
        for (int i = segmentIndex + 1; i < n; ++i) {
            tailPos.push_back(m_segments[static_cast<size_t>(i)].pos);
            tailHp.push_back(m_segments[static_cast<size_t>(i)].hp);
        }
        for (int i = n - 1; i >= segmentIndex; --i) {
            if (m_root.valid() && m_segments[static_cast<size_t>(i)].pat.valid()) {
                m_root->removeChild(m_segments[static_cast<size_t>(i)].pat.get());
            }
            m_segments.erase(m_segments.begin() + i);
        }
        if (m_lockedSeg >= segmentIndex) {
            m_lockedSeg = -1;
        }
        outSplit->buildFrom(tailPos, tailHp, -m_dirX);
        if (m_segments.empty()) {
            kill();
            return CENTI_HIT_DEAD;
        }
        syncVisual();
        return CENTI_HIT_SPLIT;
    }

    if (m_root.valid() && hit.pat.valid()) {
        m_root->removeChild(hit.pat.get());
    }
    m_segments.erase(m_segments.begin() + segmentIndex);
    if (m_lockedSeg == segmentIndex) {
        m_lockedSeg = -1;
    } else if (m_lockedSeg > segmentIndex) {
        m_lockedSeg -= 1;
    }
    if (m_segments.empty()) {
        kill();
        return CENTI_HIT_DEAD;
    }
    syncVisual();
    return CENTI_HIT_TRIM;
}

} // namespace standalone
} // namespace rc
