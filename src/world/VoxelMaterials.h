#ifndef RC_VOXEL_MATERIALS_H
#define RC_VOXEL_MATERIALS_H

#include <cstdint>

#include <osg/Vec4>

namespace rc {
namespace standalone {

// Tabla de materiales del grid. El id vive en MiniVoxel::materialId y se
// serializa en data/worlds/*.json como cuarto elemento de cada voxel.
//
// 1 y 2 conservan su significado historico (gris de terreno y rojo de
// construccion), asi que los mapas ya guardados se siguen viendo igual.
// La paleta pastel empieza en 3.
enum VoxelMaterial : uint16_t {
    MAT_EMPTY = 0,
    MAT_STONE = 1,   // terreno y bloques colocados por el jugador
    MAT_BRICK = 2,   // construccion del Arquitecto
    MAT_GRASS = 3,
    MAT_SAND  = 4,
    MAT_EARTH = 5,
    MAT_WATER = 6
};

// Color difuso del material. Ids desconocidos caen en MAT_STONE.
osg::Vec4 materialColor(uint16_t materialId);

// Ambiente asociado, mas oscuro y con el mismo tinte.
osg::Vec4 materialAmbient(uint16_t materialId);

// El agua se dibuja translucida y no bloquea el paso visual.
bool materialIsLiquid(uint16_t materialId);

} // namespace standalone
} // namespace rc

#endif
