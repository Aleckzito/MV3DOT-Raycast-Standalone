#ifndef RC_STANDALONE_WORLD_IO_H
#define RC_STANDALONE_WORLD_IO_H

#include <string>

namespace rc {
namespace standalone {

class MiniVoxelGrid;

// 11. Persistencia local JSON. Cero red / cero sockets.
bool saveWorld(const MiniVoxelGrid* grid, const std::string& filepath);
bool loadWorld(MiniVoxelGrid* grid, const std::string& filepath);

} // namespace standalone
} // namespace rc

#endif
