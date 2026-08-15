#include "DataRoot.h"

#include "data_paths.h"

#include <filesystem>
#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rc {
namespace standalone {

namespace {

// Bases candidatas, en orden: CWD y luego el exe subiendo directorios.
std::vector<std::filesystem::path> candidateBases()
{
    std::vector<std::filesystem::path> bases;
    bases.push_back(std::filesystem::current_path());
#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
        bases.push_back(exeDir);
        bases.push_back(exeDir.parent_path());
        bases.push_back(exeDir.parent_path().parent_path());
        bases.push_back(exeDir.parent_path().parent_path().parent_path());
    }
#endif
    return bases;
}

std::string resolveRootOnce()
{
    const std::vector<std::filesystem::path> bases = candidateBases();
    std::string copyRoot;
    for (size_t i = 0; i < bases.size(); ++i) {
        const std::filesystem::path& base = bases[i];
        const std::filesystem::path catalog =
            base / "data" / "content" / "registry_catalog.json";
        if (!std::filesystem::exists(catalog)) {
            continue;
        }
        // El repo de verdad trae las fuentes al lado del data/.
        const std::filesystem::path engineSrc =
            base / "src" / "standalone" / "StandaloneEngine.cpp";
        if (std::filesystem::exists(engineSrc)) {
            return base.lexically_normal().generic_string();
        }
        if (copyRoot.empty()) {
            copyRoot = base.lexically_normal().generic_string();
        }
    }
    if (!copyRoot.empty()) {
        return copyRoot;
    }
    // Sin catalogo en ningun lado: ultimo recurso, el marcador config.json.
    return rc::findRepoRoot();
}

} // namespace

const std::string& dataRoot()
{
    static const std::string root = [] {
        const std::string r = resolveRootOnce();
        std::cout << "[data] root " << r << "\n";
        return r;
    }();
    return root;
}

std::string dataPath(const std::string& relative)
{
    if (relative.empty()) {
        return dataRoot();
    }
    const std::filesystem::path rel(relative);
    if (rel.is_absolute()) {
        return rel.lexically_normal().generic_string();
    }
    return (std::filesystem::path(dataRoot()) / rel).lexically_normal().generic_string();
}

} // namespace standalone
} // namespace rc
