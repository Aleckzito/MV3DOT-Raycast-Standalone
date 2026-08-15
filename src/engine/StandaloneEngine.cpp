#include "StandaloneEngine.h"

#include "PlayerSave.h"
#include "StandaloneWorldIO.h"
#include "world_defaults.h"

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/GraphicsContext>
#include <osg/GL>
#include <osg/Light>
#include <osg/LightSource>
#include <osg/LineWidth>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/MatrixTransform>
#include <osg/PolygonMode>
#include <osg/PositionAttitudeTransform>
#include <osg/PrimitiveSet>
#include <osg/Quat>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/Vec3>
#include <osg/Vec3d>
#include <osg/Vec4>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIEventHandler>
#include <osgViewer/GraphicsWindow>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace rc {
namespace standalone {

namespace {

// Solo cierra ventana. Trackball maneja el mouse (7.3).
class CloseWindowHandler : public osgGA::GUIEventHandler {
public:
    StandaloneEngine* engine = nullptr;

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&) override
    {
        if (engine != nullptr && ea.getEventType() == osgGA::GUIEventAdapter::CLOSE_WINDOW) {
            engine->requestQuit();
        }
        return false;
    }
};

const char* compassFromYaw(float yaw)
{
    // 51.2 yaw=0 mira +Z = Norte. 90 deg = +X = Este.
    float deg = yaw * 180.0f / 3.14159265f;
    while (deg < 0.0f) {
        deg += 360.0f;
    }
    while (deg >= 360.0f) {
        deg -= 360.0f;
    }
    int sector = static_cast<int>(std::floor((deg + 22.5f) / 45.0f));
    sector = sector % 8;
    if (sector < 0) {
        sector += 8;
    }
    static const char* names[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    return names[sector];
}

int worldToVoxelIndex(float world)
{
    return static_cast<int>(std::floor(world / MINI_VOXEL_SIZE));
}

bool aabbOverlap(const AABB& a, const AABB& b)
{
    return a.minX <= b.maxX && a.maxX >= b.minX &&
           a.minY <= b.maxY && a.maxY >= b.minY &&
           a.minZ <= b.maxZ && a.maxZ >= b.minZ;
}

const int kVoxelNbs[6][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
};

float pointToSegmentDist(const osg::Vec3& p, const osg::Vec3& a, const osg::Vec3& b)
{
    const osg::Vec3 ab = b - a;
    const float ab2 = ab.length2();
    if (ab2 < 1.0e-8f) {
        return (p - a).length();
    }
    float t = ((p - a) * ab) / ab2;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    const osg::Vec3 closest = a + ab * t;
    return (p - closest).length();
}

const osg::Vec4 kVoxelColor(0.72f, 0.74f, 0.78f, 1.0f);
const osg::Vec4 kFloatDmgEnemy(1.00f, 0.92f, 0.20f, 1.0f);
const osg::Vec4 kFloatDmgPlayer(1.00f, 0.18f, 0.12f, 1.0f);
const osg::Vec4 kFloatHeal(0.25f, 1.00f, 0.32f, 1.0f);
const osg::Vec4 kFloatExp(0.10f, 0.85f, 1.00f, 1.0f);
const osg::Vec4 kFloatHunter(0.25f, 0.95f, 1.00f, 1.0f);
const int kHunterCellMax = 99;
const int kHunterStartCells = 6;
const float kDropGravity = 9.81f;
const float kDropFloorY = 0.20f;
const float kMagnetRange = 4.50f * TILE_SIZE;
const float kAbsorbRange = 0.80f * TILE_SIZE;
const float kMagnetSpeed = 14.0f;

bool aabbContainsPoint(const AABB& box, const osg::Vec3& p, float pad)
{
    return p.x() >= box.minX - pad && p.x() <= box.maxX + pad &&
           p.y() >= box.minY - pad && p.y() <= box.maxY + pad &&
           p.z() >= box.minZ - pad && p.z() <= box.maxZ + pad;
}

bool segmentHitsAabb(const osg::Vec3& a, const osg::Vec3& b, const AABB& box)
{
    const osg::Vec3 c(
        0.5f * (box.minX + box.maxX),
        0.5f * (box.minY + box.maxY),
        0.5f * (box.minZ + box.maxZ));
    const float hx = 0.5f * (box.maxX - box.minX) + 0.04f;
    const float hy = 0.5f * (box.maxY - box.minY) + 0.04f;
    const float hz = 0.5f * (box.maxZ - box.minZ) + 0.04f;
    const float r = std::sqrt(hx * hx + hy * hy + hz * hz);
    return pointToSegmentDist(c, a, b) <= r ||
           aabbContainsPoint(box, a, 0.04f) ||
           aabbContainsPoint(box, b, 0.04f);
}

bool firstSolidOnSegment(const MiniVoxelGrid& grid,
                         const osg::Vec3& from,
                         const osg::Vec3& to,
                         int* outVx, int* outVy, int* outVz)
{
    // 59.1 DDA: primer mini-voxel solido en el segmento.
    float dx = to.x() - from.x();
    float dy = to.y() - from.y();
    float dz = to.z() - from.z();
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1.0e-8f) {
        const int vx = worldToVoxelIndex(from.x());
        const int vy = worldToVoxelIndex(from.y());
        const int vz = worldToVoxelIndex(from.z());
        if (vy >= 0 && grid.getVoxel(vx, vy, vz).isActive) {
            *outVx = vx;
            *outVy = vy;
            *outVz = vz;
            return true;
        }
        return false;
    }
    dx /= len;
    dy /= len;
    dz /= len;

    int vx = worldToVoxelIndex(from.x());
    int vy = worldToVoxelIndex(from.y());
    int vz = worldToVoxelIndex(from.z());
    const int stepX = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
    const int stepY = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);
    const int stepZ = (dz > 0.0f) ? 1 : ((dz < 0.0f) ? -1 : 0);

    const float kInf = 1.0e30f;
    const float tDeltaX = (stepX == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dx));
    const float tDeltaY = (stepY == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dy));
    const float tDeltaZ = (stepZ == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dz));

    float tMaxX = kInf;
    float tMaxY = kInf;
    float tMaxZ = kInf;
    if (stepX > 0) {
        tMaxX = ((static_cast<float>(vx + 1) * MINI_VOXEL_SIZE) - from.x()) / dx;
    } else if (stepX < 0) {
        tMaxX = ((static_cast<float>(vx) * MINI_VOXEL_SIZE) - from.x()) / dx;
    }
    if (stepY > 0) {
        tMaxY = ((static_cast<float>(vy + 1) * MINI_VOXEL_SIZE) - from.y()) / dy;
    } else if (stepY < 0) {
        tMaxY = ((static_cast<float>(vy) * MINI_VOXEL_SIZE) - from.y()) / dy;
    }
    if (stepZ > 0) {
        tMaxZ = ((static_cast<float>(vz + 1) * MINI_VOXEL_SIZE) - from.z()) / dz;
    } else if (stepZ < 0) {
        tMaxZ = ((static_cast<float>(vz) * MINI_VOXEL_SIZE) - from.z()) / dz;
    }

    const float maxT = len + MINI_VOXEL_SIZE * 0.01f;
    for (int guard = 0; guard < 256; ++guard) {
        if (vy >= 0 && grid.getVoxel(vx, vy, vz).isActive) {
            *outVx = vx;
            *outVy = vy;
            *outVz = vz;
            return true;
        }
        const float tNext = (tMaxX <= tMaxY)
            ? ((tMaxX <= tMaxZ) ? tMaxX : tMaxZ)
            : ((tMaxY <= tMaxZ) ? tMaxY : tMaxZ);
        if (tNext > maxT) {
            break;
        }
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            vx += stepX;
            tMaxX += tDeltaX;
        } else if (tMaxY <= tMaxZ) {
            vy += stepY;
            tMaxY += tDeltaY;
        } else {
            vz += stepZ;
            tMaxZ += tDeltaZ;
        }
    }
    return false;
}

} // namespace

StandaloneEngine::StandaloneEngine()
    : m_initialized(false)
    , m_quit(false)
    , m_editorMode(false)
    , m_width(DEFAULT_DISPLAY_WIDTH)
    , m_height(DEFAULT_DISPLAY_HEIGHT)
    , m_currentCameraEye(4.0f, 2.5f, 1.5f)
    , m_currentCameraCenter(4.0f, 0.6f, 4.0f)
    , m_cameraInitialized(false)
    , m_phantomProbeX(0.0f)
    , m_lastPhantomVx(0x7FFFFFFF)
    , m_lastPhantomVy(0x7FFFFFFF)
    , m_lastPhantomVz(0x7FFFFFFF)
    , m_threatBehind(false)
    , m_lockedEnemyIndex(-1)
    , m_lockedBatIndex(-1)
    , m_lockedCrawlerIndex(-1)
    , m_lockedCentipedeIndex(-1)
    , m_lockedCentipedeSeg(-1)
    , m_hunterCells(kHunterStartCells)
    , m_meleeFlashTtl(0.0f)
    , m_spawnTimer(0.0f)
    , m_spawnSeed(1u)
    , m_architectTimer(180.0f)
    , m_architectSeed(4242u)
    , m_lockedArchitect(false)
    , m_killCount(0)
    , m_survivalTime(0.0f)
    , m_pendingBoss(false)
    , m_pendingCentipede(false)
    , m_centipedeAlertTtl(0.0f)
    , m_questAlertTtl(0.0f)
    , m_toolMode(TOOL_BUILD)
    , m_selectedBox(-1)
    , m_selectedBoulder(-1)
    , m_selectCursor(0)
    , m_selectPulse(0.0f)
    , m_spaceHold(0.0f)
    , m_spaceWasDown(false)
    , m_bombCharging(false)
    , m_bombIx(0)
    , m_bombIy(0)
    , m_bombIz(0)
    , m_bombPos(0.0f, 0.0f, 0.0f)
    , m_bombRingTtl(0.0f)
    , m_camShakeTtl(0.0f)
{
}

StandaloneEngine::~StandaloneEngine()
{
    shutdown();
}

bool StandaloneEngine::initialize()
{
    return initialize(std::string());
}

bool StandaloneEngine::initialize(const std::string& worldJsonPath)
{
    if (m_initialized) {
        return true;
    }

    if (!createOsgContext("MV3D Standalone")) {
        std::cerr << "[standalone] OSG context failed\n";
        return false;
    }

    // 11. Un mapa pedido por argv manda, y trae su propio meta si existe al lado
    // como <mapa>.meta.json: asi una arena suelta carga sus spawns sin que haya
    // que sobrescribir el meta del pack, que esta versionado.
    // Sin argv se invierte: manda el meta del pack y el define voxelWorld.
    std::string metaRelative;
    if (!worldJsonPath.empty()) {
        const std::string sidecar = worldMetaSidecar(worldJsonPath);
        if (!sidecar.empty()) {
            metaRelative = sidecar;
        }
    }
    loadContent(metaRelative);

    std::string worldRelative = worldJsonPath;
    if (worldRelative.empty()) {
        worldRelative = m_content.voxelWorldPath();
    }
    if (worldRelative.empty()) {
        worldRelative = "data/worlds/standalone_sandbox.json";
    }
    m_worldJsonPath = LocalContentRegistry::resolve(worldRelative);

    // 7.2 / 9.5 / 10.3 / 67. Suelo + coberturas + mesh holder.
    m_localMesher.setGrid(&m_miniVoxels);
    spawnDefaultObstacles();
    m_localPhysics.setGrid(&m_miniVoxels);
    m_localPhysics.setDummy(&m_dummyActor);
    m_localPhysics.setBoxWorld(&m_boxWorld);
    m_localPhysics.setBoulderWorld(&m_boulderWorld);

    // S3. playerSpawn del meta; si falta, el default del prototipo.
    const WorldSpawn& start = m_content.playerSpawn();
    if (start.valid) {
        m_dummyActor.teleport(start.x, start.y, start.z);
        std::cout << "[world] playerSpawn meta (" << start.x << ", " << start.y << ", "
                  << start.z << ")\n";
    } else {
        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
    }

    m_projectileRoot = new osg::Group;
    m_enemyRoot = new osg::Group;
    m_lootRoot = new osg::Group;
    m_expRoot = new osg::Group;
    m_debrisRoot = new osg::Group;
    m_floatTextRoot = new osg::Group;
    m_worldRoot->addChild(m_debugFloor.getNode());
    m_worldRoot->addChild(m_localMesher.getNode());
    m_worldRoot->addChild(m_dummyActor.getNode());
    m_worldRoot->addChild(m_buddy.getNode());
    m_worldRoot->addChild(m_architect.getNode());
    // 102. Arranca dormido: el evento lo despierta cuando vence el temporizador.
    m_architect.despawn();
    resetArchitectTimer();
    m_worldRoot->addChild(m_boxWorld.getNode());
    m_worldRoot->addChild(m_boulderWorld.getNode());
    buildSelectHilite();
    buildBombRing();
    buildGhostPreview();
    if (m_selectHilite.valid()) {
        m_worldRoot->addChild(m_selectHilite.get());
    }
    if (m_bombRing.valid()) {
        m_worldRoot->addChild(m_bombRing.get());
    }
    if (m_previewBox.valid()) {
        m_worldRoot->addChild(m_previewBox.get());
    }
    m_worldRoot->addChild(m_projectileRoot.get());
    m_worldRoot->addChild(m_enemyRoot.get());
    m_worldRoot->addChild(m_lootRoot.get());
    m_worldRoot->addChild(m_expRoot.get());
    m_worldRoot->addChild(m_debrisRoot.get());
    m_worldRoot->addChild(m_floatTextRoot.get());
    m_worldRoot->addChild(m_phantomRenderer.getNode());
    if (m_hud.create(m_width, m_height, m_graphicsContext.get()) && m_hud.getNode() != nullptr) {
        m_root->addChild(m_hud.getNode());
    }
    buildPipMirror();

    spawnSandboxActors();
    spawnSandboxNpcs();

    osg::ref_ptr<osg::LightSource> lightSource = new osg::LightSource;
    osg::Light* light = lightSource->getLight();
    light->setLightNum(0);
    light->setPosition(osg::Vec4(3.0f, 8.0f, 4.0f, 1.0f));
    light->setDiffuse(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    light->setAmbient(osg::Vec4(0.35f, 0.35f, 0.38f, 1.0f));
    m_worldRoot->addChild(lightSource.get());
    osg::StateSet* rootState = m_root->getOrCreateStateSet();
    rootState->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    rootState->setMode(GL_LIGHT0, osg::StateAttribute::ON);
    rootState->setMode(GL_LIGHT1, osg::StateAttribute::ON);

    // Trackball reservado para F2 (editor / exploracion tecnica). No se activa al inicio.
    m_trackball = new osgGA::TrackballManipulator;
    m_trackball->setHomePosition(
        osg::Vec3(1.8f, 2.4f, 3.6f),
        osg::Vec3(1.0f, 0.0f, 0.5f),
        osg::Vec3(0.0f, 1.0f, 0.0f));
    m_viewer->setCameraManipulator(nullptr);

    // 8.5 / 10.1 Handler con acceso directo a grid + mesher + cursor.
    m_inputHandler = new StandaloneInputHandler;
    m_inputHandler->bind(&m_miniVoxels, &m_localMesher, &m_phantomCursor, &m_dummyActor, this);
    m_localPhysics.setInput(m_inputHandler.get());
    m_viewer->addEventHandler(m_inputHandler.get());

    m_viewer->realize();
    if (!m_viewer->isRealized()) {
        std::cerr << "[standalone] OSG realize failed\n";
        return false;
    }
    m_initialized = true;
    m_quit = false;
    m_cameraInitialized = false;
    m_phantomProbeX = 0.0f;
    m_lastPhantomVx = 0x7FFFFFFF;
    m_lastPhantomVy = 0x7FFFFFFF;
    m_lastPhantomVz = 0x7FFFFFFF;
    m_phantomCursor.resetDda();
    std::srand(1);
    m_killCount = 0;
    std::cout << "[standalone] engine ready "
              << m_width << "x" << m_height
              << " editorMode=" << (m_editorMode ? 1 : 0)
              << " miniVoxel=" << MINI_VOXEL_SIZE
              << "\n";
    return true;
}

bool StandaloneEngine::createOsgContext(const char* title)
{
    // 1.3 / 2.2 Contexto OSG. Realize ocurre en initialize() tras armar la escena.
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 50;
    traits->y = 50;
    traits->width = m_width;
    traits->height = m_height;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->vsync = true;
    traits->windowName = title;
    traits->alpha = 8;
    traits->depth = 24;
    traits->stencil = 8;
    traits->readDISPLAY();
    traits->setUndefinedScreenDetailsToDefaultScreen();
#ifdef _WIN32
    traits->windowingSystemPreference = "Win32";
#else
    traits->windowingSystemPreference = "SDL";
#endif

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc.valid()) {
        std::cerr << "[standalone] primary OSG backend failed, trying default\n";
        traits->windowingSystemPreference.clear();
        gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    }
    if (!gc.valid()) {
        return false;
    }
    m_graphicsContext = gc;

    m_viewer = new osgViewer::Viewer;
    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    m_viewer->getCamera()->setGraphicsContext(gc.get());
    m_viewer->getCamera()->setViewport(0, 0, m_width, m_height);
    m_viewer->getCamera()->setClearColor(osg::Vec4(0.12f, 0.14f, 0.18f, 1.0f));

    const double aspect = static_cast<double>(m_width) / static_cast<double>(m_height);
    m_viewer->getCamera()->setProjectionMatrixAsPerspective(
        DEFAULT_FOV_DEGREES, aspect, 1.0, 64.0);

    m_root = new osg::Group;
    m_worldRoot = new osg::Group;
    m_root->addChild(m_worldRoot.get());
    m_viewer->setSceneData(m_root.get());
    m_viewer->setKeyEventSetsDone(false);

    osg::ref_ptr<CloseWindowHandler> closeHandler = new CloseWindowHandler;
    closeHandler->engine = this;
    m_viewer->addEventHandler(closeHandler.get());
    return true;
}

