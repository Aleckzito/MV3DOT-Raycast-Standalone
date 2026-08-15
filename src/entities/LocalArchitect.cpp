#include "LocalArchitect.h"

#include "LocalBoulder.h"
#include "LocalChunkMesher.h"
#include "LocalPhysicsSolver.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/GL>
#include <osg/Light>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>
#include <iostream>

namespace rc {
namespace standalone {

namespace {

const int kArenaMax = 23;
const float kLayerDt = 0.08f;
const int kArchitectMaxHp = 220;
// Media caja de la entidad, para que los disparos la puedan tocar.
const float kArchitectHalf = 0.42f;
const float kFleeRange = 1.25f * TILE_SIZE;
const float kSafeRange = 4.00f * TILE_SIZE;
const float kSiteClear = 2.80f * TILE_SIZE;
const float kFlySpeed = 5.20f;
const float kFleeSpeed = 9.00f;
const float kBobAmp = 0.11f;

bool aabbOverlapXZ(const AABB& a, const AABB& b)
{
    if (a.maxX <= b.minX || a.minX >= b.maxX) {
        return false;
    }
    if (a.maxZ <= b.minZ || a.minZ >= b.maxZ) {
        return false;
    }
    return true;
}

} // namespace

LocalArchitect::LocalArchitect()
    : m_pos(6.20f, 2.15f, 1.40f)
    , m_state(ARCH_SCANNING)
    , m_resume(ARCH_SCANNING)
    , m_time(0.0f)
    , m_layerTtl(0.0f)
    , m_scanTtl(0.0f)
    , m_coolTtl(2.50f)
    , m_flashTtl(0.0f)
    , m_rng(77u)
    , m_builtPos(0.0f, 0.0f, 0.0f)
    , m_builtReady(false)
    , m_alive(false)
    , m_hp(0)
    , m_maxHp(kArchitectMaxHp)
    , m_buildInterval(kLayerDt)
{
    m_job.active = false;
    m_job.kind = ARCH_BP_TOWER;
    m_job.ox = 0;
    m_job.oy = 0;
    m_job.oz = 0;
    m_job.sizeX = 0;
    m_job.sizeY = 0;
    m_job.sizeZ = 0;
    m_job.rot = 0;
    m_job.layer = 0;
    m_job.boulderN = 0;
    m_job.bvx[0] = m_job.bvx[1] = 0;
    m_job.bvy[0] = m_job.bvy[1] = 0;
    m_job.bvz[0] = m_job.bvz[1] = 0;
    buildVisual();
    syncVisual();
}

void LocalArchitect::buildVisual()
{
    osg::ref_ptr<osg::Box> prism = new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.28f, 0.42f, 0.28f);
    m_drawable = new osg::ShapeDrawable(prism.get());
    m_drawable->setColor(ARCHITECT_COLOR);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_drawable.get());
    m_material = new osg::Material;
    m_material->setDiffuse(osg::Material::FRONT_AND_BACK, ARCHITECT_COLOR);
    m_material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.00f, 0.28f, 0.34f, 1.0f));
    m_material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.10f, 0.55f, 0.65f, 1.0f));
    m_material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.95f, 1.0f, 1.0f));
    m_material->setShininess(osg::Material::FRONT_AND_BACK, 42.0f);
    osg::StateSet* st = geode->getOrCreateStateSet();
    st->setAttributeAndModes(m_material.get(), osg::StateAttribute::ON);
    st->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    osg::ref_ptr<osg::Sphere> glow = new osg::Sphere(osg::Vec3(0.0f, 0.0f, 0.0f), 0.55f);
    osg::ref_ptr<osg::ShapeDrawable> glowDraw = new osg::ShapeDrawable(glow.get());
    glowDraw->setColor(osg::Vec4(0.20f, 0.95f, 1.0f, 0.55f));
    osg::ref_ptr<osg::Geode> glowGeode = new osg::Geode;
    glowGeode->addDrawable(glowDraw.get());
    osg::ref_ptr<osg::Material> glowMat = new osg::Material;
    glowMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.20f, 0.95f, 1.0f, 0.55f));
    glowMat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.35f, 0.90f, 1.0f, 0.55f));
    glowMat->setAlpha(osg::Material::FRONT_AND_BACK, 0.55f);
    osg::StateSet* gs = glowGeode->getOrCreateStateSet();
    gs->setAttributeAndModes(glowMat.get(), osg::StateAttribute::ON);
    gs->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE),
        osg::StateAttribute::ON);
    gs->setMode(GL_BLEND, osg::StateAttribute::ON);
    gs->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    gs->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                             osg::StateAttribute::ON);
    m_flashNode = new osg::MatrixTransform;
    m_flashNode->addChild(glowGeode.get());
    m_flashNode->setNodeMask(0);

    m_flashLight = new osg::LightSource;
    osg::Light* light = m_flashLight->getLight();
    light->setLightNum(1);
    light->setDiffuse(osg::Vec4(0.20f, 0.95f, 1.0f, 1.0f));
    light->setSpecular(osg::Vec4(0.40f, 1.0f, 1.0f, 1.0f));
    light->setAmbient(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    light->setConstantAttenuation(0.35f);
    light->setLinearAttenuation(0.12f);
    light->setQuadraticAttenuation(0.045f);
    m_flashLight->setLocalStateSetModes(osg::StateAttribute::OFF);
    m_flashLight->setNodeMask(0);

    m_node = new osg::MatrixTransform;
    m_node->addChild(geode.get());
    m_node->addChild(m_flashNode.get());
    m_node->addChild(m_flashLight.get());
}

