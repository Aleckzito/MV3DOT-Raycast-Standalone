#include "LocalBoxWorld.h"

#include <osg/Geode>
#include <osg/GL>
#include <osg/Material>
#include <osg/Matrix>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <cmath>
#include <iostream>
#include <vector>

namespace rc {
namespace standalone {

namespace {

const float kBoxEdge = CELL_SIZE * 0.996f;
const float kSlideSpeed = 6.0f;
const int kMaxPush = 5;
const int kMaxStack = 16;
const float kSquashTime = 0.18f;

} // namespace

LocalBoxWorld::LocalBoxWorld()
    : m_fell(false)
    , m_placed(false)
{
    m_root = new osg::Group;
}

osg::Node* LocalBoxWorld::getNode()
{
    return m_root.get();
}

int LocalBoxWorld::worldToCell(float world) const
{
    return static_cast<int>(std::floor(world / CELL_SIZE));
}

osg::Vec3 LocalBoxWorld::cellCenter(int ix, int iy, int iz) const
{
    return osg::Vec3(
        (static_cast<float>(ix) + 0.5f) * CELL_SIZE,
        (static_cast<float>(iy) + 0.5f) * CELL_SIZE,
        (static_cast<float>(iz) + 0.5f) * CELL_SIZE);
}

void LocalBoxWorld::cardinalFromYaw(float yaw, int* dx, int* dz) const
{
    const float fx = std::sin(yaw);
    const float fz = std::cos(yaw);
    if (std::fabs(fx) >= std::fabs(fz)) {
        *dx = (fx >= 0.0f) ? 1 : -1;
        *dz = 0;
    } else {
        *dx = 0;
        *dz = (fz >= 0.0f) ? 1 : -1;
    }
}

bool LocalBoxWorld::occupied(int ix, int iy, int iz) const
{
    return findBox(ix, iy, iz) != nullptr;
}

const DynamicBox* LocalBoxWorld::findBox(int ix, int iy, int iz) const
{
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        const DynamicBox& b = m_boxes[i];
        if (b.alive && b.ix == ix && b.iy == iy && b.iz == iz) {
            return &b;
        }
    }
    return nullptr;
}

DynamicBox* LocalBoxWorld::findBox(int ix, int iy, int iz)
{
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        DynamicBox& b = m_boxes[i];
        if (b.alive && b.ix == ix && b.iy == iy && b.iz == iz) {
            return &b;
        }
    }
    return nullptr;
}

int LocalBoxWorld::indexOfCell(int ix, int iy, int iz) const
{
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        const DynamicBox& b = m_boxes[i];
        if (b.alive && b.ix == ix && b.iy == iy && b.iz == iz) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool LocalBoxWorld::boxFalling(int index) const
{
    if (!boxAlive(index)) {
        return false;
    }
    return m_boxes[static_cast<size_t>(index)].falling;
}

bool LocalBoxWorld::tryMarkCrush(int index)
{
    if (!boxFalling(index)) {
        return false;
    }
    DynamicBox& b = m_boxes[static_cast<size_t>(index)];
    if (b.crushDone) {
        return false;
    }
    b.crushDone = true;
    return true;
}

int LocalBoxWorld::getStackHeight(int ix, int iz) const
{
    int h = 0;
    for (int y = 0; y < kMaxStack; ++y) {
        if (!occupied(ix, y, iz)) {
            break;
        }
        h = y + 1;
    }
    return h;
}

void LocalBoxWorld::buildVisual(DynamicBox& box)
{
    // 77.2 Full extent con micro-gap 0.996 (anti z-fight).
    osg::ref_ptr<osg::Box> shape = new osg::Box(
        osg::Vec3(0.0f, 0.0f, 0.0f), kBoxEdge, kBoxEdge, kBoxEdge);
    osg::ref_ptr<osg::ShapeDrawable> draw = new osg::ShapeDrawable(shape.get());
    draw->setColor(osg::Vec4(0.55f, 0.85f, 1.0f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(draw.get());

    osg::ref_ptr<osg::Material> mat = new osg::Material;
    mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.15f, 0.28f, 0.38f, 1.0f));
    mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.55f, 0.85f, 1.0f, 1.0f));
    mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.35f, 0.45f, 0.55f, 1.0f));
    mat->setShininess(osg::Material::FRONT_AND_BACK, 24.0f);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    box.node = new osg::MatrixTransform;
    box.node->addChild(geode.get());
    if (m_root.valid()) {
        m_root->addChild(box.node.get());
    }
}

