#ifndef RC_PLAYER_SAVE_H
#define RC_PLAYER_SAVE_H

#include <string>

namespace rc {
namespace standalone {

// S7/S9. Separa plantilla de partida:
//   data/player/<x>.json        -> versionado, estado inicial, nunca se escribe
//   data/player/save/<x>.json   -> partida local, ignorada por git
// Asi jugar no ensucia el diff del repo.

// Ruta de lectura: la partida si existe, si no la plantilla versionada.
std::string playerLoadPath(const std::string& templateRelative);

// Ruta de escritura: siempre bajo data/player/save.
std::string playerSavePath(const std::string& templateRelative);

} // namespace standalone
} // namespace rc

#endif