osg::Node* LocalArchitect::getNode()
{
    return m_node.get();
}

void LocalArchitect::syncVisual()
{
    if (!m_node.valid()) {
        return;
    }
    const float bob = std::sin(m_time * 2.60f) * kBobAmp;
    const float flash = (m_flashTtl > 0.0f) ? (m_flashTtl / 0.45f) : 0.0f;
    const float scale = 1.0f + flash * 0.55f;
    osg::Matrix sc = osg::Matrix::scale(scale, scale, scale);
    osg::Matrix tr = osg::Matrix::translate(m_pos.x(), m_pos.y() + bob, m_pos.z());
    m_node->setMatrix(sc * tr);

    if (m_material.valid()) {
        const float e = 0.10f + flash * 0.85f;
        m_material->setEmission(osg::Material::FRONT_AND_BACK,
                                osg::Vec4(0.05f + e * 0.15f, 0.55f + e * 0.45f, 0.65f + e * 0.35f, 1.0f));
    }
    if (m_flashNode.valid()) {
        const float gs = 0.70f + flash * 1.80f;
        m_flashNode->setMatrix(osg::Matrix::scale(gs, gs, gs));
        m_flashNode->setNodeMask((m_flashTtl > 0.0f) ? 0xffffffff : 0);
    }
    if (m_flashLight.valid()) {
        osg::Light* light = m_flashLight->getLight();
        light->setPosition(osg::Vec4(0.0f, 0.15f, 0.0f, 1.0f));
        const float inten = flash;
        light->setDiffuse(osg::Vec4(0.25f * inten, 0.95f * inten, 1.0f * inten, 1.0f));
        m_flashLight->setNodeMask((m_flashTtl > 0.0f) ? 0xffffffff : 0);
        m_flashLight->setLocalStateSetModes(
            (m_flashTtl > 0.0f) ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
    }
}

void LocalArchitect::spawnAt(const osg::Vec3& pos)
{
    // Se reutiliza la misma instancia: se reinicia el estado y se reposiciona.
    m_pos = pos;
    m_alive = true;
    m_hp = m_maxHp;
    m_state = ARCH_SCANNING;
    m_resume = ARCH_SCANNING;
    m_job.active = false;
    m_job.layer = 0;
    m_job.cells.clear();
    m_layerTtl = 0.0f;
    m_scanTtl = 0.0f;
    m_coolTtl = 2.50f;
    m_flashTtl = 0.0f;
    m_builtReady = false;
    m_debris.clear();
    if (m_node.valid()) {
        m_node->setNodeMask(~0u);
    }
    syncVisual();
}

void LocalArchitect::despawn()
{
    m_alive = false;
    m_hp = 0;
    m_job.active = false;
    m_job.cells.clear();
    m_builtReady = false;
    if (m_node.valid()) {
        // Se oculta en vez de sacarlo del grafo: el holder es estable.
        m_node->setNodeMask(0u);
    }
}

bool LocalArchitect::takeDamage(int amount)
{
    if (!m_alive || amount <= 0) {
        return false;
    }
    m_hp -= amount;
    triggerFlash();
    if (m_hp > 0) {
        return false;
    }
    m_hp = 0;
    despawn();
    return true;
}

AABB LocalArchitect::makeAabb() const
{
    AABB box;
    box.minX = m_pos.x() - kArchitectHalf;
    box.minY = m_pos.y() - kArchitectHalf;
    box.minZ = m_pos.z() - kArchitectHalf;
    box.maxX = m_pos.x() + kArchitectHalf;
    box.maxY = m_pos.y() + kArchitectHalf;
    box.maxZ = m_pos.z() + kArchitectHalf;
    return box;
}

void LocalArchitect::takeDebris(std::vector<osg::Vec3>& out)
{
    for (size_t i = 0; i < m_debris.size(); ++i) {
        out.push_back(m_debris[i]);
    }
    m_debris.clear();
}

bool LocalArchitect::consumeBuilt(osg::Vec3* outPos)
{
    if (!m_builtReady) {
        return false;
    }
    m_builtReady = false;
    if (outPos != nullptr) {
        *outPos = m_builtPos;
    }
    return true;
}

void LocalArchitect::setState(ArchitectState st)
{
    if (m_state == st) {
        return;
    }
    m_state = st;
    const char* name = "SCANNING";
    if (st == ARCH_NAVIGATING) {
        name = "NAVIGATING";
    } else if (st == ARCH_BUILDING) {
        name = "BUILDING";
    } else if (st == ARCH_FLEEING) {
        name = "FLEEING";
    }
    std::cout << "[architect] " << name << "\n";
}

unsigned int LocalArchitect::nextU()
{
    m_rng = m_rng * 1103515245u + 12345u;
    return m_rng;
}

int LocalArchitect::nextRanged(int n)
{
    if (n <= 1) {
        return 0;
    }
    return static_cast<int>(nextU() % static_cast<unsigned int>(n));
}

void LocalArchitect::addCell(Job& job, int dx, int dy, int dz) const
{
    int x = dx;
    int z = dz;
    if (job.rot != 0) {
        x = dz;
        z = dx;
    }
    VoxelKey k;
    k.vx = job.ox + x;
    k.vy = job.oy + dy;
    k.vz = job.oz + z;
    job.cells.push_back(k);
}

void LocalArchitect::fillTower(Job& job) const
{
    // 102.1 A: torre 4x4 x 8, hueca, ventanas a y=4.
    job.sizeX = 4;
    job.sizeY = 8;
    job.sizeZ = 4;
    job.cells.clear();
    job.boulderN = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 4; ++x) {
            for (int z = 0; z < 4; ++z) {
                const bool wall = (x == 0 || x == 3 || z == 0 || z == 3);
                const bool slab = (y == 0 || y == 7);
                const bool window = (y == 4) &&
                    (((x == 0 || x == 3) && (z == 1 || z == 2)) ||
                     ((z == 0 || z == 3) && (x == 1 || x == 2)));
                if (window) {
                    continue;
                }
                if (slab || wall) {
                    addCell(job, x, y, z);
                }
            }
        }
    }
}

