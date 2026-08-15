#ifndef RC_LOCAL_CENTIPEDE_H
#define RC_LOCAL_CENTIPEDE_H

#include "LocalPhysicsSolver.h"
#include "MiniVoxelGrid.h"

#include <osg/Group>
#include <osg/Material>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

#include <vector>

namespace rc {
namespace standalone {

class LocalBoxWorld;
class LocalBoulderWorld;

const osg::Vec4 CENTIPEDE_COLOR(0.75f, 0.95f, 0.10f, 1.0f);
const osg::Vec4 CENTIPEDE_EYE(1.00f, 0.10f, 0.00f, 1.0f);
const osg::Vec4 CENTIPEDE_LOCK(1.00f, 0.88f, 0.12f, 1.0f);
const float CENTIPEDE_SPACING = 1.10f * MINI_VOXEL_SIZE;
const float CENTIPEDE_SPEED = 4.50f;
const int CENTIPEDE_SEG_HP = 25;
const int CENTIPEDE_SEG_MIN = 6;
const int CENTIPEDE_SEG_MAX = 8;

enum CentipedeHit {
    CENTI_HIT_NONE = 0,
    CENTI_HIT_ALIVE = 1,
    CENTI_HIT_TRIM = 2,
    CENTI_HIT_SPLIT = 3,
    CENTI_HIT_DEAD = 4
};

struct CentipedeSegment {
    osg::Vec3 pos;
    bool isHead;
    bool alive;
    int hp;
    int maxHp;
    osg::ref_ptr<osg::PositionAttitudeTransform> pat;
    osg::ref_ptr<osg::ShapeDrawable> bodyDraw;
    osg::ref_ptr<osg::Material> material;
    osg::ref_ptr<osg::Node> lockBox;
};

// 114. Boss arcade articulado: cabeza + N cubos, split y siembra de mini-voxeles.
class LocalCentipede {
public:
    LocalCentipede();
    LocalCentipede(const osg::Vec3& headPos, int nSeg, float dirX);

    osg::Node* getNode();
    void syncVisual();
    void setLockedSegment(int index);
    int lockedSegment() const { return m_lockedSeg; }

    void updateMovement(float dt, const MiniVoxelGrid& grid, const LocalBoxWorld& boxes,
                        const LocalBoulderWorld& boulders, const osg::Vec3& playerPos);
    CentipedeHit takeDamageAtSegment(int segmentIndex, int dmg, LocalCentipede* outSplit);
    void kill();

    bool isAlive() const { return m_alive && !m_segments.empty(); }
    int segmentCount() const { return static_cast<int>(m_segments.size()); }
    const CentipedeSegment& segment(int i) const { return m_segments[static_cast<size_t>(i)]; }
    AABB makeAabb(int i) const;
    osg::Vec3 lastDeadPos() const { return m_lastDeadPos; }
    float dirX() const { return m_dirX; }
    int totalHp() const;
    int totalMaxHp() const;

private:
    void spawnChain(const osg::Vec3& headPos, int nSeg, float dirX);
    void buildFrom(const std::vector<osg::Vec3>& positions, const std::vector<int>& hps, float dirX);
    void buildSegmentVisual(CentipedeSegment& seg);
    void applyTint(CentipedeSegment& seg, bool locked);
    void promoteHead(int index);
    bool probeBlocked(const osg::Vec3& probe, const MiniVoxelGrid& grid,
                      const LocalBoxWorld& boxes, const LocalBoulderWorld& boulders) const;
    void followBody();

    bool m_alive;
    bool m_orbit;
    float m_dirX;
    float m_rowSign;
    float m_speed;
    float m_stunTtl;
    float m_orbitAng;
    float m_size;
    int m_lockedSeg;
    osg::Vec3 m_lastDeadPos;
    osg::ref_ptr<osg::Group> m_root;
    std::vector<CentipedeSegment> m_segments;
};

} // namespace standalone
} // namespace rc

#endif
