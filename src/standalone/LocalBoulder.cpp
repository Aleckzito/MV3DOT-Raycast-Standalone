#include "LocalBoulder.h"

#include "LocalPhysicsSolver.h"

#include <osg/Geode>
#include <osg/GL>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/PolygonOffset>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>
#include <iostream>

namespace rc {
namespace standalone {

namespace {

const float kHalf = MINI_VOXEL_SIZE;
const float kEdge = (2.0f * MINI_VOXEL_SIZE) * 0.996f;

int worldToMinIndex(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

int worldToMaxIndex(float world)
{
    return static_cast<int>(std::floor((world - 1.0e-5f) / MINI_VOXEL_SIZE));
}

} // namespace

LocalBoulderWorld::LocalBoulderWorld()
{
    m_root = new osg::Group;
}

osg::Node* LocalBoulderWorld::getNode()
{
    return m_root.get();
}

AABB LocalBoulderWorld::aabbOf(const osg::Vec3& pos) const
{
    AABB box;
    box.minX = pos.x() - kHalf;
    box.maxX = pos.x() + kHalf;
    box.minY = pos.y() - kHalf;
    box.maxY = pos.y() + kHalf;
    box.minZ = pos.z() - kHalf;
    box.maxZ = pos.z() + kHalf;
    return box;
}

AABB LocalBoulderWorld::makeAabb(int index) const
{
    if (!boulderAlive(index)) {
        AABB empty;
        empty.minX = empty.minY = empty.minZ = 0.0f;
        empty.maxX = empty.maxY = empty.maxZ = 0.0f;
        return empty;
    }
    return aabbOf(m_boulders[static_cast<size_t>(index)].pos);
}

bool LocalBoulderWorld::boulderAlive(int index) const
{
    if (index < 0 || index >= boulderCount()) {
        return false;
    }
    return m_boulders[static_cast<size_t>(index)].alive;
}

bool LocalBoulderWorld::boulderResting(int index) const
{
    if (!boulderAlive(index)) {
        return false;
    }
    return m_boulders[static_cast<size_t>(index)].state == BOULDER_RESTING;
}

osg::Vec3 LocalBoulderWorld::boulderPos(int index) const
{
    if (!boulderAlive(index)) {
        return osg::Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_boulders[static_cast<size_t>(index)].pos;
}

BoulderState LocalBoulderWorld::boulderState(int index) const
{
    if (!boulderAlive(index)) {
        return BOULDER_RESTING;
    }
    return m_boulders[static_cast<size_t>(index)].state;
}

void LocalBoulderWorld::buildVisual(HeavyBoulder2x2& b)
{
    osg::ref_ptr<osg::Box> shape = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), kEdge, kEdge, kEdge);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(shape.get());
    draw->setColor(BOULDER_COLOR);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());

    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.22f, 0.12f, 0.05f, 1.0f));
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, BOULDER_COLOR);
    mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.18f, 0.10f, 0.06f, 1.0f));
    mat->setShininess(osg::Material::FRONT_AND_BACK, 8.0f);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    b.node = new osg::MatrixTransform;
    b.node->addChild(geode.get());
    if (m_root.valid()) {
        m_root->addChild(b.node.get());
    }
}

void LocalBoulderWorld::syncVisual(int index)
{
    if (index < 0 || index >= boulderCount()) {
        return;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (!b.node.valid()) {
        return;
    }
    b.node->setMatrix(osg::Matrix::translate(b.pos));
    b.node->setNodeMask(b.alive ? 0xffffffff : 0);
}

void LocalBoulderWorld::syncFootprint(HeavyBoulder2x2& b)
{
    b.vx = static_cast<int>(std::floor(b.pos.x() / MINI_VOXEL_SIZE + 1.0e-4f)) - 1;
    b.vy = static_cast<int>(std::floor(b.pos.y() / MINI_VOXEL_SIZE + 1.0e-4f)) - 1;
    b.vz = static_cast<int>(std::floor(b.pos.z() / MINI_VOXEL_SIZE + 1.0e-4f)) - 1;
}

void LocalBoulderWorld::carveFootprints(MiniVoxelGrid& grid) const
{
    for (size_t i = 0; i < m_boulders.size(); ++i) {
        const HeavyBoulder2x2& b = m_boulders[i];
        if (!b.alive) {
            continue;
        }
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                for (int dz = 0; dz < 2; ++dz) {
                    grid.setVoxel(b.vx + dx, b.vy + dy, b.vz + dz, 0);
                }
            }
        }
    }
}

