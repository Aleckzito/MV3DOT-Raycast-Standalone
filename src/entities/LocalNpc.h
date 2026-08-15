#ifndef RC_LOCAL_NPC_H
#define RC_LOCAL_NPC_H

#include "LocalPhysicsSolver.h"

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

#include <string>

namespace rc {
namespace standalone {

// S8. NPC local en el sandbox voxel. Sin red, sin shop UI.
class LocalNpc {
public:
    LocalNpc();
    LocalNpc(const osg::Vec3& pos, const std::string& contentId, const std::string& name,
             const std::string& talkId, const osg::Vec4& color);

    osg::Node* getNode();
    void syncVisual();
    AABB makeAabb() const;

    osg::Vec3 pos;
    std::string contentId;
    std::string name;
    std::string talkId;
    float greetCd;

private:
    void buildVisual(const osg::Vec4& color);

    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
