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

// Bases candidatas. El ejecutable manda: primero su directorio y los padres,
// y solo despues el CWD. Si el binario vive en un repo, ese repo gana siempre,
// aunque lo lances parado en otro clon.
std::vector<std::filesystem::path> candidateBases()
{
    std::vector<std::filesystem::path> bases;
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
    bases.push_back(std::filesystem::current_path());
    return bases;
}

bool hasCatalog(const std::filesystem::path& base)
{
    return std::filesystem::exists(base / "data" / "content" / "registry_catalog.json");
}

// El repo de verdad trae las fuentes al lado del data/.
bool hasSources(const std::filesystem::path& base)
{
    return std::filesystem::exists(base / "src" / "standalone" / "StandaloneEngine.cpp");
}

std::string resolveRootOnce()
{
    const std::vector<std::filesystem::path> bases = candidateBases();

    // 1. Repo completo: catalogo + fuentes. Exe primero, CWD al final.
    for (size_t i = 0; i < bases.size(); ++i) {
        if (hasCatalog(bases[i]) && hasSources(bases[i])) {
            return bases[i].lexically_normal().generic_string();
        }
    }
    // 2. Distribucion suelta: solo catalogo, mismo orden.
    for (size_t i = 0; i < bases.size(); ++i) {
        if (hasCatalog(bases[i])) {
            return bases[i].lexically_normal().generic_string();
        }
    }
    // 3. Sin catalogo en ningun lado: marcador config.json.
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
