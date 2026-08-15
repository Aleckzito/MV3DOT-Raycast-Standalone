#ifndef RC_LOCAL_BOULDER_H
#define RC_LOCAL_BOULDER_H

#include "MiniVoxelGrid.h"
#include "LocalPhysicsSolver.h"

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

#include <vector>

namespace rc {
namespace standalone {

const float BOULDER_GRAVITY = 9.81f;
const float BOULDER_SLIDE_SPEED = 16.0f;
const float BOULDER_SLIDE_DIST = 3.0f * TILE_SIZE;
const osg::Vec4 BOULDER_COLOR(0.45f, 0.24f, 0.10f, 1.0f);

enum BoulderState {
    BOULDER_RESTING = 0,
    BOULDER_FALLING = 1,
    BOULDER_SLIDING = 2
};

// 98.1 Roca pesada 2x2 mini-voxeles (caja 2*MINI en cada eje).
struct HeavyBoulder2x2 {
    osg::Vec3 pos;
    int vx;
    int vy;
    int vz;
    BoulderState state;
    float velY;
    osg::Vec3 slideFrom;
    osg::Vec3 slideDir;
    float slideRemain;
    bool alive;
    bool squashPlayer;
    std::vector<char> hitEnemy;
    osg::ref_ptr<osg::MatrixTransform> node;
};

class LocalBoulderWorld {
public:
    LocalBoulderWorld();

    osg::Node* getNode();
    void spawnAt(MiniVoxelGrid& grid, int vx, int vy, int vz);
    // 11.4 Tras recargar el mapa, revacia el hueco de las rocas ya vivas.
    void carveFootprints(MiniVoxelGrid& grid) const;

    int boulderCount() const { return static_cast<int>(m_boulders.size()); }
    bool boulderAlive(int index) const;
    bool boulderResting(int index) const;
    osg::Vec3 boulderPos(int index) const;
    BoulderState boulderState(int index) const;
    AABB makeAabb(int index) const;

    void collectSorted(const osg::Vec3& playerPos, std::vector<int>& out) const;
    bool hasSupport(int index, const MiniVoxelGrid& grid) const;
    void beginFall(int index, int enemyCount);
    void beginSlide(int index, float dirX, float dirZ, int enemyCount);
    void resetHits(int index, int enemyCount);
    bool consumePlayerSquash(int index);
    bool consumeEnemyHit(int index, int enemyIndex);

    // 99.2 Caida. true = acabo de asentarse.
    bool updateFalling(int index, float dt, const MiniVoxelGrid& grid);
    // 100.2 Deslizamiento. outHits = voxeles de impacto si choca muro.
    bool updateSliding(int index, float dt, const MiniVoxelGrid& grid,
                       std::vector<VoxelKey>& outHits);
    void settle(int index);
    void syncVisual(int index);

    static void cardinalFromYaw(float yaw, float* dx, float* dz);
    static void cardinalFromVector(float vx, float vz, float* dx, float* dz);

private:
    void buildVisual(HeavyBoulder2x2& b);
    void syncFootprint(HeavyBoulder2x2& b);
    float findSupportY(const HeavyBoulder2x2& b, const MiniVoxelGrid& grid) const;
    bool overlapsVoxels(const AABB& box, const MiniVoxelGrid& grid,
                        std::vector<VoxelKey>* outHits) const;
    AABB aabbOf(const osg::Vec3& pos) const;

    osg::ref_ptr<osg::Group> m_root;
    std::vector<HeavyBoulder2x2> m_boulders;
};

} // namespace standalone
} // namespace rc

#endif