void StandaloneEngine::update(float deltaTime)
{
    if (!m_initialized) {
        return;
    }

    if (m_inputHandler.valid()) {
        m_inputHandler->setInvertMove(m_camRig.invertMove());
        m_inputHandler->setArrowOrbit(m_camRig.arrowsOrbit());
        if (m_camRig.arrowsOrbit()) {
            float oy = 0.0f;
            float op = 0.0f;
            m_inputHandler->getOrbitAxes(oy, op);
            m_camRig.addOrbit(oy * deltaTime * 2.20f, op * deltaTime * 1.60f);
        }
    }

    m_survivalTime += deltaTime;

    m_localPhysics.updatePhysics(deltaTime);
    m_dummyActor.tickIFrames(deltaTime);
    m_dummyActor.syncVisual();

    // 27 / 28 / 29. Chase + fisica enemiga + melee vs Dummy.
    const osg::Vec3 dummyPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    const AABB dummyAabb = m_dummyActor.makeAabb();
    const float meleeR = 0.6f * TILE_SIZE;
    const float meleeR2 = meleeR * meleeR;
    for (size_t e = 0; e < m_enemies.size(); ++e) {
        LocalEnemy& enemy = m_enemies[e];
        if (!enemy.isAlive) {
            continue;
        }
        const float oldX = enemy.pos.x();
        const float oldZ = enemy.pos.z();
        if (enemy.isArcher()) {
            const osg::Vec3 from = enemy.muzzle();
            const osg::Vec3 to(
                m_dummyActor.x(),
                m_dummyActor.y() + m_dummyActor.height() * 0.70f,
                m_dummyActor.z());
            int lvx = 0;
            int lvy = 0;
            int lvz = 0;
            const bool blocked = firstSolidOnSegment(m_miniVoxels, from, to, &lvx, &lvy, &lvz);
            enemy.updateArcherAI(deltaTime, dummyPos, !blocked, nearestCover(enemy.pos));
            if (enemy.consumeArrowShot()) {
                spawnEnemyMissile(from, to);
            }
        } else {
            enemy.updateAI(deltaTime, dummyPos);
        }
        if (enemy.consumeStompPulse()) {
            resolveBossStomp(enemy);
        }
        m_localPhysics.updateEnemyPhysics(enemy, oldX, oldZ, deltaTime, &dummyAabb);
        enemy.setTargeted(static_cast<int>(e) == m_lockedEnemyIndex);
        enemy.syncVisual();

        if (enemy.isArcher()) {
            continue;
        }
        const float dx = enemy.pos.x() - m_dummyActor.x();
        const float dz = enemy.pos.z() - m_dummyActor.z();
        if (m_dummyActor.iFrames() <= 0.0f && dx * dx + dz * dz <= meleeR2) {
            handlePlayerHit(10);
        }
    }

    updateBats(deltaTime);
    updateCrawlers(deltaTime);
    updateCentipedes(deltaTime);
    updateNpcs(deltaTime);
    updateProjectiles(deltaTime);
    updateHunterBeams(deltaTime);
    m_scripts.tick(deltaTime);
    updateThrownBlocks(deltaTime);
    updateFallingColumns(deltaTime);
    updateMeleeFlash(deltaTime);
    updateSpawner(deltaTime);
    updateDebris(deltaTime);
    updateFloatingText(deltaTime);

    // 32.1 Auto-clear: muerto o > 12 tiles.
    if (m_lockedEnemyIndex >= 0) {
        bool clearLock = false;
        if (m_lockedEnemyIndex >= static_cast<int>(m_enemies.size())) {
            clearLock = true;
        } else {
            LocalEnemy& locked = m_enemies[static_cast<size_t>(m_lockedEnemyIndex)];
            if (!locked.isAlive) {
                locked.setTargeted(false);
                clearLock = true;
            } else {
                const float ldx = locked.pos.x() - m_dummyActor.x();
                const float ldz = locked.pos.z() - m_dummyActor.z();
                const float lockLimit = 12.0f * TILE_SIZE;
                if (ldx * ldx + ldz * ldz > lockLimit * lockLimit) {
                    locked.setTargeted(false);
                    locked.syncVisual();
                    clearLock = true;
                }
            }
        }
        if (clearLock) {
            m_lockedEnemyIndex = -1;
            std::cout << "[lock] clear\n";
        }
    }
    if (m_lockedBatIndex >= 0) {
        bool clearBat = false;
        if (m_lockedBatIndex >= static_cast<int>(m_bats.size()) ||
            !m_bats[static_cast<size_t>(m_lockedBatIndex)].isAlive()) {
            clearBat = true;
        } else {
            const osg::Vec3 bp = m_bats[static_cast<size_t>(m_lockedBatIndex)].pos;
            const float ldx = bp.x() - m_dummyActor.x();
            const float ldz = bp.z() - m_dummyActor.z();
            const float lockLimit = 12.0f * TILE_SIZE;
            if (ldx * ldx + ldz * ldz > lockLimit * lockLimit) {
                m_bats[static_cast<size_t>(m_lockedBatIndex)].setTargeted(false);
                m_bats[static_cast<size_t>(m_lockedBatIndex)].syncVisual();
                clearBat = true;
            }
        }
        if (clearBat) {
            m_lockedBatIndex = -1;
        }
    }
    if (m_lockedCrawlerIndex >= 0) {
        bool clearCrawler = false;
        if (m_lockedCrawlerIndex >= static_cast<int>(m_crawlers.size()) ||
            !m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].isAlive()) {
            clearCrawler = true;
        } else {
            const osg::Vec3 cp = m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].pos;
            const float ldx = cp.x() - m_dummyActor.x();
            const float ldz = cp.z() - m_dummyActor.z();
            const float lockLimit = 12.0f * TILE_SIZE;
            if (ldx * ldx + ldz * ldz > lockLimit * lockLimit) {
                m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].setTargeted(false);
                m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].syncVisual();
                clearCrawler = true;
            }
        }
        if (clearCrawler) {
            m_lockedCrawlerIndex = -1;
        }
    }
    if (m_lockedCentipedeIndex >= 0) {
        bool clearCent = false;
        if (m_lockedCentipedeIndex >= static_cast<int>(m_centipedes.size()) ||
            !m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].isAlive()) {
            clearCent = true;
        } else {
            LocalCentipede& worm = m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)];
            if (m_lockedCentipedeSeg < 0 || m_lockedCentipedeSeg >= worm.segmentCount()) {
                clearCent = true;
            } else {
                const osg::Vec3 cp = worm.segment(m_lockedCentipedeSeg).pos;
                const float ldx = cp.x() - m_dummyActor.x();
                const float ldz = cp.z() - m_dummyActor.z();
                const float lockLimit = 12.0f * TILE_SIZE;
                if (ldx * ldx + ldz * ldz > lockLimit * lockLimit) {
                    worm.setLockedSegment(-1);
                    clearCent = true;
                }
            }
        }
        if (clearCent) {
            if (m_lockedCentipedeIndex >= 0 &&
                m_lockedCentipedeIndex < static_cast<int>(m_centipedes.size())) {
                m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].setLockedSegment(-1);
            }
            m_lockedCentipedeIndex = -1;
            m_lockedCentipedeSeg = -1;
        }
    }

    // 25.1 / 25.2 Amenaza trasera: dummy vivo a <= 6 tiles y dot(frente, toEnemy) < 0.
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const float alertR = 6.0f * TILE_SIZE;
    const float alertR2 = alertR * alertR;
    bool threatBehind = false;
    for (size_t e = 0; e < m_enemies.size(); ++e) {
        if (!m_enemies[e].isAlive) {
            continue;
        }
        const float dx = m_enemies[e].pos.x() - m_dummyActor.x();
        const float dz = m_enemies[e].pos.z() - m_dummyActor.z();
        if (dx * dx + dz * dz > alertR2) {
            continue;
        }
        const float toDot = fx * dx + fz * dz;
        if (toDot < 0.0f) {
            threatBehind = true;
            break;
        }
    }
    if (!threatBehind) {
        for (size_t b = 0; b < m_bats.size(); ++b) {
            LocalFlyingBat& bat = m_bats[b];
            if (!bat.isAlive()) {
                continue;
            }
            if (bat.isDiveStrike()) {
                threatBehind = true;
                break;
            }
            const float dx = bat.pos.x() - m_dummyActor.x();
            const float dz = bat.pos.z() - m_dummyActor.z();
            if (dx * dx + dz * dz > alertR2) {
                continue;
            }
            if (fx * dx + fz * dz < 0.0f) {
                threatBehind = true;
                break;
            }
        }
    }
    if (!threatBehind) {
        for (size_t c = 0; c < m_crawlers.size(); ++c) {
            if (!m_crawlers[c].isAlive()) {
                continue;
            }
            const float dx = m_crawlers[c].pos.x() - m_dummyActor.x();
            const float dz = m_crawlers[c].pos.z() - m_dummyActor.z();
            if (dx * dx + dz * dz > alertR2) {
                continue;
            }
            if (fx * dx + fz * dz < 0.0f) {
                threatBehind = true;
                break;
            }
        }
    }
    if (!threatBehind) {
        for (size_t w = 0; w < m_centipedes.size() && !threatBehind; ++w) {
            LocalCentipede& worm = m_centipedes[w];
            if (!worm.isAlive()) {
                continue;
            }
            for (int s = 0; s < worm.segmentCount(); ++s) {
                const osg::Vec3 p = worm.segment(s).pos;
                const float dx = p.x() - m_dummyActor.x();
                const float dz = p.z() - m_dummyActor.z();
                if (dx * dx + dz * dz > alertR2) {
                    continue;
                }
                if (fx * dx + fz * dz < 0.0f) {
                    threatBehind = true;
                    break;
                }
            }
        }
    }
    if (threatBehind != m_threatBehind) {
        m_threatBehind = threatBehind;
        std::cout << "[fary] " << (threatBehind ? "REARGUARD" : "LEAD_WEAVE") << "\n";
    }

    m_boxWorld.update(deltaTime);
    updateBoulders(deltaTime);
    {
        const AABB dummyAabb = m_dummyActor.makeAabb();
        const float half = CELL_SIZE * 0.5f;
        for (int i = 0; i < m_boxWorld.boxCount(); ++i) {
            if (!m_boxWorld.boxFalling(i)) {
                continue;
            }
            const osg::Vec3 p = m_boxWorld.boxPos(i);
            AABB box;
            box.minX = p.x() - half;
            box.maxX = p.x() + half;
            box.minY = p.y() - half;
            box.maxY = p.y() + half;
            box.minZ = p.z() - half;
            box.maxZ = p.z() + half;
            if (aabbOverlap(box, dummyAabb) && m_boxWorld.tryMarkCrush(i)) {
                applyCrush(p);
            }
        }
    }
    if (m_boxWorld.consumePlaced()) {
        m_buddy.notifyPlace();
    }
    if (m_boxWorld.consumeFell()) {
        m_buddy.notifyDestroy();
    }
    {
        osg::Vec3 facing(fx, 0.0f, fz);
        hintBuddyFetch();
        m_buddy.update(
            deltaTime,
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()),
            facing,
            yaw,
            threatBehind);
        updateDrops(deltaTime);
    }
    updateArchitectEvent(deltaTime);
    {
        const osg::Vec3 dummyNow(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
        m_architect.update(deltaTime, dummyNow, m_miniVoxels, m_localMesher, m_boulderWorld);
        std::vector<osg::Vec3> cyan;
        m_architect.takeDebris(cyan);
        for (size_t i = 0; i < cyan.size(); ++i) {
            spawnDebris(cyan[i], ARCHITECT_COLOR);
        }
        osg::Vec3 built;
        if (m_architect.consumeBuilt(&built)) {
            spawnLootAt(built, ENERGY_CELL);
            std::cout << "[energy] structure drop\n";
        }
    }
    updateBuddyCamera();
    updateSelectTools(deltaTime);
    updateGhostPreview();

    {
        int targetHp = 0;
        int targetMax = 0;
        bool hasTarget = false;
        if (m_lockedEnemyIndex >= 0 &&
            m_lockedEnemyIndex < static_cast<int>(m_enemies.size()) &&
            m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].isAlive) {
            hasTarget = true;
            targetHp = m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].hp();
            targetMax = m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].maxHp();
        } else if (m_lockedBatIndex >= 0 &&
                   m_lockedBatIndex < static_cast<int>(m_bats.size()) &&
                   m_bats[static_cast<size_t>(m_lockedBatIndex)].isAlive()) {
            hasTarget = true;
            targetHp = m_bats[static_cast<size_t>(m_lockedBatIndex)].hp();
            targetMax = m_bats[static_cast<size_t>(m_lockedBatIndex)].maxHp();
        } else if (m_lockedCrawlerIndex >= 0 &&
                   m_lockedCrawlerIndex < static_cast<int>(m_crawlers.size()) &&
                   m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].isAlive()) {
            hasTarget = true;
            targetHp = m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].hp();
            targetMax = m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].maxHp();
        } else if (m_lockedCentipedeIndex >= 0 &&
                   m_lockedCentipedeIndex < static_cast<int>(m_centipedes.size()) &&
                   m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].isAlive() &&
                   m_lockedCentipedeSeg >= 0 &&
                   m_lockedCentipedeSeg < m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].segmentCount()) {
            hasTarget = true;
            targetHp = m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].segment(m_lockedCentipedeSeg).hp;
            targetMax = CENTIPEDE_SEG_HP;
        }
        if (m_centipedeAlertTtl > 0.0f) {
            m_centipedeAlertTtl -= deltaTime;
            if (m_centipedeAlertTtl < 0.0f) {
                m_centipedeAlertTtl = 0.0f;
            }
        }
        if (m_questAlertTtl > 0.0f) {
            m_questAlertTtl -= deltaTime;
            if (m_questAlertTtl < 0.0f) {
                m_questAlertTtl = 0.0f;
            }
        }
        std::string alert;
        if (m_centipedeAlertTtl > 0.0f) {
            alert = "[ !ALERTA: CENTIPEDE BOSS DETECTADO! ]";
        } else if (m_questAlertTtl > 0.0f) {
            alert = m_questAlertText;
        }
        std::string camLabel(m_camRig.modeName());
        if (!m_editorMode) {
            camLabel += (m_toolMode == TOOL_SELECT) ? " | SELECCION" : " | ARMADO";
            if (m_quests.vocationId() != 0) {
                camLabel += " | ";
                camLabel += m_quests.vocationName();
            }
        }
        m_hud.update(
            m_dummyActor.hp(),
            m_dummyActor.maxHp(),
            m_dummyActor.stamina(),
            hasTarget,
            targetHp,
            targetMax,
            m_killCount,
            m_survivalTime,
            m_dummyActor.level(),
            m_dummyActor.exp(),
            m_dummyActor.expToNext(),
            std::string(compassFromYaw(m_dummyActor.yaw())),
            camLabel,
            m_camRig.modeColor(),
            m_hunterCells,
            alert);
    }

    if (!m_editorMode) {
        m_camRig.update(
            deltaTime,
            m_dummyActor,
            m_buddy,
            m_boxWorld,
            m_miniVoxels,
            m_currentCameraEye,
            m_currentCameraCenter,
            m_cameraInitialized);
        if (m_camShakeTtl > 0.0f) {
            m_camShakeTtl -= deltaTime;
            if (m_camShakeTtl < 0.0f) {
                m_camShakeTtl = 0.0f;
            }
            const float k = m_camShakeTtl / 0.18f;
            m_currentCameraEye.x() += std::sin(m_survivalTime * 48.0f) * 0.04f * k;
            m_currentCameraEye.y() += std::cos(m_survivalTime * 41.0f) * 0.028f * k;
        }
        const osg::Matrix view = osg::Matrix::lookAt(
            m_currentCameraEye, m_currentCameraCenter, osg::Vec3(0.0f, 1.0f, 0.0f));
        m_viewer->getCamera()->setViewMatrix(view);
    }
    updateCameraXRay();

    // 5.4 / 5.5 Rayo simulado hacia el suelo y=0, barriendo +X.
    m_phantomProbeX += deltaTime * 0.40f;
    if (m_phantomProbeX > TILE_SIZE * 2.0f) {
        m_phantomProbeX = 0.0f;
        m_phantomCursor.resetDda();
        m_lastPhantomVx = 0x7FFFFFFF;
        m_lastPhantomVy = 0x7FFFFFFF;
        m_lastPhantomVz = 0x7FFFFFFF;
    }

    const SnappedPosition snap = m_phantomCursor.calculateSnappedPosition(
        m_phantomProbeX, 2.0f, 0.5f,
        m_phantomProbeX, 0.0f, 0.5f,
        &m_miniVoxels);

    // 7.4 Cubito cyan exactamente en la celda DDA.
    m_phantomRenderer.setPosition(snap);

    if (snap.vx != m_lastPhantomVx || snap.vy != m_lastPhantomVy || snap.vz != m_lastPhantomVz) {
        int dvx = 0;
        int dvy = 0;
        int dvz = 0;
        if (m_lastPhantomVx != 0x7FFFFFFF) {
            dvx = snap.vx - m_lastPhantomVx;
            dvy = snap.vy - m_lastPhantomVy;
            dvz = snap.vz - m_lastPhantomVz;
        }
        std::cout << "[phantom] hitX=" << m_phantomProbeX
                  << " snap=(" << snap.x << ", " << snap.y << ", " << snap.z << ")"
                  << " idx=(" << snap.vx << ", " << snap.vy << ", " << snap.vz << ")"
                  << " dv=(" << dvx << ", " << dvy << ", " << dvz << ")"
                  << "\n";
        if (std::abs(dvx) > 1 || std::abs(dvy) > 1 || std::abs(dvz) > 1) {
            std::cout << "[phantom] WARN skip detected\n";
        }
        m_lastPhantomVx = snap.vx;
        m_lastPhantomVy = snap.vy;
        m_lastPhantomVz = snap.vz;
    }

}

void StandaloneEngine::updatePlayCamera(float deltaTime)
{
    // 18.2 Ideal: atras del Dummy segun yaw (frente = sin/cos) + un poco arriba.
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const float headY = m_dummyActor.y() + m_dummyActor.height() * 0.85f;
    const osg::Vec3 idealCenter(
        m_dummyActor.x(),
        headY,
        m_dummyActor.z());
    const float dist = 2.6f;
    const float height = 1.15f;
    const osg::Vec3 idealEye(
        idealCenter.x() - fx * dist,
        idealCenter.y() + height,
        idealCenter.z() - fz * dist);

    if (!m_cameraInitialized) {
        m_currentCameraEye = idealEye;
        m_currentCameraCenter = idealCenter;
        m_cameraInitialized = true;
    } else {
        // 18.3 / 18.4 Lerp elastico: current += (ideal - current) * (dt * speed)
        float tEye = deltaTime * 6.0f;
        float tLook = deltaTime * 8.0f;
        if (tEye > 1.0f) {
            tEye = 1.0f;
        }
        if (tLook > 1.0f) {
            tLook = 1.0f;
        }
        m_currentCameraEye = m_currentCameraEye + (idealEye - m_currentCameraEye) * tEye;
        m_currentCameraCenter = m_currentCameraCenter + (idealCenter - m_currentCameraCenter) * tLook;
    }

    const osg::Matrix view = osg::Matrix::lookAt(
        m_currentCameraEye, m_currentCameraCenter, osg::Vec3(0.0f, 1.0f, 0.0f));
    m_viewer->getCamera()->setViewMatrix(view);
}

void StandaloneEngine::collectOcclusionVoxels(const osg::Vec3& cameraPos, const osg::Vec3& dummyHead,
                                              std::vector<VoxelKey>& out)
{
    // 56.2 DDA mini-voxel camara -> cabeza. Cilindro ~1 celda para un hueco jugable.
    out.clear();
    float dx = dummyHead.x() - cameraPos.x();
    float dy = dummyHead.y() - cameraPos.y();
    float dz = dummyHead.z() - cameraPos.z();
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1.0e-4f) {
        return;
    }
    dx /= len;
    dy /= len;
    dz /= len;

    const AABB dummyBox = m_dummyActor.makeAabb();
    const int skipMinX = worldToVoxelIndex(dummyBox.minX);
    const int skipMaxX = worldToVoxelIndex(dummyBox.maxX - 0.0001f);
    const int skipMinY = worldToVoxelIndex(dummyBox.minY);
    const int skipMaxY = worldToVoxelIndex(dummyBox.maxY - 0.0001f);
    const int skipMinZ = worldToVoxelIndex(dummyBox.minZ);
    const int skipMaxZ = worldToVoxelIndex(dummyBox.maxZ - 0.0001f);

    int vx = worldToVoxelIndex(cameraPos.x());
    int vy = worldToVoxelIndex(cameraPos.y());
    int vz = worldToVoxelIndex(cameraPos.z());
    const int stepX = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
    const int stepY = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);
    const int stepZ = (dz > 0.0f) ? 1 : ((dz < 0.0f) ? -1 : 0);

    const float kInf = 1.0e30f;
    const float tDeltaX = (stepX == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dx));
    const float tDeltaY = (stepY == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dy));
    const float tDeltaZ = (stepZ == 0) ? kInf : (MINI_VOXEL_SIZE / std::fabs(dz));

    float tMaxX = kInf;
    float tMaxY = kInf;
    float tMaxZ = kInf;
    if (stepX > 0) {
        tMaxX = ((static_cast<float>(vx + 1) * MINI_VOXEL_SIZE) - cameraPos.x()) / dx;
    } else if (stepX < 0) {
        tMaxX = ((static_cast<float>(vx) * MINI_VOXEL_SIZE) - cameraPos.x()) / dx;
    }
    if (stepY > 0) {
        tMaxY = ((static_cast<float>(vy + 1) * MINI_VOXEL_SIZE) - cameraPos.y()) / dy;
    } else if (stepY < 0) {
        tMaxY = ((static_cast<float>(vy) * MINI_VOXEL_SIZE) - cameraPos.y()) / dy;
    }
    if (stepZ > 0) {
        tMaxZ = ((static_cast<float>(vz + 1) * MINI_VOXEL_SIZE) - cameraPos.z()) / dz;
    } else if (stepZ < 0) {
        tMaxZ = ((static_cast<float>(vz) * MINI_VOXEL_SIZE) - cameraPos.z()) / dz;
    }

    std::unordered_set<VoxelKey, VoxelKeyHash> seen;
    const float radius = MINI_VOXEL_SIZE * 0.85f;
    const float maxT = len + MINI_VOXEL_SIZE * 0.01f;

    for (int guard = 0; guard < 512; ++guard) {
        for (int ox = -1; ox <= 1; ++ox) {
            for (int oy = -1; oy <= 1; ++oy) {
                for (int oz = -1; oz <= 1; ++oz) {
                    const int nx = vx + ox;
                    const int ny = vy + oy;
                    const int nz = vz + oz;
                    if (ny < 0) {
                        continue;
                    }
                    if (nx >= skipMinX && nx <= skipMaxX &&
                        ny >= skipMinY && ny <= skipMaxY &&
                        nz >= skipMinZ && nz <= skipMaxZ) {
                        continue;
                    }
                    if (!m_miniVoxels.getVoxel(nx, ny, nz).isActive) {
                        continue;
                    }
                    VoxelKey key;
                    key.vx = nx;
                    key.vy = ny;
                    key.vz = nz;
                    if (seen.find(key) != seen.end()) {
                        continue;
                    }
                    const osg::Vec3 center(
                        (static_cast<float>(nx) + 0.5f) * MINI_VOXEL_SIZE,
                        (static_cast<float>(ny) + 0.5f) * MINI_VOXEL_SIZE,
                        (static_cast<float>(nz) + 0.5f) * MINI_VOXEL_SIZE);
                    if (pointToSegmentDist(center, cameraPos, dummyHead) > radius) {
                        continue;
                    }
                    seen.insert(key);
                    out.push_back(key);
                }
            }
        }

        const float tNext = (tMaxX <= tMaxY)
            ? ((tMaxX <= tMaxZ) ? tMaxX : tMaxZ)
            : ((tMaxY <= tMaxZ) ? tMaxY : tMaxZ);
        if (tNext > maxT) {
            break;
        }
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            vx += stepX;
            tMaxX += tDeltaX;
        } else if (tMaxY <= tMaxZ) {
            vy += stepY;
            tMaxY += tDeltaY;
        } else {
            vz += stepZ;
            tMaxZ += tDeltaZ;
        }
    }
}

void StandaloneEngine::updateCameraXRay()
{
    // 56.1 / 57 / 58. Rayo camara -> cabeza. Translucidez solo mientras ocluye.
    if (!m_viewer.valid() || m_viewer->getCamera() == nullptr) {
        return;
    }

    osg::Vec3 cameraPos = m_currentCameraEye;
    if (m_editorMode) {
        osg::Vec3d eye;
        osg::Vec3d center;
        osg::Vec3d up;
        m_viewer->getCamera()->getViewMatrixAsLookAt(eye, center, up);
        cameraPos.set(static_cast<float>(eye.x()),
                      static_cast<float>(eye.y()),
                      static_cast<float>(eye.z()));
    }

    const osg::Vec3 dummyHead(
        m_dummyActor.x(),
        m_dummyActor.y() + m_dummyActor.height() * 0.85f,
        m_dummyActor.z());

    std::vector<VoxelKey> now;
    collectOcclusionVoxels(cameraPos, dummyHead, now);

    std::unordered_set<VoxelKey, VoxelKeyHash> nowSet;
    for (size_t i = 0; i < now.size(); ++i) {
        nowSet.insert(now[i]);
    }

    for (size_t i = 0; i < m_xrayVoxels.size(); ++i) {
        if (nowSet.find(m_xrayVoxels[i]) == nowSet.end()) {
            m_localMesher.setXRay(m_xrayVoxels[i].vx, m_xrayVoxels[i].vy, m_xrayVoxels[i].vz, false);
        }
    }
    for (size_t i = 0; i < now.size(); ++i) {
        m_localMesher.setXRay(now[i].vx, now[i].vy, now[i].vz, true);
    }
    m_xrayVoxels.swap(now);
}

void StandaloneEngine::cycleEnemyTarget()
{
    cycleEntityLockOn();
}

void StandaloneEngine::cycleTarget()
{
    cycleEntityLockOn();
}

