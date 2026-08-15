#include "VoxelMaterials.h"

namespace rc {
namespace standalone {

namespace {

struct MaterialDef {
    osg::Vec4 color;
    osg::Vec4 ambient;
    bool liquid;
};

// Paleta pastel: saturacion baja y luminosidad alta, para que el relieve se lea
// por la iluminacion de las caras y no por contraste de color.
const MaterialDef kStone = {
    osg::Vec4(0.72f, 0.74f, 0.78f, 1.0f), osg::Vec4(0.28f, 0.30f, 0.32f, 1.0f), false
};
const MaterialDef kBrick = {
    osg::Vec4(0.78f, 0.22f, 0.10f, 1.0f), osg::Vec4(0.32f, 0.08f, 0.04f, 1.0f), false
};
const MaterialDef kGrass = {
    osg::Vec4(0.62f, 0.84f, 0.58f, 1.0f), osg::Vec4(0.24f, 0.34f, 0.22f, 1.0f), false
};
const MaterialDef kSand = {
    osg::Vec4(0.94f, 0.88f, 0.68f, 1.0f), osg::Vec4(0.36f, 0.34f, 0.26f, 1.0f), false
};
const MaterialDef kEarth = {
    osg::Vec4(0.76f, 0.62f, 0.50f, 1.0f), osg::Vec4(0.30f, 0.24f, 0.19f, 1.0f), false
};
const MaterialDef kWater = {
    osg::Vec4(0.55f, 0.78f, 0.90f, 0.62f), osg::Vec4(0.20f, 0.30f, 0.36f, 0.62f), true
};

const MaterialDef& defOf(uint16_t materialId)
{
    switch (materialId) {
        case MAT_BRICK: return kBrick;
        case MAT_GRASS: return kGrass;
        case MAT_SAND:  return kSand;
        case MAT_EARTH: return kEarth;
        case MAT_WATER: return kWater;
        case MAT_STONE:
        default:        return kStone;
    }
}

} // namespace

osg::Vec4 materialColor(uint16_t materialId)
{
    return defOf(materialId).color;
}

osg::Vec4 materialAmbient(uint16_t materialId)
{
    return defOf(materialId).ambient;
}

bool materialIsLiquid(uint16_t materialId)
{
    return defOf(materialId).liquid;
}

} // namespace standalone
} // namespace rc
