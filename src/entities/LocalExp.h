#ifndef RC_LOCAL_EXP_H
#define RC_LOCAL_EXP_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 47. Drop de EXP local. Cubo azul. Cero red.
class LocalExp {
public:
    LocalExp();
    LocalExp(const osg::Vec3& spawnPos);
    LocalExp(const osg::Vec3& spawnPos, int expValue);

    osg::Node* getNode();
    void syncVisual();
    void activate(const osg::Vec3& spawnPos, int expValue);
    void deactivate();
    void tick(float dt);
    int expValue() const { return m_expValue; }
    float halfExtent() const { return 0.06f; }

    osg::Vec3 pos;
    osg::Vec3 velocity;
    bool isActive;
    bool isGrounded;

private:
    void buildVisual();

    int m_expValue;
    float m_spin;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