void LocalArchitect::fillArch(Job& job) const
{
    // 102.1 B: pilares 2x2, vano 4, techo transitable.
    job.sizeX = (job.rot == 0) ? 8 : 2;
    job.sizeY = 6;
    job.sizeZ = (job.rot == 0) ? 2 : 8;
    job.cells.clear();
    job.boulderN = 0;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 2; ++x) {
            for (int z = 0; z < 2; ++z) {
                addCell(job, x, y, z);
                addCell(job, x + 6, y, z);
            }
        }
    }
    for (int x = 0; x < 8; ++x) {
        for (int z = 0; z < 2; ++z) {
            addCell(job, x, 5, z);
        }
    }
}

void LocalArchitect::fillFort(Job& job)
{
    // 102.1 C: muro perimetral 4x4x6 + 1/2 rocas en dintel y=4.
    job.sizeX = 4;
    job.sizeY = 6;
    job.sizeZ = 4;
    job.cells.clear();
    job.boulderN = 1 + nextRanged(2);
    job.bvx[0] = job.ox + 1;
    job.bvy[0] = job.oy + 4;
    job.bvz[0] = job.oz + 0;
    job.bvx[1] = job.ox + 1;
    job.bvy[1] = job.oy + 4;
    job.bvz[1] = job.oz + 2;

    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 4; ++x) {
            for (int z = 0; z < 4; ++z) {
                if (!(x == 0 || x == 3 || z == 0 || z == 3)) {
                    continue;
                }
                bool skip = false;
                for (int b = 0; b < job.boulderN; ++b) {
                    const int bx = job.bvx[b] - job.ox;
                    const int by = job.bvy[b] - job.oy;
                    const int bz = job.bvz[b] - job.oz;
                    if (x >= bx && x < bx + 2 && y >= by && y < by + 2 && z >= bz && z < bz + 2) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    addCell(job, x, y, z);
                }
            }
        }
    }
}

