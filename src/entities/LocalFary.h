#ifndef RC_LOCAL_FARY_H
#define RC_LOCAL_FARY_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 19 / 26. Proto-Fary S10: Lead Weave delante, REARGUARD atras. Lerp elastico.
class LocalFary {
public:
    LocalFary();

    osg::Node* getNode();
    void update(float dt, const osg::Vec3& targetPos, float targetYaw, bool threatBehind);

private:
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
    osg::Vec3 m_pos;
    bool m_hasPos;
    float m_time;
};

} // namespace standalone
} // namespace rc

#endif