void StandaloneEngine::cycleEntityLockOn()
{
    // 108.1 TAB: solo amenazas vivas (Dummy, Arquero, Murcielago, Boss). Cero rocas.
    const float lockR = 12.0f * TILE_SIZE;
    const float lockR2 = lockR * lockR;
    const osg::Vec3 player(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    const float fx = std::sin(m_dummyActor.yaw());
    const float fz = std::cos(m_dummyActor.yaw());
    struct Item {
        int kind;
        int id;
        int sub;
        float score;
    };
    std::vector<Item> items;
    const int nEn = static_cast<int>(m_enemies.size());
    for (int i = 0; i < nEn; ++i) {
        const LocalEnemy& en = m_enemies[static_cast<size_t>(i)];
        if (!en.isAlive || en.hp() <= 0) {
            continue;
        }
        const float dx = en.pos.x() - player.x();
        const float dy = (en.pos.y() + en.height() * 0.5f) - (player.y() + m_dummyActor.height() * 0.5f);
        const float dz = en.pos.z() - player.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > lockR2) {
            continue;
        }
        const float dist = std::sqrt(d2 + dy * dy);
        float facing = 0.0f;
        if (d2 > 0.0001f) {
            const float inv = 1.0f / std::sqrt(d2);
            facing = fx * dx * inv + fz * dz * inv;
        }
        float kindW = 1.00f;
        if (en.isArcher()) {
            kindW = 1.35f;
        } else if (en.isBoss()) {
            kindW = 1.15f;
        }
        Item it;
        it.kind = 0;
        it.id = i;
        it.sub = 0;
        it.score = kindW * (0.25f + 0.75f * (facing + 1.0f) * 0.5f) / (0.40f + dist);
        items.push_back(it);
    }
    // 102.5 El Arquitecto es objetivo prioritario del TAB mientras dure el evento.
    if (m_architect.isAlive()) {
        const osg::Vec3 ap = m_architect.position();
        const float dx = ap.x() - player.x();
        const float dy = ap.y() - (player.y() + m_dummyActor.height() * 0.5f);
        const float dz = ap.z() - player.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 <= lockR2) {
            const float dist = std::sqrt(d2 + dy * dy);
            float facing = 0.0f;
            if (d2 > 0.0001f) {
                const float inv = 1.0f / std::sqrt(d2);
                facing = fx * dx * inv + fz * dz * inv;
            }
            Item it;
            it.kind = 4;
            it.id = 0;
            it.sub = 0;
            it.score = 1.60f * (0.25f + 0.75f * (facing + 1.0f) * 0.5f) / (0.40f + dist);
            items.push_back(it);
        }
    }
    const int nBat = static_cast<int>(m_bats.size());
    for (int i = 0; i < nBat; ++i) {
        const LocalFlyingBat& bat = m_bats[static_cast<size_t>(i)];
        if (!bat.isAlive() || bat.hp() <= 0) {
            continue;
        }
        const float dx = bat.pos.x() - player.x();
        const float dy = bat.pos.y() - (player.y() + m_dummyActor.height() * 0.5f);
        const float dz = bat.pos.z() - player.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > lockR2) {
            continue;
        }
        const float dist = std::sqrt(d2 + dy * dy);
        float facing = 0.0f;
        if (d2 > 0.0001f) {
            const float inv = 1.0f / std::sqrt(d2);
            facing = fx * dx * inv + fz * dz * inv;
        }
        float kindW = 1.45f;
        if (bat.isDiveStrike()) {
            kindW = 1.75f;
        }
        Item it;
        it.kind = 1;
        it.id = i;
        it.sub = 0;
        it.score = kindW * (0.25f + 0.75f * (facing + 1.0f) * 0.5f) / (0.40f + dist);
        items.push_back(it);
    }
    const int nCrawl = static_cast<int>(m_crawlers.size());
    for (int i = 0; i < nCrawl; ++i) {
        const LocalCrawler& cr = m_crawlers[static_cast<size_t>(i)];
        if (!cr.isAlive() || cr.hp() <= 0) {
            continue;
        }
        const float dx = cr.pos.x() - player.x();
        const float dy = (cr.pos.y() + cr.height() * 0.5f) - (player.y() + m_dummyActor.height() * 0.5f);
        const float dz = cr.pos.z() - player.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > lockR2) {
            continue;
        }
        const float dist = std::sqrt(d2 + dy * dy);
        float facing = 0.0f;
        if (d2 > 0.0001f) {
            const float inv = 1.0f / std::sqrt(d2);
            facing = fx * dx * inv + fz * dz * inv;
        }
        Item it;
        it.kind = 2;
        it.id = i;
        it.sub = 0;
        it.score = 1.10f * (0.25f + 0.75f * (facing + 1.0f) * 0.5f) / (0.40f + dist);
        items.push_back(it);
    }
    const int nCent = static_cast<int>(m_centipedes.size());
    for (int w = 0; w < nCent; ++w) {
        const LocalCentipede& worm = m_centipedes[static_cast<size_t>(w)];
        if (!worm.isAlive()) {
            continue;
        }
        const int nSeg = worm.segmentCount();
        for (int s = 0; s < nSeg; ++s) {
            const osg::Vec3 sp = worm.segment(s).pos;
            const float dx = sp.x() - player.x();
            const float dy = sp.y() - (player.y() + m_dummyActor.height() * 0.5f);
            const float dz = sp.z() - player.z();
            const float d2 = dx * dx + dz * dz;
            if (d2 > lockR2) {
                continue;
            }
            const float dist = std::sqrt(d2 + dy * dy);
            float facing = 0.0f;
            if (d2 > 0.0001f) {
                const float inv = 1.0f / std::sqrt(d2);
                facing = fx * dx * inv + fz * dz * inv;
            }
            const float tailW = (nSeg <= 1)
                ? 1.0f
                : (0.35f + 0.90f * (static_cast<float>(s) / static_cast<float>(nSeg - 1)));
            Item it;
            it.kind = 3;
            it.id = w;
            it.sub = s;
            it.score = 1.70f * tailW * (0.20f + 0.80f * (facing + 1.0f) * 0.5f) / (0.35f + dist);
            items.push_back(it);
        }
    }
    for (size_t a = 0; a < items.size(); ++a) {
        size_t best = a;
        for (size_t b = a + 1; b < items.size(); ++b) {
            if (items[b].score > items[best].score) {
                best = b;
            }
        }
        const Item tmp = items[a];
        items[a] = items[best];
        items[best] = tmp;
    }

    if (m_lockedEnemyIndex >= 0 && m_lockedEnemyIndex < nEn) {
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].setTargeted(false);
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].syncVisual();
    }
    if (m_lockedBatIndex >= 0 && m_lockedBatIndex < nBat) {
        m_bats[static_cast<size_t>(m_lockedBatIndex)].setTargeted(false);
        m_bats[static_cast<size_t>(m_lockedBatIndex)].syncVisual();
    }
    if (m_lockedCrawlerIndex >= 0 && m_lockedCrawlerIndex < nCrawl) {
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].setTargeted(false);
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].syncVisual();
    }
    if (m_lockedCentipedeIndex >= 0 && m_lockedCentipedeIndex < nCent) {
        m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].setLockedSegment(-1);
    }

    int curKind = -1;
    int curId = -1;
    int curSub = 0;
    if (m_lockedCentipedeIndex >= 0) {
        curKind = 3;
        curId = m_lockedCentipedeIndex;
        curSub = m_lockedCentipedeSeg;
    } else if (m_lockedCrawlerIndex >= 0) {
        curKind = 2;
        curId = m_lockedCrawlerIndex;
    } else if (m_lockedBatIndex >= 0) {
        curKind = 1;
        curId = m_lockedBatIndex;
    } else if (m_lockedEnemyIndex >= 0) {
        curKind = 0;
        curId = m_lockedEnemyIndex;
    }

    int found = -1;
    for (int k = 0; k < static_cast<int>(items.size()); ++k) {
        if (items[static_cast<size_t>(k)].kind == curKind &&
            items[static_cast<size_t>(k)].id == curId &&
            items[static_cast<size_t>(k)].sub == curSub) {
            found = k;
            break;
        }
    }
    const int next = found + 1;
    m_lockedEnemyIndex = -1;
    m_lockedBatIndex = -1;
    m_lockedCrawlerIndex = -1;
    m_lockedCentipedeIndex = -1;
    m_lockedCentipedeSeg = -1;
    m_lockedArchitect = false;
    if (items.empty() || next >= static_cast<int>(items.size())) {
        std::cout << "[lock] none\n";
        return;
    }

    const Item& pick = items[static_cast<size_t>(next)];
    if (pick.kind == 0) {
        m_lockedEnemyIndex = pick.id;
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].setTargeted(true);
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].syncVisual();
        std::cout << "[lock] enemy " << m_lockedEnemyIndex << "\n";
        fireContentHook("onLockOn", "enemy");
        return;
    }
    if (pick.kind == 1) {
        m_lockedBatIndex = pick.id;
        m_bats[static_cast<size_t>(m_lockedBatIndex)].setTargeted(true);
        m_bats[static_cast<size_t>(m_lockedBatIndex)].syncVisual();
        std::cout << "[lock] bat " << m_lockedBatIndex << "\n";
        fireContentHook("onLockOn", "bat");
        return;
    }
    if (pick.kind == 2) {
        m_lockedCrawlerIndex = pick.id;
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].setTargeted(true);
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].syncVisual();
        std::cout << "[lock] crawler " << m_lockedCrawlerIndex << "\n";
        fireContentHook("onLockOn", "crawler");
        return;
    }
    if (pick.kind == 4) {
        m_lockedArchitect = true;
        std::cout << "[lock] architect\n";
        fireContentHook("onLockOn", "architect");
        return;
    }
    m_lockedCentipedeIndex = pick.id;
    m_lockedCentipedeSeg = pick.sub;
    m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].setLockedSegment(m_lockedCentipedeSeg);
    std::cout << "[lock] centipede " << m_lockedCentipedeIndex
              << " seg " << m_lockedCentipedeSeg << "\n";
    fireContentHook("onLockOn", "centipede");
}

void StandaloneEngine::cycleBoxTarget()
{
    // 109.1 SHIFT+TAB: solo cubos de hielo y rocas 2x2, marco amarillo.
    if (m_editorMode) {
        return;
    }
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    struct Item {
        int kind;
        int id;
        float d2;
    };
    std::vector<Item> items;
    std::vector<int> boxes;
    m_boxWorld.collectSorted(playerPos, boxes);
    for (size_t i = 0; i < boxes.size(); ++i) {
        const osg::Vec3 p = m_boxWorld.boxPos(boxes[i]);
        const float dx = p.x() - playerPos.x();
        const float dy = p.y() - playerPos.y();
        const float dz = p.z() - playerPos.z();
        Item it;
        it.kind = 0;
        it.id = boxes[i];
        it.d2 = dx * dx + dy * dy + dz * dz;
        items.push_back(it);
    }
    std::vector<int> rocks;
    m_boulderWorld.collectSorted(playerPos, rocks);
    for (size_t i = 0; i < rocks.size(); ++i) {
        const osg::Vec3 p = m_boulderWorld.boulderPos(rocks[i]);
        const float dx = p.x() - playerPos.x();
        const float dy = p.y() - playerPos.y();
        const float dz = p.z() - playerPos.z();
        Item it;
        it.kind = 1;
        it.id = rocks[i];
        it.d2 = dx * dx + dy * dy + dz * dz;
        items.push_back(it);
    }
    for (size_t a = 0; a < items.size(); ++a) {
        size_t best = a;
        for (size_t b = a + 1; b < items.size(); ++b) {
            if (items[b].d2 < items[best].d2) {
                best = b;
            }
        }
        const Item tmp = items[a];
        items[a] = items[best];
        items[best] = tmp;
    }
    if (items.empty()) {
        clearSelection();
        std::cout << "[lock] box none\n";
        return;
    }

    int curKind = -1;
    int curId = -1;
    if (m_toolMode == TOOL_SELECT) {
        if (m_boulderWorld.boulderAlive(m_selectedBoulder)) {
            curKind = 1;
            curId = m_selectedBoulder;
        } else if (m_boxWorld.boxAlive(m_selectedBox)) {
            curKind = 0;
            curId = m_selectedBox;
        }
    }
    int found = -1;
    for (int k = 0; k < static_cast<int>(items.size()); ++k) {
        if (items[static_cast<size_t>(k)].kind == curKind &&
            items[static_cast<size_t>(k)].id == curId) {
            found = k;
            break;
        }
    }
    const int next = found + 1;
    if (next >= static_cast<int>(items.size())) {
        clearSelection();
        std::cout << "[lock] box clear\n";
        return;
    }
    const Item& pick = items[static_cast<size_t>(next)];
    m_toolMode = TOOL_SELECT;
    m_selectCursor = next;
    if (pick.kind == 0) {
        m_selectedBoulder = -1;
        m_selectedBox = pick.id;
        std::cout << "[lock] box " << m_selectedBox << "\n";
        return;
    }
    m_selectedBox = -1;
    m_selectedBoulder = pick.id;
    std::cout << "[lock] boulder " << m_selectedBoulder << "\n";
}

void StandaloneEngine::removeEnemyNode(LocalEnemy& enemy)
{
    if (m_enemyRoot.valid() && enemy.getNode() != nullptr) {
        m_enemyRoot->removeChild(enemy.getNode());
    }
}

void StandaloneEngine::onEnemyKilled(LocalEnemy& enemy, int index)
{
    // 42.2 Una sola vez por muerte (melee o proyectil pasan por aqui).
    m_killCount += 1;
    if (m_killCount > 0 && (m_killCount % 10) == 0) {
        m_pendingBoss = true;
    }
    noteKill();
    fireContentHook("onKill", enemy.isBoss() ? "boss" : (enemy.isArcher() ? "archer" : "grunt"));

    if (enemy.isBoss()) {
        // 55. 2x LARGE_HP + EXP 50 + 30 min stamina.
        spawnLootAt(osg::Vec3(enemy.pos.x() + 0.45f, enemy.pos.y(), enemy.pos.z()), LARGE_HP);
        spawnLootAt(osg::Vec3(enemy.pos.x() - 0.45f, enemy.pos.y(), enemy.pos.z()), LARGE_HP);
        spawnExpAt(enemy.pos, 50);
        m_dummyActor.addStamina(1800.0f);
        std::cout << "[boss] down. +30min SP=" << m_dummyActor.stamina() << "\n";
    } else {
        spawnExpAt(enemy.pos, 0);
        const int roll = std::rand() % 100;
        if (roll < 60) {
            spawnLootAt(enemy.pos, SMALL_HP);
        } else if (roll < 80) {
            spawnLootAt(enemy.pos, LARGE_HP);
        }
        std::cout << "[kills] " << m_killCount << " roll=" << roll << "\n";
    }

    removeEnemyNode(enemy);
    if (index == m_lockedEnemyIndex) {
        m_lockedEnemyIndex = -1;
        std::cout << "[lock] clear\n";
    }
    if (enemy.isBoss()) {
        std::cout << "[kills] " << m_killCount << "\n";
    }
}

void StandaloneEngine::resolveBossStomp(LocalEnemy& enemy)
{
    // 54.4 Radio 3 tiles. Dash (iFrames) esquiva.
    const float dx = m_dummyActor.x() - enemy.pos.x();
    const float dz = m_dummyActor.z() - enemy.pos.z();
    const float dist = std::sqrt(dx * dx + dz * dz);
    const float radius = 3.0f * TILE_SIZE;
    if (dist > radius) {
        std::cout << "[boss] stomp miss\n";
        return;
    }
    if (m_dummyActor.iFrames() > 0.0f) {
        std::cout << "[boss] stomp dodged\n";
        return;
    }

    float nx = 0.0f;
    float nz = 1.0f;
    if (dist > 0.0001f) {
        const float inv = 1.0f / dist;
        nx = dx * inv;
        nz = dz * inv;
    }
    m_dummyActor.takeDamage(25);
    m_dummyActor.applyKnockback(nx * 18.0f, nz * 18.0f);
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "-%d", 25);
        spawnFloatingText(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
            buf, kFloatDmgPlayer);
    }
    std::cout << "[boss] stomp hit! HP: " << m_dummyActor.hp()
              << "/" << m_dummyActor.maxHp() << "\n";
    if (m_dummyActor.hp() <= 0) {
        std::cout << "[combat] Player DIED! Respawning...\n";
        m_survivalTime = 0.0f;
        m_dummyActor.restoreHp();
        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
        m_dummyActor.syncVisual();
    }
}

void StandaloneEngine::handlePlayerHit(int amount)
{
    if (amount <= 0) {
        return;
    }
    const int hpBefore = m_dummyActor.hp();
    m_dummyActor.takeDamage(amount);
    if (m_dummyActor.hp() >= hpBefore) {
        return;
    }
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "-%d", amount);
        spawnFloatingText(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
            buf, kFloatDmgPlayer);
    }
    std::cout << "[combat] Player hit! HP: " << m_dummyActor.hp()
              << "/" << m_dummyActor.maxHp() << "\n";
    if (m_dummyActor.hp() <= 0) {
        std::cout << "[combat] Player DIED! Respawning...\n";
        m_survivalTime = 0.0f;
        m_dummyActor.restoreHp();
        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
        m_dummyActor.syncVisual();
    }
}

osg::Vec3 StandaloneEngine::nearestCover(const osg::Vec3& from) const
{
    const int bases[4][2] = {
        { 4, 4 },
        { 16, 4 },
        { 4, 16 },
        { 16, 16 }
    };
    osg::Vec3 best(
        (static_cast<float>(bases[0][0]) + 2.0f) * MINI_VOXEL_SIZE,
        0.0f,
        (static_cast<float>(bases[0][1]) + 2.0f) * MINI_VOXEL_SIZE);
    float bestD = 1.0e30f;
    for (int i = 0; i < 4; ++i) {
        const osg::Vec3 c(
            (static_cast<float>(bases[i][0]) + 2.0f) * MINI_VOXEL_SIZE,
            0.0f,
            (static_cast<float>(bases[i][1]) + 2.0f) * MINI_VOXEL_SIZE);
        const float dx = c.x() - from.x();
        const float dz = c.z() - from.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 < bestD) {
            bestD = d2;
            best = c;
        }
    }
    return best;
}

void StandaloneEngine::spawnEnemyMissile(const osg::Vec3& origin, const osg::Vec3& target)
{
    osg::Vec3 dir = target - origin;
    const float len = dir.length();
    if (len < 0.0001f) {
        return;
    }
    dir = dir * (18.0f / len);
    const osg::Vec3 pos = origin + dir * (0.08f / 18.0f);
    LocalProjectile shot(pos, dir, 2.20f, true);
    if (m_projectileRoot.valid() && shot.getNode() != nullptr) {
        m_projectileRoot->addChild(shot.getNode());
    }
    m_projectiles.push_back(shot);
}

void StandaloneEngine::resetArchitectTimer()
{
    // 180-300 s. RNG propio: el mismo LCG del resto del motor, no rand().
    m_architectSeed = m_architectSeed * 1103515245u + 12345u;
    m_architectTimer = 180.0f + static_cast<float>(m_architectSeed % 121u);
    if (const char* fast = std::getenv("RC_ARCHITECT_FAST")) {
        // Solo para pruebas: acorta la espera sin recompilar.
        m_architectTimer = static_cast<float>(std::atof(fast));
    }
}

void StandaloneEngine::updateArchitectEvent(float deltaTime)
{
    if (m_architect.isAlive()) {
        return;  // 102.1 Concurrencia maxima: un Arquitecto a la vez.
    }
    m_architectTimer -= deltaTime;
    if (m_architectTimer <= 0.0f) {
        triggerArchitectSpawn();
    }
}

void StandaloneEngine::triggerArchitectSpawn()
{
    // 102.2 Cuadrantes lejanos, en coordenadas de MUNDO. El mapa de 120x120
    // mini-voxels mide 40x40 unidades, asi que (25,95) voxels son (8.3, 31.7).
    struct Sector {
        const char* name;
        float x;
        float z;
    };
    const Sector sectors[4] = {
        { "NOROESTE", 25.0f * MINI_VOXEL_SIZE, 95.0f * MINI_VOXEL_SIZE },
        { "NORESTE",  95.0f * MINI_VOXEL_SIZE, 95.0f * MINI_VOXEL_SIZE },
        { "SUROESTE", 25.0f * MINI_VOXEL_SIZE, 25.0f * MINI_VOXEL_SIZE },
        { "SURESTE",  95.0f * MINI_VOXEL_SIZE, 25.0f * MINI_VOXEL_SIZE }
    };

    m_architectSeed = m_architectSeed * 1103515245u + 12345u;
    const int chosen = static_cast<int>((m_architectSeed >> 16) % 4u);
    const Sector& sector = sectors[chosen];

    const osg::Vec3 spawnPos(sector.x, ARCHITECT_SPAWN_ALT, sector.z);
    m_architect.spawnAt(spawnPos);
    // Ritmo pausado: cada capa dispara un rebuild del mesher.
    m_architect.setBuildInterval(3.5f);

    m_questAlertText = std::string("[ ALERTA ] ARQUITECTO AZUL EN SECTOR ") + sector.name;
    m_questAlertTtl = 5.0f;
    spawnFloatingText(
        osg::Vec3(spawnPos.x(), spawnPos.y() + 1.4f, spawnPos.z()),
        "ARQUITECTO", ARCHITECT_COLOR, 0.18f);

    std::cout << "[architect] spawn sector=" << sector.name
              << " (" << spawnPos.x() << ", " << spawnPos.y() << ", " << spawnPos.z()
              << ") hp=" << m_architect.maxHp() << "\n";
    fireContentHook("onArchitect", sector.name);
}

bool StandaloneEngine::damageArchitect(int amount)
{
    if (!m_architect.isAlive()) {
        return false;
    }
    const osg::Vec3 pos = m_architect.position();
    const bool killed = m_architect.takeDamage(amount);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "-%d", amount);
    spawnFloatingText(pos, buf, kFloatDmgEnemy);

    if (killed) {
        onArchitectEliminated(pos);
    }
    return killed;
}

void StandaloneEngine::onArchitectEliminated(const osg::Vec3& dropPos)
{
    // 102.3 Recompensa: celdas de energia + EXP alta, y el reloj vuelve a correr.
    for (int i = 0; i < 3; ++i) {
        const float off = (static_cast<float>(i) - 1.0f) * 0.45f;
        spawnLootAt(osg::Vec3(dropPos.x() + off, dropPos.y(), dropPos.z()), ENERGY_CELL);
    }
    spawnExpAt(dropPos, 250);
    spawnDebris(dropPos, ARCHITECT_COLOR);

    m_questAlertText = "[ ARQUITECTO ELIMINADO ] SECTOR ASEGURADO";
    m_questAlertTtl = 3.5f;
    spawnFloatingText(dropPos, "SECTOR ASEGURADO", ARCHITECT_COLOR, 0.18f);

    m_lockedArchitect = false;
    resetArchitectTimer();
    std::cout << "[architect] eliminado, proximo en " << m_architectTimer << " s\n";
    fireContentHook("onArchitect", "eliminated");
}

void StandaloneEngine::spawnDefaultObstacles()
{
    // 11. El JSON manda. Sin JSON, los pilares hardcodeados del prototipo.
    const bool fromJson = loadWorldJson(m_worldJsonPath);
    if (!fromJson) {
        spawnDefaultPillars();
    }
    // 98.2 Las rocas vacian su celda, asi que van despues del mapa.
    spawnDefaultBoulders();
    m_localMesher.rebuildMesh();
}

bool StandaloneEngine::loadWorldJson(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    if (!loadWorld(&m_miniVoxels, path)) {
        std::cerr << "[world] WARN sin mapa JSON, usando arena por defecto\n";
        return false;
    }
    std::cout << "[world] mapa <- " << path << "\n";
    return true;
}

bool StandaloneEngine::reloadWorldJson()
{
    if (!loadWorldJson(m_worldJsonPath)) {
        return false;
    }
    // 11.4 Las rocas ya existen: solo revaciar su hueco, no volver a spawnearlas.
    m_boulderWorld.carveFootprints(m_miniVoxels);
    m_localMesher.rebuildMesh();
    m_phantomCursor.resetDda();
    return true;
}

void StandaloneEngine::spawnDefaultPillars()
{
    // 67. Cuatro pilares 4x4 x 6 de alto. Cuadrantes alrededor del centro (4, 4).
    // Offset ~2 tiles (intermedio). +/- 4 tiles caeria en el borde del suelo 8x8.
    const int bases[4][2] = {
        { 4, 4 },
        { 16, 4 },
        { 4, 16 },
        { 16, 16 }
    };
    const int wide = 4;
    const int tall = 6;
    for (int p = 0; p < 4; ++p) {
        const int ox = bases[p][0];
        const int oz = bases[p][1];
        for (int dx = 0; dx < wide; ++dx) {
            for (int dz = 0; dz < wide; ++dz) {
                for (int dy = 0; dy < tall; ++dy) {
                    m_miniVoxels.setVoxel(ox + dx, dy, oz + dz, 1);
                }
            }
        }
    }
    std::cout << "[arena] 4 cover pillars 4x4x6 (default C++)\n";
}

void StandaloneEngine::spawnDefaultBoulders()
{
    // 98.2 Tres rocas cafe a media altura, esquina interior (suelo de mini-voxeles debajo).
    m_boulderWorld.spawnAt(m_miniVoxels, 6, 2, 6);
    m_boulderWorld.spawnAt(m_miniVoxels, 16, 2, 6);
    m_boulderWorld.spawnAt(m_miniVoxels, 6, 2, 16);
    std::cout << "[arena] 3 HeavyBoulder2x2\n";
}

std::string StandaloneEngine::worldMetaSidecar(const std::string& worldRelative)
{
    if (worldRelative.empty()) {
        return std::string();
    }
    std::string base = worldRelative;
    const size_t dot = base.rfind('.');
    const size_t slash = base.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        base = base.substr(0, dot);
    }
    const std::string sidecar = base + ".meta.json";
    // Solo cuenta si existe de verdad; si no, se cae al meta del pack.
    const std::string resolved = LocalContentRegistry::resolve(sidecar);
    if (!std::filesystem::exists(resolved)) {
        return std::string();
    }
    return sidecar;
}

void StandaloneEngine::loadContent(const std::string& metaRelative)
{
    if (!m_content.loadAll(metaRelative)) {
        std::cerr << "[content] WARN defaults C++\n";
    }
    if (!m_scripts.init() || !m_scripts.loadManifest("data/scripts/scripts.json")) {
        std::cerr << "[script] WARN Luau off\n";
    }
    m_inventory.load(playerLoadPath("data/player/sandbox_inventory.json"));
    m_quests.load();
    m_dummyActor.setVocation(m_quests.vocationId());
}

void StandaloneEngine::fireContentHook(const std::string& name, const std::string& kind)
{
    m_scripts.fireHook(name, kind);
}

void StandaloneEngine::applyEnemyStats(LocalEnemy& enemy)
{
    const char* id = "otr.actor.sandbox.grunt";
    if (enemy.isBoss()) {
        id = "otr.actor.sandbox.boss";
    } else if (enemy.isArcher()) {
        id = "otr.actor.sandbox.archer";
    }
    const ActorStats stats = m_content.actor(id);
    if (stats.valid) {
        enemy.applyStats(stats.maxHp, stats.speed);
    }
}

void StandaloneEngine::spawnSandboxActors()
{
    if (m_content.hasSpawns()) {
        const std::vector<SpawnPoint>& list = m_content.spawns();
        for (size_t i = 0; i < list.size(); ++i) {
            const SpawnPoint& sp = list[i];
            const osg::Vec3 pos(sp.x, sp.y, sp.z);
            if (sp.kind == "archer") {
                spawnEnemyAt(pos, ENEMY_ARCHER);
            } else if (sp.kind == "boss") {
                spawnEnemyAt(pos, ENEMY_BOSS);
            } else if (sp.kind == "bat") {
                spawnBatAt(pos);
            } else if (sp.kind == "crawler") {
                spawnCrawlerAt(pos);
            } else {
                spawnEnemyAt(pos, ENEMY_GRUNT);
            }
        }
        return;
    }
    const osg::Vec3 dummySpawns[3] = {
        osg::Vec3(4.0f, 0.0f, 1.20f),
        osg::Vec3(6.40f, 0.0f, 5.60f),
        osg::Vec3(1.70f, 0.0f, 6.10f)
    };
    for (int s = 0; s < 3; ++s) {
        spawnEnemyAt(dummySpawns[s], ENEMY_GRUNT);
    }
    spawnEnemyAt(osg::Vec3(4.0f, 0.0f, -8.0f), ENEMY_ARCHER);
    spawnEnemyAt(osg::Vec3(16.0f, 0.0f, 4.0f), ENEMY_ARCHER);
    spawnBatAt(osg::Vec3(2.20f, BAT_CRUISE_ALT, 6.40f));
    spawnBatAt(osg::Vec3(6.50f, BAT_CRUISE_ALT, 1.60f));
    spawnCrawlerAt(osg::Vec3(7.20f, 0.0f, 3.10f));
    spawnCrawlerAt(osg::Vec3(1.30f, 0.0f, 7.10f));
}

