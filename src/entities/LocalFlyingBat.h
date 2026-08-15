#ifndef RC_LOCAL_FLYING_BAT_H
#define RC_LOCAL_FLYING_BAT_H

#include "LocalPhysicsSolver.h"

#include <osg/Geometry>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

const osg::Vec4 BAT_COLOR(0.70f, 0.10f, 0.85f, 1.0f);
const float BAT_CRUISE_ALT = 2.80f;
const float BAT_FLEE_ALT = 3.50f;
const float BAT_DIVE_SPEED = 22.0f;

enum BatState {
    BAT_STALK_BEHIND = 0,
    BAT_DIVE_STRIKE = 1,
    BAT_DISENGAGE_FLEE = 2,
    BAT_STUNNED = 3
};

// 104. Murcielago volador: flanqueo aereo, picada y huida.
class LocalFlyingBat {
public:
    LocalFlyingBat();
    explicit LocalFlyingBat(const osg::Vec3& spawnPos);

    osg::Node* getNode();
    void syncVisual();
    void setTargeted(bool targeted);
    bool isTargeted() const { return m_targeted; }

    void updateAI(float dt, const osg::Vec3& playerPos, const osg::Vec3& facing, float headY);
    void notifyHit();
    void notifyMiss();
    void notifyWall();
    void kill();
    void takeDamage(int amount, const osg::Vec3& origin);

    AABB makeAabb() const;
    // Desplazamiento por segundo del ultimo update. La IA mueve pos
    // directamente y updateAI tiene varias salidas por estado, asi que la
    // deriva el motor alrededor de la llamada en vez de integrarse aqui.
    osg::Vec3 velocity() const { return m_velocity; }
    void setVelocity(const osg::Vec3& v) { m_velocity = v; }
    bool isAlive() const { return m_alive; }
    bool isDiveStrike() const { return m_alive && m_state == BAT_DIVE_STRIKE; }
    BatState state() const { return m_state; }
    osg::Vec3 diveDir() const { return m_diveDir; }
    void applyStats(int maxHp);
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    float width() const { return m_width; }
    float height() const { return m_height; }
    float depth() const { return m_depth; }

    osg::Vec3 pos;

private:
    void buildVisual();
    void applyTint();
    void beginDive(const osg::Vec3& target);
    void beginFlee(const osg::Vec3& away);
    void flyToward(const osg::Vec3& goal, float speed, float dt);

    bool m_alive;
    bool m_targeted;
    BatState m_state;
    float m_time;
    float m_stateTtl;
    float m_chirpTtl;
    float m_orbit;
    int m_hp;
    int m_maxHp;
    float m_width;
    float m_height;
    float m_depth;
    osg::Vec3 m_diveDir;
    osg::Vec3 m_velocity;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
    osg::ref_ptr<osg::MatrixTransform> m_wingL;
    osg::ref_ptr<osg::MatrixTransform> m_wingR;
    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::Material> m_material;
};

} // namespace standalone
} // namespace rc

#endif
