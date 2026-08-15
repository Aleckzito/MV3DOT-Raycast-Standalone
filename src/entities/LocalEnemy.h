#ifndef RC_LOCAL_ENEMY_H
#define RC_LOCAL_ENEMY_H

#include "LocalPhysicsSolver.h"

#include <osg/Material>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

enum EnemyKind {
    ENEMY_GRUNT = 0,
    ENEMY_BOSS = 1,
    ENEMY_ARCHER = 2
};

// 23 / 27 / 31 / 34 / 37 / 53 / 91. Grunt, Boss o Arquero verde.
class LocalEnemy {
public:
    LocalEnemy();
    LocalEnemy(const osg::Vec3& spawnPos);
    LocalEnemy(const osg::Vec3& spawnPos, bool isBoss);
    LocalEnemy(const osg::Vec3& spawnPos, EnemyKind kind);

    osg::Node* getNode();
    void syncVisual();
    void setTargeted(bool targeted);
    bool isTargeted() const { return m_targeted; }
    bool isBoss() const { return m_kind == ENEMY_BOSS; }
    bool isArcher() const { return m_kind == ENEMY_ARCHER; }
    EnemyKind kind() const { return m_kind; }
    bool consumeStompPulse();
    bool consumeArrowShot();
    osg::Vec3 muzzle() const;
    void kill();
    void applyStats(int maxHp, float speed);
    void takeDamage(int amount, const osg::Vec3& origin);
    void updateAI(float dt, const osg::Vec3& playerPos);
    void updateArcherAI(float dt, const osg::Vec3& playerPos, bool hasLos, const osg::Vec3& coverPos);

    osg::Vec3 pos;
    bool isAlive;
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    float width() const { return m_width; }
    float height() const { return m_height; }
    float depth() const { return m_depth; }
    float velY() const { return m_velY; }
    void setVelY(float velY) { m_velY = velY; }
    // 123. Desplazamiento horizontal real por segundo, ya aplicada la fisica.
    osg::Vec3 velocity() const { return m_velocity; }
    void setVelocity(const osg::Vec3& v) { m_velocity = v; }

    AABB makeAabb() const;
    AABB makeAabbAt(float x, float y, float z) const;

private:
    void buildVisual();
    void applyTargetTint();
    void tickStompVisual();

    float m_speed;
    float m_velY;
    osg::Vec3 m_velocity;
    float m_width;
    float m_height;
    float m_depth;
    int m_hp;
    int m_maxHp;
    bool m_targeted;
    EnemyKind m_kind;
    float m_abilityTimer;
    float m_stompTtl;
    bool m_stompPulse;
    bool m_arrowShot;
    osg::Vec3 m_knockbackVel;
    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::Material> m_material;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_stompPat;
};

} // namespace standalone
} // namespace rc

#endif
