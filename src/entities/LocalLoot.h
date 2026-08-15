#ifndef RC_LOCAL_LOOT_H
#define RC_LOCAL_LOOT_H

#include <osg/Material>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

enum LootSize {
    SMALL_HP = 0,
    LARGE_HP = 1,
    ENERGY_CELL = 2
};

// 39 / 40 / 113. Capsulas Mega Man + Celda de Energia (cubo azul electrico).
class LocalLoot {
public:
    LocalLoot();
    LocalLoot(const osg::Vec3& spawnPos, LootSize size);

    osg::Node* getNode();
    void syncVisual();
    void activate(const osg::Vec3& spawnPos, LootSize size);
    void deactivate();
    void tick(float dt);
    LootSize size() const { return m_size; }
    bool isEnergy() const { return m_size == ENERGY_CELL; }
    float halfExtent() const;

    osg::Vec3 pos;
    osg::Vec3 velocity;
    bool isActive;
    bool isGrounded;

private:
    void buildVisual();
    void applySize();

    LootSize m_size;
    float m_spin;
    osg::ref_ptr<osg::Box> m_box;
    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::ShapeDrawable> m_haloDraw;
    osg::ref_ptr<osg::Material> m_material;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
