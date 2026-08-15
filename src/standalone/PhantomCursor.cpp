#include "PhantomCursor.h"
#include "MiniVoxelGrid.h"

#include <cmath>

namespace rc {
namespace standalone {

namespace {

int worldToIndex(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

bool isSolidCell(int vx, int vy, int vz, const MiniVoxelGrid* grid)
{
    // 7.1 Suelo y=0: todo vy < 0 es solido.
    if (vy < 0) {
        return true;
    }
    if (grid == nullptr) {
        return false;
    }
    return grid->getVoxel(vx, vy, vz).isActive;
}

SnappedPosition makePose(int vx, int vy, int vz)
{
    SnappedPosition pose;
    pose.vx = vx;
    pose.vy = vy;
    pose.vz = vz;
    pose.x = static_cast<float>(vx) * MINI_VOXEL_SIZE;
    pose.y = static_cast<float>(vy) * MINI_VOXEL_SIZE;
    pose.z = static_cast<float>(vz) * MINI_VOXEL_SIZE;
    return pose;
}

} // namespace

PhantomCursor::PhantomCursor()
    : m_hasPrev(false)
    , m_prevVx(0)
    , m_prevVy(0)
    , m_prevVz(0)
    , m_hasHitSolid(false)
    , m_hitVx(0)
    , m_hitVy(0)
    , m_hitVz(0)
{
}

void PhantomCursor::resetDda()
{
    m_hasPrev = false;
    m_hasHitSolid = false;
}

SnappedPosition PhantomCursor::currentPose() const
{
    return makePose(m_prevVx, m_prevVy, m_prevVz);
}

SnappedPosition PhantomCursor::calculateSnappedPosition(
    float originX, float originY, float originZ,
    float hitX, float hitY, float hitZ,
    const MiniVoxelGrid* grid)
{
    // 5.5 Flujo DDA:
    // 1) origen = celda previa (centro) o rayo nuevo
    // 2) cada paso cambia UN solo eje en +/-1
    // 3) solido (suelo o voxel) -> devolver ultima celda vacia (5.7)
    // 4) con trail: maximo 1 paso por llamada para que |dv| <= 1 (5.6)
    float ox = originX;
    float oy = originY;
    float oz = originZ;
    int maxStepsThisCall = 256;
    m_hasHitSolid = false;

    // 10.4 Si el trail quedo dentro de un voxel recien colocado, recastear desde el rayo.
    if (m_hasPrev && isSolidCell(m_prevVx, m_prevVy, m_prevVz, grid)) {
        m_hasPrev = false;
    }

    if (m_hasPrev) {
        ox = (static_cast<float>(m_prevVx) + 0.5f) * MINI_VOXEL_SIZE;
        oy = (static_cast<float>(m_prevVy) + 0.5f) * MINI_VOXEL_SIZE;
        oz = (static_cast<float>(m_prevVz) + 0.5f) * MINI_VOXEL_SIZE;
        maxStepsThisCall = 1;
        if (hitY < MINI_VOXEL_SIZE) {
            hitY = MINI_VOXEL_SIZE * 0.5f;
        }
    }

    float dx = hitX - ox;
    float dy = hitY - oy;
    float dz = hitZ - oz;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1.0e-8f) {
        const SnappedPosition stay = makePose(worldToIndex(ox), worldToIndex(oy), worldToIndex(oz));
        if (isSolidCell(stay.vx, stay.vy, stay.vz, grid)) {
            m_hasHitSolid = true;
            m_hitVx = stay.vx;
            m_hitVy = stay.vy;
            m_hitVz = stay.vz;
            if (m_hasPrev) {
                return makePose(m_prevVx, m_prevVy, m_prevVz);
            }
            return stay;
        }
        m_hasPrev = true;
        m_prevVx = stay.vx;
        m_prevVy = stay.vy;
        m_prevVz = stay.vz;
        return stay;
    }
    dx /= len;
    dy /= len;
    dz /= len;

    int vx = worldToIndex(ox);
    int vy = worldToIndex(oy);
    int vz = worldToIndex(oz);

    const int stepX = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
    const int stepY = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);
    const int stepZ = (dz > 0.0f) ? 1 : ((dz < 0.0f) ? -1 : 0);

    const float kInf = 1.0e30f;
    const float tDeltaX = (stepX == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dx));
    const float tDeltaY = (stepY == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dy));
    const float tDeltaZ = (stepZ == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dz));

    float tMaxX = kInf;
    float tMaxY = kInf;
    float tMaxZ = kInf;
    if (stepX > 0) {
        tMaxX = ((static_cast<float>(vx + 1) * MINI_VOXEL_SIZE) - ox) / dx;
    } else if (stepX < 0) {
        tMaxX = ((static_cast<float>(vx) * MINI_VOXEL_SIZE) - ox) / dx;
    }
    if (stepY > 0) {
        tMaxY = ((static_cast<float>(vy + 1) * MINI_VOXEL_SIZE) - oy) / dy;
    } else if (stepY < 0) {
        tMaxY = ((static_cast<float>(vy) * MINI_VOXEL_SIZE) - oy) / dy;
    }
    if (stepZ > 0) {
        tMaxZ = ((static_cast<float>(vz + 1) * MINI_VOXEL_SIZE) - oz) / dz;
    } else if (stepZ < 0) {
        tMaxZ = ((static_cast<float>(vz) * MINI_VOXEL_SIZE) - oz) / dz;
    }

    int lastVx = vx;
    int lastVy = vy;
    int lastVz = vz;
    bool haveEmpty = false;
    int stepsTaken = 0;
    const float maxDist = len + MINI_VOXEL_SIZE * 0.01f;

    for (int guard = 0; guard < 512; ++guard) {
        // 5.7 / 10.4 Impacto: no entrar al solido. Quedarse en la cara vacia exterior.
        if (isSolidCell(vx, vy, vz, grid)) {
            m_hasHitSolid = true;
            m_hitVx = vx;
            m_hitVy = vy;
            m_hitVz = vz;
            break;
        }
        haveEmpty = true;
        lastVx = vx;
        lastVy = vy;
        lastVz = vz;

        if (m_hasPrev && stepsTaken >= maxStepsThisCall) {
            break;
        }
        if (!m_hasPrev && stepsTaken >= maxStepsThisCall) {
            break;
        }

        const float tNext = (tMaxX <= tMaxY)
            ? ((tMaxX <= tMaxZ) ? tMaxX : tMaxZ)
            : ((tMaxY <= tMaxZ) ? tMaxY : tMaxZ);
        if (tNext > maxDist) {
            break;
        }

        // 5.6 Un eje por paso. |dvx|,|dvy|,|dvz| internos <= 1.
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            vx += stepX;
            tMaxX += tDeltaX;
        } else if (tMaxY <= tMaxZ) {
            vy += stepY;
            tMaxY += tDeltaY;
        } else {
            vz += stepZ;
            tMaxZ += tDeltaZ;
        }
        stepsTaken += 1;
    }

    if (!haveEmpty) {
        return makePose(m_prevVx, m_prevVy, m_prevVz);
    }

    m_hasPrev = true;
    m_prevVx = lastVx;
    m_prevVy = lastVy;
    m_prevVz = lastVz;
    return makePose(lastVx, lastVy, lastVz);
}

} // namespace standalone
} // namespace rc
