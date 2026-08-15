#ifndef RC_STANDALONE_ENGINE_H
#define RC_STANDALONE_ENGINE_H

#include "LocalBoxWorld.h"
#include "LocalBoulder.h"
#include "LocalArchitect.h"
#include "LocalBuddyController.h"
#include "LocalCameraRig.h"
#include "LocalCentipede.h"
#include "LocalContentRegistry.h"
#include "LocalCrawler.h"
#include "LocalDebris.h"
#include "LocalFloatingText.h"
#include "DummyActor.h"
#include "LocalChunkMesher.h"
#include "LocalEnemy.h"
#include "LocalExp.h"
#include "LocalFlyingBat.h"
#include "LocalInventory.h"
#include "LocalLoot.h"
#include "LocalNpc.h"
#include "LocalQuestLog.h"
#include "LocalPhysicsSolver.h"
#include "LocalProjectile.h"
#include "LocalScriptHost.h"
#include "MiniVoxelGrid.h"
#include "OsgDebugFloor.h"
#include "OsgHud.h"
#include "OsgPhantomRenderer.h"
#include "PhantomCursor.h"
#include "StandaloneInputHandler.h"

#include <osg/Array>
#include <osg/Camera>
#include <osg/Group>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>

#include <cstdint>
#include <string>
#include <vector>

namespace rc {
namespace standalone {

// 2. Composition Root local. Cero sockets. Fisica/red se enganchan despues.
class StandaloneEngine {
public:
    StandaloneEngine();
    ~StandaloneEngine();

    // 2.2 Bucle de vida: init -> update(dt) -> render.
    bool initialize();
    bool initialize(const std::string& worldJsonPath);
    void update(float deltaTime);
    void render();
    void shutdown();

    // 2.3 Play = Brazo. F2 = editor trackball (exploracion tecnica).
    void setEditorMode(bool active);
    bool isEditorMode() const { return m_editorMode; }

    bool shouldQuit() const { return m_quit; }
    void requestQuit() { m_quit = true; }

    // 11. Ruta efectiva del mapa de voxeles (resuelta en initialize).
    const std::string& worldJsonPath() const { return m_worldJsonPath; }
    bool reloadWorldJson();

