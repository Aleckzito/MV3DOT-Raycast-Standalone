#ifndef RC_DATA_ROOT_H
#define RC_DATA_ROOT_H

#include <string>

namespace rc {
namespace standalone {

// Raiz unica de datos: contenido, mapas, scripts, plantillas y partidas.
//
// CMake copia data/ y config.json junto al exe, asi que en disco pueden existir
// varios arboles data/ validos a la vez. La regla es una sola y **no depende del
// CWD**: manda la ubicacion del ejecutable.
//
//   1. Repo completo (data/content/registry_catalog.json + src/standalone),
//      buscando desde el directorio del exe hacia arriba; el CWD va el ultimo.
//   2. Distribucion suelta: solo catalogo, en ese mismo orden.
//   3. Sin catalogo: findRepoRoot() (marcador config.json).
//
// Asi el mismo binario elige siempre la misma raiz, aunque lo lances parado en
// otro clon del proyecto. Se resuelve una vez por proceso y se loguea al arrancar.
const std::string& dataRoot();

// dataRoot() + relative. No comprueba existencia: sirve para escribir.
std::string dataPath(const std::string& relative);

} // namespace standalone
} // namespace rc

#endif
