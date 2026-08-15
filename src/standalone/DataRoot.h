#ifndef RC_DATA_ROOT_H
#define RC_DATA_ROOT_H

#include <string>

namespace rc {
namespace standalone {

// Raiz unica de datos: contenido, mapas, scripts, plantillas y partidas.
//
// CMake copia data/ y config.json junto al exe, asi que en disco pueden existir
// varios arboles data/ validos a la vez. La regla es una sola: gana la base que
// tiene data/content/registry_catalog.json **y** src/standalone, es decir el
// repo de verdad; la copia junto al exe solo se usa si no hay repo (distribucion
// suelta). Se resuelve una vez por proceso y se loguea al arrancar.
const std::string& dataRoot();

// dataRoot() + relative. No comprueba existencia: sirve para escribir.
std::string dataPath(const std::string& relative);

} // namespace standalone
} // namespace rc

#endif
