#include "StandaloneWorldIO.h"
#include "MiniVoxelGrid.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace rc {
namespace standalone {

bool saveWorld(const MiniVoxelGrid* grid, const std::string& filepath)
{
    // 11.2 / 11.3 Solo voxels activos: [vx, vy, vz, matId]
    if (grid == nullptr || filepath.empty()) {
        return false;
    }

    nlohmann::json voxels = nlohmann::json::array();
    const VoxelMap& map = grid->voxels();
    VoxelMap::const_iterator it = map.begin();
    while (it != map.end()) {
        if (it->second.isActive) {
            nlohmann::json row = nlohmann::json::array();
            row.push_back(it->first.vx);
            row.push_back(it->first.vy);
            row.push_back(it->first.vz);
            row.push_back(it->second.materialId);
            voxels.push_back(row);
        }
        ++it;
    }

    nlohmann::json doc;
    doc["version"] = 1;
    doc["voxels"] = voxels;

    const std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[world-io] save failed: " << filepath << "\n";
        return false;
    }
    out << doc.dump(2);
    out << "\n";
    std::cout << "[world-io] saved " << voxels.size() << " voxels -> " << filepath << "\n";
    return true;
}

bool loadWorld(MiniVoxelGrid* grid, const std::string& filepath)
{
    if (grid == nullptr || filepath.empty()) {
        return false;
    }

    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        std::cerr << "[world-io] load failed (missing): " << filepath << "\n";
        return false;
    }

    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception& ex) {
        std::cerr << "[world-io] parse error: " << ex.what() << "\n";
        return false;
    }

    const nlohmann::json* voxels = nullptr;
    if (doc.is_array()) {
        voxels = &doc;
    } else if (doc.is_object() && doc.contains("voxels") && doc["voxels"].is_array()) {
        voxels = &doc["voxels"];
    } else {
        std::cerr << "[world-io] invalid format (need array or {voxels:[]})\n";
        return false;
    }

    grid->clear();
    int loaded = 0;
    for (nlohmann::json::const_iterator it = voxels->begin(); it != voxels->end(); ++it) {
        if (!it->is_array() || it->size() < 4) {
            continue;
        }
        const int vx = it->at(0).get<int>();
        const int vy = it->at(1).get<int>();
        const int vz = it->at(2).get<int>();
        const uint16_t matId = it->at(3).get<uint16_t>();
        if (matId == 0) {
            continue;
        }
        grid->setVoxel(vx, vy, vz, matId);
        loaded += 1;
    }

    std::cout << "[world-io] loaded " << loaded << " voxels <- " << filepath << "\n";
    return true;
}

} // namespace standalone
} // namespace rc