void LocalBoxWorld::syncBox(DynamicBox& box)
{
    if (!box.node.valid()) {
        return;
    }
    float sx = 1.0f;
    float sy = 1.0f;
    float sz = 1.0f;
    if (box.squashTtl > 0.0f) {
        const float t = box.squashTtl / kSquashTime;
        sx = 1.0f + 0.12f * t;
        sy = 1.0f - 0.18f * t;
        sz = 1.0f + 0.12f * t;
    }
    osg::Matrix sc = osg::Matrix::scale(sx, sy, sz);
    osg::Matrix tr = osg::Matrix::translate(box.pos);
    box.node->setMatrix(sc * tr);
    box.node->setNodeMask(box.alive ? 0xffffffff : 0);
}

bool LocalBoxWorld::queryPlaceSlot(const osg::Vec3& playerPos, float yaw, osg::Vec3* outPos) const
{
    int dx = 0;
    int dz = 0;
    cardinalFromYaw(yaw, &dx, &dz);
    const int ix = worldToCell(playerPos.x()) + dx;
    const int iz = worldToCell(playerPos.z()) + dz;
    const int iy = getStackHeight(ix, iz);
    if (iy >= kMaxStack) {
        return false;
    }
    if (occupied(ix, iy, iz)) {
        return false;
    }
    if (outPos != nullptr) {
        *outPos = cellCenter(ix, iy, iz);
    }
    return true;
}

bool LocalBoxWorld::placeBox(const osg::Vec3& playerPos, float yaw)
{
    osg::Vec3 center;
    if (!queryPlaceSlot(playerPos, yaw, &center)) {
        return false;
    }
    int dx = 0;
    int dz = 0;
    cardinalFromYaw(yaw, &dx, &dz);
    const int ix = worldToCell(playerPos.x()) + dx;
    const int iz = worldToCell(playerPos.z()) + dz;
    const int iy = getStackHeight(ix, iz);

    DynamicBox box;
    box.ix = ix;
    box.iy = iy;
    box.iz = iz;
    box.pos = center;
    box.sliding = false;
    box.falling = false;
    box.settle = false;
    box.velY = 0.0f;
    box.fallTime = 0.0f;
    box.squashTtl = 0.0f;
    box.slideT = 0.0f;
    box.slideDur = 0.01f;
    box.alive = true;
    box.crushDone = false;
    buildVisual(box);
    syncBox(box);
    m_boxes.push_back(box);
    m_placed = true;
    std::cout << "[box] place (" << ix << ", " << iy << ", " << iz << ")\n";
    return true;
}

int LocalBoxWorld::frontIndex(const osg::Vec3& playerPos, float yaw) const
{
    int dx = 0;
    int dz = 0;
    cardinalFromYaw(yaw, &dx, &dz);
    const int fx = worldToCell(playerPos.x()) + dx;
    const int fz = worldToCell(playerPos.z()) + dz;
    for (int y = 0; y < kMaxStack; ++y) {
        const DynamicBox* b = findBox(fx, y, fz);
        if (b != nullptr && !b->sliding && !b->falling) {
            return static_cast<int>(b - &m_boxes[0]);
        }
    }
    return -1;
}

