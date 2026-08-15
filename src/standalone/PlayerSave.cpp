#include "PlayerSave.h"

#include "data_paths.h"

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
    const std::string root = rc::findRepoRoot();
    const std::filesystem::path base =
        std::filesystem::path(root.empty() ? "." : root) / "data" / "player" / "save";
    return (base / fileNameOf(templateRelative)).lexically_normal().generic_string();
}

std::string playerLoadPath(const std::string& templateRelative)
{
    const std::string save = playerSavePath(templateRelative);
    if (!save.empty() && std::filesystem::exists(save)) {
        return save;
    }
    return rc::findDataFile(templateRelative);
}

} // namespace standalone
} // namespace rc
