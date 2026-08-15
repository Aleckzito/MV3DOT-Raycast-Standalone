#ifndef RC_LOCAL_CRAWLER_H
#define RC_LOCAL_CRAWLER_H

#include "LocalPhysicsSolver.h"

#include <osg/Material>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

const osg::Vec4 CRAWLER_COLOR(0.35f, 0.35f, 0.40f, 1.0f);
const osg::Vec4 CRAWLER_RUST(0.72f, 0.32f, 0.10f, 1.0f);

// 112. Escarabajo acorazado terrestre: lento, 120 HP, armadura frontal.
class LocalCrawler {
public:
    LocalCrawler();
    explicit LocalCrawler(const osg::Vec3& spawnPos);

    osg::Node* getNode();
    void syncVisual();
    void setTargeted(bool targeted);
    bool isTargeted() const { return m_targeted; }

    void updateAI(float dt, const osg::Vec3& playerPos);
    void kill();
    void takeDamage(int amount, const osg::Vec3& origin);
    bool isFrontalHit(const osg::Vec3& from) const;

    AABB makeAabb() const;
    AABB makeAabbAt(float x, float y, float z) const;
    bool isAlive() const { return m_alive; }
    // 123. Desplazamiento real por segundo, ya resueltas las colisiones.
    osg::Vec3 velocity() const { return m_velocity; }
    void setVelocity(const osg::Vec3& v) { m_velocity = v; }
    void applyStats(int maxHp, float speed);
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    float width() const { return m_width; }
    float height() const { return m_height; }
    float depth() const { return m_depth; }
    float faceX() const { return m_faceX; }
    float faceZ() const { return m_faceZ; }

    osg::Vec3 pos;

private:
    void buildVisual();
    void applyTint();

    bool m_alive;
    bool m_targeted;
    osg::Vec3 m_velocity;
    float m_speed;
    float m_width;
    float m_height;
    float m_depth;
    float m_faceX;
    float m_faceZ;
    int m_hp;
    int m_maxHp;
    osg::Vec3 m_knock;
    osg::ref_ptr<osg::ShapeDrawable> m_bodyDraw;
    osg::ref_ptr<osg::Material> m_material;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