    // 21.3 Disparo local en la direccion del Dummy / auto-aim.
    void fireProjectile();
    void fireHunterLaser();
    void cycleTarget();
    void cycleEnemyTarget();
    void cycleEntityLockOn();
    void cycleBoxTarget();
    void performMeleeAttack();
    void placeBox();
    void tryPushBox();
    void tryCollapse();
    void cycleCamera();
    void cycleToolTab();
    bool isOrbitCamera() const;
    void applyOrbitMouse(float deltaX, float deltaY);
    bool isSelectMode() const;
    void fireSelectShot();
    void detonateBomb();
    void triggerBomb3x3();
    void hasteBurst();
    bool hasBoxSelection() const;
    bool hasBoulderSelection() const;
    void throwSelectedBlock();
    bool tryPushBoulder();

private:
    bool removeMiniVoxel(int vx, int vy, int vz);
    void spawnDebris(const osg::Vec3& pos, const osg::Vec4& color);
    void spawnFloatingText(const osg::Vec3& pos, const std::string& msg, const osg::Vec4& color,
                           float scale = 0.12f);
    void spawnDefaultObstacles();
    void spawnDefaultPillars();
    void spawnDefaultBoulders();
    bool loadWorldJson(const std::string& path);
    void loadContent(const std::string& metaRelative = std::string());
    // <mapa>.json -> <mapa>.meta.json, vacio si no existe al lado.
    static std::string worldMetaSidecar(const std::string& worldRelative);
    void spawnSandboxActors();
    void spawnSandboxNpcs();
    void updateNpcs(float dt);
    void applyEnemyStats(LocalEnemy& enemy);
    void fireContentHook(const std::string& name, const std::string& kind);
    void updateDebris(float deltaTime);
    void updateFloatingText(float deltaTime);
    void updatePlayCamera(float deltaTime);
    void updateCameraXRay();
    void collectOcclusionVoxels(const osg::Vec3& cameraPos, const osg::Vec3& dummyHead,
                                std::vector<VoxelKey>& out);
    void updateProjectiles(float deltaTime);
    void updateMeleeFlash(float deltaTime);
    void spawnMeleeFlash();
    void removeEnemyNode(LocalEnemy& enemy);
    void onEnemyKilled(LocalEnemy& enemy, int index);
    void spawnEnemyAt(const osg::Vec3& pos, bool isBoss);
    void spawnEnemyAt(const osg::Vec3& pos, EnemyKind kind);
    void spawnEnemyMissile(const osg::Vec3& origin, const osg::Vec3& target);
    void tryStartColumnFall(int vx, int holeVy, int vz);
    void updateFallingColumns(float dt);
    void spawnFallingCluster(const std::vector<VoxelKey>& cells);
    void applyCrush(const osg::Vec3& impact);
    void updateThrownBlocks(float dt);
    void updateBoulders(float dt);
    void applySquash(const osg::Vec3& impact, int amount);
    void updateBats(float dt);
    void spawnBatAt(const osg::Vec3& pos);
    void onBatKilled(LocalFlyingBat& bat, int index);
    void updateCrawlers(float dt);
    void spawnCrawlerAt(const osg::Vec3& pos);
    void onCrawlerKilled(LocalCrawler& crawler, int index);
    void updateCentipedes(float dt);
    void spawnCentipedeBoss();
    void adoptCentipede(LocalCentipede& worm);
    void plantCentipedeBrick(const osg::Vec3& pos);
    void applyCentipedeDamage(int worm, int seg, int dmg);
    void onCentipedeKilled(int index);
    void noteKill();
    void fireHunterBeam();
    void applyHunterDamageLocked(const osg::Vec3& muzzle);
    bool applyHunterFrontal(const osg::Vec3& muzzle, const osg::Vec3& facing, osg::Vec3* outHit);
    void spawnHunterBeam(const osg::Vec3& from, const osg::Vec3& to);
    void updateHunterBeams(float dt);
    bool hasCombatLock() const;
    bool lockedTargetCenter(osg::Vec3* out) const;
    int findFrontBoulder() const;
    int nearestEnemyInCone(const osg::Vec3& origin, const osg::Vec3& facing, float maxDist) const;
    osg::Vec3 nearestCover(const osg::Vec3& from) const;
    void handlePlayerHit(int amount);
    void spawnLootAt(const osg::Vec3& pos, LootSize size);
    void spawnExpAt(const osg::Vec3& pos, int expValue);
    void resolveBossStomp(LocalEnemy& enemy);
    void updateSpawner(float deltaTime);
    void updateLoot(float deltaTime);
    void updateExpDrops(float deltaTime);
    void updateDrops(float deltaTime);
    float dropHitFloorY(float x, float y, float z) const;
    void stepDropKinematics(osg::Vec3& pos, osg::Vec3& vel, bool& grounded, float dt,
                            const osg::Vec3& playerCenter);
    void collectLoot(LocalLoot& loot, bool byFary);
    void collectExp(LocalExp& drop, bool byFary);
    void hintBuddyFetch();
    void updateSelectTools(float deltaTime);
    void clearSelection();
    void spawnCollapse(const osg::Vec3& pos);
    void buildSelectHilite();
    void buildBombRing();
    void buildGhostPreview();
    void updateGhostPreview();
    void buildPipMirror();
    void updateBuddyCamera();

    bool m_initialized;
    bool m_quit;
    bool m_editorMode;

    int m_width;
    int m_height;

    osg::ref_ptr<osgViewer::Viewer> m_viewer;
    osg::ref_ptr<osg::GraphicsContext> m_graphicsContext;
    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Group> m_worldRoot;
    osg::ref_ptr<osg::Camera> m_pipFaryCamera;
    osg::ref_ptr<osg::Camera> m_pipFrame;
    osg::ref_ptr<osgGA::TrackballManipulator> m_trackball;
    osg::ref_ptr<StandaloneInputHandler> m_inputHandler;