void StandaloneEngine::spawnSandboxNpcs()
{
    const std::vector<NpcSpawn>& list = m_content.npcs();
    for (size_t i = 0; i < list.size(); ++i) {
        const NpcSpawn& def = list[i];
        if (!def.valid) {
            continue;
        }
        osg::Vec4 color(0.90f, 0.90f, 0.90f, 1.0f);
        if (def.talkId == "oracle") {
            color.set(1.00f, 0.82f, 0.18f, 1.0f);
        } else if (def.talkId == "merchant") {
            color.set(0.20f, 0.78f, 0.35f, 1.0f);
        }
        LocalNpc npc(osg::Vec3(def.x, def.y, def.z), def.contentId, def.name, def.talkId, color);
        if (m_enemyRoot.valid() && npc.getNode() != nullptr) {
            m_enemyRoot->addChild(npc.getNode());
        }
        m_npcs.push_back(npc);
        std::cout << "[npc] " << def.name << " (" << def.x << ", " << def.y << ", " << def.z
                  << ")\n";
    }
}

void StandaloneEngine::updateNpcs(float dt)
{
    const float greetR2 = 1.80f * 1.80f;
    const osg::Vec3 player(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    for (size_t i = 0; i < m_npcs.size(); ++i) {
        LocalNpc& npc = m_npcs[i];
        if (npc.greetCd > 0.0f) {
            npc.greetCd -= dt;
        }
        npc.syncVisual();
        const float dx = npc.pos.x() - player.x();
        const float dz = npc.pos.z() - player.z();
        if (dx * dx + dz * dz > greetR2 || npc.greetCd > 0.0f) {
            continue;
        }
        npc.greetCd = 4.0f;
        spawnFloatingText(
            osg::Vec3(npc.pos.x(), npc.pos.y() + 1.2f, npc.pos.z()),
            npc.name, osg::Vec4(1.00f, 0.92f, 0.35f, 1.0f), 0.14f);
        m_scripts.processTalk(npc.talkId, "hi");
        if (npc.talkId == "oracle" &&
            m_quests.isDone("otr.quest.sandbox.first_blood") &&
            m_quests.trySetVocation(1)) {
            m_dummyActor.setVocation(1);
            spawnFloatingText(
                osg::Vec3(npc.pos.x(), npc.pos.y() + 1.6f, npc.pos.z()),
                "SCOUT", osg::Vec4(0.35f, 0.95f, 1.00f, 1.0f), 0.16f);
            m_questAlertText = "[ VOCATION: SCOUT ]";
            m_questAlertTtl = 3.50f;
            fireContentHook("onVocation", "Scout");
        }
    }
}

void StandaloneEngine::spawnEnemyAt(const osg::Vec3& pos, bool isBoss)
{
    spawnEnemyAt(pos, isBoss ? ENEMY_BOSS : ENEMY_GRUNT);
}

void StandaloneEngine::spawnEnemyAt(const osg::Vec3& pos, EnemyKind kind)
{
    int slot = -1;
    for (size_t i = 0; i < m_enemies.size(); ++i) {
        if (!m_enemies[i].isAlive) {
            slot = static_cast<int>(i);
            break;
        }
    }

    LocalEnemy spawned(pos, kind);
    applyEnemyStats(spawned);
    if (slot >= 0) {
        m_enemies[static_cast<size_t>(slot)] = spawned;
        if (m_enemyRoot.valid() && m_enemies[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->addChild(m_enemies[static_cast<size_t>(slot)].getNode());
        }
    } else {
        m_enemies.push_back(spawned);
        if (m_enemyRoot.valid() && m_enemies.back().getNode() != nullptr) {
            m_enemyRoot->addChild(m_enemies.back().getNode());
        }
    }
    if (kind == ENEMY_BOSS) {
        std::cout << "[spawn] BOSS (" << pos.x() << ", 0, " << pos.z() << ")\n";
    } else if (kind == ENEMY_ARCHER) {
        std::cout << "[spawn] archer (" << pos.x() << ", 0, " << pos.z() << ")\n";
    } else {
        std::cout << "[spawn] enemy (" << pos.x() << ", 0, " << pos.z() << ")\n";
    }
}

void StandaloneEngine::spawnBatAt(const osg::Vec3& pos)
{
    int slot = -1;
    for (size_t i = 0; i < m_bats.size(); ++i) {
        if (!m_bats[i].isAlive()) {
            slot = static_cast<int>(i);
            break;
        }
    }
    LocalFlyingBat spawned(pos);
    {
        const ActorStats stats = m_content.actor("otr.actor.sandbox.bat");
        if (stats.valid) {
            spawned.applyStats(stats.maxHp);
        }
    }
    if (slot >= 0) {
        if (m_enemyRoot.valid() && m_bats[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->removeChild(m_bats[static_cast<size_t>(slot)].getNode());
        }
        m_bats[static_cast<size_t>(slot)] = spawned;
        if (m_enemyRoot.valid() && m_bats[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->addChild(m_bats[static_cast<size_t>(slot)].getNode());
        }
    } else {
        m_bats.push_back(spawned);
        if (m_enemyRoot.valid() && m_bats.back().getNode() != nullptr) {
            m_enemyRoot->addChild(m_bats.back().getNode());
        }
    }
    std::cout << "[bat] spawn (" << pos.x() << ", " << pos.y() << ", " << pos.z() << ")\n";
}

void StandaloneEngine::onBatKilled(LocalFlyingBat& bat, int index)
{
    m_killCount += 1;
    noteKill();
    fireContentHook("onKill", "bat");
    spawnExpAt(bat.pos, 0);
    const int roll = std::rand() % 100;
    if (roll < 60) {
        spawnLootAt(bat.pos, SMALL_HP);
    }
    if (m_enemyRoot.valid() && bat.getNode() != nullptr) {
        m_enemyRoot->removeChild(bat.getNode());
    }
    if (index == m_lockedBatIndex) {
        m_lockedBatIndex = -1;
        std::cout << "[lock] clear\n";
    }
    std::cout << "[kills] " << m_killCount << " bat\n";
}

void StandaloneEngine::spawnCrawlerAt(const osg::Vec3& pos)
{
    int slot = -1;
    for (size_t i = 0; i < m_crawlers.size(); ++i) {
        if (!m_crawlers[i].isAlive()) {
            slot = static_cast<int>(i);
            break;
        }
    }
    LocalCrawler spawned(pos);
    {
        const ActorStats stats = m_content.actor("otr.actor.sandbox.crawler");
        if (stats.valid) {
            spawned.applyStats(stats.maxHp, stats.speed);
        }
    }
    if (slot >= 0) {
        if (m_enemyRoot.valid() && m_crawlers[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->removeChild(m_crawlers[static_cast<size_t>(slot)].getNode());
        }
        m_crawlers[static_cast<size_t>(slot)] = spawned;
        if (m_enemyRoot.valid() && m_crawlers[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->addChild(m_crawlers[static_cast<size_t>(slot)].getNode());
        }
    } else {
        m_crawlers.push_back(spawned);
        if (m_enemyRoot.valid() && m_crawlers.back().getNode() != nullptr) {
            m_enemyRoot->addChild(m_crawlers.back().getNode());
        }
    }
    std::cout << "[crawler] spawn (" << pos.x() << ", " << pos.y() << ", " << pos.z() << ")\n";
}

void StandaloneEngine::onCrawlerKilled(LocalCrawler& crawler, int index)
{
    m_killCount += 1;
    noteKill();
    fireContentHook("onKill", "crawler");
    spawnExpAt(crawler.pos, 0);
    spawnLootAt(crawler.pos, ENERGY_CELL);
    if (m_enemyRoot.valid() && crawler.getNode() != nullptr) {
        m_enemyRoot->removeChild(crawler.getNode());
    }
    if (index == m_lockedCrawlerIndex) {
        m_lockedCrawlerIndex = -1;
        std::cout << "[lock] clear\n";
    }
    std::cout << "[kills] " << m_killCount << " crawler energy drop\n";
}

void StandaloneEngine::updateCrawlers(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    const AABB dummyAabb = m_dummyActor.makeAabb();
    const float meleeR = 0.55f * TILE_SIZE;
    const float meleeR2 = meleeR * meleeR;
    for (size_t i = 0; i < m_crawlers.size(); ++i) {
        LocalCrawler& cr = m_crawlers[i];
        if (!cr.isAlive()) {
            continue;
        }
        const float oldX = cr.pos.x();
        const float oldZ = cr.pos.z();
        cr.updateAI(dt, playerPos);

        const float nx = cr.pos.x();
        const float nz = cr.pos.z();
        cr.pos.x() = nx;
        cr.pos.z() = oldZ;
        if (m_localPhysics.checkCollision(cr.makeAabb()) || aabbOverlap(cr.makeAabb(), dummyAabb)) {
            cr.pos.x() = oldX;
        }
        cr.pos.z() = nz;
        if (m_localPhysics.checkCollision(cr.makeAabb()) || aabbOverlap(cr.makeAabb(), dummyAabb)) {
            cr.pos.z() = oldZ;
        }
        if (cr.pos.y() < 0.0f) {
            cr.pos.y() = 0.0f;
        }

        cr.setTargeted(static_cast<int>(i) == m_lockedCrawlerIndex);
        cr.syncVisual();

        const float dx = cr.pos.x() - m_dummyActor.x();
        const float dz = cr.pos.z() - m_dummyActor.z();
        if (m_dummyActor.iFrames() <= 0.0f && dx * dx + dz * dz <= meleeR2) {
            handlePlayerHit(10);
        }
    }
}

void StandaloneEngine::noteKill()
{
    if (m_killCount >= 20 && (m_killCount % 20) == 0) {
        m_pendingCentipede = true;
    }
    if (m_quests.onKill(m_killCount)) {
        m_questAlertText = std::string("[ QUEST: ") + m_quests.lastEvent() + " ]";
        m_questAlertTtl = 3.50f;
        spawnFloatingText(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
            "QUEST DONE", osg::Vec4(1.00f, 0.85f, 0.20f, 1.0f), 0.16f);
        fireContentHook("onQuest", m_quests.lastEvent());
    }
}

void StandaloneEngine::plantCentipedeBrick(const osg::Vec3& pos)
{
    int vx = worldToVoxelIndex(pos.x());
    int vy = worldToVoxelIndex(pos.y());
    int vz = worldToVoxelIndex(pos.z());
    if (vy < 0) {
        vy = 0;
    }
    if (m_miniVoxels.getVoxel(vx, vy, vz).isActive) {
        vy += 1;
    }
    m_miniVoxels.setVoxel(vx, vy, vz, 2);
    m_localMesher.rebuildMesh();
    m_phantomCursor.resetDda();
    spawnDebris(pos, osg::Vec4(0.78f, 0.22f, 0.10f, 1.0f));
}

void StandaloneEngine::adoptCentipede(LocalCentipede& worm)
{
    if (!worm.isAlive()) {
        return;
    }
    int slot = -1;
    for (size_t i = 0; i < m_centipedes.size(); ++i) {
        if (!m_centipedes[i].isAlive()) {
            slot = static_cast<int>(i);
            break;
        }
    }
    if (slot >= 0) {
        if (m_enemyRoot.valid() && m_centipedes[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->removeChild(m_centipedes[static_cast<size_t>(slot)].getNode());
        }
        m_centipedes[static_cast<size_t>(slot)] = worm;
        if (m_enemyRoot.valid() && m_centipedes[static_cast<size_t>(slot)].getNode() != nullptr) {
            m_enemyRoot->addChild(m_centipedes[static_cast<size_t>(slot)].getNode());
        }
    } else {
        m_centipedes.push_back(worm);
        if (m_enemyRoot.valid() && m_centipedes.back().getNode() != nullptr) {
            m_enemyRoot->addChild(m_centipedes.back().getNode());
        }
    }
}

void StandaloneEngine::spawnCentipedeBoss()
{
    m_pendingCentipede = false;
    const osg::Vec3 head(
        1.15f,
        (6.0f + 0.5f) * MINI_VOXEL_SIZE,
        (4.0f + 2.0f) * MINI_VOXEL_SIZE);
    LocalCentipede worm(head, 8, 1.0f);
    adoptCentipede(worm);
    m_centipedeAlertTtl = 4.50f;
    std::cout << "[centipede] BOSS spawn y=" << head.y() << " segs=8\n";
}

void StandaloneEngine::onCentipedeKilled(int index)
{
    m_killCount += 1;
    noteKill();
    fireContentHook("onKill", "centipede");
    if (index >= 0 && index < static_cast<int>(m_centipedes.size())) {
        LocalCentipede& worm = m_centipedes[static_cast<size_t>(index)];
        spawnExpAt(worm.lastDeadPos(), 50);
        spawnLootAt(worm.lastDeadPos(), LARGE_HP);
        spawnLootAt(worm.lastDeadPos(), ENERGY_CELL);
        if (m_enemyRoot.valid() && worm.getNode() != nullptr) {
            m_enemyRoot->removeChild(worm.getNode());
        }
    }
    if (index == m_lockedCentipedeIndex) {
        m_lockedCentipedeIndex = -1;
        m_lockedCentipedeSeg = -1;
        std::cout << "[lock] clear\n";
    }
    std::cout << "[kills] " << m_killCount << " centipede\n";
}

void StandaloneEngine::applyCentipedeDamage(int worm, int seg, int dmg)
{
    if (worm < 0 || worm >= static_cast<int>(m_centipedes.size())) {
        return;
    }
    LocalCentipede& src = m_centipedes[static_cast<size_t>(worm)];
    if (!src.isAlive()) {
        return;
    }
    LocalCentipede split;
    const CentipedeHit hit = src.takeDamageAtSegment(seg, dmg, &split);
    if (hit == CENTI_HIT_NONE) {
        return;
    }
    if (hit == CENTI_HIT_TRIM || hit == CENTI_HIT_SPLIT || hit == CENTI_HIT_DEAD) {
        plantCentipedeBrick(src.lastDeadPos());
    }
    if (hit == CENTI_HIT_SPLIT) {
        adoptCentipede(split);
        std::cout << "[centipede] split worm=" << worm << "\n";
    }
    if (hit == CENTI_HIT_DEAD) {
        onCentipedeKilled(worm);
        return;
    }
    if (m_lockedCentipedeIndex == worm) {
        if (!src.isAlive() || m_lockedCentipedeSeg < 0 ||
            m_lockedCentipedeSeg >= src.segmentCount()) {
            src.setLockedSegment(-1);
            m_lockedCentipedeIndex = -1;
            m_lockedCentipedeSeg = -1;
        } else {
            src.setLockedSegment(m_lockedCentipedeSeg);
        }
    }
}

void StandaloneEngine::updateCentipedes(float dt)
{
    if (m_pendingCentipede) {
        spawnCentipedeBoss();
    }
    if (dt <= 0.0f) {
        return;
    }
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    const AABB dummyAabb = m_dummyActor.makeAabb();
    for (size_t w = 0; w < m_centipedes.size(); ++w) {
        LocalCentipede& worm = m_centipedes[w];
        if (!worm.isAlive()) {
            continue;
        }
        worm.updateMovement(dt, m_miniVoxels, m_boxWorld, m_boulderWorld, playerPos);
        if (static_cast<int>(w) == m_lockedCentipedeIndex) {
            worm.setLockedSegment(m_lockedCentipedeSeg);
        } else {
            worm.setLockedSegment(-1);
        }
        if (m_dummyActor.iFrames() <= 0.0f && worm.segmentCount() > 0) {
            if (aabbOverlap(worm.makeAabb(0), dummyAabb)) {
                handlePlayerHit(15);
            }
        }
    }
}

void StandaloneEngine::updateBats(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    const osg::Vec3 facing(std::sin(m_dummyActor.yaw()), 0.0f, std::cos(m_dummyActor.yaw()));
    const float headY = m_dummyActor.y() + m_dummyActor.height() * 0.85f;
    const AABB dummyAabb = m_dummyActor.makeAabb();

    for (size_t i = 0; i < m_bats.size(); ++i) {
        LocalFlyingBat& bat = m_bats[i];
        if (!bat.isAlive()) {
            continue;
        }
        const osg::Vec3 prev = bat.pos;
        bat.updateAI(dt, playerPos, facing, headY);
        bat.setTargeted(static_cast<int>(i) == m_lockedBatIndex);

        if (bat.state() == BAT_DIVE_STRIKE) {
            int vx = 0;
            int vy = 0;
            int vz = 0;
            if (firstSolidOnSegment(m_miniVoxels, prev, bat.pos, &vx, &vy, &vz)) {
                if (removeMiniVoxel(vx, vy, vz)) {
                    m_localMesher.rebuildMesh();
                }
                spawnDebris(
                    osg::Vec3((static_cast<float>(vx) + 0.5f) * MINI_VOXEL_SIZE,
                              (static_cast<float>(vy) + 0.5f) * MINI_VOXEL_SIZE,
                              (static_cast<float>(vz) + 0.5f) * MINI_VOXEL_SIZE),
                    kVoxelColor);
                bat.notifyWall();
            } else if (aabbOverlap(bat.makeAabb(), dummyAabb)) {
                if (m_dummyActor.dashTimer() > 0.0f) {
                    bat.notifyMiss();
                    std::cout << "[bat] dive dodged\n";
                } else {
                    m_dummyActor.applyDamage(20);
                    spawnFloatingText(
                        osg::Vec3(m_dummyActor.x(),
                                  m_dummyActor.y() + m_dummyActor.height(),
                                  m_dummyActor.z()),
                        "-20", kFloatDmgPlayer, 0.14f);
                    float kx = m_dummyActor.x() - bat.pos.x();
                    float kz = m_dummyActor.z() - bat.pos.z();
                    const float klen = std::sqrt(kx * kx + kz * kz);
                    if (klen > 0.0001f) {
                        m_dummyActor.applyKnockback(kx / klen * 10.0f, kz / klen * 10.0f);
                    } else {
                        m_dummyActor.applyKnockback(facing.x() * 10.0f, facing.z() * 10.0f);
                    }
                    std::cout << "[bat] hit player -20 HP=" << m_dummyActor.hp() << "\n";
                    if (m_dummyActor.hp() <= 0) {
                        std::cout << "[combat] Player DIED! Respawning...\n";
                        m_survivalTime = 0.0f;
                        m_dummyActor.restoreHp();
                        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
                        m_dummyActor.syncVisual();
                    }
                    bat.notifyHit();
                }
            }
        }
        bat.syncVisual();
    }
}

void StandaloneEngine::spawnLootAt(const osg::Vec3& pos, LootSize size)
{
    int slot = -1;
    for (size_t i = 0; i < m_loots.size(); ++i) {
        if (!m_loots[i].isActive) {
            slot = static_cast<int>(i);
            break;
        }
    }

    if (slot >= 0) {
        m_loots[static_cast<size_t>(slot)].activate(pos, size);
        if (m_lootRoot.valid() && m_loots[static_cast<size_t>(slot)].getNode() != nullptr &&
            m_loots[static_cast<size_t>(slot)].getNode()->getNumParents() == 0) {
            m_lootRoot->addChild(m_loots[static_cast<size_t>(slot)].getNode());
        }
        return;
    }

    m_loots.push_back(LocalLoot(pos, size));
    if (m_lootRoot.valid() && m_loots.back().getNode() != nullptr) {
        m_lootRoot->addChild(m_loots.back().getNode());
    }
}

void StandaloneEngine::spawnExpAt(const osg::Vec3& pos, int expValue)
{
    int slot = -1;
    for (size_t i = 0; i < m_exps.size(); ++i) {
        if (!m_exps[i].isActive) {
            slot = static_cast<int>(i);
            break;
        }
    }

    if (slot >= 0) {
        m_exps[static_cast<size_t>(slot)].activate(pos, expValue);
        if (m_expRoot.valid() && m_exps[static_cast<size_t>(slot)].getNode() != nullptr &&
            m_exps[static_cast<size_t>(slot)].getNode()->getNumParents() == 0) {
            m_expRoot->addChild(m_exps[static_cast<size_t>(slot)].getNode());
        }
        return;
    }

    m_exps.push_back(LocalExp(pos, expValue));
    if (m_expRoot.valid() && m_exps.back().getNode() != nullptr) {
        m_expRoot->addChild(m_exps.back().getNode());
    }
}

void StandaloneEngine::updateSpawner(float deltaTime)
{
    // 38.2 / 38.3 / 38.4 Cada 3s, si hay < 4 vivos, spawn a 10 tiles.
    m_spawnTimer += deltaTime;
    if (m_spawnTimer <= 3.0f) {
        return;
    }
    m_spawnTimer = 0.0f;

    int alive = 0;
    for (size_t i = 0; i < m_enemies.size(); ++i) {
        if (m_enemies[i].isAlive) {
            alive += 1;
        }
    }
    if (alive >= 4 && !m_pendingBoss) {
        return;
    }

    m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
    const float ang = static_cast<float>(m_spawnSeed % 6283u) * 0.001f;
    const float dist = 10.0f * TILE_SIZE;
    const osg::Vec3 spawnPos(
        m_dummyActor.x() + std::sin(ang) * dist,
        0.0f,
        m_dummyActor.z() + std::cos(ang) * dist);
    const bool boss = m_pendingBoss;
    m_pendingBoss = false;
    if (boss) {
        spawnEnemyAt(spawnPos, ENEMY_BOSS);
    } else if ((m_spawnSeed % 3u) == 0u) {
        spawnEnemyAt(spawnPos, ENEMY_ARCHER);
    } else {
        spawnEnemyAt(spawnPos, ENEMY_GRUNT);
    }
}

void StandaloneEngine::updateLoot(float deltaTime)
{
    const osg::Vec3 player(
        m_dummyActor.x(),
        m_dummyActor.y() + m_dummyActor.height() * 0.45f,
        m_dummyActor.z());
    const float absorbR2 = kAbsorbRange * kAbsorbRange;
    for (size_t i = 0; i < m_loots.size(); ++i) {
        LocalLoot& loot = m_loots[i];
        if (!loot.isActive) {
            continue;
        }
        stepDropKinematics(loot.pos, loot.velocity, loot.isGrounded, deltaTime, player);
        loot.tick(deltaTime);
        if (m_buddy.inFetchRange(loot.pos)) {
            collectLoot(loot, true);
            continue;
        }
        const osg::Vec3 d = player - loot.pos;
        if (d.length2() <= absorbR2) {
            collectLoot(loot, false);
        }
    }
}

void StandaloneEngine::updateExpDrops(float deltaTime)
{
    const osg::Vec3 player(
        m_dummyActor.x(),
        m_dummyActor.y() + m_dummyActor.height() * 0.45f,
        m_dummyActor.z());
    const float absorbR2 = kAbsorbRange * kAbsorbRange;
    for (size_t i = 0; i < m_exps.size(); ++i) {
        LocalExp& drop = m_exps[i];
        if (!drop.isActive) {
            continue;
        }
        stepDropKinematics(drop.pos, drop.velocity, drop.isGrounded, deltaTime, player);
        drop.tick(deltaTime);
        if (m_buddy.inFetchRange(drop.pos)) {
            collectExp(drop, true);
            continue;
        }
        const osg::Vec3 d = player - drop.pos;
        if (d.length2() <= absorbR2) {
            collectExp(drop, false);
        }
    }
}

void StandaloneEngine::updateDrops(float deltaTime)
{
    updateLoot(deltaTime);
    updateExpDrops(deltaTime);
}

float StandaloneEngine::dropHitFloorY(float x, float y, float z) const
{
    float hit = kDropFloorY;
    const int vx = worldToVoxelIndex(x);
    const int vz = worldToVoxelIndex(z);
    const int maxVy = worldToVoxelIndex(y + 0.08f);
    int vy = 0;
    while (vy <= maxVy) {
        if (m_miniVoxels.getVoxel(vx, vy, vz).isActive) {
            const float top = static_cast<float>(vy + 1) * MINI_VOXEL_SIZE;
            if (top > hit) {
                hit = top;
            }
        }
        vy += 1;
    }
    const float half = CELL_SIZE * 0.5f;
    const int n = m_boxWorld.boxCount();
    for (int i = 0; i < n; ++i) {
        if (!m_boxWorld.boxAlive(i)) {
            continue;
        }
        const osg::Vec3 p = m_boxWorld.boxPos(i);
        if (std::fabs(x - p.x()) > half + 0.06f || std::fabs(z - p.z()) > half + 0.06f) {
            continue;
        }
        const float top = p.y() + half;
        if (top <= y + 0.18f && top > hit) {
            hit = top;
        }
    }
    return hit;
}

void StandaloneEngine::stepDropKinematics(osg::Vec3& pos, osg::Vec3& vel, bool& grounded, float dt,
                                         const osg::Vec3& playerCenter)
{
    if (dt <= 0.0f) {
        return;
    }
    osg::Vec3 dir = playerCenter - pos;
    const float dist = dir.length();
    if (dist <= kMagnetRange && dist > 0.0001f) {
        grounded = false;
        dir = dir * (1.0f / dist);
        pos = pos + dir * (kMagnetSpeed * dt);
        vel.set(0.0f, 0.0f, 0.0f);
        return;
    }
    if (grounded) {
        const float floorY = dropHitFloorY(pos.x(), pos.y() + 0.05f, pos.z());
        pos.y() = floorY;
        vel.set(0.0f, 0.0f, 0.0f);
        return;
    }
    vel.y() -= kDropGravity * dt;
    pos.y() += vel.y() * dt;
    const float floorY = dropHitFloorY(pos.x(), pos.y(), pos.z());
    if (pos.y() <= floorY) {
        pos.y() = floorY;
        vel.set(0.0f, 0.0f, 0.0f);
        grounded = true;
    }
}

void StandaloneEngine::collectLoot(LocalLoot& loot, bool byFary)
{
    loot.deactivate();
    if (m_lootRoot.valid() && loot.getNode() != nullptr) {
        m_lootRoot->removeChild(loot.getNode());
    }
    const osg::Vec3 pop(
        m_dummyActor.x(),
        m_dummyActor.y() + m_dummyActor.height(),
        m_dummyActor.z());
    if (byFary) {
        m_buddy.notifyFetch();
        spawnDebris(loot.pos, loot.isEnergy() ? kFloatHunter : kFloatHeal);
    }
    if (loot.isEnergy()) {
        m_hunterCells += m_content.energyPickup();
        if (m_hunterCells > kHunterCellMax) {
            m_hunterCells = kHunterCellMax;
        }
        m_inventory.add("otr.item.sandbox.energy_cell", m_content.energyPickup());
        spawnFloatingText(pop, "+ENERGY", kFloatHunter);
        std::cout << "[energy] +" << m_content.energyPickup() << " cells=" << m_hunterCells
                  << (byFary ? " fary\n" : "\n");
        fireContentHook("onLoot", "energy");
        return;
    }
    const int healAmt = (loot.size() == LARGE_HP) ? 35 : 10;
    m_dummyActor.heal(healAmt);
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "+%d HP", healAmt);
        spawnFloatingText(pop, buf, kFloatHeal);
    }
    std::cout << "[loot] heal +" << healAmt << (byFary ? " fary\n" : "\n");
    fireContentHook("onLoot", "heal");
}

void StandaloneEngine::collectExp(LocalExp& drop, bool byFary)
{
    drop.deactivate();
    if (m_expRoot.valid() && drop.getNode() != nullptr) {
        m_expRoot->removeChild(drop.getNode());
    }
    const osg::Vec3 pop(
        m_dummyActor.x(),
        m_dummyActor.y() + m_dummyActor.height(),
        m_dummyActor.z());
    if (byFary) {
        m_buddy.notifyFetch();
        spawnDebris(drop.pos, kFloatExp);
    }
    int grant = drop.expValue();
    if (grant <= 0) {
        grant = 10;
    }
    m_dummyActor.addExp(grant);
    spawnFloatingText(pop, "+EXP", kFloatExp);
    std::cout << "[exp] +" << grant << (byFary ? " fary" : "")
              << " LVL " << m_dummyActor.level() << "\n";
}

void StandaloneEngine::hintBuddyFetch()
{
    const osg::Vec3 fary = m_buddy.position();
    const float maxR = 4.50f * TILE_SIZE;
    const float maxR2 = maxR * maxR;
    int bestKind = -1;
    int bestId = -1;
    float bestD = maxR2;
    for (size_t i = 0; i < m_loots.size(); ++i) {
        if (!m_loots[i].isActive) {
            continue;
        }
        const osg::Vec3 d = m_loots[i].pos - fary;
        const float d2 = d.length2();
        if (d2 < bestD) {
            bestD = d2;
            bestKind = 0;
            bestId = static_cast<int>(i);
        }
    }
    for (size_t i = 0; i < m_exps.size(); ++i) {
        if (!m_exps[i].isActive) {
            continue;
        }
        const osg::Vec3 d = m_exps[i].pos - fary;
        const float d2 = d.length2();
        if (d2 < bestD) {
            bestD = d2;
            bestKind = 1;
            bestId = static_cast<int>(i);
        }
    }
    if (bestKind < 0) {
        m_buddy.clearFetchHint();
        return;
    }
    if (bestKind == 0) {
        m_buddy.setFetchHint(m_loots[static_cast<size_t>(bestId)].pos);
    } else {
        m_buddy.setFetchHint(m_exps[static_cast<size_t>(bestId)].pos);
    }
}

void StandaloneEngine::spawnMeleeFlash()
{
    // 35.1 / 35.2 Caja translucida delante del Dummy. TTL 0.15s.
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const osg::Vec3 flashPos(
        m_dummyActor.x() + fx * 0.85f,
        m_dummyActor.y() + m_dummyActor.height() * 0.45f,
        m_dummyActor.z() + fz * 0.85f);

    if (!m_meleeFlash.valid()) {
        // 73. osg::Box = arista total del flash.
        osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.55f, 0.22f, 0.70f);
        osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(box.get());
        drawable->setColor(osg::Vec4(1.0f, 0.95f, 0.45f, 0.40f));

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(drawable.get());

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.35f, 0.10f, 0.40f));
        material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 0.95f, 0.45f, 0.40f));
        material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.50f, 0.15f, 0.40f));
        material->setAlpha(osg::Material::FRONT_AND_BACK, 0.40f);

        osg::StateSet* state = geode->getOrCreateStateSet();
        state->setAttributeAndModes(material.get(), osg::StateAttribute::ON);
        state->setAttributeAndModes(
            new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON);
        state->setMode(GL_BLEND, osg::StateAttribute::ON);
        state->setMode(GL_LIGHTING, osg::StateAttribute::ON);
        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                    osg::StateAttribute::ON);

        m_meleeFlash = new osg::PositionAttitudeTransform;
        m_meleeFlash->addChild(geode.get());
    }

    m_meleeFlash->setPosition(flashPos);
    m_meleeFlash->setAttitude(osg::Quat(static_cast<double>(yaw), osg::Vec3d(0.0, 1.0, 0.0)));
    if (m_worldRoot.valid() && m_meleeFlash->getNumParents() == 0) {
        m_worldRoot->addChild(m_meleeFlash.get());
    }
    m_meleeFlashTtl = 0.15f;
}

