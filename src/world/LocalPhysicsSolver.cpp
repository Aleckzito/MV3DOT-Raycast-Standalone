#include "LocalPhysicsSolver.h"

#include "DummyActor.h"
#include "LocalBoxWorld.h"
#include "LocalBoulder.h"
#include "LocalEnemy.h"
#include "MiniVoxelGrid.h"
#include "StandaloneInputHandler.h"

#include <cmath>

namespace rc {
namespace standalone {

namespace {

const float kMoveSpeed = 2.6f;
const float kTurnSpeed = 2.4f;
const float kJumpSpeed = 5.0f;
const float kGravity = -9.81f;
const float kWalkStepCubes = CELL_SIZE;

int worldToMinIndex(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

int worldToMaxIndex(float world)
{
    return static_cast<int>(std::floor((world - 1.0e-5f) / MINI_VOXEL_SIZE));
}

bool aabbOverlap(const AABB& a, const AABB& b)
{
    if (a.maxX <= b.minX || a.minX >= b.maxX) {
        return false;
    }
    if (a.maxY <= b.minY || a.minY >= b.maxY) {
        return false;
    }
    if (a.maxZ <= b.minZ || a.minZ >= b.maxZ) {
        return false;
    }
    return true;
}

bool aabbOverlapXZ(const AABB& a, const AABB& b)
{
    if (a.maxX <= b.minX || a.minX >= b.maxX) {
        return false;
    }
    if (a.maxZ <= b.minZ || a.minZ >= b.maxZ) {
        return false;
    }
    return true;
}

} // namespace

LocalPhysicsSolver::LocalPhysicsSolver()
    : m_grid(nullptr)
    , m_dummy(nullptr)
    , m_boxes(nullptr)
    , m_boulders(nullptr)
    , m_input(nullptr)
{
}

void LocalPhysicsSolver::setGrid(const MiniVoxelGrid* grid)
{
    m_grid = grid;
}

void LocalPhysicsSolver::setDummy(DummyActor* dummy)
{
    m_dummy = dummy;
}

void LocalPhysicsSolver::setInput(StandaloneInputHandler* input)
{
    m_input = input;
}

void LocalPhysicsSolver::setBoxWorld(const LocalBoxWorld* boxes)
{
    m_boxes = boxes;
}

void LocalPhysicsSolver::setBoulderWorld(const LocalBoulderWorld* boulders)
{
    m_boulders = boulders;
}

bool LocalPhysicsSolver::checkCollision(const AABB& box) const
{
    if (box.minY < 0.0f) {
        return true;
    }

    const int minVx = worldToMinIndex(box.minX);
    const int maxVx = worldToMaxIndex(box.maxX);
    const int minVy = worldToMinIndex(box.minY);
    const int maxVy = worldToMaxIndex(box.maxY);
    const int minVz = worldToMinIndex(box.minZ);
    const int maxVz = worldToMaxIndex(box.maxZ);
    if (m_grid != nullptr && maxVx >= minVx && maxVy >= minVy && maxVz >= minVz) {
        int vy = minVy;
        while (vy <= maxVy) {
            int vz = minVz;
            while (vz <= maxVz) {
                int vx = minVx;
                while (vx <= maxVx) {
                    if (m_grid->getVoxel(vx, vy, vz).isActive) {
                        return true;
                    }
                    vx += 1;
                }
                vz += 1;
            }
            vy += 1;
        }
    }
    return overlapsBoxes(box) || overlapsBoulders(box);
}

float LocalPhysicsSolver::findSupportY(const AABB& footprint, float fromY) const
{
    float best = 0.0f;
    if (m_grid != nullptr) {
        const int minVx = worldToMinIndex(footprint.minX);
        const int maxVx = worldToMaxIndex(footprint.maxX);
        const int minVz = worldToMinIndex(footprint.minZ);
        const int maxVz = worldToMaxIndex(footprint.maxZ);
        const int maxVy = worldToMaxIndex(fromY + 0.001f);
        if (maxVx >= minVx && maxVz >= minVz) {
            int vy = 0;
            while (vy <= maxVy) {
                int vz = minVz;
                while (vz <= maxVz) {
                    int vx = minVx;
                    while (vx <= maxVx) {
                        if (m_grid->getVoxel(vx, vy, vz).isActive) {
                            const float top = static_cast<float>(vy + 1) * MINI_VOXEL_SIZE;
                            if (top > best && top <= fromY + 0.001f) {
                                best = top;
                            }
                        }
                        vx += 1;
                    }
                    vz += 1;
                }
                vy += 1;
            }
        }
    }
    if (m_boxes != nullptr) {
        const float h = CELL_SIZE * 0.5f;
        const int n = m_boxes->boxCount();
        for (int i = 0; i < n; ++i) {
            if (!m_boxes->boxAlive(i)) {
                continue;
            }
            const osg::Vec3 p = m_boxes->boxPos(i);
            AABB b;
            b.minX = p.x() - h;
            b.maxX = p.x() + h;
            b.minY = p.y() - h;
            b.maxY = p.y() + h;
            b.minZ = p.z() - h;
            b.maxZ = p.z() + h;
            if (!aabbOverlapXZ(footprint, b)) {
                continue;
            }
            const float top = b.maxY;
            if (top > best && top <= fromY + 0.001f) {
                best = top;
            }
        }
    }
    if (m_boulders != nullptr) {
        const int n = m_boulders->boulderCount();
        for (int i = 0; i < n; ++i) {
            if (!m_boulders->boulderAlive(i)) {
                continue;
            }
            const AABB b = m_boulders->makeAabb(i);
            if (!aabbOverlapXZ(footprint, b)) {
                continue;
            }
            const float top = b.maxY;
            if (top > best && top <= fromY + 0.001f) {
                best = top;
            }
        }
    }
    return best;
}

bool LocalPhysicsSolver::overlapsBoxes(const AABB& box) const
{
    // 80.1 Cubos dinamicos = solido AABB.
    if (m_boxes == nullptr) {
        return false;
    }
    const float h = CELL_SIZE * 0.5f;
    const int n = m_boxes->boxCount();
    for (int i = 0; i < n; ++i) {
        if (!m_boxes->boxAlive(i)) {
            continue;
        }
        const osg::Vec3 p = m_boxes->boxPos(i);
        AABB b;
        b.minX = p.x() - h;
        b.maxX = p.x() + h;
        b.minY = p.y() - h;
        b.maxY = p.y() + h;
        b.minZ = p.z() - h;
        b.maxZ = p.z() + h;
        if (aabbOverlap(box, b)) {
            return true;
        }
    }
    return false;
}

bool LocalPhysicsSolver::overlapsBoulders(const AABB& box) const
{
    if (m_boulders == nullptr) {
        return false;
    }
    const int n = m_boulders->boulderCount();
    for (int i = 0; i < n; ++i) {
        if (!m_boulders->boulderAlive(i)) {
            continue;
        }
        if (aabbOverlap(box, m_boulders->makeAabb(i))) {
            return true;
        }
    }
    return false;
}

void LocalPhysicsSolver::moveAxisX(float deltaTime, bool canStep)
{
    // 15.2 Eje X aislado. Si choca, velX=0; Z sigue libre (slide).
    const float nextX = m_dummy->x() + m_dummy->velX() * deltaTime;
    const AABB moved = m_dummy->makeAabbAt(nextX, m_dummy->y(), m_dummy->z());
    if (!checkCollision(moved)) {
        m_dummy->setPosition(nextX, m_dummy->y(), m_dummy->z());
        return;
    }

    // 15.3 Auto-step mini-voxel. 80.1 tryStepOnto 1 cubo (CELL_SIZE).
    if (canStep) {
        const float stepMini = m_dummy->y() + MINI_VOXEL_SIZE;
        const AABB steppedMini = m_dummy->makeAabbAt(nextX, stepMini, m_dummy->z());
        if (!checkCollision(steppedMini)) {
            m_dummy->setPosition(nextX, stepMini, m_dummy->z());
            m_dummy->setVelY(0.0f);
            return;
        }
        const float stepCube = m_dummy->y() + kWalkStepCubes;
        const AABB steppedCube = m_dummy->makeAabbAt(nextX, stepCube, m_dummy->z());
        if (!checkCollision(steppedCube)) {
            m_dummy->setPosition(nextX, stepCube, m_dummy->z());
            m_dummy->setVelY(0.0f);
            return;
        }
    }
    m_dummy->setVelX(0.0f);
}

void LocalPhysicsSolver::moveAxisZ(float deltaTime, bool canStep)
{
    // 15.2 Eje Z aislado. Independiente de X.
    const float nextZ = m_dummy->z() + m_dummy->velZ() * deltaTime;
    const AABB moved = m_dummy->makeAabbAt(m_dummy->x(), m_dummy->y(), nextZ);
    if (!checkCollision(moved)) {
        m_dummy->setPosition(m_dummy->x(), m_dummy->y(), nextZ);
        return;
    }

    if (canStep) {
        const float stepMini = m_dummy->y() + MINI_VOXEL_SIZE;
        const AABB steppedMini = m_dummy->makeAabbAt(m_dummy->x(), stepMini, nextZ);
        if (!checkCollision(steppedMini)) {
            m_dummy->setPosition(m_dummy->x(), stepMini, nextZ);
            m_dummy->setVelY(0.0f);
            return;
        }
        const float stepCube = m_dummy->y() + kWalkStepCubes;
        const AABB steppedCube = m_dummy->makeAabbAt(m_dummy->x(), stepCube, nextZ);
        if (!checkCollision(steppedCube)) {
            m_dummy->setPosition(m_dummy->x(), stepCube, nextZ);
            m_dummy->setVelY(0.0f);
            return;
        }
    }
    m_dummy->setVelZ(0.0f);
}

namespace {

AABB makeBodyAabb(float x, float y, float z, float width, float height, float depth)
{
    AABB box;
    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;
    box.minX = x - halfW;
    box.maxX = x + halfW;
    box.minY = y;
    box.maxY = y + height;
    box.minZ = z - halfD;
    box.maxZ = z + halfD;
    return box;
}

} // namespace

void LocalPhysicsSolver::updateEnemyPhysics(LocalEnemy& enemy, float oldX, float oldZ, float dt, const AABB* blocker)
{
    // 28.1 / 28.2 Gravedad Y + slide X/Z vs voxels + no atravesar jugador.
    if (!enemy.isAlive) {
        return;
    }
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    const float desiredX = enemy.pos.x();
    const float desiredZ = enemy.pos.z();
    float x = oldX;
    float y = enemy.pos.y();
    float z = oldZ;
    const float w = enemy.width();
    const float h = enemy.height();
    const float d = enemy.depth();

    bool grounded = false;
    AABB probe = makeBodyAabb(x, y, z, w, h, d);
    probe.minY -= 0.004f;
    if (enemy.velY() <= 0.0f && checkCollision(probe)) {
        y = findSupportY(makeBodyAabb(x, y, z, w, h, d), y + 0.05f);
        enemy.setVelY(0.0f);
        grounded = true;
    }

    if (!grounded) {
        const float velY = enemy.velY() + kGravity * dt;
        const float nextY = y + velY * dt;
        if (checkCollision(makeBodyAabb(x, nextY, z, w, h, d))) {
            y = findSupportY(makeBodyAabb(x, y, z, w, h, d), y);
            enemy.setVelY(0.0f);
            grounded = true;
        } else {
            enemy.setVelY(velY);
            y = nextY;
        }
    }

    if (!checkCollision(makeBodyAabb(desiredX, y, z, w, h, d))) {
        x = desiredX;
    } else if (grounded) {
        const float stepY = y + MINI_VOXEL_SIZE;
        if (!checkCollision(makeBodyAabb(desiredX, stepY, z, w, h, d))) {
            x = desiredX;
            y = stepY;
            enemy.setVelY(0.0f);
        }
    }

    if (!checkCollision(makeBodyAabb(x, y, desiredZ, w, h, d))) {
        z = desiredZ;
    } else if (grounded) {
        const float stepY = y + MINI_VOXEL_SIZE;
        if (!checkCollision(makeBodyAabb(x, stepY, desiredZ, w, h, d))) {
            z = desiredZ;
            y = stepY;
            enemy.setVelY(0.0f);
        }
    }

    if (blocker != nullptr) {
        AABB box = makeBodyAabb(x, y, z, w, h, d);
        if (aabbOverlap(box, *blocker)) {
            const float penXPos = box.maxX - blocker->minX;
            const float penXNeg = blocker->maxX - box.minX;
            const float penZPos = box.maxZ - blocker->minZ;
            const float penZNeg = blocker->maxZ - box.minZ;
            const float penX = (penXPos < penXNeg) ? penXPos : penXNeg;
            const float penZ = (penZPos < penZNeg) ? penZPos : penZNeg;
            if (penX < penZ) {
                if (penXPos < penXNeg) {
                    x -= penXPos;
                } else {
                    x += penXNeg;
                }
            } else {
                if (penZPos < penZNeg) {
                    z -= penZPos;
                } else {
                    z += penZNeg;
                }
            }
        }
    }

    enemy.pos = osg::Vec3(x, y, z);
}

void LocalPhysicsSolver::updatePhysics(float deltaTime)
{
    if (m_dummy == nullptr) {
        return;
    }
    if (deltaTime <= 0.0f) {
        return;
    }
    if (deltaTime > 0.05f) {
        deltaTime = 0.05f;
    }

    // 1) Suelo / gravedad / salto (eje Y).
    bool grounded = false;
    AABB probe = m_dummy->makeAabb();
    probe.minY -= 0.004f;
    if (m_dummy->velY() <= 0.0f && checkCollision(probe)) {
        const float support = findSupportY(m_dummy->makeAabb(), m_dummy->y() + 0.05f);
        m_dummy->setPosition(m_dummy->x(), support, m_dummy->z());
        m_dummy->setVelY(0.0f);
        grounded = true;
    }

    if (grounded && m_input != nullptr && m_input->isJumpPressed()) {
        m_dummy->setVelY(kJumpSpeed);
        grounded = false;
    }

    if (!grounded) {
        const float velY = m_dummy->velY() + kGravity * deltaTime;
        const float nextY = m_dummy->y() + velY * deltaTime;
        if (checkCollision(m_dummy->makeAabbAtY(nextY))) {
            const float support = findSupportY(m_dummy->makeAabb(), m_dummy->y());
            m_dummy->setPosition(m_dummy->x(), support, m_dummy->z());
            m_dummy->setVelY(0.0f);
            grounded = true;
        } else {
            m_dummy->setVelY(velY);
            m_dummy->setPosition(m_dummy->x(), nextY, m_dummy->z());
        }
    }

    // 17. Tank: LEFT/RIGHT giran yaw. UP/DOWN avanzan sobre el frente.
    // Frente mundo: (sin(yaw), 0, cos(yaw)). yaw=0 mira +Z. Nariz local +Z.
    float forward = 0.0f;
    float strafe = 0.0f;
    float turn = 0.0f;
    if (m_input != nullptr) {
        m_input->getMoveAxes(forward, strafe, turn);
    }
    m_dummy->addYaw(turn * kTurnSpeed * deltaTime);

    // 50.3 / 50.4 / 50.5 Frente + strafe, normalizado.
    const float yaw = m_dummy->yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const float rx = std::cos(yaw);
    const float rz = -std::sin(yaw);
    float mx = fx * forward + rx * strafe;
    float mz = fz * forward + rz * strafe;
    const float len = std::sqrt(mx * mx + mz * mz);
    if (len > 0.0001f) {
        const float inv = 1.0f / len;
        mx = mx * inv * kMoveSpeed;
        mz = mz * inv * kMoveSpeed;
    } else {
        mx = 0.0f;
        mz = 0.0f;
    }
    m_dummy->setVelX(mx);
    m_dummy->setVelZ(mz);

    // 44.4 Haste Burst: vel * 3.5 mientras dure el dash.
    if (m_dummy->dashTimer() > 0.0f) {
        m_dummy->setVelX(m_dummy->velX() * 3.5f);
        m_dummy->setVelZ(m_dummy->velZ() * 3.5f);
        m_dummy->tickDash(deltaTime);
    }

    m_dummy->setVelX(m_dummy->velX() + m_dummy->knockVelX());
    m_dummy->setVelZ(m_dummy->velZ() + m_dummy->knockVelZ());
    m_dummy->tickKnockback(deltaTime);

    // 3) 15.2 Separacion de ejes: X luego Z. Slide en paredes. Auto-step si grounded.
    moveAxisX(deltaTime, grounded);
    moveAxisZ(deltaTime, grounded);
}

} // namespace standalone
} // namespace rc
