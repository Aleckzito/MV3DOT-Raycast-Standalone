#include "LocalCameraRig.h"

#include "DummyActor.h"
#include "LocalBoxWorld.h"
#include "LocalBuddyController.h"
#include "MiniVoxelGrid.h"

#include <algorithm>
#include <cmath>

namespace rc {
namespace standalone {

namespace {

int worldToMini(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

} // namespace

LocalCameraRig::LocalCameraRig()
    : m_mode(CAM_RED)
    , m_orbitYaw(3.14159265f)
    , m_orbitPitch(0.28f)
    , m_eye(4.0f, 2.5f, 1.5f)
    , m_center(4.0f, 0.6f, 4.0f)
{
}

void LocalCameraRig::cycle()
{
    m_mode = (m_mode == CAM_RED) ? CAM_BLUE : CAM_RED;
}

const char* LocalCameraRig::modeName() const
{
    if (m_mode == CAM_BLUE) {
        return "Orbita";
    }
    return "Brazo";
}

osg::Vec4 LocalCameraRig::modeColor() const
{
    if (m_mode == CAM_BLUE) {
        return osg::Vec4(0.25f, 0.55f, 1.00f, 1.0f);
    }
    return osg::Vec4(1.00f, 0.22f, 0.18f, 1.0f);
}

void LocalCameraRig::addOrbit(float yawDelta, float pitchDelta)
{
    m_orbitYaw += yawDelta;
    m_orbitPitch = std::clamp(m_orbitPitch + pitchDelta, -0.60f, 0.85f);
}

void LocalCameraRig::nudgeOrbitFromMouse(float deltaX, float deltaY)
{
    m_orbitYaw -= deltaX * 0.003f;
    m_orbitPitch = std::clamp(m_orbitPitch - deltaY * 0.003f, -0.60f, 0.85f);
}

float LocalCameraRig::boomHit(const osg::Vec3& pivot, const osg::Vec3& idealEye,
                              const LocalBoxWorld& boxes, const MiniVoxelGrid& voxels) const
{
    float tHit = 1.0f;
    float boxT = 1.0f;
    if (boxes.rayHits(pivot, idealEye, &boxT) && boxT < tHit) {
        tHit = boxT;
    }
    const osg::Vec3 d = idealEye - pivot;
    const int steps = 10;
    for (int s = 1; s < steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const osg::Vec3 p = pivot + d * t;
        const int vx = worldToMini(p.x());
        const int vy = worldToMini(p.y());
        const int vz = worldToMini(p.z());
        if (vy >= 0 && voxels.getVoxel(vx, vy, vz).isActive) {
            if (t < tHit) {
                tHit = t;
            }
            break;
        }
    }
    if (tHit < 0.18f) {
        tHit = 0.18f;
    }
    return tHit;
}

void LocalCameraRig::update(float dt,
                            const DummyActor& dummy,
                            const LocalBuddyController& buddy,
                            const LocalBoxWorld& boxes,
                            const MiniVoxelGrid& voxels,
                            osg::Vec3& eye,
                            osg::Vec3& center,
                            bool& initialized)
{
    const float headY = dummy.y() + dummy.height() * 0.85f;
    const osg::Vec3 dummyHead(dummy.x(), headY, dummy.z());

    osg::Vec3 idealEye;
    osg::Vec3 idealCenter = dummyHead;
    (void)buddy;

    if (m_mode == CAM_BLUE) {
        // 89.3 Ojo orbital: yaw/pitch del mouse respecto al jugador.
        const float cy = std::cos(m_orbitPitch);
        const float dist = 3.40f;
        idealEye.set(
            dummy.x() + std::sin(m_orbitYaw) * dist * cy,
            dummyHead.y() + std::sin(m_orbitPitch) * dist + 0.35f,
            dummy.z() + std::cos(m_orbitYaw) * dist * cy);
    } else {
        const float yaw = dummy.yaw();
        const float fx = std::sin(yaw);
        const float fz = std::cos(yaw);
        const float dist = 2.6f;
        const float height = 1.15f;
        osg::Vec3 boomEye(
            dummyHead.x() - fx * dist,
            dummyHead.y() + height,
            dummyHead.z() - fz * dist);
        const float t = boomHit(dummyHead, boomEye, boxes, voxels);
        boomEye = dummyHead + (boomEye - dummyHead) * t;
        idealEye = boomEye;
    }

    if (!initialized) {
        m_eye = idealEye;
        m_center = idealCenter;
        initialized = true;
    } else {
        float tEye = dt * 6.0f;
        float tLook = dt * 8.0f;
        if (tEye > 1.0f) {
            tEye = 1.0f;
        }
        if (tLook > 1.0f) {
            tLook = 1.0f;
        }
        m_eye = m_eye + (idealEye - m_eye) * tEye;
        m_center = m_center + (idealCenter - m_center) * tLook;
    }

    eye = m_eye;
    center = m_center;
}

} // namespace standalone
} // namespace rc