bool LocalBoxWorld::tryPush(const osg::Vec3& playerPos, float yaw)
{
    int dx = 0;
    int dz = 0;
    cardinalFromYaw(yaw, &dx, &dz);
    const int fx = worldToCell(playerPos.x()) + dx;
    const int fz = worldToCell(playerPos.z()) + dz;
    DynamicBox* target = nullptr;
    for (int y = 0; y < kMaxStack; ++y) {
        DynamicBox* b = findBox(fx, y, fz);
        if (b != nullptr && !b->sliding && !b->falling) {
            target = b;
            break;
        }
    }
    if (target == nullptr) {
        return false;
    }

    int steps = 0;
    for (int s = 1; s <= kMaxPush; ++s) {
        const int nx = target->ix + dx * s;
        const int nz = target->iz + dz * s;
        if (occupied(nx, target->iy, nz)) {
            break;
        }
        steps = s;
    }
    if (steps <= 0) {
        return false;
    }

    target->slideFrom = target->pos;
    target->ix += dx * steps;
    target->iz += dz * steps;
    target->slideTo = cellCenter(target->ix, target->iy, target->iz);
    target->slideT = 0.0f;
    target->slideDur = static_cast<float>(steps) * CELL_SIZE / kSlideSpeed;
    if (target->slideDur < 0.05f) {
        target->slideDur = 0.05f;
    }
    target->sliding = true;
    std::cout << "[box] push " << steps << " cells\n";
    return true;
}

void LocalBoxWorld::settleBoxes()
{
    // 95. Integridad: BFS 6-vecinos. Solo cae el cluster sin ruta a iy==0.
    const int nbs[6][3] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
    };
    std::vector<char> seen(m_boxes.size(), 0);
    for (size_t seed = 0; seed < m_boxes.size(); ++seed) {
        DynamicBox& origin = m_boxes[seed];
        if (!origin.alive || origin.falling || origin.sliding || seen[seed]) {
            continue;
        }
        std::vector<int> comp;
        std::vector<int> stack;
        stack.push_back(static_cast<int>(seed));
        seen[seed] = 1;
        bool grounded = false;
        while (!stack.empty()) {
            const int id = stack.back();
            stack.pop_back();
            comp.push_back(id);
            const DynamicBox& b = m_boxes[static_cast<size_t>(id)];
            if (b.iy <= 0) {
                grounded = true;
            }
            for (int n = 0; n < 6; ++n) {
                const int nid = indexOfCell(b.ix + nbs[n][0], b.iy + nbs[n][1], b.iz + nbs[n][2]);
                if (nid < 0 || seen[static_cast<size_t>(nid)]) {
                    continue;
                }
                DynamicBox& nb = m_boxes[static_cast<size_t>(nid)];
                if (!nb.alive || nb.falling) {
                    continue;
                }
                seen[static_cast<size_t>(nid)] = 1;
                stack.push_back(nid);
            }
        }
        if (grounded) {
            continue;
        }

        int drop = 0;
        for (;;) {
            const int next = drop + 1;
            bool ok = true;
            for (size_t c = 0; c < comp.size(); ++c) {
                const DynamicBox& b = m_boxes[static_cast<size_t>(comp[c])];
                const int ny = b.iy - next;
                if (ny < 0) {
                    ok = false;
                    break;
                }
                const int occ = indexOfCell(b.ix, ny, b.iz);
                if (occ < 0) {
                    continue;
                }
                bool inComp = false;
                for (size_t k = 0; k < comp.size(); ++k) {
                    if (comp[k] == occ) {
                        inComp = true;
                        break;
                    }
                }
                const DynamicBox& ob = m_boxes[static_cast<size_t>(occ)];
                if (!inComp && ob.alive && !ob.falling) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                break;
            }
            drop = next;
            if (drop > kMaxStack) {
                break;
            }
        }
        if (drop <= 0) {
            continue;
        }
        for (size_t c = 0; c < comp.size(); ++c) {
            DynamicBox& b = m_boxes[static_cast<size_t>(comp[c])];
            b.iy -= drop;
            b.falling = true;
            b.settle = true;
            b.velY = 0.0f;
            b.fallTime = 0.0f;
            b.crushDone = false;
            m_fell = true;
        }
    }
}

