#ifndef RC_LOCAL_BUDDY_CONTROLLER_H
#define RC_LOCAL_BUDDY_CONTROLLER_H

#include <osg/Geometry>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

enum BuddyState {
    BUDDY_PATROL = 0,
    BUDDY_EXCITE = 1,
    BUDDY_SAD = 2,
    BUDDY_OVERLOAD = 3,
    BUDDY_BURST = 4
};

// 71. Fary companion: Patrol / Excite / Sad / Overload.
class LocalBuddyController {
public:
    LocalBuddyController();

    osg::Node* getNode();
    void update(float dt, const osg::Vec3& playerPos, const osg::Vec3& facing, float bodyYaw,
                bool threatBehind);
    void notifyPlace();
    void notifyDestroy();
    void notifyFetch();
    void setFetchHint(const osg::Vec3& pos);
    void clearFetchHint();
    bool inFetchRange(const osg::Vec3& dropPos) const;
    void setOverload(bool on);
    void startBurst(float seconds);
    void startOverload(float seconds);
    bool consumeOverloadDone();
    void setHidden(bool hidden);

    BuddyState state() const { return m_state; }
    osg::Vec3 position() const { return m_pos; }
    const char* stateName() const;

private:
    void buildVisual();
    void applyTint();
    osg::Vec3 goalForState(const osg::Vec3& playerPos, const osg::Vec3& facing) const;

    osg::ref_ptr<osg::MatrixTransform> m_node;
    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::Material> m_material;
    osg::ref_ptr<osg::Vec4Array> m_lineColor;
    osg::Vec3 m_pos;
    float m_yaw;
    float m_time;
    float m_stateTtl;
    float m_side;
    BuddyState m_state;
    bool m_hasPos;
    bool m_hidden;
    bool m_overloadDone;
    bool m_threatBehind;
    bool m_hasFetch;
    osg::Vec3 m_fetchHint;
    float m_fetchFlashTtl;
};

} // namespace standalone
} // namespace rc

#endif
