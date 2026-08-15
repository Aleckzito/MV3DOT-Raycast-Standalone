#ifndef RC_LOCAL_BOX_WORLD_H
#define RC_LOCAL_BOX_WORLD_H

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Vec3>
#include <osg/ref_ptr>

#include <vector>

namespace rc {
namespace standalone {

const float CELL_SIZE = 0.5f;
const float BOX_GRAVITY = 28.0f;

// 70.1 Cubo dinamico: slide hielo, caida, squash.
struct DynamicBox {
    osg::Vec3 pos;
    bool sliding;
    bool falling;
    bool settle;
    osg::ref_ptr<osg::MatrixTransform> node;

    int ix;
    int iy;
    int iz;
    float velY;
    float fallTime;
    float squashTtl;
    osg::Vec3 slideFrom;
    osg::Vec3 slideTo;
    float slideT;
    float slideDur;
    bool alive;
    bool crushDone;
};

class LocalBoxWorld {
public:
    LocalBoxWorld();

    osg::Node* getNode();
    void update(float dt);

    bool placeBox(const osg::Vec3& playerPos, float yaw);
    bool queryPlaceSlot(const osg::Vec3& playerPos, float yaw, osg::Vec3* outPos) const;
    bool tryPush(const osg::Vec3& playerPos, float yaw);
    int frontIndex(const osg::Vec3& playerPos, float yaw) const;
    void settleBoxes();
    bool isStructureGrounded(int ix, int iy, int iz) const;
    bool boxFalling(int index) const;
    bool tryMarkCrush(int index);

    int getStackHeight(int ix, int iz) const;
    bool occupied(int ix, int iy, int iz) const;
    bool rayHits(const osg::Vec3& from, const osg::Vec3& to, float* hitT) const;
    bool consumeFell() { const bool v = m_fell; m_fell = false; return v; }
    bool consumePlaced() { const bool v = m_placed; m_placed = false; return v; }

    int boxCount() const { return static_cast<int>(m_boxes.size()); }

    int nearestIndex(const osg::Vec3& playerPos) const;
    void collectSorted(const osg::Vec3& playerPos, std::vector<int>& out) const;
    bool boxAlive(int index) const;
    osg::Vec3 boxPos(int index) const;
    bool boxCell(int index, int* ix, int* iy, int* iz) const;
    bool destroyBox(int index, osg::Vec3* outPos);
    int destroyLineFrom(int index, const osg::Vec3& playerPos, int maxN, std::vector<osg::Vec3>& outPos);
    int explodeBombAt(int ix, int iy, int iz, std::vector<osg::Vec3>& outPos);

private:
    void buildVisual(DynamicBox& box);
    void syncBox(DynamicBox& box);
    int worldToCell(float world) const;
    osg::Vec3 cellCenter(int ix, int iy, int iz) const;
    DynamicBox* findBox(int ix, int iy, int iz);
    const DynamicBox* findBox(int ix, int iy, int iz) const;
    int indexOfCell(int ix, int iy, int iz) const;
    void cardinalFromYaw(float yaw, int* dx, int* dz) const;

    osg::ref_ptr<osg::Group> m_root;
    std::vector<DynamicBox> m_boxes;
    bool m_fell;
    bool m_placed;
};

} // namespace standalone
} // namespace rc

#endif
