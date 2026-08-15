#ifndef RC_PHANTOM_CURSOR_H
#define RC_PHANTOM_CURSOR_H

namespace rc {
namespace standalone {

class MiniVoxelGrid;

// 5. Pose del cubito fantasma: origen de celda (min corner) + indices 1/3.
struct SnappedPosition {
    float x;
    float y;
    float z;
    int vx;
    int vy;
    int vz;
};

class PhantomCursor {
public:
    PhantomCursor();

    void resetDda();

    // 5.5 DDA 3D: origen (o celda previa) -> hit. grid puede ser nullptr.
    SnappedPosition calculateSnappedPosition(
        float originX, float originY, float originZ,
        float hitX, float hitY, float hitZ,
        const MiniVoxelGrid* grid);

    bool hasPose() const { return m_hasPrev; }
    SnappedPosition currentPose() const;

    // 10.4 Celda solida donde el DDA se detuvo (cara exterior = currentPose).
    bool hasHitSolid() const { return m_hasHitSolid; }
    int hitVx() const { return m_hitVx; }
    int hitVy() const { return m_hitVy; }
    int hitVz() const { return m_hitVz; }

private:
    bool m_hasPrev;
    int m_prevVx;
    int m_prevVy;
    int m_prevVz;

    bool m_hasHitSolid;
    int m_hitVx;
    int m_hitVy;
    int m_hitVz;
};

} // namespace standalone
} // namespace rc

#endif