void LocalBoulderWorld::spawnAt(MiniVoxelGrid& grid, int vx, int vy, int vz)
{
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            for (int dz = 0; dz < 2; ++dz) {
                grid.setVoxel(vx + dx, vy + dy, vz + dz, 0);
            }
        }
    }

    HeavyBoulder2x2 b;
    b.vx = vx;
    b.vy = vy;
    b.vz = vz;
    b.pos = osg::Vec3(
        static_cast<float>(vx + 1) * MINI_VOXEL_SIZE,
        static_cast<float>(vy + 1) * MINI_VOXEL_SIZE,
        static_cast<float>(vz + 1) * MINI_VOXEL_SIZE);
    b.state = BOULDER_RESTING;
    b.velY = 0.0f;
    b.slideFrom = b.pos;
    b.slideDir = osg::Vec3(0.0f, 0.0f, 0.0f);
    b.slideRemain = 0.0f;
    b.alive = true;
    b.squashPlayer = false;
    buildVisual(b);
    m_boulders.push_back(b);
    syncVisual(boulderCount() - 1);
    std::cout << "[boulder] spawn 2x2 at vx=" << vx << " vy=" << vy << " vz=" << vz << "\n";
}

void LocalBoulderWorld::collectSorted(const osg::Vec3& playerPos, std::vector<int>& out) const
{
    out.clear();
    for (int i = 0; i < boulderCount(); ++i) {
        if (boulderAlive(i)) {
            out.push_back(i);
        }
    }
    for (size_t a = 0; a < out.size(); ++a) {
        size_t best = a;
        float bestD = 1.0e30f;
        for (size_t b = a; b < out.size(); ++b) {
            const osg::Vec3 p = m_boulders[static_cast<size_t>(out[b])].pos;
            const float dx = p.x() - playerPos.x();
            const float dy = p.y() - playerPos.y();
            const float dz = p.z() - playerPos.z();
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < bestD) {
                bestD = d;
                best = b;
            }
        }
        const int tmp = out[a];
        out[a] = out[best];
        out[best] = tmp;
    }
}

bool LocalBoulderWorld::hasSupport(int index, const MiniVoxelGrid& grid) const
{
    if (!boulderAlive(index)) {
        return true;
    }
    const HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (b.vy <= 0) {
        return true;
    }
    for (int dx = 0; dx < 2; ++dx) {
        for (int dz = 0; dz < 2; ++dz) {
            if (grid.getVoxel(b.vx + dx, b.vy - 1, b.vz + dz).isActive) {
                return true;
            }
        }
    }
    return false;
}

