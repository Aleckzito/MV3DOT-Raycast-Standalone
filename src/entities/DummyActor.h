#ifndef RC_DUMMY_ACTOR_H
#define RC_DUMMY_ACTOR_H

#include "LocalPhysicsSolver.h"

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 13 / 17 / 29 / 43 / 44 / 48. Dummy tank + HP + SP + EXP. Frente = (sin,0,cos).
class DummyActor {
public:
    DummyActor();

    osg::Node* getNode();
    void syncVisual();

    void setPosition(float x, float y, float z);
    void teleport(float x, float y, float z);

    float x() const { return m_x; }
    float y() const { return m_y; }
    float z() const { return m_z; }
    float velX() const { return m_velX; }
    float velY() const { return m_velY; }
    float velZ() const { return m_velZ; }
    void setVelX(float velX) { m_velX = velX; }
    void setVelY(float velY) { m_velY = velY; }
    void setVelZ(float velZ) { m_velZ = velZ; }

    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    void setYaw(float yaw) { m_yaw = yaw; }
    void addYaw(float delta);
    void addPitch(float delta);

    float width() const { return m_width; }
    float height() const { return m_height; }
    float depth() const { return m_depth; }

    AABB makeAabb() const;
    AABB makeAabbAtY(float y) const;
    AABB makeAabbAt(float x, float y, float z) const;

    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    float stamina() const { return m_stamina; }
    float maxStamina() const { return m_maxStamina; }
    float dashTimer() const { return m_dashTimer; }
    float iFrames() const { return m_iFrames; }
    void tickIFrames(float dt);
    void tickDash(float dt);
    bool activateDash();
    void takeDamage(int amount);
    void applyDamage(int amount);
    void heal(int amount);
    void restoreHp();
    void addExp(int amount);
    void addStamina(float amount);
    void applyKnockback(float velX, float velZ);
    void tickKnockback(float dt);
    float knockVelX() const { return m_knockVelX; }
    float knockVelZ() const { return m_knockVelZ; }

    int level() const { return m_level; }
    int exp() const { return m_exp; }
    int expToNext() const { return m_expToNext; }
    int vocation() const { return m_vocation; }
    void setVocation(int id) { m_vocation = id; }

private:
    float m_x;
    float m_y;
    float m_z;
    float m_velX;
    float m_velY;
    float m_velZ;
    float m_yaw;
    float m_pitch;
    float m_width;
    float m_height;
    float m_depth;
    int m_hp;
    int m_maxHp;
    float m_iFrames;
    float m_stamina;
    float m_maxStamina;
    float m_dashTimer;
    float m_knockVelX;
    float m_knockVelZ;
    int m_level;
    int m_exp;
    int m_expToNext;
    int m_vocation;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