void StandaloneEngine::updateMeleeFlash(float deltaTime)
{
    if (m_meleeFlashTtl <= 0.0f) {
        return;
    }
    m_meleeFlashTtl -= deltaTime;
    if (m_meleeFlashTtl > 0.0f) {
        return;
    }
    m_meleeFlashTtl = 0.0f;
    if (m_worldRoot.valid() && m_meleeFlash.valid()) {
        m_worldRoot->removeChild(m_meleeFlash.get());
    }
}

void StandaloneEngine::performMeleeAttack()
{
    // 34.3 / 34.4 Cono frontal: dist < 1.5 tiles y dot(frente, toEnemy) > 0.5.
    spawnMeleeFlash();

    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const float range = 1.5f * TILE_SIZE;
    const float range2 = range * range;

    for (size_t e = 0; e < m_enemies.size(); ++e) {
        LocalEnemy& enemy = m_enemies[e];
        if (!enemy.isAlive) {
            continue;
        }
        const float dx = enemy.pos.x() - m_dummyActor.x();
        const float dz = enemy.pos.z() - m_dummyActor.z();
        const float dist2 = dx * dx + dz * dz;
        if (dist2 > range2) {
            continue;
        }
        const float dist = std::sqrt(dist2);
        float ndx = 0.0f;
        float ndz = 0.0f;
        if (dist < 0.0001f) {
            ndx = fx;
            ndz = fz;
        } else {
            const float inv = 1.0f / dist;
            ndx = dx * inv;
            ndz = dz * inv;
        }
        const float facing = fx * ndx + fz * ndz;
        if (facing <= 0.5f) {
            continue;
        }

        enemy.takeDamage(15, osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()));
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "-%d", 15);
            spawnFloatingText(
                osg::Vec3(enemy.pos.x(), enemy.pos.y() + enemy.height() * 0.65f, enemy.pos.z()),
                buf, kFloatDmgEnemy);
        }
        std::cout << "[melee] hit enemy " << e << " HP: " << enemy.hp()
                  << "/" << enemy.maxHp() << "\n";
        if (!enemy.isAlive) {
            onEnemyKilled(enemy, static_cast<int>(e));
        }
    }

    for (size_t b = 0; b < m_bats.size(); ++b) {
        LocalFlyingBat& bat = m_bats[b];
        if (!bat.isAlive()) {
            continue;
        }
        const float dx = bat.pos.x() - m_dummyActor.x();
        const float dz = bat.pos.z() - m_dummyActor.z();
        const float dist2 = dx * dx + dz * dz;
        if (dist2 > range2) {
            continue;
        }
        if (bat.pos.y() > m_dummyActor.y() + m_dummyActor.height() + 1.10f) {
            continue;
        }
        const float dist = std::sqrt(dist2);
        float ndx = fx;
        float ndz = fz;
        if (dist > 0.0001f) {
            ndx = dx / dist;
            ndz = dz / dist;
        }
        if (fx * ndx + fz * ndz <= 0.5f) {
            continue;
        }
        bat.takeDamage(15, osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()));
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "-%d", 15);
            spawnFloatingText(
                osg::Vec3(bat.pos.x(), bat.pos.y() + 0.25f, bat.pos.z()),
                buf, kFloatDmgEnemy);
        }
        std::cout << "[melee] hit bat " << b << " HP: " << bat.hp() << "/" << bat.maxHp() << "\n";
        if (!bat.isAlive()) {
            onBatKilled(bat, static_cast<int>(b));
        }
    }

    for (size_t c = 0; c < m_crawlers.size(); ++c) {
        LocalCrawler& cr = m_crawlers[c];
        if (!cr.isAlive()) {
            continue;
        }
        const float dx = cr.pos.x() - m_dummyActor.x();
        const float dz = cr.pos.z() - m_dummyActor.z();
        const float dist2 = dx * dx + dz * dz;
        if (dist2 > range2) {
            continue;
        }
        const float dist = std::sqrt(dist2);
        float ndx = fx;
        float ndz = fz;
        if (dist > 0.0001f) {
            ndx = dx / dist;
            ndz = dz / dist;
        }
        if (fx * ndx + fz * ndz <= 0.5f) {
            continue;
        }
        cr.takeDamage(15, osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()));
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "-%d", 15);
            spawnFloatingText(
                osg::Vec3(cr.pos.x(), cr.pos.y() + cr.height() * 0.80f, cr.pos.z()),
                buf, kFloatDmgEnemy);
        }
        std::cout << "[melee] hit crawler " << c << " HP: " << cr.hp() << "/" << cr.maxHp() << "\n";
        if (!cr.isAlive()) {
            onCrawlerKilled(cr, static_cast<int>(c));
        }
    }

    // 102.4 Melee sobre el Arquitecto, mismo alcance y cono que el resto.
    if (m_architect.isAlive()) {
        const osg::Vec3 ap = m_architect.position();
        const float adx = ap.x() - m_dummyActor.x();
        const float adz = ap.z() - m_dummyActor.z();
        const float adist2 = adx * adx + adz * adz;
        if (adist2 <= range2) {
            const float adist = std::sqrt(adist2);
            float ndx = fx;
            float ndz = fz;
            if (adist > 0.0001f) {
                ndx = adx / adist;
                ndz = adz / adist;
            }
            if (fx * ndx + fz * ndz > 0.5f) {
                damageArchitect(15);
                std::cout << "[melee] hit architect HP: " << m_architect.hp()
                          << "/" << m_architect.maxHp() << "\n";
            }
        }
    }

    for (size_t w = 0; w < m_centipedes.size(); ++w) {
        LocalCentipede& worm = m_centipedes[w];
        if (!worm.isAlive()) {
            continue;
        }
        for (int s = 0; s < worm.segmentCount(); ++s) {
            const osg::Vec3 sp = worm.segment(s).pos;
            const float dx = sp.x() - m_dummyActor.x();
            const float dz = sp.z() - m_dummyActor.z();
            const float dist2 = dx * dx + dz * dz;
            if (dist2 > range2) {
                continue;
            }
            const float dist = std::sqrt(dist2);
            float ndx = fx;
            float ndz = fz;
            if (dist > 0.0001f) {
                ndx = dx / dist;
                ndz = dz / dist;
            }
            if (fx * ndx + fz * ndz <= 0.5f) {
                continue;
            }
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "-%d", 15);
                spawnFloatingText(sp, buf, kFloatDmgEnemy);
            }
            std::cout << "[melee] hit centipede " << w << " seg " << s << "\n";
            applyCentipedeDamage(static_cast<int>(w), s, 15);
            break;
        }
    }

    // 59.3 Voxels destruibles en cono frontal < 1.5 tiles.
    const float px = m_dummyActor.x();
    const float py = m_dummyActor.y();
    const float pz = m_dummyActor.z();
    const float midY = py + m_dummyActor.height() * 0.50f;
    const int minVx = worldToVoxelIndex(px - range);
    const int maxVx = worldToVoxelIndex(px + range);
    const int minVy = worldToVoxelIndex(py - 0.10f);
    const int maxVy = worldToVoxelIndex(py + m_dummyActor.height() + 0.35f);
    const int minVz = worldToVoxelIndex(pz - range);
    const int maxVz = worldToVoxelIndex(pz + range);

    std::vector<VoxelKey> smash;
    int vy = minVy;
    while (vy <= maxVy) {
        int vz = minVz;
        while (vz <= maxVz) {
            int vx = minVx;
            while (vx <= maxVx) {
                if (vy >= 0 && m_miniVoxels.getVoxel(vx, vy, vz).isActive) {
                    const float cx = (static_cast<float>(vx) + 0.5f) * MINI_VOXEL_SIZE;
                    const float cy = (static_cast<float>(vy) + 0.5f) * MINI_VOXEL_SIZE;
                    const float cz = (static_cast<float>(vz) + 0.5f) * MINI_VOXEL_SIZE;
                    const float dx = cx - px;
                    const float dz = cz - pz;
                    const float dist2 = dx * dx + (cy - midY) * (cy - midY) + dz * dz;
                    if (dist2 <= range2) {
                        const float dist = std::sqrt(dx * dx + dz * dz);
                        float ndx = fx;
                        float ndz = fz;
                        if (dist > 0.0001f) {
                            const float inv = 1.0f / dist;
                            ndx = dx * inv;
                            ndz = dz * inv;
                        }
                        if (fx * ndx + fz * ndz > 0.5f) {
                            VoxelKey key;
                            key.vx = vx;
                            key.vy = vy;
                            key.vz = vz;
                            smash.push_back(key);
                        }
                    }
                }
                vx += 1;
            }
            vz += 1;
        }
        vy += 1;
    }

    bool smashed = false;
    for (size_t i = 0; i < smash.size(); ++i) {
        if (removeMiniVoxel(smash[i].vx, smash[i].vy, smash[i].vz)) {
            smashed = true;
        }
    }
    if (smashed) {
        m_localMesher.rebuildMesh();
    }
}

bool StandaloneEngine::removeMiniVoxel(int vx, int vy, int vz)
{
    // 59.2 Borra de MiniVoxelGrid. El mesher se reconstruye en el caller (mismo frame).
    if (vy < 0) {
        return false;
    }
    if (!m_miniVoxels.getVoxel(vx, vy, vz).isActive) {
        return false;
    }
    const osg::Vec3 world(
        (static_cast<float>(vx) + 0.5f) * MINI_VOXEL_SIZE,
        (static_cast<float>(vy) + 0.5f) * MINI_VOXEL_SIZE,
        (static_cast<float>(vz) + 0.5f) * MINI_VOXEL_SIZE);
    spawnDebris(world, kVoxelColor);
    m_miniVoxels.setVoxel(vx, vy, vz, 0);
    m_phantomCursor.resetDda();
    tryStartColumnFall(vx, vy, vz);
    return true;
}

void StandaloneEngine::tryStartColumnFall(int vx, int holeVy, int vz)
{
    // 95. BFS 6-vecinos desde cada vecino del hueco. Solo cae el cluster huérfano.
    for (int n = 0; n < 6; ++n) {
        const int sx = vx + kVoxelNbs[n][0];
        const int sy = holeVy + kVoxelNbs[n][1];
        const int sz = vz + kVoxelNbs[n][2];
        if (!m_miniVoxels.getVoxel(sx, sy, sz).isActive) {
            continue;
        }

        std::vector<VoxelKey> cells;
        std::unordered_set<VoxelKey, VoxelKeyHash> seen;
        std::vector<VoxelKey> stack;
        VoxelKey start;
        start.vx = sx;
        start.vy = sy;
        start.vz = sz;
        stack.push_back(start);
        seen.insert(start);
        bool grounded = false;
        while (!stack.empty() && cells.size() < 512) {
            const VoxelKey cur = stack.back();
            stack.pop_back();
            cells.push_back(cur);
            if (cur.vy <= 0) {
                grounded = true;
            }
            for (int k = 0; k < 6; ++k) {
                VoxelKey nb;
                nb.vx = cur.vx + kVoxelNbs[k][0];
                nb.vy = cur.vy + kVoxelNbs[k][1];
                nb.vz = cur.vz + kVoxelNbs[k][2];
                if (seen.find(nb) != seen.end()) {
                    continue;
                }
                if (!m_miniVoxels.getVoxel(nb.vx, nb.vy, nb.vz).isActive) {
                    continue;
                }
                seen.insert(nb);
                stack.push_back(nb);
            }
        }
        if (grounded || cells.empty()) {
            continue;
        }
        spawnFallingCluster(cells);
    }
}

void StandaloneEngine::spawnFallingCluster(const std::vector<VoxelKey>& cells)
{
    FallingCluster col;
    col.yOff = 0.0f;
    col.velY = 0.0f;
    col.crushed = false;
    col.active = true;
    col.node = new osg::MatrixTransform;
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, kVoxelColor);
    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.28f, 0.30f, 0.32f, 1.0f));
    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    const float edge = MINI_VOXEL_SIZE * 0.996f;
    for (size_t i = 0; i < cells.size(); ++i) {
        const VoxelKey& k = cells[i];
        FallingCluster::Cell c;
        c.vx = k.vx;
        c.vy = k.vy;
        c.vz = k.vz;
        c.mat = m_miniVoxels.getVoxel(k.vx, k.vy, k.vz).materialId;
        if (c.mat == 0) {
            c.mat = 1;
        }
        m_miniVoxels.setVoxel(k.vx, k.vy, k.vz, 0);
        col.cells.push_back(c);
        const osg::Vec3 p(
            (static_cast<float>(k.vx) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(k.vy) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(k.vz) + 0.5f) * MINI_VOXEL_SIZE);
        osg::ref_ptr<osg::Box> box = new osg::Box(p, edge, edge, edge);
        osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(box.get());
        draw->setColor(kVoxelColor);
        geode->addDrawable(draw.get());
    }
    col.node->addChild(geode.get());
    col.node->setMatrix(osg::Matrix::identity());
    if (m_worldRoot.valid()) {
        m_worldRoot->addChild(col.node.get());
    }
    m_fallingCols.push_back(col);
    m_localMesher.rebuildMesh();
}

