#include "StandaloneWorldIO.h"
#include "MiniVoxelGrid.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace rc {
namespace standalone {

bool saveWorld(const MiniVoxelGrid* grid, const std::string& filepath)
{
    // 11.2 / 11.3 Solo voxels activos: [vx, vy, vz, matId]
    if (grid == nullptr || filepath.empty()) {
        return false;
    }

    // El grid es un unordered_map: su orden de iteracion depende de la
    // implementacion de la stdlib. Se ordena por (vx, vy, vz) antes de escribir
    // para que el archivo sea reproducible entre compiladores y plataformas, no
    // solo dentro de la misma maquina.
    struct Row {
        int vx;
        int vy;
        int vz;
        uint16_t mat;
    };
    std::vector<Row> rows;
    const VoxelMap& map = grid->voxels();
    rows.reserve(map.size());
    for (VoxelMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (!it->second.isActive) {
            continue;
        }
        Row row;
        row.vx = it->first.vx;
        row.vy = it->first.vy;
        row.vz = it->first.vz;
        row.mat = it->second.materialId;
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.vx != b.vx) return a.vx < b.vx;
        if (a.vy != b.vy) return a.vy < b.vy;
        return a.vz < b.vz;
    });

    nlohmann::json voxels = nlohmann::json::array();
    for (size_t i = 0; i < rows.size(); ++i) {
        nlohmann::json row = nlohmann::json::array();
        row.push_back(rows[i].vx);
        row.push_back(rows[i].vy);
        row.push_back(rows[i].vz);
        row.push_back(rows[i].mat);
        voxels.push_back(row);
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
