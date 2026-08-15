#include "LocalInventory.h"

#include "DataRoot.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace rc {
namespace standalone {

bool LocalInventory::load(const std::string& relativePath)
{
    m_stacks.clear();
    // Ya viene resuelta por playerLoadPath/playerSavePath; no re-resolver.
    const std::string path = dataPath(relativePath);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception& ex) {
        std::cerr << "[inv] parse: " << ex.what() << "\n";
        return false;
    }
    if (!doc.contains("slots") || !doc["slots"].is_array()) {
        return true;
    }
    for (const nlohmann::json& row : doc["slots"]) {
        if (!row.is_object()) {
            continue;
        }
        const std::string id = row.value("contentId", "");
        const int n = row.value("count", 0);
        if (!id.empty() && n > 0) {
            m_stacks[id] = n;
        }
    }
    return true;
}

bool LocalInventory::save(const std::string& relativePath) const
{
    nlohmann::json slots = nlohmann::json::array();
    for (auto it = m_stacks.begin(); it != m_stacks.end(); ++it) {
        if (it->second <= 0) {
            continue;
        }
        nlohmann::json row;
        row["contentId"] = it->first;
        row["count"] = it->second;
        slots.push_back(row);
    }
    nlohmann::json doc;
    doc["schema"] = "otraycast.player.inventory.v1";
    doc["version"] = 1;
    doc["slots"] = slots;
    // Ya viene resuelta por playerLoadPath/playerSavePath; no re-resolver.
    const std::string path = dataPath(relativePath);
    std::filesystem::path outPath(path.empty() ? relativePath : path);
    if (outPath.has_parent_path()) {
        std::filesystem::create_directories(outPath.parent_path());
    }
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        return false;
    }
    out << doc.dump(2) << "\n";
    return true;
}

void LocalInventory::add(const std::string& contentId, int count)
{
    if (contentId.empty() || count == 0) {
        return;
    }
    m_stacks[contentId] += count;
    if (m_stacks[contentId] < 0) {
        m_stacks[contentId] = 0;
    }
}

int LocalInventory::count(const std::string& contentId) const
{
    const auto it = m_stacks.find(contentId);
    if (it == m_stacks.end()) {
        return 0;
    }
    return it->second;
}

} // namespace standalone
} // namespace rc