void StandaloneEngine::updateFallingColumns(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    const AABB dummyAabb = m_dummyActor.makeAabb();
    bool settledAny = false;
    for (size_t i = 0; i < m_fallingCols.size();) {
        FallingCluster& col = m_fallingCols[i];
        if (!col.active) {
            m_fallingCols.erase(m_fallingCols.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        col.velY -= 9.81f * dt;
        col.yOff += col.velY * dt;

        AABB clusterAabb;
        clusterAabb.minX = 1.0e30f;
        clusterAabb.minY = 1.0e30f;
        clusterAabb.minZ = 1.0e30f;
        clusterAabb.maxX = -1.0e30f;
        clusterAabb.maxY = -1.0e30f;
        clusterAabb.maxZ = -1.0e30f;
        osg::Vec3 impact(0.0f, 0.0f, 0.0f);
        const float half = MINI_VOXEL_SIZE * 0.5f;
        for (size_t c = 0; c < col.cells.size(); ++c) {
            const FallingCluster::Cell& cell = col.cells[c];
            const float x = (static_cast<float>(cell.vx) + 0.5f) * MINI_VOXEL_SIZE;
            const float y = (static_cast<float>(cell.vy) + 0.5f) * MINI_VOXEL_SIZE + col.yOff;
            const float z = (static_cast<float>(cell.vz) + 0.5f) * MINI_VOXEL_SIZE;
            if (x - half < clusterAabb.minX) {
                clusterAabb.minX = x - half;
            }
            if (y - half < clusterAabb.minY) {
                clusterAabb.minY = y - half;
            }
            if (z - half < clusterAabb.minZ) {
                clusterAabb.minZ = z - half;
            }
            if (x + half > clusterAabb.maxX) {
                clusterAabb.maxX = x + half;
            }
            if (y + half > clusterAabb.maxY) {
                clusterAabb.maxY = y + half;
            }
            if (z + half > clusterAabb.maxZ) {
                clusterAabb.maxZ = z + half;
            }
            impact.x() += x;
            impact.y() += y;
            impact.z() += z;
        }
        if (!col.cells.empty()) {
            const float inv = 1.0f / static_cast<float>(col.cells.size());
            impact = impact * inv;
        }
        if (!col.crushed && aabbOverlap(clusterAabb, dummyAabb)) {
            col.crushed = true;
            applyCrush(impact);
        }

        bool settle = false;
        for (size_t c = 0; c < col.cells.size(); ++c) {
            const FallingCluster::Cell& cell = col.cells[c];
            const float wy = (static_cast<float>(cell.vy) + 0.5f) * MINI_VOXEL_SIZE + col.yOff;
            const int gy = worldToVoxelIndex(wy);
            if (gy < 0) {
                settle = true;
                break;
            }
            if (gy == 0 && wy <= half + 0.001f) {
                settle = true;
                break;
            }
            if (gy > 0 && m_miniVoxels.getVoxel(cell.vx, gy - 1, cell.vz).isActive) {
                settle = true;
                break;
            }
        }

        if (settle) {
            for (size_t c = 0; c < col.cells.size(); ++c) {
                const FallingCluster::Cell& cell = col.cells[c];
                const float wy = (static_cast<float>(cell.vy) + 0.5f) * MINI_VOXEL_SIZE + col.yOff;
                int gy = worldToVoxelIndex(wy);
                if (gy < 0) {
                    gy = 0;
                }
                while (m_miniVoxels.getVoxel(cell.vx, gy, cell.vz).isActive) {
                    gy += 1;
                }
                m_miniVoxels.setVoxel(cell.vx, gy, cell.vz, cell.mat);
            }
            if (m_worldRoot.valid() && col.node.valid()) {
                m_worldRoot->removeChild(col.node.get());
            }
            col.active = false;
            settledAny = true;
            m_fallingCols.erase(m_fallingCols.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (col.node.valid()) {
            col.node->setMatrix(osg::Matrix::translate(0.0f, col.yOff, 0.0f));
        }
        i += 1;
    }
    if (settledAny) {
        m_localMesher.rebuildMesh();
        m_phantomCursor.resetDda();
    }
}

void StandaloneEngine::applyCrush(const osg::Vec3& impact)
{
    const int dmg = static_cast<int>(static_cast<float>(m_dummyActor.maxHp()) * 0.25f + 0.5f);
    m_dummyActor.applyDamage(dmg < 1 ? 1 : dmg);
    spawnFloatingText(
        osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
        "CRUSH! -25%", osg::Vec4(1.0f, 0.10f, 0.10f, 1.0f), 0.14f);
    float px = m_dummyActor.x() - impact.x();
    float pz = m_dummyActor.z() - impact.z();
    const float plen = std::sqrt(px * px + pz * pz);
    if (plen < 0.0001f) {
        px = std::sin(m_dummyActor.yaw() + 1.5707963f);
        pz = std::cos(m_dummyActor.yaw() + 1.5707963f);
    } else {
        px /= plen;
        pz /= plen;
    }
    m_dummyActor.applyKnockback(px * 16.0f, pz * 16.0f);
    m_dummyActor.setVelY(m_dummyActor.velY() + 4.0f);
    std::cout << "[crush] -25% HP=" << m_dummyActor.hp() << "/" << m_dummyActor.maxHp() << "\n";
    if (m_dummyActor.hp() <= 0) {
        std::cout << "[combat] Player DIED! Respawning...\n";
        m_survivalTime = 0.0f;
        m_dummyActor.restoreHp();
        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
        m_dummyActor.syncVisual();
    }
}

bool StandaloneEngine::hasBoxSelection() const
{
    return !m_editorMode && m_boxWorld.boxAlive(m_selectedBox);
}

bool StandaloneEngine::hasBoulderSelection() const
{
    return !m_editorMode && m_boulderWorld.boulderResting(m_selectedBoulder);
}

int StandaloneEngine::nearestEnemyInCone(const osg::Vec3& origin, const osg::Vec3& facing,
                                         float maxDist) const
{
    const float maxD2 = maxDist * maxDist;
    const float cos45 = 0.70710678f;
    int best = -1;
    float bestD = 1.0e30f;
    for (size_t e = 0; e < m_enemies.size(); ++e) {
        const LocalEnemy& en = m_enemies[e];
        if (!en.isAlive) {
            continue;
        }
        const float dx = en.pos.x() - origin.x();
        const float dz = en.pos.z() - origin.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > maxD2 || d2 < 0.0001f) {
            continue;
        }
        const float dist = std::sqrt(d2);
        const float dot = (facing.x() * dx + facing.z() * dz) / dist;
        if (dot < cos45) {
            continue;
        }
        if (d2 < bestD) {
            bestD = d2;
            best = static_cast<int>(e);
        }
    }
    return best;
}

void StandaloneEngine::throwSelectedBlock()
{
    if (m_editorMode || !m_boxWorld.boxAlive(m_selectedBox)) {
        return;
    }
    osg::Vec3 pos;
    const int idx = m_selectedBox;
    if (!m_boxWorld.destroyBox(idx, &pos)) {
        return;
    }
    clearSelection();

    const float yaw = m_dummyActor.yaw();
    osg::Vec3 facing(std::sin(yaw), 0.0f, std::cos(yaw));
    const int tgt = nearestEnemyInCone(pos, facing, 16.0f * TILE_SIZE);
    osg::Vec3 vel = facing * 24.0f;
    vel.y() = 3.2f;
    if (tgt >= 0) {
        const LocalEnemy& en = m_enemies[static_cast<size_t>(tgt)];
        osg::Vec3 aim(
            en.pos.x() - pos.x(),
            (en.pos.y() + en.height() * 0.55f) - pos.y(),
            en.pos.z() - pos.z());
        const float len = aim.length();
        if (len > 0.0001f) {
            vel = aim * (24.0f / len);
            vel.y() += 2.4f;
        }
    }

    ThrownBlock missile;
    missile.pos = pos;
    missile.vel = vel;
    missile.ttl = 2.50f;
    missile.target = tgt;
    missile.alive = true;
    osg::ref_ptr<osg::Box> shape = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), CELL_SIZE * 0.996f, CELL_SIZE * 0.996f, CELL_SIZE * 0.996f);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(shape.get());
    draw->setColor(osg::Vec4(0.55f, 0.85f, 1.0f, 1.0f));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());
    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.85f, 1.0f, 1.0f));
    mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.12f, 0.22f, 0.30f, 1.0f));
    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    missile.node = new osg::MatrixTransform;
    missile.node->addChild(geode.get());
    missile.node->setMatrix(osg::Matrix::translate(pos));
    if (m_worldRoot.valid()) {
        m_worldRoot->addChild(missile.node.get());
    }
    m_thrown.push_back(missile);
    std::cout << "[throw] block missile tgt=" << tgt << "\n";
}

bool StandaloneEngine::tryPushBoulder()
{
    if (m_editorMode) {
        return false;
    }
    int idx = -1;
    if (m_boulderWorld.boulderResting(m_selectedBoulder)) {
        idx = m_selectedBoulder;
    } else {
        idx = findFrontBoulder();
    }
    if (idx < 0 || !m_boulderWorld.boulderResting(idx)) {
        return false;
    }
    float dx = 0.0f;
    float dz = 0.0f;
    LocalBoulderWorld::cardinalFromYaw(m_dummyActor.yaw(), &dx, &dz);
    const int nHit = static_cast<int>(m_enemies.size()) + static_cast<int>(m_bats.size()) +
                     static_cast<int>(m_crawlers.size()) +
                     static_cast<int>(m_centipedes.size()) * 16;
    m_boulderWorld.beginSlide(idx, dx, dz, nHit);
    std::cout << "[pengo] boulder " << idx << " dir=(" << dx << "," << dz << ")\n";
    return true;
}

int StandaloneEngine::findFrontBoulder() const
{
    float cdx = 0.0f;
    float cdz = 0.0f;
    LocalBoulderWorld::cardinalFromYaw(m_dummyActor.yaw(), &cdx, &cdz);
    const float maxD = 1.5f * TILE_SIZE;
    const float align = MINI_VOXEL_SIZE * 1.35f;
    const float px = m_dummyActor.x();
    const float pz = m_dummyActor.z();
    int best = -1;
    float bestAlong = maxD + 0.01f;
    const int n = m_boulderWorld.boulderCount();
    for (int i = 0; i < n; ++i) {
        if (!m_boulderWorld.boulderResting(i)) {
            continue;
        }
        const osg::Vec3 b = m_boulderWorld.boulderPos(i);
        const float rx = b.x() - px;
        const float rz = b.z() - pz;
        float along = 0.0f;
        float side = 0.0f;
        if (cdx != 0.0f) {
            along = rx * cdx;
            side = std::fabs(rz);
        } else {
            along = rz * cdz;
            side = std::fabs(rx);
        }
        if (along <= 0.05f || along > maxD || side > align) {
            continue;
        }
        if (along < bestAlong) {
            bestAlong = along;
            best = i;
        }
    }
    return best;
}

void StandaloneEngine::applySquash(const osg::Vec3& impact, int amount)
{
    m_dummyActor.applyDamage(amount);
    spawnFloatingText(
        osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
        "¡SQUASH!", osg::Vec4(1.0f, 0.45f, 0.10f, 1.0f), 0.16f);
    spawnDebris(impact, BOULDER_COLOR);
    spawnDebris(osg::Vec3(impact.x(), impact.y() + 0.15f, impact.z()), BOULDER_COLOR);
    std::cout << "[squash] player -" << amount << " HP=" << m_dummyActor.hp()
              << "/" << m_dummyActor.maxHp() << "\n";
    if (m_dummyActor.hp() <= 0) {
        std::cout << "[combat] Player DIED! Respawning...\n";
        m_survivalTime = 0.0f;
        m_dummyActor.restoreHp();
        m_dummyActor.teleport(4.0f, 5.0f, 4.0f);
        m_dummyActor.syncVisual();
    }
}

void StandaloneEngine::updateBoulders(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    const AABB dummyAabb = m_dummyActor.makeAabb();
    const int nEn = static_cast<int>(m_enemies.size());
    const int nBat = static_cast<int>(m_bats.size());
    const int nCrawl = static_cast<int>(m_crawlers.size());
    const int nCent = static_cast<int>(m_centipedes.size());
    const int nHit = nEn + nBat + nCrawl + nCent * 16;
    const int nB = m_boulderWorld.boulderCount();
    for (int i = 0; i < nB; ++i) {
        if (!m_boulderWorld.boulderAlive(i)) {
            continue;
        }
        if (m_boulderWorld.boulderResting(i) && !m_boulderWorld.hasSupport(i, m_miniVoxels)) {
            m_boulderWorld.beginFall(i, nHit);
            std::cout << "[digdug] boulder " << i << " falling\n";
        }

        const osg::Vec3 prev = m_boulderWorld.boulderPos(i);
        const BoulderState st = m_boulderWorld.boulderState(i);
        bool landed = false;
        std::vector<VoxelKey> wallHits;
        if (st == BOULDER_FALLING) {
            landed = m_boulderWorld.updateFalling(i, dt, m_miniVoxels);
        } else if (st == BOULDER_SLIDING) {
            landed = m_boulderWorld.updateSliding(i, dt, m_miniVoxels, wallHits);
        } else {
            m_boulderWorld.syncVisual(i);
        }

        AABB box = m_boulderWorld.makeAabb(i);
        const float hx = MINI_VOXEL_SIZE;
        if (prev.x() - hx < box.minX) {
            box.minX = prev.x() - hx;
        }
        if (prev.x() + hx > box.maxX) {
            box.maxX = prev.x() + hx;
        }
        if (prev.z() - hx < box.minZ) {
            box.minZ = prev.z() - hx;
        }
        if (prev.z() + hx > box.maxZ) {
            box.maxZ = prev.z() + hx;
        }

        const bool crushing = (st == BOULDER_FALLING);
        const bool sliding = (st == BOULDER_SLIDING);
        if (crushing && aabbOverlap(box, dummyAabb) && m_boulderWorld.consumePlayerSquash(i)) {
            applySquash(m_boulderWorld.boulderPos(i), 60);
        }
        if (crushing || sliding) {
            for (int e = 0; e < nEn; ++e) {
                LocalEnemy& enemy = m_enemies[static_cast<size_t>(e)];
                if (!enemy.isAlive) {
                    continue;
                }
                if (!aabbOverlap(box, enemy.makeAabb())) {
                    continue;
                }
                if (!m_boulderWorld.consumeEnemyHit(i, e)) {
                    continue;
                }
                const int dmg = crushing ? 60 : 100;
                enemy.takeDamage(dmg, m_boulderWorld.boulderPos(i));
                spawnFloatingText(
                    osg::Vec3(enemy.pos.x(), enemy.pos.y() + enemy.height() * 0.70f, enemy.pos.z()),
                    "¡SQUASH!", osg::Vec4(1.0f, 0.45f, 0.10f, 1.0f), 0.16f);
                spawnDebris(enemy.pos, BOULDER_COLOR);
                spawnDebris(osg::Vec3(enemy.pos.x(), enemy.pos.y() + 0.20f, enemy.pos.z()),
                            BOULDER_COLOR);
                std::cout << "[squash] enemy " << e << " -" << dmg << "\n";
                if (!enemy.isAlive) {
                    onEnemyKilled(enemy, e);
                }
            }
            for (int b = 0; b < nBat; ++b) {
                LocalFlyingBat& bat = m_bats[static_cast<size_t>(b)];
                if (!bat.isAlive()) {
                    continue;
                }
                if (!aabbOverlap(box, bat.makeAabb())) {
                    continue;
                }
                if (!m_boulderWorld.consumeEnemyHit(i, nEn + b)) {
                    continue;
                }
                const int dmg = crushing ? 60 : 100;
                bat.takeDamage(dmg, m_boulderWorld.boulderPos(i));
                spawnFloatingText(
                    osg::Vec3(bat.pos.x(), bat.pos.y() + 0.20f, bat.pos.z()),
                    "¡SQUASH!", osg::Vec4(1.0f, 0.45f, 0.10f, 1.0f), 0.16f);
                spawnDebris(bat.pos, BOULDER_COLOR);
                std::cout << "[squash] bat " << b << " -" << dmg << "\n";
                if (!bat.isAlive()) {
                    onBatKilled(bat, b);
                }
            }
            for (int c = 0; c < nCrawl; ++c) {
                LocalCrawler& cr = m_crawlers[static_cast<size_t>(c)];
                if (!cr.isAlive()) {
                    continue;
                }
                if (!aabbOverlap(box, cr.makeAabb())) {
                    continue;
                }
                if (!m_boulderWorld.consumeEnemyHit(i, nEn + nBat + c)) {
                    continue;
                }
                const int dmg = crushing ? 60 : 100;
                cr.takeDamage(dmg, m_boulderWorld.boulderPos(i));
                spawnFloatingText(
                    osg::Vec3(cr.pos.x(), cr.pos.y() + cr.height() * 0.80f, cr.pos.z()),
                    "¡SQUASH!", osg::Vec4(1.0f, 0.45f, 0.10f, 1.0f), 0.16f);
                spawnDebris(cr.pos, BOULDER_COLOR);
                std::cout << "[squash] crawler " << c << " -" << dmg << "\n";
                if (!cr.isAlive()) {
                    onCrawlerKilled(cr, c);
                }
            }
            for (int w = 0; w < nCent; ++w) {
                LocalCentipede& worm = m_centipedes[static_cast<size_t>(w)];
                if (!worm.isAlive()) {
                    continue;
                }
                for (int s = 0; s < worm.segmentCount(); ++s) {
                    if (!aabbOverlap(box, worm.makeAabb(s))) {
                        continue;
                    }
                    const int hitId = nEn + nBat + nCrawl + w * 16 + s;
                    if (!m_boulderWorld.consumeEnemyHit(i, hitId)) {
                        continue;
                    }
                    const int dmg = crushing ? 60 : 100;
                    spawnFloatingText(worm.segment(s).pos, "¡SQUASH!",
                                      osg::Vec4(1.0f, 0.45f, 0.10f, 1.0f), 0.16f);
                    spawnDebris(worm.segment(s).pos, BOULDER_COLOR);
                    std::cout << "[squash] centipede " << w << " seg " << s << " -" << dmg << "\n";
                    applyCentipedeDamage(w, s, dmg);
                    break;
                }
            }
        }

        if (!wallHits.empty()) {
            bool smashed = false;
            for (size_t h = 0; h < wallHits.size(); ++h) {
                if (removeMiniVoxel(wallHits[h].vx, wallHits[h].vy, wallHits[h].vz)) {
                    smashed = true;
                }
            }
            if (smashed) {
                m_localMesher.rebuildMesh();
            }
            m_camShakeTtl = 0.12f;
        }
        if (landed && st == BOULDER_FALLING) {
            m_camShakeTtl = 0.18f;
            spawnDebris(m_boulderWorld.boulderPos(i), BOULDER_COLOR);
        }
    }
}

void StandaloneEngine::updateThrownBlocks(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    size_t i = 0;
    while (i < m_thrown.size()) {
        ThrownBlock& shot = m_thrown[i];
        if (!shot.alive) {
            m_thrown.erase(m_thrown.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        const osg::Vec3 prev = shot.pos;
        if (shot.target >= 0 &&
            shot.target < static_cast<int>(m_enemies.size()) &&
            m_enemies[static_cast<size_t>(shot.target)].isAlive) {
            const LocalEnemy& en = m_enemies[static_cast<size_t>(shot.target)];
            osg::Vec3 aim(
                en.pos.x() - shot.pos.x(),
                (en.pos.y() + en.height() * 0.55f) - shot.pos.y(),
                en.pos.z() - shot.pos.z());
            const float len = aim.length();
            if (len > 0.0001f) {
                const osg::Vec3 des = aim * (24.0f / len);
                shot.vel.x() += (des.x() - shot.vel.x()) * 8.0f * dt;
                shot.vel.z() += (des.z() - shot.vel.z()) * 8.0f * dt;
                shot.vel.y() += (des.y() + 0.35f - shot.vel.y()) * 3.0f * dt;
            }
        } else {
            shot.vel.y() -= 9.0f * dt;
        }
        shot.pos = shot.pos + shot.vel * dt;
        shot.ttl -= dt;

        bool dead = shot.ttl <= 0.0f || shot.pos.y() < 0.0f;
        if (!dead) {
            const float hitR = CELL_SIZE * 0.65f;
            const float hitR2 = hitR * hitR;
            for (size_t e = 0; e < m_enemies.size(); ++e) {
                LocalEnemy& enemy = m_enemies[e];
                if (!enemy.isAlive) {
                    continue;
                }
                const float cx = enemy.pos.x();
                const float cy = enemy.pos.y() + enemy.height() * 0.5f;
                const float cz = enemy.pos.z();
                const float dx = shot.pos.x() - cx;
                const float dy = shot.pos.y() - cy;
                const float dz = shot.pos.z() - cz;
                if (dx * dx + dy * dy + dz * dz <= hitR2) {
                    {
                        char buf[16];
                        std::snprintf(buf, sizeof(buf), "-%d", 45);
                        spawnFloatingText(
                            osg::Vec3(enemy.pos.x(), enemy.pos.y() + enemy.height() * 0.65f, enemy.pos.z()),
                            buf, kFloatDmgEnemy);
                    }
                    enemy.takeDamage(45, shot.pos);
                    if (!enemy.isAlive) {
                        onEnemyKilled(enemy, static_cast<int>(e));
                    }
                    spawnCollapse(shot.pos);
                    dead = true;
                    std::cout << "[throw] hit enemy dmg=45\n";
                    break;
                }
            }
        }
        if (!dead) {
            const float hitR = CELL_SIZE * 0.65f;
            const float hitR2 = hitR * hitR;
            for (size_t b = 0; b < m_bats.size(); ++b) {
                LocalFlyingBat& bat = m_bats[b];
                if (!bat.isAlive()) {
                    continue;
                }
                const float dx = shot.pos.x() - bat.pos.x();
                const float dy = shot.pos.y() - bat.pos.y();
                const float dz = shot.pos.z() - bat.pos.z();
                if (dx * dx + dy * dy + dz * dz > hitR2) {
                    continue;
                }
                {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "-%d", 45);
                    spawnFloatingText(
                        osg::Vec3(bat.pos.x(), bat.pos.y() + 0.25f, bat.pos.z()),
                        buf, kFloatDmgEnemy);
                }
                bat.takeDamage(45, shot.pos);
                if (!bat.isAlive()) {
                    onBatKilled(bat, static_cast<int>(b));
                }
                spawnCollapse(shot.pos);
                dead = true;
                std::cout << "[throw] hit bat dmg=45\n";
                break;
            }
        }
        if (!dead) {
            const float hitR = CELL_SIZE * 0.65f;
            const float hitR2 = hitR * hitR;
            for (size_t c = 0; c < m_crawlers.size(); ++c) {
                LocalCrawler& cr = m_crawlers[c];
                if (!cr.isAlive()) {
                    continue;
                }
                const float cx = cr.pos.x();
                const float cy = cr.pos.y() + cr.height() * 0.5f;
                const float cz = cr.pos.z();
                const float dx = shot.pos.x() - cx;
                const float dy = shot.pos.y() - cy;
                const float dz = shot.pos.z() - cz;
                if (dx * dx + dy * dy + dz * dz > hitR2) {
                    continue;
                }
                {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "-%d", 45);
                    spawnFloatingText(
                        osg::Vec3(cr.pos.x(), cr.pos.y() + cr.height() * 0.80f, cr.pos.z()),
                        buf, kFloatDmgEnemy);
                }
                cr.takeDamage(45, shot.pos);
                if (!cr.isAlive()) {
                    onCrawlerKilled(cr, static_cast<int>(c));
                }
                spawnCollapse(shot.pos);
                dead = true;
                std::cout << "[throw] hit crawler dmg=45\n";
                break;
            }
        }
        if (!dead) {
            const float hitR = CELL_SIZE * 0.65f;
            const float hitR2 = hitR * hitR;
            for (size_t w = 0; w < m_centipedes.size(); ++w) {
                LocalCentipede& worm = m_centipedes[w];
                if (!worm.isAlive()) {
                    continue;
                }
                bool hitSeg = false;
                for (int s = 0; s < worm.segmentCount(); ++s) {
                    const osg::Vec3 sp = worm.segment(s).pos;
                    const float dx = shot.pos.x() - sp.x();
                    const float dy = shot.pos.y() - sp.y();
                    const float dz = shot.pos.z() - sp.z();
                    if (dx * dx + dy * dy + dz * dz > hitR2) {
                        continue;
                    }
                    {
                        char buf[16];
                        std::snprintf(buf, sizeof(buf), "-%d", 45);
                        spawnFloatingText(sp, buf, kFloatDmgEnemy);
                    }
                    applyCentipedeDamage(static_cast<int>(w), s, 45);
                    spawnCollapse(shot.pos);
                    dead = true;
                    hitSeg = true;
                    std::cout << "[throw] hit centipede dmg=45\n";
                    break;
                }
                if (hitSeg) {
                    break;
                }
            }
        }
        if (!dead) {
            int vx = 0;
            int vy = 0;
            int vz = 0;
            if (firstSolidOnSegment(m_miniVoxels, prev, shot.pos, &vx, &vy, &vz)) {
                if (removeMiniVoxel(vx, vy, vz)) {
                    m_localMesher.rebuildMesh();
                }
                spawnCollapse(shot.pos);
                dead = true;
                std::cout << "[throw] hit voxel\n";
            }
        }
        if (dead) {
            if (m_worldRoot.valid() && shot.node.valid()) {
                m_worldRoot->removeChild(shot.node.get());
            }
            m_thrown.erase(m_thrown.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (shot.node.valid()) {
            shot.node->setMatrix(osg::Matrix::translate(shot.pos));
        }
        i += 1;
    }
}

void StandaloneEngine::spawnDebris(const osg::Vec3& pos, const osg::Vec4& color)
{
    // 60.2 / 60.3 Burst 4-6 cubos. Velocidad hacia afuera y arriba.
    m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
    const int count = 4 + static_cast<int>(m_spawnSeed % 3u);
    for (int n = 0; n < count; ++n) {
        m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
        const float ang = static_cast<float>(m_spawnSeed % 6283u) * 0.001f;
        m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
        const float spread = 1.4f + static_cast<float>(m_spawnSeed % 1800u) * 0.001f;
        m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
        const float up = 2.4f + static_cast<float>(m_spawnSeed % 2200u) * 0.001f;
        m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
        const float life = 0.35f + static_cast<float>(m_spawnSeed % 350u) * 0.001f;
        const osg::Vec3 vel(std::sin(ang) * spread, up, std::cos(ang) * spread);

        int slot = -1;
        for (size_t i = 0; i < m_debris.size(); ++i) {
            if (!m_debris[i].isActive) {
                slot = static_cast<int>(i);
                break;
            }
        }

        if (slot >= 0) {
            m_debris[static_cast<size_t>(slot)].activate(pos, vel, color, life);
            if (m_debrisRoot.valid() && m_debris[static_cast<size_t>(slot)].getNode() != nullptr &&
                m_debris[static_cast<size_t>(slot)].getNode()->getNumParents() == 0) {
                m_debrisRoot->addChild(m_debris[static_cast<size_t>(slot)].getNode());
            }
        } else {
            m_debris.push_back(LocalDebris(pos, vel, color, life));
            if (m_debrisRoot.valid() && m_debris.back().getNode() != nullptr) {
                m_debrisRoot->addChild(m_debris.back().getNode());
            }
        }
    }
}

void StandaloneEngine::updateDebris(float deltaTime)
{
    // 61.1 Gravedad Y, TTL, quitar nodo al morir.
    if (deltaTime <= 0.0f) {
        return;
    }
    for (size_t i = 0; i < m_debris.size(); ++i) {
        LocalDebris& bit = m_debris[i];
        if (!bit.isActive) {
            continue;
        }
        bit.vel.y() -= 9.8f * deltaTime;
        bit.pos = bit.pos + bit.vel * deltaTime;
        if (bit.pos.y() < 0.0f) {
            bit.pos.y() = 0.0f;
            bit.vel.y() = 0.0f;
        }
        bit.ttl -= deltaTime;
        if (bit.ttl <= 0.0f) {
            bit.deactivate();
            if (m_debrisRoot.valid() && bit.getNode() != nullptr) {
                m_debrisRoot->removeChild(bit.getNode());
            }
            continue;
        }
        bit.syncVisual();
    }
}

void StandaloneEngine::spawnFloatingText(const osg::Vec3& pos, const std::string& msg,
                                         const osg::Vec4& color, float scale)
{
    // 65.2 Sube 0.8 u/s con jitter XZ.
    m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
    const float jx = (static_cast<float>(m_spawnSeed % 800u) - 400.0f) * 0.001f;
    m_spawnSeed = m_spawnSeed * 1103515245u + 12345u;
    const float jz = (static_cast<float>(m_spawnSeed % 800u) - 400.0f) * 0.001f;
    const osg::Vec3 vel(jx, 0.8f, jz);
    const float life = 0.80f;

    int slot = -1;
    for (size_t i = 0; i < m_floatTexts.size(); ++i) {
        if (!m_floatTexts[i].isActive) {
            slot = static_cast<int>(i);
            break;
        }
    }

    if (slot >= 0) {
        m_floatTexts[static_cast<size_t>(slot)].activate(pos, vel, msg, color, scale, life);
        if (m_floatTextRoot.valid() && m_floatTexts[static_cast<size_t>(slot)].getNode() != nullptr &&
            m_floatTexts[static_cast<size_t>(slot)].getNode()->getNumParents() == 0) {
            m_floatTextRoot->addChild(m_floatTexts[static_cast<size_t>(slot)].getNode());
        }
        return;
    }

    LocalFloatingText popup;
    popup.activate(pos, vel, msg, color, scale, life);
    m_floatTexts.push_back(popup);
    if (m_floatTextRoot.valid() && m_floatTexts.back().getNode() != nullptr) {
        m_floatTextRoot->addChild(m_floatTexts.back().getNode());
    }
}

void StandaloneEngine::updateFloatingText(float deltaTime)
{
    // 64.1 / 64.2 Sube, fade alpha = ttl/maxTtl, limpia al expirar.
    if (deltaTime <= 0.0f) {
        return;
    }
    for (size_t i = 0; i < m_floatTexts.size(); ++i) {
        LocalFloatingText& ft = m_floatTexts[i];
        if (!ft.isActive) {
            continue;
        }
        ft.pos = ft.pos + ft.vel * deltaTime;
        ft.ttl -= deltaTime;
        if (ft.ttl <= 0.0f) {
            ft.deactivate();
            if (m_floatTextRoot.valid() && ft.getNode() != nullptr) {
                m_floatTextRoot->removeChild(ft.getNode());
            }
            continue;
        }
        float alpha = 1.0f;
        if (ft.maxTtl > 0.0001f) {
            alpha = ft.ttl / ft.maxTtl;
        }
        ft.applyAlpha(alpha);
        ft.syncVisual();
    }
}

void StandaloneEngine::placeBox()
{
    // 82.1 SHIFT+1: slot del ghost preview.
    m_boxWorld.placeBox(
        osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()),
        m_dummyActor.yaw());
}

void StandaloneEngine::tryPushBox()
{
    // 82.1 SHIFT+2: hielo hasta 5 celdas en eje cardinal.
    if (m_boxWorld.tryPush(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()),
            m_dummyActor.yaw())) {
        m_boxWorld.settleBoxes();
    }
}

