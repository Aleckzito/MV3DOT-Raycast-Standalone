#ifndef RC_LOCAL_ARCHITECT_H
#define RC_LOCAL_ARCHITECT_H

#include "LocalPhysicsSolver.h"
#include "MiniVoxelGrid.h"

#include <osg/LightSource>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/ShapeDrawable>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>

#include <vector>

namespace rc {
namespace standalone {

class LocalChunkMesher;
class LocalBoulderWorld;

const osg::Vec4 ARCHITECT_COLOR(0.0f, 0.9f, 1.0f, 1.0f);
// Altura de vuelo a la que aparece en su sector.
const float ARCHITECT_SPAWN_ALT = 2.15f;

enum ArchitectState {
    ARCH_SCANNING = 0,
    ARCH_NAVIGATING = 1,
    ARCH_BUILDING = 2,
    ARCH_FLEEING = 3
};

enum ArchitectBlueprint {
    ARCH_BP_TOWER = 0,
    ARCH_BP_ARCH = 1,
    ARCH_BP_FORT = 2
};

// 101.1 Arquitecto Azul: patrulla, vuela al sitio y construye.
class LocalArchitect {
public:
    LocalArchitect();

    osg::Node* getNode();
    void update(float dt, const osg::Vec3& playerPos, MiniVoxelGrid& grid,
                LocalChunkMesher& mesher, LocalBoulderWorld& boulders);

    ArchitectState state() const { return m_state; }
    osg::Vec3 position() const { return m_pos; }
    void takeDebris(std::vector<osg::Vec3>& out);
    bool consumeBuilt(osg::Vec3* outPos);

    // 102. Evento de caza. Una sola instancia reutilizable: el motor la
    // despierta en un sector, el jugador la elimina y vuelve a dormir.
    void spawnAt(const osg::Vec3& pos);
    void despawn();
    bool isAlive() const { return m_alive; }
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    // true si este golpe lo mata.
    bool takeDamage(int amount);
    AABB makeAabb() const;

    // Segundos entre capas. Cada capa dispara un rebuild del mesher, asi que en
    // mapas grandes conviene un ritmo pausado.
    void setBuildInterval(float seconds) { m_buildInterval = seconds; }

private:
    struct Job {
        ArchitectBlueprint kind;
        int ox;
        int oy;
        int oz;
        int sizeX;
        int sizeY;
        int sizeZ;
        int rot;
        int layer;
        int boulderN;
        int bvx[2];
        int bvy[2];
        int bvz[2];
        std::vector<VoxelKey> cells;
        bool active;
    };

    void buildVisual();
    void syncVisual();
    void setState(ArchitectState st);
    unsigned int nextU();
    int nextRanged(int n);
    void fillTower(Job& job) const;
    void fillArch(Job& job) const;
    void fillFort(Job& job);
    bool cellsFit(const Job& job, const MiniVoxelGrid& grid, const LocalBoulderWorld& boulders,
                  const osg::Vec3& playerPos) const;
    bool tryPickSite(const MiniVoxelGrid& grid, const LocalBoulderWorld& boulders,
                     const osg::Vec3& playerPos, Job* out);
    void hoverGoal(const osg::Vec3& playerPos, osg::Vec3* out) const;
    void jobHover(const Job& job, osg::Vec3* out) const;
    bool flyToward(const osg::Vec3& goal, float speed, float dt);
    void placeLayer(MiniVoxelGrid& grid, LocalChunkMesher& mesher);
    void finishJob(LocalBoulderWorld& boulders, MiniVoxelGrid& grid);
    void triggerFlash();
    void addCell(Job& job, int dx, int dy, int dz) const;

    osg::ref_ptr<osg::MatrixTransform> m_node;
    osg::ref_ptr<osg::ShapeDrawable> m_drawable;
    osg::ref_ptr<osg::Material> m_material;
    osg::ref_ptr<osg::MatrixTransform> m_flashNode;
    osg::ref_ptr<osg::LightSource> m_flashLight;
    osg::Vec3 m_pos;
    ArchitectState m_state;
    ArchitectState m_resume;
    Job m_job;
    float m_time;
    float m_layerTtl;
    float m_scanTtl;
    float m_coolTtl;
    float m_flashTtl;
    unsigned int m_rng;
    std::vector<osg::Vec3> m_debris;
    osg::Vec3 m_builtPos;
    bool m_builtReady;
    bool m_alive;
    int m_hp;
    int m_maxHp;
    float m_buildInterval;
};

} // namespace standalone
} // namespace rc

#endif
