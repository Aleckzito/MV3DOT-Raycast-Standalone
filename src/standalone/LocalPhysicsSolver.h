#ifndef RC_LOCAL_PHYSICS_SOLVER_H
#define RC_LOCAL_PHYSICS_SOLVER_H

namespace rc {
namespace standalone {

class MiniVoxelGrid;
class DummyActor;
class LocalEnemy;
class LocalBoxWorld;
class LocalBoulderWorld;
class StandaloneInputHandler;

struct AABB {
    float minX;
    float minY;
    float minZ;
    float maxX;
    float maxY;
    float maxZ;
};

// 12 / 15. Solver local: Y + X/Z separados + auto-step.
class LocalPhysicsSolver {
public:
    LocalPhysicsSolver();

    void setGrid(const MiniVoxelGrid* grid);
    void setDummy(DummyActor* dummy);
    void setBoxWorld(const LocalBoxWorld* boxes);
    void setBoulderWorld(const LocalBoulderWorld* boulders);
    void setInput(StandaloneInputHandler* input);

    bool checkCollision(const AABB& box) const;
    void updatePhysics(float deltaTime);
    // 28. AABB X/Z + gravedad Y. blocker = jugador (no atravesarlo).
    void updateEnemyPhysics(LocalEnemy& enemy, float oldX, float oldZ, float dt, const AABB* blocker);

private:
    float findSupportY(const AABB& footprint, float fromY) const;
    void moveAxisX(float deltaTime, bool canStep);
    void moveAxisZ(float deltaTime, bool canStep);
    bool overlapsBoxes(const AABB& box) const;
    bool overlapsBoulders(const AABB& box) const;

    const MiniVoxelGrid* m_grid;
    DummyActor* m_dummy;
    const LocalBoxWorld* m_boxes;
    const LocalBoulderWorld* m_boulders;
    StandaloneInputHandler* m_input;
};

} // namespace standalone
} // namespace rc

#endif