void StandaloneEngine::tryCollapse()
{
    // 82.1 SHIFT+3: destruye cubo seleccionado o el frontal.
    int idx = -1;
    if (m_boxWorld.boxAlive(m_selectedBox)) {
        idx = m_selectedBox;
    } else {
        idx = m_boxWorld.frontIndex(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()),
            m_dummyActor.yaw());
    }
    osg::Vec3 p;
    if (idx < 0 || !m_boxWorld.destroyBox(idx, &p)) {
        return;
    }
    spawnCollapse(p);
    if (idx == m_selectedBox) {
        clearSelection();
    }
    std::cout << "[box] collapse idx=" << idx << "\n";
}

void StandaloneEngine::triggerBomb3x3()
{
    detonateBomb();
}

void StandaloneEngine::hasteBurst()
{
    if (m_dummyActor.activateDash()) {
        std::cout << "[dash] haste burst SP=" << m_dummyActor.stamina() << "\n";
    }
}

void StandaloneEngine::cycleCamera()
{
    // 89.1 C: Brazo mecanico / Orbita libre.
    m_camRig.cycle();
    m_cameraInitialized = false;
    std::cout << "[cam] " << m_camRig.modeName() << "\n";
}

bool StandaloneEngine::isOrbitCamera() const
{
    return m_camRig.isOrbit();
}

void StandaloneEngine::applyOrbitMouse(float deltaX, float deltaY)
{
    m_camRig.nudgeOrbitFromMouse(deltaX, deltaY);
}

bool StandaloneEngine::isSelectMode() const
{
    return !m_editorMode && m_toolMode == TOOL_SELECT;
}

void StandaloneEngine::clearSelection()
{
    m_toolMode = TOOL_BUILD;
    m_selectedBox = -1;
    m_selectedBoulder = -1;
    m_selectCursor = 0;
    m_spaceHold = 0.0f;
    if (m_selectHilite.valid()) {
        m_selectHilite->setNodeMask(0);
    }
    m_buddy.setHidden(false);
}

void StandaloneEngine::spawnCollapse(const osg::Vec3& pos)
{
    spawnDebris(pos, osg::Vec4(0.55f, 0.85f, 1.0f, 1.0f));
}

void StandaloneEngine::buildSelectHilite()
{
    // 74.3 Marco alambre. osg::Box = arista total CELL_SIZE.
    osg::ref_ptr<osg::Box> shape = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), CELL_SIZE, CELL_SIZE, CELL_SIZE);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(shape.get());
    draw->setColor(osg::Vec4(1.0f, 0.90f, 0.15f, 1.0f));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK,
                                                     osg::PolygonMode::LINE),
                                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    state->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    state->setMode(GL_BLEND, osg::StateAttribute::OFF);
    m_selectHilite = new osg::MatrixTransform;
    m_selectHilite->addChild(geode.get());
    m_selectHilite->setNodeMask(0);
}

void StandaloneEngine::buildBombRing()
{
    osg::ref_ptr<osg::Cylinder> cyl =
        new osg::Cylinder(osg::Vec3(0.0f, 0.0f, 0.0f), CELL_SIZE * 1.70f, 0.04f);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(cyl.get());
    draw->setColor(osg::Vec4(1.0f, 0.824f, 0.290f, 0.85f));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());
    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(1.0f, 0.824f, 0.290f, 0.85f));
    mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.38f, 0.06f, 0.85f));
    mat->setAlpha(osg::Material::FRONT_AND_BACK, 0.85f);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                osg::StateAttribute::ON);
    osg::ref_ptr<osg::MatrixTransform> spin = new osg::MatrixTransform;
    spin->setMatrix(osg::Matrix::rotate(1.5707963, osg::Vec3(1.0f, 0.0f, 0.0f)));
    spin->addChild(geode.get());
    m_bombRing = new osg::MatrixTransform;
    m_bombRing->addChild(spin.get());
    m_bombRing->setNodeMask(0);
}

void StandaloneEngine::buildGhostPreview()
{
    // 79.1 Ghost verde translucido, sin depth-write.
    osg::ref_ptr<osg::Box> shape = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), CELL_SIZE, CELL_SIZE, CELL_SIZE);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(shape.get());
    draw->setColor(osg::Vec4(0.24f, 1.0f, 0.6f, 0.30f));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());
    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.24f, 1.0f, 0.6f, 0.30f));
    mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.08f, 0.35f, 0.18f, 0.30f));
    mat->setAlpha(osg::Material::FRONT_AND_BACK, 0.30f);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                                osg::StateAttribute::ON);
    m_previewBox = new osg::MatrixTransform;
    m_previewBox->addChild(geode.get());
    m_previewBox->setNodeMask(0);
}

void StandaloneEngine::updateGhostPreview()
{
    if (!m_previewBox.valid()) {
        return;
    }
    if (m_editorMode) {
        m_previewBox->setNodeMask(0);
        return;
    }
    osg::Vec3 slot;
    const bool ok = m_boxWorld.queryPlaceSlot(
        osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()),
        m_dummyActor.yaw(),
        &slot);
    if (!ok) {
        m_previewBox->setNodeMask(0);
        return;
    }
    m_previewBox->setMatrix(osg::Matrix::translate(slot));
    m_previewBox->setNodeMask(0xffffffff);
}

void StandaloneEngine::buildPipMirror()
{
    // 88.1 PIP Fary: 260x180, margen 16, esquina superior derecha.
    const int pipW = 260;
    const int pipH = 180;
    const int margin = 16;
    const int pipX = m_width - margin - pipW;
    const int pipY = m_height - margin - pipH;
    const double aspect = static_cast<double>(pipW) / static_cast<double>(pipH);

    m_pipFaryCamera = new osg::Camera;
    if (m_graphicsContext.valid()) {
        m_pipFaryCamera->setGraphicsContext(m_graphicsContext.get());
    }
    m_pipFaryCamera->setViewport(pipX, pipY, pipW, pipH);
    m_pipFaryCamera->setRenderOrder(osg::Camera::POST_RENDER, 0);
    m_pipFaryCamera->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    m_pipFaryCamera->setClearColor(osg::Vec4(0.10f, 0.06f, 0.04f, 1.0f));
    m_pipFaryCamera->setAllowEventFocus(false);
    m_pipFaryCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_pipFaryCamera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
    m_pipFaryCamera->setProjectionMatrixAsPerspective(70.0, aspect, 0.15, 64.0);
    m_pipFaryCamera->setViewMatrix(osg::Matrix::identity());
    if (m_worldRoot.valid()) {
        m_pipFaryCamera->addChild(m_worldRoot.get());
    }
    m_pipFaryCamera->setNodeMask(0);
    m_root->addChild(m_pipFaryCamera.get());

    // 88.3 Marco alámbrico cyan / naranja alrededor del espejo.
    m_pipFrame = new osg::Camera;
    if (m_graphicsContext.valid()) {
        m_pipFrame->setGraphicsContext(m_graphicsContext.get());
    }
    m_pipFrame->setViewport(0, 0, m_width, m_height);
    m_pipFrame->setClearMask(0);
    m_pipFrame->setRenderOrder(osg::Camera::POST_RENDER, 50);
    m_pipFrame->setAllowEventFocus(false);
    m_pipFrame->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_pipFrame->setProjectionMatrix(osg::Matrix::ortho2D(0.0, m_width, 0.0, m_height));
    m_pipFrame->setViewMatrix(osg::Matrix::identity());
    m_pipFrame->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);

    osg::StateSet* fs = m_pipFrame->getOrCreateStateSet();
    fs->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    fs->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    fs->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);

    auto makeLoop = [](float x0, float y0, float x1, float y1, osg::Vec4Array* colors) {
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(x0, y0, 0.0f));
        verts->push_back(osg::Vec3(x1, y0, 0.0f));
        verts->push_back(osg::Vec3(x1, y1, 0.0f));
        verts->push_back(osg::Vec3(x0, y1, 0.0f));
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setVertexArray(verts.get());
        geom->setColorArray(colors, osg::Array::BIND_OVERALL);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_LINE_LOOP, 0, 4));
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        geom->setDataVariance(osg::Object::DYNAMIC);
        return geom;
    };

    const float fx0 = static_cast<float>(pipX);
    const float fy0 = static_cast<float>(pipY);
    const float fx1 = static_cast<float>(pipX + pipW);
    const float fy1 = static_cast<float>(pipY + pipH);
    m_pipOuterColor = new osg::Vec4Array;
    m_pipOuterColor->push_back(osg::Vec4(1.00f, 0.42f, 0.12f, 1.0f));
    m_pipInnerColor = new osg::Vec4Array;
    m_pipInnerColor->push_back(osg::Vec4(0.25f, 0.95f, 1.00f, 1.0f));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(makeLoop(fx0 - 2.0f, fy0 - 2.0f, fx1 + 2.0f, fy1 + 2.0f,
                                m_pipOuterColor.get()).get());
    geode->addDrawable(makeLoop(fx0, fy0, fx1, fy1, m_pipInnerColor.get()).get());
    m_pipFrame->addChild(geode.get());
    m_pipFrame->setNodeMask(0);
    m_root->addChild(m_pipFrame.get());
}

void StandaloneEngine::updateBuddyCamera()
{
    if (!m_pipFaryCamera.valid()) {
        return;
    }

    bool pipAlert = false;
    if (!m_editorMode) {
        pipAlert = m_threatBehind;
        if (!pipAlert) {
            const float yaw = m_dummyActor.yaw();
            const float fxx = std::sin(yaw);
            const float fzz = std::cos(yaw);
            const float missR = 8.0f * TILE_SIZE;
            const float missR2 = missR * missR;
            for (size_t p = 0; p < m_projectiles.size(); ++p) {
                if (!m_projectiles[p].isHostile() || !m_projectiles[p].alive()) {
                    continue;
                }
                const float dx = m_projectiles[p].m_pos.x() - m_dummyActor.x();
                const float dz = m_projectiles[p].m_pos.z() - m_dummyActor.z();
                const float d2 = dx * dx + dz * dz;
                if (d2 > missR2) {
                    continue;
                }
                if (fxx * dx + fzz * dz < 0.0f) {
                    pipAlert = true;
                    break;
                }
            }
        }
    }

    const unsigned int vis = pipAlert ? 0xffffffffu : 0u;
    m_pipFaryCamera->setNodeMask(vis);
    if (m_pipFrame.valid()) {
        m_pipFrame->setNodeMask(vis);
    }
    if (!pipAlert) {
        return;
    }

    if (m_pipOuterColor.valid() && !m_pipOuterColor->empty() &&
        m_pipInnerColor.valid() && !m_pipInnerColor->empty()) {
        const float pulse = 0.40f + 0.60f * (0.5f + 0.5f * std::sin(m_survivalTime * 14.0f));
        (*m_pipOuterColor)[0].set(1.00f * pulse, 0.22f * pulse, 0.08f, 1.0f);
        (*m_pipInnerColor)[0].set(1.00f * pulse, 0.55f * pulse, 0.12f, 1.0f);
        m_pipOuterColor->dirty();
        m_pipInnerColor->dirty();
    }

    const osg::Vec3 buddyPos = m_buddy.position();
    const osg::Vec3 eye(buddyPos.x(), buddyPos.y() + 0.45f, buddyPos.z());
    const float yaw = m_dummyActor.yaw();
    const osg::Vec3 facing(std::sin(yaw), 0.0f, std::cos(yaw));
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());

    osg::Vec3 lookAt = playerPos - facing * 4.0f;
    lookAt.y() = playerPos.y() + m_dummyActor.height() * 0.55f;

    const float alertR = 6.0f * TILE_SIZE;
    const float alertR2 = alertR * alertR;
    int bestBehind = -1;
    int bestNear = -1;
    float bestBehindD = 1.0e30f;
    float bestNearD = 1.0e30f;
    for (size_t e = 0; e < m_enemies.size(); ++e) {
        if (!m_enemies[e].isAlive) {
            continue;
        }
        const float dx = m_enemies[e].pos.x() - playerPos.x();
        const float dz = m_enemies[e].pos.z() - playerPos.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > alertR2) {
            continue;
        }
        if (d2 < bestNearD) {
            bestNearD = d2;
            bestNear = static_cast<int>(e);
        }
        const float toDot = facing.x() * dx + facing.z() * dz;
        if (toDot < 0.0f && d2 < bestBehindD) {
            bestBehindD = d2;
            bestBehind = static_cast<int>(e);
        }
    }
    const int pick = (bestBehind >= 0) ? bestBehind : bestNear;
    if (pick >= 0) {
        const LocalEnemy& en = m_enemies[static_cast<size_t>(pick)];
        lookAt = en.pos;
        lookAt.y() += en.height() * 0.55f;
    }
    for (size_t b = 0; b < m_bats.size(); ++b) {
        LocalFlyingBat& bat = m_bats[b];
        if (!bat.isAlive()) {
            continue;
        }
        if (bat.isDiveStrike()) {
            lookAt = bat.pos;
            break;
        }
        const float dx = bat.pos.x() - playerPos.x();
        const float dz = bat.pos.z() - playerPos.z();
        const float d2 = dx * dx + dz * dz;
        if (d2 > alertR2) {
            continue;
        }
        if (facing.x() * dx + facing.z() * dz < 0.0f) {
            lookAt = bat.pos;
        }
    }

    osg::Vec3 dir = lookAt - eye;
    if (dir.length2() < 1.0e-6f) {
        lookAt = eye - facing;
    }
    m_pipFaryCamera->setViewMatrix(
        osg::Matrix::lookAt(eye, lookAt, osg::Vec3(0.0f, 1.0f, 0.0f)));
}

void StandaloneEngine::detonateBomb()
{
    // 78.1 Tecla 4: detona 3x3 en cubo lock o el mas cercano.
    if (m_editorMode) {
        return;
    }
    int idx = -1;
    if (m_boxWorld.boxAlive(m_selectedBox)) {
        idx = m_selectedBox;
    } else {
        idx = m_boxWorld.nearestIndex(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z()));
    }
    int ix = 0;
    int iy = 0;
    int iz = 0;
    if (idx < 0 || !m_boxWorld.boxCell(idx, &ix, &iy, &iz)) {
        return;
    }
    std::vector<osg::Vec3> hits;
    const int n = m_boxWorld.explodeBombAt(ix, iy, iz, hits);
    for (size_t i = 0; i < hits.size(); ++i) {
        spawnCollapse(hits[i]);
    }
    m_bombPos = hits.empty() ? m_boxWorld.boxPos(idx) : hits[0];
    m_bombRingTtl = 4.0f;
    m_bombCharging = false;
    clearSelection();
    m_buddy.setHidden(false);
    m_buddy.startBurst(4.0f);
    std::cout << "[bomb] 3x3 n=" << n << "\n";
}

void StandaloneEngine::cycleToolTab()
{
    cycleBoxTarget();
}

void StandaloneEngine::fireSelectShot()
{
    // 75.1 Tap F/CTRL/Espacio<0.4s: linea cardinal max 3 + Burst 4s.
    if (m_editorMode || m_toolMode != TOOL_SELECT || m_bombCharging) {
        return;
    }
    if (!m_boxWorld.boxAlive(m_selectedBox)) {
        clearSelection();
        return;
    }
    const osg::Vec3 playerPos(m_dummyActor.x(), m_dummyActor.y(), m_dummyActor.z());
    std::vector<osg::Vec3> hits;
    const int n = m_boxWorld.destroyLineFrom(m_selectedBox, playerPos, 3, hits);
    for (size_t i = 0; i < hits.size(); ++i) {
        spawnCollapse(hits[i]);
    }
    clearSelection();
    m_buddy.setHidden(false);
    m_buddy.startBurst(4.0f);
    std::cout << "[shot] line n=" << n << " burst\n";
}

void StandaloneEngine::updateSelectTools(float deltaTime)
{
    m_selectPulse += deltaTime;

    if (m_bombRing.valid()) {
        if (m_bombRingTtl > 0.0f) {
            m_bombRingTtl -= deltaTime;
            const float spin = m_selectPulse * 4.0f;
            const float fade = (m_bombRingTtl > 0.0f) ? (m_bombRingTtl / 4.0f) : 0.0f;
            osg::Matrix rot = osg::Matrix::rotate(spin, osg::Vec3(0.0f, 1.0f, 0.0f));
            osg::Matrix tr = osg::Matrix::translate(m_bombPos);
            const float s = 1.0f + (1.0f - fade) * 0.35f;
            m_bombRing->setMatrix(osg::Matrix::scale(s, 1.0f, s) * rot * tr);
            m_bombRing->setNodeMask((m_bombRingTtl > 0.0f) ? 0xffffffff : 0);
            osg::StateSet* st = m_bombRing->getOrCreateStateSet();
            osg::Material* mat = dynamic_cast<osg::Material*>(
                st->getAttribute(osg::StateAttribute::MATERIAL));
            if (mat != nullptr) {
                const osg::Vec4 col(1.0f, 0.824f, 0.290f, 0.85f * fade);
                mat->setDiffuse(osg::Material::FRONT_AND_BACK, col);
                mat->setAlpha(osg::Material::FRONT_AND_BACK, col.a());
            }
        } else {
            m_bombRing->setNodeMask(0);
        }
    }

    if (m_bombCharging && m_buddy.consumeOverloadDone()) {
        // 76.2 Detona 3x3 y deja aro residual 4s.
        std::vector<osg::Vec3> hits;
        const int n = m_boxWorld.explodeBombAt(m_bombIx, m_bombIy, m_bombIz, hits);
        for (size_t i = 0; i < hits.size(); ++i) {
            spawnCollapse(hits[i]);
        }
        m_bombPos = (hits.empty() ? m_bombPos : hits[0]);
        m_bombRingTtl = 4.0f;
        m_bombCharging = false;
        clearSelection();
        std::cout << "[bomb] 3x3 n=" << n << "\n";
    }

    if (m_editorMode || m_toolMode != TOOL_SELECT) {
        if (m_selectHilite.valid()) {
            m_selectHilite->setNodeMask(0);
        }
        if (!m_bombCharging) {
            m_buddy.setHidden(false);
        }
        return;
    }

    osg::Vec3 p;
    float baseEdge = CELL_SIZE;
    if (m_boulderWorld.boulderAlive(m_selectedBoulder)) {
        p = m_boulderWorld.boulderPos(m_selectedBoulder);
        baseEdge = 2.0f * MINI_VOXEL_SIZE;
    } else if (m_boxWorld.boxAlive(m_selectedBox)) {
        p = m_boxWorld.boxPos(m_selectedBox);
    } else {
        clearSelection();
        return;
    }

    const float pulse = m_selectPulse;
    const float s = baseEdge * (1.05f + 0.08f * std::sin(pulse * 9.0f));
    osg::Matrix sc = osg::Matrix::scale(s / CELL_SIZE, s / CELL_SIZE, s / CELL_SIZE);
    osg::Matrix rot = osg::Matrix::rotate(pulse * 1.15f, osg::Vec3(0.0f, 1.0f, 0.0f));
    osg::Matrix tr = osg::Matrix::translate(p);
    m_selectHilite->setMatrix(sc * rot * tr);
    bool show = true;
    if (m_bombCharging) {
        const float hz = std::fmod(pulse * 18.0f, 1.0f);
        show = (hz < 0.5f);
    }
    m_selectHilite->setNodeMask(show ? 0xffffffff : 0);
    m_buddy.setHidden(false);
}

void StandaloneEngine::fireProjectile()
{
    // 121. Tecla 2: auto-aim 3D si hay TAB; si no, frente Dummy. 30 u/s.
    if (m_editorMode) {
        return;
    }
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const osg::Vec3 pos(
        m_dummyActor.x() + fx * 0.28f,
        m_dummyActor.y() + m_dummyActor.height() * 0.70f,
        m_dummyActor.z() + fz * 0.28f);
    osg::Vec3 vel(fx * m_content.gunSpeed(), 0.0f, fz * m_content.gunSpeed());
    osg::Vec3 aim;
    if (lockedTargetCenter(&aim)) {
        osg::Vec3 dir = aim - pos;
        const float len = dir.length();
        if (len > 0.0001f) {
            vel = dir * (m_content.gunSpeed() / len);
        }
    }
    LocalProjectile shot(pos, vel, 2.00f);
    if (m_projectileRoot.valid() && shot.getNode() != nullptr) {
        m_projectileRoot->addChild(shot.getNode());
    }
    m_projectiles.push_back(shot);
    fireContentHook("onFire", "gun");
}

void StandaloneEngine::fireHunterLaser()
{
    // 122. Tecla 5: hitscan Hunter. Sin celdas = popup + bala normal.
    if (m_editorMode) {
        return;
    }
    if (m_hunterCells <= 0) {
        spawnFloatingText(
            osg::Vec3(m_dummyActor.x(), m_dummyActor.y() + m_dummyActor.height(), m_dummyActor.z()),
            "SIN ENERGIA", kFloatHunter, 0.16f);
        std::cout << "[hunter] SIN ENERGIA\n";
        fireProjectile();
        return;
    }
    m_hunterCells -= 1;
    fireContentHook("onHunter", hasCombatLock() ? "lock" : "frontal");
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const osg::Vec3 muzzle(
        m_dummyActor.x() + fx * 0.32f,
        m_dummyActor.y() + m_dummyActor.height() * 0.72f,
        m_dummyActor.z() + fz * 0.32f);
    if (hasCombatLock()) {
        fireHunterBeam();
        return;
    }
    osg::Vec3 hit = muzzle + osg::Vec3(fx, 0.0f, fz) * m_content.hunterRange();
    applyHunterFrontal(muzzle, osg::Vec3(fx, 0.0f, fz), &hit);
    spawnHunterBeam(muzzle, hit);
}

