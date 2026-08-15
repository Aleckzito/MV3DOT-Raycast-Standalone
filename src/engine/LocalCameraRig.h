#ifndef RC_LOCAL_CAMERA_RIG_H
#define RC_LOCAL_CAMERA_RIG_H

#include <osg/Vec3>
#include <osg/Vec4>

namespace rc {
namespace standalone {

class DummyActor;
class LocalBuddyController;
class LocalBoxWorld;
class MiniVoxelGrid;

enum CamMode {
    CAM_RED = 0,
    CAM_BLUE = 1
};

// 89. Dual: Roja brazo mecanico, Azul orbita libre. Fary vive en PIP.
class LocalCameraRig {
public:
    LocalCameraRig();

    void cycle();
    CamMode mode() const { return m_mode; }
    bool invertMove() const { return false; }
    bool arrowsOrbit() const { return m_mode == CAM_BLUE; }
    bool isOrbit() const { return m_mode == CAM_BLUE; }
    const char* modeName() const;
    osg::Vec4 modeColor() const;

    void addOrbit(float yawDelta, float pitchDelta);
    void nudgeOrbitFromMouse(float deltaX, float deltaY);
    void update(float dt,
                const DummyActor& dummy,
                const LocalBuddyController& buddy,
                const LocalBoxWorld& boxes,
                const MiniVoxelGrid& voxels,
                osg::Vec3& eye,
                osg::Vec3& center,
                bool& initialized);

private:
    float boomHit(const osg::Vec3& pivot, const osg::Vec3& idealEye,
                  const LocalBoxWorld& boxes, const MiniVoxelGrid& voxels) const;

    CamMode m_mode;
    float m_orbitYaw;
    float m_orbitPitch;
    osg::Vec3 m_eye;
    osg::Vec3 m_center;
};

} // namespace standalone
} // namespace rc

#endif
