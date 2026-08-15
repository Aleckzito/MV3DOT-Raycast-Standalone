#ifndef RC_LOCAL_PROJECTILE_H
#define RC_LOCAL_PROJECTILE_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 21 / 92. Proyectil local: aliado naranja o flecha hostil verde.
class LocalProjectile {
public:
    LocalProjectile();
    LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl);
    LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl, bool hostile);

    osg::Node* getNode();
    void syncVisual();
    bool alive() const { return m_alive; }
    void kill() { m_alive = false; }
    bool isHostile() const { return m_hostile; }

    osg::Vec3 m_pos;
    osg::Vec3 m_vel;
    float m_ttl;

private:
    void buildVisual();

    bool m_alive;
    bool m_hostile;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
