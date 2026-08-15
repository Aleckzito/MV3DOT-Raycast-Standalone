#ifndef RC_OSG_DEBUG_FLOOR_H
#define RC_OSG_DEBUG_FLOOR_H

#include <osg/Node>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 7.1 Plataforma gris en y=0, alineada a TILE_SIZE entero.
class OsgDebugFloor {
public:
    OsgDebugFloor();

    osg::Node* getNode();

private:
    osg::ref_ptr<osg::Node> m_root;
};

} // namespace standalone
} // namespace rc

#endif