bool LocalArchitect::cellsFit(const Job& job, const MiniVoxelGrid& grid,
                              const LocalBoulderWorld& boulders, const osg::Vec3& playerPos) const
{
    if (job.sizeX <= 0 || job.sizeZ <= 0) {
        return false;
    }
    if (job.ox < 0 || job.oz < 0) {
        return false;
    }
    if (job.ox + job.sizeX - 1 > kArenaMax || job.oz + job.sizeZ - 1 > kArenaMax) {
        return false;
    }

    const float cx = (static_cast<float>(job.ox) + static_cast<float>(job.sizeX) * 0.5f) * MINI_VOXEL_SIZE;
    const float cz = (static_cast<float>(job.oz) + static_cast<float>(job.sizeZ) * 0.5f) * MINI_VOXEL_SIZE;
    const float pdx = cx - playerPos.x();
    const float pdz = cz - playerPos.z();
    if (pdx * pdx + pdz * pdz < kSiteClear * kSiteClear) {
        return false;
    }

    for (size_t i = 0; i < job.cells.size(); ++i) {
        const VoxelKey& k = job.cells[i];
        if (k.vy < 0 || k.vx < 0 || k.vz < 0 || k.vx > kArenaMax || k.vz > kArenaMax) {
            return false;
        }
        if (grid.getVoxel(k.vx, k.vy, k.vz).isActive) {
            return false;
        }
    }

    AABB foot;
    foot.minX = static_cast<float>(job.ox) * MINI_VOXEL_SIZE;
    foot.maxX = static_cast<float>(job.ox + job.sizeX) * MINI_VOXEL_SIZE;
    foot.minY = 0.0f;
    foot.maxY = static_cast<float>(job.sizeY) * MINI_VOXEL_SIZE;
    foot.minZ = static_cast<float>(job.oz) * MINI_VOXEL_SIZE;
    foot.maxZ = static_cast<float>(job.oz + job.sizeZ) * MINI_VOXEL_SIZE;
    const int nb = boulders.boulderCount();
    for (int i = 0; i < nb; ++i) {
        if (!boulders.boulderAlive(i)) {
            continue;
        }
        if (aabbOverlapXZ(foot, boulders.makeAabb(i))) {
            return false;
        }
    }
    return true;
}

bool LocalArchitect::tryPickSite(const MiniVoxelGrid& grid, const LocalBoulderWorld& boulders,
                                 const osg::Vec3& playerPos, Job* out)
{
    ArchitectBlueprint order[3] = { ARCH_BP_TOWER, ARCH_BP_ARCH, ARCH_BP_FORT };
    for (int s = 0; s < 3; ++s) {
        const int a = nextRanged(3);
        const ArchitectBlueprint tmp = order[s];
        order[s] = order[a];
        order[a] = tmp;
    }

    for (int n = 0; n < 14; ++n) {
        Job job;
        job.active = false;
        job.kind = order[nextRanged(3)];
        job.oy = 0;
        job.rot = (job.kind == ARCH_BP_ARCH) ? nextRanged(2) : 0;
        job.layer = 0;
        job.boulderN = 0;
        job.cells.clear();

        if (job.kind == ARCH_BP_TOWER) {
            fillTower(job);
        } else if (job.kind == ARCH_BP_ARCH) {
            fillArch(job);
        } else {
            job.ox = 0;
            job.oz = 0;
            fillFort(job);
        }

        const int maxX = kArenaMax - job.sizeX + 1;
        const int maxZ = kArenaMax - job.sizeZ + 1;
        if (maxX < 0 || maxZ < 0) {
            continue;
        }
        job.ox = nextRanged(maxX + 1);
        job.oz = nextRanged(maxZ + 1);

        job.cells.clear();
        if (job.kind == ARCH_BP_TOWER) {
            fillTower(job);
        } else if (job.kind == ARCH_BP_ARCH) {
            fillArch(job);
        } else {
            fillFort(job);
        }

        if (cellsFit(job, grid, boulders, playerPos)) {
            job.active = true;
            *out = job;
            return true;
        }
    }
    return false;
}