void LocalBoulderWorld::resetHits(int index, int enemyCount)
{
    if (!boulderAlive(index)) {
        return;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    b.squashPlayer = false;
    b.hitEnemy.assign(enemyCount > 0 ? static_cast<size_t>(enemyCount) : 0, 0);
}

void LocalBoulderWorld::beginFall(int index, int enemyCount)
{
    if (!boulderAlive(index)) {
        return;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    b.state = BOULDER_FALLING;
    b.velY = 0.0f;
    resetHits(index, enemyCount);
}

void LocalBoulderWorld::beginSlide(int index, float dirX, float dirZ, int enemyCount)
{
    if (!boulderResting(index)) {
        return;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    b.state = BOULDER_SLIDING;
    b.velY = 0.0f;
    b.slideFrom = b.pos;
    b.slideDir = osg::Vec3(dirX, 0.0f, dirZ);
    b.slideRemain = BOULDER_SLIDE_DIST;
    resetHits(index, enemyCount);
}

bool LocalBoulderWorld::consumePlayerSquash(int index)
{
    if (!boulderAlive(index)) {
        return false;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (b.squashPlayer) {
        return false;
    }
    b.squashPlayer = true;
    return true;
}

bool LocalBoulderWorld::consumeEnemyHit(int index, int enemyIndex)
{
    if (!boulderAlive(index) || enemyIndex < 0) {
        return false;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (static_cast<size_t>(enemyIndex) >= b.hitEnemy.size()) {
        b.hitEnemy.resize(static_cast<size_t>(enemyIndex) + 1, 0);
    }
    if (b.hitEnemy[static_cast<size_t>(enemyIndex)] != 0) {
        return false;
    }
    b.hitEnemy[static_cast<size_t>(enemyIndex)] = 1;
    return true;
}

bool LocalBoulderWorld::overlapsVoxels(const AABB& box, const MiniVoxelGrid& grid,
                                       std::vector<VoxelKey>* outHits) const
{
    const int minVx = worldToMinIndex(box.minX);
    const int maxVx = worldToMaxIndex(box.maxX);
    const int minVy = worldToMinIndex(box.minY);
    const int maxVy = worldToMaxIndex(box.maxY);
    const int minVz = worldToMinIndex(box.minZ);
    const int maxVz = worldToMaxIndex(box.maxZ);
    bool hit = false;
    if (maxVx < minVx || maxVy < minVy || maxVz < minVz) {
        return false;
    }
    for (int vy = minVy; vy <= maxVy; ++vy) {
        if (vy < 0) {
            continue;
        }
        for (int vz = minVz; vz <= maxVz; ++vz) {
            for (int vx = minVx; vx <= maxVx; ++vx) {
                if (!grid.getVoxel(vx, vy, vz).isActive) {
                    continue;
                }
                hit = true;
                if (outHits != nullptr) {
                    VoxelKey key;
                    key.vx = vx;
                    key.vy = vy;
                    key.vz = vz;
                    outHits->push_back(key);
                }
            }
        }
    }
    return hit;
}

float LocalBoulderWorld::findSupportY(const HeavyBoulder2x2& b, const MiniVoxelGrid& grid) const
{
    const AABB foot = aabbOf(b.pos);
    float best = 0.0f;
    const int minVx = worldToMinIndex(foot.minX);
    const int maxVx = worldToMaxIndex(foot.maxX);
    const int minVz = worldToMinIndex(foot.minZ);
    const int maxVz = worldToMaxIndex(foot.maxZ);
    const int maxVy = worldToMaxIndex(foot.minY + 0.20f);
    if (maxVx < minVx || maxVz < minVz) {
        return best;
    }
    for (int vy = 0; vy <= maxVy; ++vy) {
        for (int vz = minVz; vz <= maxVz; ++vz) {
            for (int vx = minVx; vx <= maxVx; ++vx) {
                if (!grid.getVoxel(vx, vy, vz).isActive) {
                    continue;
                }
                const float top = static_cast<float>(vy + 1) * MINI_VOXEL_SIZE;
                if (top > best && top <= b.pos.y() + 0.001f) {
                    best = top;
                }
            }
        }
    }
    return best;
}

void LocalBoulderWorld::settle(int index)
{
    if (!boulderAlive(index)) {
        return;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    b.state = BOULDER_RESTING;
    b.velY = 0.0f;
    b.slideRemain = 0.0f;
    syncFootprint(b);
    b.pos.x() = static_cast<float>(b.vx + 1) * MINI_VOXEL_SIZE;
    b.pos.y() = static_cast<float>(b.vy + 1) * MINI_VOXEL_SIZE;
    b.pos.z() = static_cast<float>(b.vz + 1) * MINI_VOXEL_SIZE;
    syncVisual(index);
}

bool LocalBoulderWorld::updateFalling(int index, float dt, const MiniVoxelGrid& grid)
{
    if (!boulderAlive(index) || dt <= 0.0f) {
        return false;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (b.state != BOULDER_FALLING) {
        return false;
    }

    b.velY -= BOULDER_GRAVITY * dt;
    b.pos.y() += b.velY * dt;

    const AABB box = aabbOf(b.pos);
    const bool hitGround = (box.minY <= 0.0f);
    const bool hitVoxel = overlapsVoxels(box, grid, nullptr);
    if (!hitGround && !hitVoxel) {
        syncFootprint(b);
        syncVisual(index);
        return false;
    }

    const float support = hitGround ? 0.0f : findSupportY(b, grid);
    b.pos.y() = support + kHalf;
    if (b.pos.y() < kHalf) {
        b.pos.y() = kHalf;
    }
    settle(index);
    return true;
}

bool LocalBoulderWorld::updateSliding(int index, float dt, const MiniVoxelGrid& grid,
                                      std::vector<VoxelKey>& outHits)
{
    outHits.clear();
    if (!boulderAlive(index) || dt <= 0.0f) {
        return false;
    }
    HeavyBoulder2x2& b = m_boulders[static_cast<size_t>(index)];
    if (b.state != BOULDER_SLIDING) {
        return false;
    }

    float speed = BOULDER_SLIDE_SPEED;
    if (b.slideRemain < 0.35f) {
        speed *= (b.slideRemain / 0.35f);
        if (speed < 2.0f) {
            speed = 2.0f;
        }
    }
    float step = speed * dt;
    if (step > b.slideRemain) {
        step = b.slideRemain;
    }

    const osg::Vec3 next = b.pos + b.slideDir * step;
    const AABB nextBox = aabbOf(next);
    if (overlapsVoxels(nextBox, grid, &outHits)) {
        settle(index);
        return true;
    }

    b.pos = next;
    b.slideRemain -= step;
    if (b.slideRemain <= 0.001f) {
        b.pos = b.slideFrom + b.slideDir * BOULDER_SLIDE_DIST;
        settle(index);
        return true;
    }
    syncFootprint(b);
    syncVisual(index);
    return false;
}

void LocalBoulderWorld::cardinalFromYaw(float yaw, float* dx, float* dz)
{
    cardinalFromVector(std::sin(yaw), std::cos(yaw), dx, dz);
}

void LocalBoulderWorld::cardinalFromVector(float vx, float vz, float* dx, float* dz)
{
    if (std::fabs(vx) >= std::fabs(vz)) {
        *dx = (vx >= 0.0f) ? 1.0f : -1.0f;
        *dz = 0.0f;
    } else {
        *dx = 0.0f;
        *dz = (vz >= 0.0f) ? 1.0f : -1.0f;
    }
}

} // namespace standalone
} // namespace rc