    LocalPhysicsSolver m_localPhysics;
    MiniVoxelGrid m_miniVoxels;
    LocalContentRegistry m_content;
    std::string m_worldJsonPath;
    LocalScriptHost m_scripts;
    LocalInventory m_inventory;
    LocalQuestLog m_quests;
    PhantomCursor m_phantomCursor;
    OsgPhantomRenderer m_phantomRenderer;
    OsgDebugFloor m_debugFloor;
    LocalChunkMesher m_localMesher;
    DummyActor m_dummyActor;
    LocalBoxWorld m_boxWorld;
    LocalBoulderWorld m_boulderWorld;
    LocalArchitect m_architect;
    LocalBuddyController m_buddy;
    LocalCameraRig m_camRig;
    OsgHud m_hud;
    osg::ref_ptr<osg::Group> m_projectileRoot;
    osg::ref_ptr<osg::Group> m_enemyRoot;
    osg::ref_ptr<osg::Group> m_lootRoot;
    osg::ref_ptr<osg::Group> m_expRoot;
    osg::ref_ptr<osg::Group> m_debrisRoot;
    osg::ref_ptr<osg::Group> m_floatTextRoot;
    std::vector<LocalProjectile> m_projectiles;
    std::vector<LocalEnemy> m_enemies;
    std::vector<LocalFlyingBat> m_bats;
    std::vector<LocalCrawler> m_crawlers;
    std::vector<LocalCentipede> m_centipedes;
    std::vector<LocalNpc> m_npcs;
    std::vector<LocalLoot> m_loots;
    std::vector<LocalExp> m_exps;
    std::vector<LocalDebris> m_debris;
    std::vector<LocalFloatingText> m_floatTexts;
    struct FallingCluster {
        struct Cell {
            int vx;
            int vy;
            int vz;
            uint16_t mat;
        };
        std::vector<Cell> cells;
        float yOff;
        float velY;
        bool crushed;
        bool active;
        osg::ref_ptr<osg::MatrixTransform> node;
    };
    std::vector<FallingCluster> m_fallingCols;
    struct ThrownBlock {
        osg::Vec3 pos;
        osg::Vec3 vel;
        float ttl;
        int target;
        bool alive;
        osg::ref_ptr<osg::MatrixTransform> node;
    };
    std::vector<ThrownBlock> m_thrown;
    osg::ref_ptr<osg::Vec4Array> m_pipOuterColor;
    osg::ref_ptr<osg::Vec4Array> m_pipInnerColor;
    bool m_threatBehind;
    struct HunterBeam {
        osg::ref_ptr<osg::MatrixTransform> node;
        osg::ref_ptr<osg::Material> material;
        float ttl;
        float life;
    };
    std::vector<HunterBeam> m_hunterBeams;
    int m_lockedEnemyIndex;
    int m_lockedBatIndex;
    int m_lockedCrawlerIndex;
    int m_lockedCentipedeIndex;
    int m_lockedCentipedeSeg;
    int m_hunterCells;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_meleeFlash;
    float m_meleeFlashTtl;
    float m_spawnTimer;
    unsigned int m_spawnSeed;
    int m_killCount;
    float m_survivalTime;
    bool m_pendingBoss;
    bool m_pendingCentipede;
    float m_centipedeAlertTtl;
    float m_questAlertTtl;
    std::string m_questAlertText;
    std::vector<VoxelKey> m_xrayVoxels;

    osg::Vec3 m_currentCameraEye;
    osg::Vec3 m_currentCameraCenter;
    bool m_cameraInitialized;

    float m_phantomProbeX;
    int m_lastPhantomVx;
    int m_lastPhantomVy;
    int m_lastPhantomVz;

    enum ToolMode {
        TOOL_BUILD = 0,
        TOOL_SELECT = 1
    };
    ToolMode m_toolMode;
    int m_selectedBox;
    int m_selectedBoulder;
    int m_selectCursor;
    float m_selectPulse;
    float m_spaceHold;
    bool m_spaceWasDown;
    bool m_bombCharging;
    int m_bombIx;
    int m_bombIy;
    int m_bombIz;
    osg::Vec3 m_bombPos;
    float m_bombRingTtl;
    float m_camShakeTtl;
    osg::ref_ptr<osg::MatrixTransform> m_selectHilite;
    osg::ref_ptr<osg::MatrixTransform> m_bombRing;
    osg::ref_ptr<osg::MatrixTransform> m_previewBox;

    bool createOsgContext(const char* title);
};

} // namespace standalone
} // namespace rc

#endif