void LocalArchitect::hoverGoal(const osg::Vec3& playerPos, osg::Vec3* out) const
{
    osg::Vec3 away(m_pos.x() - playerPos.x(), 0.0f, m_pos.z() - playerPos.z());
    float len = away.length();
    if (len < 0.001f) {
        away.set(1.0f, 0.0f, 0.0f);
        len = 1.0f;
    }
    away = away * (1.0f / len);
    osg::Vec3 goal = playerPos + away * kSafeRange;
    if (goal.x() < 0.80f) {
        goal.x() = 0.80f;
    }
    if (goal.x() > 7.20f) {
        goal.x() = 7.20f;
    }
    if (goal.z() < 0.80f) {
        goal.z() = 0.80f;
    }
    if (goal.z() > 7.20f) {
        goal.z() = 7.20f;
    }
    goal.y() = 2.15f;
    *out = goal;
}

void LocalArchitect::jobHover(const Job& job, osg::Vec3* out) const
{
    out->x() = (static_cast<float>(job.ox) + static_cast<float>(job.sizeX) * 0.5f) * MINI_VOXEL_SIZE;
    out->y() = static_cast<float>(job.sizeY) * MINI_VOXEL_SIZE + 0.85f;
    out->z() = (static_cast<float>(job.oz) + static_cast<float>(job.sizeZ) * 0.5f) * MINI_VOXEL_SIZE;
}

bool LocalArchitect::flyToward(const osg::Vec3& goal, float speed, float dt)
{
    osg::Vec3 d = goal - m_pos;
    const float dist = d.length();
    if (dist <= 0.22f) {
        m_pos = goal;
        return true;
    }
    const float step = speed * dt;
    if (step >= dist) {
        m_pos = goal;
        return true;
    }
    m_pos = m_pos + d * (step / dist);
    return false;
}

void LocalArchitect::placeLayer(MiniVoxelGrid& grid, LocalChunkMesher& mesher)
{
    osg::Vec3 acc(0.0f, 0.0f, 0.0f);
    int n = 0;
    for (size_t i = 0; i < m_job.cells.size(); ++i) {
        const VoxelKey& k = m_job.cells[i];
        if (k.vy != m_job.layer) {
            continue;
        }
        if (grid.getVoxel(k.vx, k.vy, k.vz).isActive) {
            continue;
        }
        grid.setVoxel(k.vx, k.vy, k.vz, 1);
        const osg::Vec3 c(
            (static_cast<float>(k.vx) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(k.vy) + 0.5f) * MINI_VOXEL_SIZE,
            (static_cast<float>(k.vz) + 0.5f) * MINI_VOXEL_SIZE);
        acc = acc + c;
        n += 1;
        if ((n % 4) == 1) {
            m_debris.push_back(c);
        }
    }
    if (n > 0) {
        acc = acc * (1.0f / static_cast<float>(n));
        m_debris.push_back(acc);
        mesher.rebuildMesh();
    }
    m_job.layer += 1;
}

void LocalArchitect::triggerFlash()
{
    m_flashTtl = 0.45f;
    m_debris.push_back(m_pos);
    osg::Vec3 site;
    jobHover(m_job, &site);
    site.y() = static_cast<float>(m_job.sizeY) * MINI_VOXEL_SIZE * 0.55f;
    m_debris.push_back(site);
}

void LocalArchitect::finishJob(LocalBoulderWorld& boulders, MiniVoxelGrid& grid)
{
    if (m_job.kind == ARCH_BP_FORT) {
        for (int b = 0; b < m_job.boulderN; ++b) {
            boulders.spawnAt(grid, m_job.bvx[b], m_job.bvy[b], m_job.bvz[b]);
        }
    }
    triggerFlash();
    m_builtReady = true;
    m_builtPos.set(
        (static_cast<float>(m_job.ox) + static_cast<float>(m_job.sizeX) * 0.5f) * MINI_VOXEL_SIZE,
        0.12f,
        (static_cast<float>(m_job.oz) + static_cast<float>(m_job.sizeZ) * 0.5f) * MINI_VOXEL_SIZE);
    m_coolTtl = 8.0f + static_cast<float>(nextRanged(4001)) * 0.001f;
    m_job.active = false;
    const char* kind = "tower";
    if (m_job.kind == ARCH_BP_ARCH) {
        kind = "arch";
    } else if (m_job.kind == ARCH_BP_FORT) {
        kind = "fort";
    }
    std::cout << "[architect] done " << kind << " cooldown=" << m_coolTtl << "s\n";
    setState(ARCH_SCANNING);
}

