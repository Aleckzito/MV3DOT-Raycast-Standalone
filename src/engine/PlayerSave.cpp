#include "PlayerSave.h"

#include "DataRoot.h"

#include <filesystem>

namespace rc {
namespace standalone {

namespace {

std::string fileNameOf(const std::string& relative)
{
    return std::filesystem::path(relative).filename().generic_string();
}

} // namespace

std::string playerSavePath(const std::string& templateRelative)
{
    // Misma raiz que el contenido: partida y plantilla nunca salen de arboles distintos.
    return dataPath("data/player/save/" + fileNameOf(templateRelative));
}

std::string playerLoadPath(const std::string& templateRelative)
{
    const std::string save = playerSavePath(templateRelative);
    if (!save.empty() && std::filesystem::exists(save)) {
        return save;
    }
    return dataPath(templateRelative);
}

} // namespace standalone
} // namespace rc