bool LocalBoxWorld::isStructureGrounded(int ix, int iy, int iz) const
{
    const int start = indexOfCell(ix, iy, iz);
    if (start < 0) {
        return iy <= 0;
    }
    const int nbs[6][3] = {
        { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
    };
    std::vector<char> seen(m_boxes.size(), 0);
    std::vector<int> stack;
    stack.push_back(start);
    seen[static_cast<size_t>(start)] = 1;
    while (!stack.empty()) {
        const int id = stack.back();
        stack.pop_back();
        const DynamicBox& b = m_boxes[static_cast<size_t>(id)];
        if (b.iy <= 0) {
            return true;
        }
        for (int n = 0; n < 6; ++n) {
            const int nid = indexOfCell(b.ix + nbs[n][0], b.iy + nbs[n][1], b.iz + nbs[n][2]);
            if (nid < 0 || seen[static_cast<size_t>(nid)]) {
                continue;
            }
            if (!m_boxes[static_cast<size_t>(nid)].alive || m_boxes[static_cast<size_t>(nid)].falling) {
                continue;
            }
            seen[static_cast<size_t>(nid)] = 1;
            stack.push_back(nid);
        }
    }
    return false;
}

void LocalBoxWorld::update(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    bool needSettle = false;
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        DynamicBox& b = m_boxes[i];
        if (!b.alive) {
            continue;
        }

        if (b.sliding) {
            b.slideT += dt;
            float u = b.slideT / b.slideDur;
            if (u >= 1.0f) {
                u = 1.0f;
                b.sliding = false;
                b.pos = b.slideTo;
                needSettle = true;
            } else {
                b.pos = b.slideFrom * (1.0f - u) + b.slideTo * u;
            }
        }

        if (b.falling) {
            b.fallTime += dt;
            b.velY -= BOX_GRAVITY * dt;
            b.pos.y() += b.velY * dt;
            b.pos.x() += std::sin(b.fallTime * 14.0f) * 0.012f;
            const float destY = (static_cast<float>(b.iy) + 0.5f) * CELL_SIZE;
            if (b.pos.y() <= destY) {
                b.pos = cellCenter(b.ix, b.iy, b.iz);
                b.falling = false;
                b.settle = false;
                b.velY = 0.0f;
                b.squashTtl = kSquashTime;
                needSettle = true;
            }
        }

        if (b.squashTtl > 0.0f) {
            b.squashTtl -= dt;
            if (b.squashTtl < 0.0f) {
                b.squashTtl = 0.0f;
            }
        }
        syncBox(b);
    }

    if (needSettle) {
        settleBoxes();
    }
}

bool LocalBoxWorld::rayHits(const osg::Vec3& from, const osg::Vec3& to, float* hitT) const
{
    const osg::Vec3 d = to - from;
    const float len = d.length();
    if (len < 0.001f) {
        return false;
    }
    const int steps = 12;
    for (int s = 1; s < steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const osg::Vec3 p = from + d * t;
        const int ix = worldToCell(p.x());
        const int iy = worldToCell(p.y());
        const int iz = worldToCell(p.z());
        if (occupied(ix, iy, iz)) {
            if (hitT != nullptr) {
                *hitT = t;
            }
            return true;
        }
    }
    return false;
}

bool LocalBoxWorld::boxAlive(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_boxes.size())) {
        return false;
    }
    return m_boxes[static_cast<size_t>(index)].alive;
}