void LocalArchitect::update(float dt, const osg::Vec3& playerPos, MiniVoxelGrid& grid,
                            LocalChunkMesher& mesher, LocalBoulderWorld& boulders)
{
    if (dt <= 0.0f || !m_alive) {
        return;
    }
    m_time += dt;
    if (m_flashTtl > 0.0f) {
        m_flashTtl -= dt;
        if (m_flashTtl < 0.0f) {
            m_flashTtl = 0.0f;
        }
    }
    if (m_coolTtl > 0.0f) {
        m_coolTtl -= dt;
        if (m_coolTtl < 0.0f) {
            m_coolTtl = 0.0f;
        }
    }

    const float pdx = m_pos.x() - playerPos.x();
    const float pdz = m_pos.z() - playerPos.z();
    const float pdist2 = pdx * pdx + pdz * pdz;
    if (m_state != ARCH_FLEEING && pdist2 <= kFleeRange * kFleeRange) {
        m_resume = (m_state == ARCH_BUILDING || m_state == ARCH_NAVIGATING) ? ARCH_NAVIGATING
                                                                           : ARCH_SCANNING;
        setState(ARCH_FLEEING);
    }

    if (m_state == ARCH_FLEEING) {
        osg::Vec3 goal;
        hoverGoal(playerPos, &goal);
        goal.y() = 2.55f;
        if (flyToward(goal, kFleeSpeed, dt) && pdist2 > kSafeRange * kSafeRange) {
            setState(m_resume);
        }
        syncVisual();
        return;
    }

    if (m_state == ARCH_SCANNING) {
        osg::Vec3 patrol;
        hoverGoal(playerPos, &patrol);
        patrol.x() += std::cos(m_time * 0.55f) * 0.45f;
        patrol.z() += std::sin(m_time * 0.47f) * 0.45f;
        patrol.y() = 2.15f;
        flyToward(patrol, kFlySpeed * 0.72f, dt);

        if (m_coolTtl <= 0.0f) {
            m_scanTtl -= dt;
            if (m_scanTtl <= 0.0f) {
                m_scanTtl = 0.18f;
                Job picked;
                if (tryPickSite(grid, boulders, playerPos, &picked)) {
                    m_job = picked;
                    m_job.layer = 0;
                    m_layerTtl = 0.0f;
                    setState(ARCH_NAVIGATING);
                    const char* kind = "tower";
                    if (m_job.kind == ARCH_BP_ARCH) {
                        kind = "arch";
                    } else if (m_job.kind == ARCH_BP_FORT) {
                        kind = "fort";
                    }
                    std::cout << "[architect] site " << kind << " ox=" << m_job.ox
                              << " oz=" << m_job.oz << "\n";
                }
            }
        }
        syncVisual();
        return;
    }

    if (m_state == ARCH_NAVIGATING) {
        if (!m_job.active) {
            setState(ARCH_SCANNING);
            syncVisual();
            return;
        }
        osg::Vec3 goal;
        jobHover(m_job, &goal);
        if (flyToward(goal, kFlySpeed, dt)) {
            m_layerTtl = 0.0f;
            setState(ARCH_BUILDING);
        }
        syncVisual();
        return;
    }

    if (m_state == ARCH_BUILDING) {
        osg::Vec3 goal;
        jobHover(m_job, &goal);
        flyToward(goal, kFlySpeed * 0.45f, dt);
        m_layerTtl -= dt;
        if (m_layerTtl <= 0.0f) {
            // Cada capa dispara un rebuild completo del mesher. En la arena de
            // 120x120 eso son ~30k voxels por rebuild, asi que el ritmo lo fija
            // el motor y no una constante de 0.08 s.
            m_layerTtl = m_buildInterval;
            placeLayer(grid, mesher);
            if (m_job.layer >= m_job.sizeY) {
                finishJob(boulders, grid);
            }
        }
        syncVisual();
        return;
    }

    syncVisual();
}

} // namespace standalone
} // namespace rc