void StandaloneEngine::applyHunterDamageLocked(const osg::Vec3& muzzle)
{
    if (m_lockedEnemyIndex >= 0 &&
        m_lockedEnemyIndex < static_cast<int>(m_enemies.size()) &&
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].isAlive) {
        LocalEnemy& en = m_enemies[static_cast<size_t>(m_lockedEnemyIndex)];
        en.takeDamage(m_content.hunterDamage(), muzzle);
        std::cout << "[hunter] enemy -" << m_content.hunterDamage() << " HP=" << en.hp() << "\n";
        if (!en.isAlive) {
            onEnemyKilled(en, m_lockedEnemyIndex);
        }
        return;
    }
    if (m_lockedBatIndex >= 0 &&
        m_lockedBatIndex < static_cast<int>(m_bats.size()) &&
        m_bats[static_cast<size_t>(m_lockedBatIndex)].isAlive()) {
        LocalFlyingBat& bat = m_bats[static_cast<size_t>(m_lockedBatIndex)];
        bat.takeDamage(m_content.hunterDamage(), muzzle);
        std::cout << "[hunter] bat -" << m_content.hunterDamage() << " HP=" << bat.hp() << "\n";
        if (!bat.isAlive()) {
            onBatKilled(bat, m_lockedBatIndex);
        }
        return;
    }
    if (m_lockedCrawlerIndex >= 0 &&
        m_lockedCrawlerIndex < static_cast<int>(m_crawlers.size()) &&
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].isAlive()) {
        LocalCrawler& cr = m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)];
        cr.takeDamage(m_content.hunterDamage(), muzzle);
        std::cout << "[hunter] crawler -" << m_content.hunterDamage() << " HP=" << cr.hp() << "\n";
        if (!cr.isAlive()) {
            onCrawlerKilled(cr, m_lockedCrawlerIndex);
        }
        return;
    }
    if (m_lockedCentipedeIndex >= 0 &&
        m_lockedCentipedeIndex < static_cast<int>(m_centipedes.size()) &&
        m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].isAlive()) {
        std::cout << "[hunter] centipede -" << m_content.hunterDamage() << "\n";
        applyCentipedeDamage(m_lockedCentipedeIndex, m_lockedCentipedeSeg, m_content.hunterDamage());
    }
}

bool StandaloneEngine::applyHunterFrontal(const osg::Vec3& muzzle, const osg::Vec3& facing,
                                         osg::Vec3* outHit)
{
    osg::Vec3 end = muzzle + facing * m_content.hunterRange();
    float bestT = m_content.hunterRange();
    osg::Vec3 bestPos = end;
    int kind = -1;
    int id = -1;
    int sub = 0;

    int vx = 0;
    int vy = 0;
    int vz = 0;
    if (firstSolidOnSegment(m_miniVoxels, muzzle, end, &vx, &vy, &vz)) {
        bestPos.set(
            (static_cast<float>(vx) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(vy) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(vz) + 0.5f) * MINI_VOXEL_SIZE);
        bestT = (bestPos - muzzle).length();
        kind = 0;
        id = vx;
        sub = (vy << 16) | (vz & 0xffff);
    }

    for (size_t e = 0; e < m_enemies.size(); ++e) {
        LocalEnemy& en = m_enemies[e];
        if (!en.isAlive) {
            continue;
        }
        if (!segmentHitsAabb(muzzle, end, en.makeAabb())) {
            continue;
        }
        const osg::Vec3 c(en.pos.x(), en.pos.y() + en.height() * 0.5f, en.pos.z());
        const float t = (c - muzzle).length();
        if (t < bestT) {
            bestT = t;
            bestPos = c;
            kind = 1;
            id = static_cast<int>(e);
        }
    }
    for (size_t b = 0; b < m_bats.size(); ++b) {
        LocalFlyingBat& bat = m_bats[b];
        if (!bat.isAlive()) {
            continue;
        }
        if (!segmentHitsAabb(muzzle, end, bat.makeAabb())) {
            continue;
        }
        const float t = (bat.pos - muzzle).length();
        if (t < bestT) {
            bestT = t;
            bestPos = bat.pos;
            kind = 2;
            id = static_cast<int>(b);
        }
    }
    for (size_t c = 0; c < m_crawlers.size(); ++c) {
        LocalCrawler& cr = m_crawlers[c];
        if (!cr.isAlive()) {
            continue;
        }
        if (!segmentHitsAabb(muzzle, end, cr.makeAabb())) {
            continue;
        }
        const osg::Vec3 p(cr.pos.x(), cr.pos.y() + cr.height() * 0.5f, cr.pos.z());
        const float t = (p - muzzle).length();
        if (t < bestT) {
            bestT = t;
            bestPos = p;
            kind = 3;
            id = static_cast<int>(c);
        }
    }
    for (size_t w = 0; w < m_centipedes.size(); ++w) {
        LocalCentipede& worm = m_centipedes[w];
        if (!worm.isAlive()) {
            continue;
        }
        for (int s = 0; s < worm.segmentCount(); ++s) {
            if (!segmentHitsAabb(muzzle, end, worm.makeAabb(s))) {
                continue;
            }
            const osg::Vec3 p = worm.segment(s).pos;
            const float t = (p - muzzle).length();
            if (t < bestT) {
                bestT = t;
                bestPos = p;
                kind = 4;
                id = static_cast<int>(w);
                sub = s;
            }
        }
    }

    if (outHit != nullptr) {
        *outHit = bestPos;
    }
    if (kind == 0) {
        const int hitVy = (sub >> 16);
        const int hitVz = (sub & 0xffff);
        if (removeMiniVoxel(id, hitVy, hitVz)) {
            m_localMesher.rebuildMesh();
        }
        spawnDebris(bestPos, kFloatHunter);
        std::cout << "[hunter] frontal voxel\n";
        return true;
    }
    if (kind < 1) {
        return false;
    }
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "-%d", m_content.hunterDamage());
        spawnFloatingText(bestPos, buf, kFloatHunter, 0.14f);
    }
    if (kind == 1) {
        LocalEnemy& en = m_enemies[static_cast<size_t>(id)];
        en.takeDamage(m_content.hunterDamage(), muzzle);
        if (!en.isAlive) {
            onEnemyKilled(en, id);
        }
    } else if (kind == 2) {
        LocalFlyingBat& bat = m_bats[static_cast<size_t>(id)];
        bat.takeDamage(m_content.hunterDamage(), muzzle);
        if (!bat.isAlive()) {
            onBatKilled(bat, id);
        }
    } else if (kind == 3) {
        LocalCrawler& cr = m_crawlers[static_cast<size_t>(id)];
        cr.takeDamage(m_content.hunterDamage(), muzzle);
        if (!cr.isAlive()) {
            onCrawlerKilled(cr, id);
        }
    } else if (kind == 4) {
        applyCentipedeDamage(id, sub, m_content.hunterDamage());
    }
    return true;
}

bool StandaloneEngine::hasCombatLock() const
{
    if (m_lockedEnemyIndex >= 0 &&
        m_lockedEnemyIndex < static_cast<int>(m_enemies.size()) &&
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].isAlive) {
        return true;
    }
    if (m_lockedBatIndex >= 0 &&
        m_lockedBatIndex < static_cast<int>(m_bats.size()) &&
        m_bats[static_cast<size_t>(m_lockedBatIndex)].isAlive()) {
        return true;
    }
    if (m_lockedCrawlerIndex >= 0 &&
        m_lockedCrawlerIndex < static_cast<int>(m_crawlers.size()) &&
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].isAlive()) {
        return true;
    }
    if (m_lockedCentipedeIndex >= 0 &&
        m_lockedCentipedeIndex < static_cast<int>(m_centipedes.size()) &&
        m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].isAlive() &&
        m_lockedCentipedeSeg >= 0 &&
        m_lockedCentipedeSeg < m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].segmentCount()) {
        return true;
    }
    return false;
}

bool StandaloneEngine::lockedTargetCenter(osg::Vec3* out) const
{
    if (out == nullptr) {
        return false;
    }
    if (m_lockedArchitect && m_architect.isAlive()) {
        *out = m_architect.position();
        return true;
    }
    if (m_lockedEnemyIndex >= 0 &&
        m_lockedEnemyIndex < static_cast<int>(m_enemies.size()) &&
        m_enemies[static_cast<size_t>(m_lockedEnemyIndex)].isAlive) {
        const LocalEnemy& en = m_enemies[static_cast<size_t>(m_lockedEnemyIndex)];
        *out = osg::Vec3(en.pos.x(), en.pos.y() + en.height() * 0.5f, en.pos.z());
        return true;
    }
    if (m_lockedBatIndex >= 0 &&
        m_lockedBatIndex < static_cast<int>(m_bats.size()) &&
        m_bats[static_cast<size_t>(m_lockedBatIndex)].isAlive()) {
        *out = m_bats[static_cast<size_t>(m_lockedBatIndex)].pos;
        return true;
    }
    if (m_lockedCrawlerIndex >= 0 &&
        m_lockedCrawlerIndex < static_cast<int>(m_crawlers.size()) &&
        m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)].isAlive()) {
        const LocalCrawler& cr = m_crawlers[static_cast<size_t>(m_lockedCrawlerIndex)];
        *out = osg::Vec3(cr.pos.x(), cr.pos.y() + cr.height() * 0.5f, cr.pos.z());
        return true;
    }
    if (m_lockedCentipedeIndex >= 0 &&
        m_lockedCentipedeIndex < static_cast<int>(m_centipedes.size()) &&
        m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].isAlive() &&
        m_lockedCentipedeSeg >= 0 &&
        m_lockedCentipedeSeg < m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].segmentCount()) {
        *out = m_centipedes[static_cast<size_t>(m_lockedCentipedeIndex)].segment(m_lockedCentipedeSeg).pos;
        return true;
    }
    return false;
}

void StandaloneEngine::fireHunterBeam()
{
    // 122. Hitscan al lock TAB. La celda la gasta fireHunterLaser().
    const float yaw = m_dummyActor.yaw();
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    const osg::Vec3 muzzle(
        m_dummyActor.x() + fx * 0.32f,
        m_dummyActor.y() + m_dummyActor.height() * 0.72f,
        m_dummyActor.z() + fz * 0.32f);
    osg::Vec3 target = muzzle + osg::Vec3(fx, 0.0f, fz) * m_content.hunterRange();
    if (!lockedTargetCenter(&target)) {
        return;
    }

    int vx = 0;
    int vy = 0;
    int vz = 0;
    const bool blocked = firstSolidOnSegment(m_miniVoxels, muzzle, target, &vx, &vy, &vz);
    osg::Vec3 beamEnd = target;
    if (blocked) {
        beamEnd.set(
            (static_cast<float>(vx) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(vy) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(vz) + 0.5f) * MINI_VOXEL_SIZE);
        spawnHunterBeam(muzzle, beamEnd);
        spawnDebris(beamEnd, kFloatHunter);
        std::cout << "[hunter] blocked cells=" << m_hunterCells << "\n";
        return;
    }

    spawnHunterBeam(muzzle, target);
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "-%d", m_content.hunterDamage());
        spawnFloatingText(target, buf, kFloatHunter, 0.14f);
    }
    applyHunterDamageLocked(muzzle);
}

void StandaloneEngine::spawnHunterBeam(const osg::Vec3& from, const osg::Vec3& to)
{
    osg::Vec3 dir = to - from;
    float len = dir.length();
    if (len < 0.001f) {
        return;
    }
    dir = dir * (1.0f / len);
    const osg::Vec3 mid = (from + to) * 0.5f;

    osg::ref_ptr<osg::Cylinder> core = new osg::Cylinder(osg::Vec3(0.0f, 0.0f, 0.0f), 0.022f, len);
    osg::ref_ptr<osg::ShapeDrawable> coreDraw = new osg::ShapeDrawable(core.get());
    coreDraw->setColor(osg::Vec4(0.30f, 0.95f, 1.00f, 0.95f));

    osg::ref_ptr<osg::Cylinder> glow = new osg::Cylinder(osg::Vec3(0.0f, 0.0f, 0.0f), 0.045f, len);
    osg::ref_ptr<osg::ShapeDrawable> glowDraw = new osg::ShapeDrawable(glow.get());
    glowDraw->setColor(osg::Vec4(1.00f, 0.72f, 0.18f, 0.35f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(coreDraw.get());
    geode->addDrawable(glowDraw.get());

    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.30f, 0.95f, 1.00f, 0.90f));
    mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.20f, 0.70f, 0.95f, 0.90f));
    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.10f, 0.35f, 0.45f, 0.90f));
    mat->setAlpha(osg::Material::FRONT_AND_BACK, 0.90f);

    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    st->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    st->setMode(GL_BLEND, osg::StateAttribute::ON);
    st->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    osg::ref_ptr<osg::Geometry> line = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
    verts->push_back(osg::Vec3(0.0f, 0.0f, -len * 0.5f));
    verts->push_back(osg::Vec3(0.0f, 0.0f, len * 0.5f));
    osg::ref_ptr<osg::Vec4Array> cols = new osg::Vec4Array;
    cols->push_back(osg::Vec4(1.00f, 0.78f, 0.20f, 1.0f));
    cols->push_back(osg::Vec4(0.25f, 0.95f, 1.00f, 1.0f));
    line->setVertexArray(verts.get());
    line->setColorArray(cols.get(), osg::Array::BIND_PER_VERTEX);
    line->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 2));
    osg::StateSet* ls = line->getOrCreateStateSet();
    ls->setAttributeAndModes(new osg::LineWidth(2.5f), osg::StateAttribute::ON);
    ls->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    geode->addDrawable(line.get());

    osg::Quat q;
    q.makeRotate(osg::Vec3(0.0f, 0.0f, 1.0f), dir);
    osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
    mt->setMatrix(osg::Matrix::rotate(q) * osg::Matrix::translate(mid));
    mt->addChild(geode.get());
    if (m_projectileRoot.valid()) {
        m_projectileRoot->addChild(mt.get());
    }

    HunterBeam beam;
    beam.node = mt;
    beam.material = mat;
    beam.ttl = m_content.hunterLife();
    beam.life = m_content.hunterLife();
    m_hunterBeams.push_back(beam);
}

void StandaloneEngine::updateHunterBeams(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    size_t i = 0;
    while (i < m_hunterBeams.size()) {
        HunterBeam& beam = m_hunterBeams[i];
        beam.ttl -= dt;
        if (beam.ttl <= 0.0f) {
            if (m_projectileRoot.valid() && beam.node.valid()) {
                m_projectileRoot->removeChild(beam.node.get());
            }
            m_hunterBeams.erase(m_hunterBeams.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (beam.material.valid() && beam.life > 0.0001f) {
            const float a = beam.ttl / beam.life;
            osg::Vec4 col(0.30f, 0.95f, 1.00f, a);
            beam.material->setDiffuse(osg::Material::FRONT_AND_BACK, col);
            beam.material->setEmission(osg::Material::FRONT_AND_BACK,
                                       osg::Vec4(0.20f * a, 0.70f * a, 0.95f * a, a));
            beam.material->setAlpha(osg::Material::FRONT_AND_BACK, a);
        }
        i += 1;
    }
}

void StandaloneEngine::updateProjectiles(float deltaTime)
{
    // 22.1 / 22.2 / 22.3 / 22.4 / 59.1 Balistica local vs MiniVoxelGrid (DDA).
    size_t i = 0;
    while (i < m_projectiles.size()) {
        LocalProjectile& shot = m_projectiles[i];
        if (!shot.isHostile()) {
            osg::Vec3 aim;
            if (lockedTargetCenter(&aim)) {
                osg::Vec3 dir = aim - shot.m_pos;
                const float len = dir.length();
                if (len > 0.0001f) {
                    shot.m_vel = dir * (m_content.gunSpeed() / len);
                }
            }
        }
        const osg::Vec3 prev = shot.m_pos;
        shot.m_pos = shot.m_pos + shot.m_vel * deltaTime;
        shot.m_ttl -= deltaTime;

        bool dead = !shot.alive() || shot.m_ttl <= 0.0f || shot.m_pos.y() < 0.0f;

        // 110. Balas rectas vs AABB enemigos / crawler (armadura frontal) / murcielago / rocas.
        if (!dead && !shot.isHostile()) {
            for (size_t e = 0; e < m_enemies.size(); ++e) {
                LocalEnemy& enemy = m_enemies[e];
                if (!enemy.isAlive) {
                    continue;
                }
                if (!segmentHitsAabb(prev, shot.m_pos, enemy.makeAabb())) {
                    continue;
                }
                const int shown = enemy.hp();
                {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "-%d", shown);
                    spawnFloatingText(
                        osg::Vec3(enemy.pos.x(), enemy.pos.y() + enemy.height() * 0.65f, enemy.pos.z()),
                        buf, kFloatDmgEnemy);
                }
                enemy.kill();
                onEnemyKilled(enemy, static_cast<int>(e));
                dead = true;
                std::cout << "[shot] hit enemy\n";
                break;
            }
            if (!dead) {
                for (size_t b = 0; b < m_bats.size(); ++b) {
                    LocalFlyingBat& bat = m_bats[b];
                    if (!bat.isAlive()) {
                        continue;
                    }
                    if (!segmentHitsAabb(prev, shot.m_pos, bat.makeAabb())) {
                        continue;
                    }
                    {
                        char buf[16];
                        std::snprintf(buf, sizeof(buf), "-%d", 20);
                        spawnFloatingText(
                            osg::Vec3(bat.pos.x(), bat.pos.y() + 0.25f, bat.pos.z()),
                            buf, kFloatDmgEnemy);
                    }
                    bat.takeDamage(20, shot.m_pos);
                    if (!bat.isAlive()) {
                        onBatKilled(bat, static_cast<int>(b));
                    }
                    dead = true;
                    std::cout << "[shot] hit bat\n";
                    break;
                }
            }
            // 102.4 El Arquitecto entra en el pipeline de disparo: sin esto el
            // evento de caza no se puede completar.
            if (!dead && m_architect.isAlive() &&
                segmentHitsAabb(prev, shot.m_pos, m_architect.makeAabb())) {
                damageArchitect(40);
                spawnDebris(shot.m_pos, ARCHITECT_COLOR);
                dead = true;
            }
            if (!dead) {
                for (size_t c = 0; c < m_crawlers.size(); ++c) {
                    LocalCrawler& cr = m_crawlers[c];
                    if (!cr.isAlive()) {
                        continue;
                    }
                    if (!segmentHitsAabb(prev, shot.m_pos, cr.makeAabb())) {
                        continue;
                    }
                    if (cr.isFrontalHit(prev)) {
                        shot.m_vel.x() = -shot.m_vel.x() * 0.55f;
                        shot.m_vel.z() = -shot.m_vel.z() * 0.55f;
                        spawnDebris(shot.m_pos, CRAWLER_COLOR);
                        std::cout << "[shot] crawler bounce\n";
                        break;
                    }
                    cr.takeDamage(40, shot.m_pos);
                    {
                        char buf[16];
                        std::snprintf(buf, sizeof(buf), "-%d", 40);
                        spawnFloatingText(
                            osg::Vec3(cr.pos.x(), cr.pos.y() + cr.height() * 0.80f, cr.pos.z()),
                            buf, kFloatDmgEnemy);
                    }
                    if (!cr.isAlive()) {
                        onCrawlerKilled(cr, static_cast<int>(c));
                    }
                    dead = true;
                    std::cout << "[shot] crawler rear\n";
                    break;
                }
            }
            if (!dead) {
                for (size_t w = 0; w < m_centipedes.size(); ++w) {
                    LocalCentipede& worm = m_centipedes[w];
                    if (!worm.isAlive()) {
                        continue;
                    }
                    bool hitSeg = false;
                    for (int s = 0; s < worm.segmentCount(); ++s) {
                        if (!segmentHitsAabb(prev, shot.m_pos, worm.makeAabb(s))) {
                            continue;
                        }
                        {
                            char buf[16];
                            std::snprintf(buf, sizeof(buf), "-%d", 25);
                            spawnFloatingText(worm.segment(s).pos, buf, kFloatDmgEnemy);
                        }
                        applyCentipedeDamage(static_cast<int>(w), s, 25);
                        dead = true;
                        hitSeg = true;
                        std::cout << "[shot] hit centipede seg " << s << "\n";
                        break;
                    }
                    if (hitSeg) {
                        break;
                    }
                }
            }
            if (!dead) {
                const int nB = m_boulderWorld.boulderCount();
                for (int b = 0; b < nB; ++b) {
                    if (!m_boulderWorld.boulderAlive(b)) {
                        continue;
                    }
                    if (!segmentHitsAabb(prev, shot.m_pos, m_boulderWorld.makeAabb(b))) {
                        continue;
                    }
                    spawnDebris(shot.m_pos, BOULDER_COLOR);
                    dead = true;
                    std::cout << "[shot] hit boulder\n";
                    break;
                }
            }
        }

        if (!dead && shot.isHostile()) {
            const AABB box = m_dummyActor.makeAabb();
            const float pad = MINI_VOXEL_SIZE * 0.15f;
            if (shot.m_pos.x() >= box.minX - pad && shot.m_pos.x() <= box.maxX + pad &&
                shot.m_pos.y() >= box.minY - pad && shot.m_pos.y() <= box.maxY + pad &&
                shot.m_pos.z() >= box.minZ - pad && shot.m_pos.z() <= box.maxZ + pad) {
                if (m_dummyActor.dashTimer() <= 0.0f) {
                    handlePlayerHit(15);
                }
                dead = true;
                std::cout << "[arrow] hit player\n";
            }
        }

        if (!dead) {
            int vx = 0;
            int vy = 0;
            int vz = 0;
            if (firstSolidOnSegment(m_miniVoxels, prev, shot.m_pos, &vx, &vy, &vz)) {
                if (removeMiniVoxel(vx, vy, vz)) {
                    m_localMesher.rebuildMesh();
                }
                dead = true;
                std::cout << "[shot] hit voxel (" << vx << ", " << vy << ", " << vz << ")\n";
            }
        }

        if (dead) {
            if (m_projectileRoot.valid() && shot.getNode() != nullptr) {
                m_projectileRoot->removeChild(shot.getNode());
            }
            m_projectiles.erase(m_projectiles.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        shot.syncVisual();
        i += 1;
    }
}

void StandaloneEngine::render()
{
    if (!m_initialized || !m_viewer.valid()) {
        return;
    }

    m_viewer->frame();
}

void StandaloneEngine::setEditorMode(bool active)
{
    if (m_editorMode == active) {
        return;
    }
    m_editorMode = active;
    if (m_editorMode) {
        clearSelection();
    }
    if (!m_viewer.valid()) {
        return;
    }
    if (m_editorMode) {
        // 16.2 Editor: Trackball libre.
        m_viewer->setCameraManipulator(m_trackball.get());
        if (m_trackball.valid()) {
            m_trackball->home(0.0);
        }
    } else {
        // 16.3 Play: sin manipulator. View matrix manual en update().
        m_viewer->setCameraManipulator(nullptr);
        m_cameraInitialized = false;
        updatePlayCamera(0.016f);
    }
    std::cout << "[standalone] editorMode="
              << (m_editorMode ? "1 editor/trackball" : "0 play/dummy-cam")
              << "\n";
}

void StandaloneEngine::shutdown()
{
    m_inventory.save(playerSavePath("data/player/sandbox_inventory.json"));
    m_quests.save();
    if (m_viewer.valid()) {
        m_viewer->setDone(true);
        m_viewer = nullptr;
    }
    m_trackball = nullptr;
    m_inputHandler = nullptr;
    m_projectiles.clear();
    m_projectileRoot = nullptr;
    m_pipFaryCamera = nullptr;
    m_pipFrame = nullptr;
    m_worldRoot = nullptr;
    m_root = nullptr;
    m_graphicsContext = nullptr;
    m_initialized = false;
}

} // namespace standalone
} // namespace rc
