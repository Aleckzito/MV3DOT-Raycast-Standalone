#ifndef RC_OSG_PHANTOM_RENDERER_H
#define RC_OSG_PHANTOM_RENDERER_H

#include "PhantomCursor.h"

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 6. Cubito fantasma cyan translucido. Tamano exacto MINI_VOXEL_SIZE.
class OsgPhantomRenderer {
public:
    OsgPhantomRenderer();

    osg::Node* getNode();
    void setPosition(const SnappedPosition& pose);

private:
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
