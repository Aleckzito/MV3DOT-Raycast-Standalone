#ifndef RC_LOCAL_DEBRIS_H
#define RC_LOCAL_DEBRIS_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 60. Escombro local: cubo chico + vel + TTL. Cero red.
class LocalDebris {
public:
    LocalDebris();
    LocalDebris(const osg::Vec3& spawnPos, const osg::Vec3& vel, const osg::Vec4& color, float ttl);

    osg::Node* getNode();
    void syncVisual();
    void activate(const osg::Vec3& spawnPos, const osg::Vec3& vel, const osg::Vec4& color, float ttl);
    void deactivate();

    osg::Vec3 pos;
    osg::Vec3 vel;
    float ttl;
    bool isActive;

private:
    void buildVisual();
    void applyColor(const osg::Vec4& color);

    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
