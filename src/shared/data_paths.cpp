#include "data_paths.h"

#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rc {

namespace {

bool fileExists(const std::string& path)
{
    if (path.empty()) return false;
    const std::filesystem::path fsPath(path);
    return std::filesystem::exists(fsPath);
}

std::string normalizeExistingPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

void appendUnique(std::vector<std::string>& roots, const std::string& root)
{
    if (root.empty()) return;
    const std::string normalized = std::filesystem::path(root).lexically_normal().generic_string();
    for (const std::string& existing : roots) {
        if (existing == normalized) return;
    }
    roots.push_back(normalized);
}

} // namespace

std::string findRepoRoot()
{
    std::vector<std::string> roots;
    appendUnique(roots, std::filesystem::current_path().generic_string());

#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
        appendUnique(roots, exeDir.generic_string());
        appendUnique(roots, exeDir.parent_path().generic_string());
        appendUnique(roots, exeDir.parent_path().parent_path().generic_string());
    }
#endif

    for (const std::string& root : roots) {
        const std::filesystem::path configPath = std::filesystem::path(root) / "config.json";
        if (fileExists(configPath.generic_string())) {
            return normalizeExistingPath(configPath.parent_path());
        }
        const std::filesystem::path itemsPath =
            std::filesystem::path(root) / "data" / "items" / "items.json";
        if (fileExists(itemsPath.generic_string())) {
            return normalizeExistingPath(itemsPath.parent_path().parent_path().parent_path());
        }
    }

    return std::filesystem::current_path().lexically_normal().generic_string();
}

std::string findDataFile(const std::string& relativePath)
{
    if (relativePath.empty()) return {};

    const std::string repoRoot = findRepoRoot();
    const std::vector<std::string> candidates = {
        (std::filesystem::path(repoRoot) / relativePath).generic_string(),
        relativePath,
    };

#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
        for (const std::filesystem::path& base :
             {exeDir, exeDir.parent_path(), exeDir.parent_path().parent_path()}) {
            const std::string candidate = (base / relativePath).generic_string();
            if (fileExists(candidate)) {
                return normalizeExistingPath(base / relativePath);
            }
        }
    }
#endif

    for (const std::string& candidate : candidates) {
        if (fileExists(candidate)) {
            return normalizeExistingPath(std::filesystem::path(candidate));
        }
    }

    return relativePath;
}

} // namespace rc