osg::Vec3 LocalBoxWorld::boxPos(int index) const
{
    if (!boxAlive(index)) {
        return osg::Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_boxes[static_cast<size_t>(index)].pos;
}

bool LocalBoxWorld::boxCell(int index, int* ix, int* iy, int* iz) const
{
    if (!boxAlive(index)) {
        return false;
    }
    const DynamicBox& b = m_boxes[static_cast<size_t>(index)];
    if (ix != nullptr) {
        *ix = b.ix;
    }
    if (iy != nullptr) {
        *iy = b.iy;
    }
    if (iz != nullptr) {
        *iz = b.iz;
    }
    return true;
}

int LocalBoxWorld::nearestIndex(const osg::Vec3& playerPos) const
{
    int best = -1;
    float bestD = 1.0e30f;
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        const DynamicBox& b = m_boxes[i];
        if (!b.alive) {
            continue;
        }
        const float dx = b.pos.x() - playerPos.x();
        const float dy = b.pos.y() - playerPos.y();
        const float dz = b.pos.z() - playerPos.z();
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void LocalBoxWorld::collectSorted(const osg::Vec3& playerPos, std::vector<int>& out) const
{
    out.clear();
    for (size_t i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes[i].alive) {
            out.push_back(static_cast<int>(i));
        }
    }
    for (size_t a = 0; a < out.size(); ++a) {
        size_t best = a;
        float bestD = 1.0e30f;
        for (size_t b = a; b < out.size(); ++b) {
            const osg::Vec3& p = m_boxes[static_cast<size_t>(out[b])].pos;
            const float dx = p.x() - playerPos.x();
            const float dy = p.y() - playerPos.y();
            const float dz = p.z() - playerPos.z();
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < bestD) {
                bestD = d;
                best = b;
            }
        }
        const int tmp = out[a];
        out[a] = out[best];
        out[best] = tmp;
    }
}

bool LocalBoxWorld::destroyBox(int index, osg::Vec3* outPos)
{
    if (!boxAlive(index)) {
        return false;
    }
    DynamicBox& b = m_boxes[static_cast<size_t>(index)];
    if (outPos != nullptr) {
        *outPos = b.pos;
    }
    b.alive = false;
    b.sliding = false;
    b.falling = false;
    if (m_root.valid() && b.node.valid()) {
        m_root->removeChild(b.node.get());
    }
    b.node = nullptr;
    settleBoxes();
    return true;
}

int LocalBoxWorld::destroyLineFrom(int index, const osg::Vec3& playerPos, int maxN,
                                   std::vector<osg::Vec3>& outPos)
{
    // 75.1 Hasta 3 cubos en eje cardinal desde el seleccionado.
    if (!boxAlive(index) || maxN < 1) {
        return 0;
    }
    const DynamicBox& seed = m_boxes[static_cast<size_t>(index)];
    int dx = 0;
    int dz = 0;
    cardinalFromYaw(std::atan2(seed.pos.x() - playerPos.x(), seed.pos.z() - playerPos.z()), &dx, &dz);

    int ids[3];
    int nIds = 0;
    int cx = seed.ix;
    int cy = seed.iy;
    int cz = seed.iz;
    for (int n = 0; n < maxN && n < 3; ++n) {
        DynamicBox* hit = findBox(cx, cy, cz);
        if (hit == nullptr) {
            break;
        }
        ids[nIds] = static_cast<int>(hit - &m_boxes[0]);
        nIds += 1;
        cx += dx;
        cz += dz;
    }
    int killed = 0;
    for (int i = 0; i < nIds; ++i) {
        osg::Vec3 p;
        if (destroyBox(ids[i], &p)) {
            outPos.push_back(p);
            killed += 1;
        }
    }
    return killed;
}

int LocalBoxWorld::explodeBombAt(int ix, int iy, int iz, std::vector<osg::Vec3>& outPos)
{
    // 76.2 Piso 3x3: centro + 8 vecinos.
    int ids[9];
    int nIds = 0;
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oz = -1; oz <= 1; ++oz) {
            DynamicBox* hit = findBox(ix + ox, iy, iz + oz);
            if (hit == nullptr) {
                continue;
            }
            ids[nIds] = static_cast<int>(hit - &m_boxes[0]);
            nIds += 1;
        }
    }
    int killed = 0;
    for (int i = 0; i < nIds; ++i) {
        osg::Vec3 p;
        if (destroyBox(ids[i], &p)) {
            outPos.push_back(p);
            killed += 1;
        }
    }
    return killed;
}

} // namespace standalone
} // namespace rc
