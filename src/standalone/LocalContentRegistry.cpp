#include "LocalContentRegistry.h"

#include "data_paths.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

std::string readText(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

nlohmann::json parseFile(const std::string& path)
{
    const std::string text = readText(path);
    if (text.empty()) {
        return nlohmann::json();
    }
    try {
        return nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        std::cerr << "[content] parse " << path << ": " << ex.what() << "\n";
        return nlohmann::json();
    }
}

std::string standaloneRoot()
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
    std::string copyRoot;
    for (size_t i = 0; i < bases.size(); ++i) {
        const std::filesystem::path& base = bases[i];
        const std::filesystem::path catalog =
            base / "data" / "content" / "registry_catalog.json";
        if (!std::filesystem::exists(catalog)) {
            continue;
        }
        const std::filesystem::path engineSrc =
            base / "src" / "standalone" / "StandaloneEngine.cpp";
        if (std::filesystem::exists(engineSrc)) {
            return base.lexically_normal().generic_string();
        }
        if (copyRoot.empty()) {
            copyRoot = base.lexically_normal().generic_string();
        }
    }
    return copyRoot;
}

} // namespace

std::string LocalContentRegistry::resolve(const std::string& relative)
{
    const std::filesystem::path rel(relative);
    if (rel.is_absolute() && std::filesystem::exists(rel)) {
        return rel.lexically_normal().generic_string();
    }
    const std::string root = standaloneRoot();
    if (!root.empty()) {
        const std::filesystem::path p = std::filesystem::path(root) / relative;
        if (std::filesystem::exists(p)) {
            return p.lexically_normal().generic_string();
        }
    }
    const std::string found = findDataFile(relative);
    if (!found.empty() && std::filesystem::exists(found)) {
        return found;
    }
    const std::filesystem::path cwd = std::filesystem::current_path() / relative;
    if (std::filesystem::exists(cwd)) {
        return cwd.lexically_normal().generic_string();
    }
    return relative;
}

bool LocalContentRegistry::loadAll()
{
    m_entries.clear();
    m_byId.clear();
    m_actors.clear();
    m_abilities.clear();
    m_spawns.clear();
    m_npcSpawns.clear();
    m_voxelWorld.clear();
    m_playerSpawn = WorldSpawn();
    m_loot.clear();
    m_energyPickup = 3;

    const std::string catalog = resolve("data/content/registry_catalog.json");
    if (!loadCatalog(catalog)) {
        std::cerr << "[content] WARN catalog missing, using C++ defaults\n";
        return false;
    }
    for (const ContentEntry& entry : m_entries) {
        if (entry.definitionPath.empty()) {
            continue;
        }
        loadDefinition(entry);
    }
    loadMeta(resolve("data/worlds/standalone_sandbox.meta.json"));
    loadLoot(resolve("data/loot/sandbox_loot.json"));
    std::cout << "[content] catalog=" << m_entries.size()
              << " actors=" << m_actors.size()
              << " abilities=" << m_abilities.size()
              << " spawns=" << m_spawns.size() << "\n";
    return true;
}

bool LocalContentRegistry::loadCatalog(const std::string& path)
{
    const nlohmann::json doc = parseFile(path);
    if (!doc.is_object() || !doc.contains("entries") || !doc["entries"].is_array()) {
        return false;
    }
    const std::string schema = doc.value("schema", "");
    if (!schema.empty() && schema != "otraycast.content_registry.catalog.v1") {
        std::cerr << "[content] unexpected schema: " << schema << "\n";
        return false;
    }
    for (const nlohmann::json& row : doc["entries"]) {
        if (!row.is_object()) {
            continue;
        }
        ContentEntry entry;
        entry.contentId = row.value("contentId", "");
        entry.runtimeId = static_cast<uint16_t>(row.value("runtimeId", 0));
        entry.kind = row.value("kind", "");
        entry.status = row.value("status", "");
        entry.displayName = row.value("displayName", "");
        entry.definitionPath = row.value("definitionPath", "");
        if (entry.contentId.empty()) {
            continue;
        }
        if (m_byId.count(entry.contentId) != 0) {
            std::cerr << "[content] duplicate contentId: " << entry.contentId << "\n";
            continue;
        }
        m_byId[entry.contentId] = m_entries.size();
        m_entries.push_back(entry);
    }
    return !m_entries.empty();
}

bool LocalContentRegistry::loadDefinition(const ContentEntry& entry)
{
    const std::string path = resolve(entry.definitionPath);
    const nlohmann::json doc = parseFile(path);
    if (!doc.is_object()) {
        std::cerr << "[content] WARN skip " << entry.definitionPath << "\n";
        return false;
    }
    const nlohmann::json combat = doc.contains("combat") && doc["combat"].is_object()
        ? doc["combat"]
        : nlohmann::json::object();
    if (entry.kind == "actor") {
        ActorStats stats;
        stats.maxHp = combat.value("maxHp", 0);
        stats.speed = combat.value("speed", 0.0f);
        stats.attackDamage = combat.value("attackDamage", 0);
        stats.cruiseAlt = combat.value("cruiseAlt", 0.0f);
        stats.segmentHp = combat.value("segmentHp", 0);
        stats.valid = true;
        m_actors[entry.contentId] = stats;
        return true;
    }
    if (entry.kind == "ability") {
        AbilityStats stats;
        stats.damage = combat.value("damage", 0);
        stats.speed = combat.value("speed", 0.0f);
        stats.range = combat.value("range", 0.0f);
        stats.duration = combat.value("duration", 0.0f);
        stats.energyCost = combat.value("energyCost", 0);
        stats.homing = combat.value("homing", false);
        stats.valid = true;
        m_abilities[entry.contentId] = stats;
        return true;
    }
    if (entry.kind == "item" && entry.contentId == "otr.item.sandbox.energy_cell") {
        m_energyPickup = combat.value("energyCells", m_energyPickup);
        return true;
    }
    if (entry.kind == "npc") {
        NpcSpawn npc;
        npc.contentId = entry.contentId;
        npc.name = doc.value("displayName", doc.value("name", "NPC"));
        npc.talkId = "npc";
        if (entry.contentId.find("oracle") != std::string::npos) {
            npc.talkId = "oracle";
        } else if (entry.contentId.find("merchant") != std::string::npos) {
            npc.talkId = "merchant";
        }
        if (doc.contains("spawnHint") && doc["spawnHint"].is_object()) {
            const nlohmann::json& hint = doc["spawnHint"];
            npc.x = hint.value("x", 0.0f);
            npc.y = hint.value("y", 0.0f);
            npc.z = hint.value("z", 0.0f);
        }
        npc.valid = true;
        m_npcSpawns.push_back(npc);
        return true;
    }
    return true;
}

bool LocalContentRegistry::loadMeta(const std::string& path)
{
    const nlohmann::json doc = parseFile(path);
    if (!doc.is_object()) {
        return false;
    }

    // S3. El meta declara el mapa de voxeles y donde arranca el jugador.
    m_voxelWorld = doc.value("voxelWorld", "");
    if (doc.contains("playerSpawn") && doc["playerSpawn"].is_object()) {
        const nlohmann::json& ps = doc["playerSpawn"];
        m_playerSpawn.x = ps.value("x", 0.0f);
        m_playerSpawn.y = ps.value("y", 0.0f);
        m_playerSpawn.z = ps.value("z", 0.0f);
        m_playerSpawn.valid = true;
    }

    if (!doc.contains("spawns") || !doc["spawns"].is_array()) {
        return false;
    }
    for (const nlohmann::json& row : doc["spawns"]) {
        if (!row.is_object()) {
            continue;
        }
        SpawnPoint sp;
        sp.kind = row.value("kind", "");
        sp.contentId = row.value("contentId", "");
        sp.x = row.value("x", 0.0f);
        sp.y = row.value("y", 0.0f);
        sp.z = row.value("z", 0.0f);
        if (!sp.kind.empty()) {
            m_spawns.push_back(sp);
        }
    }
    return true;
}

bool LocalContentRegistry::loadLoot(const std::string& path)
{
    const nlohmann::json doc = parseFile(path);
    if (!doc.is_object()) {
        return false;
    }
    m_energyPickup = doc.value("energyPickupCells", m_energyPickup);
    if (!doc.contains("tables") || !doc["tables"].is_object()) {
        return true;
    }
    for (auto it = doc["tables"].begin(); it != doc["tables"].end(); ++it) {
        if (!it.value().is_object()) {
            continue;
        }
        LootRow row;
        row.exp = it.value().value("exp", 0);
        row.smallHpChance = it.value().value("smallHpChance", 0);
        row.largeHpChance = it.value().value("largeHpChance", 0);
        row.energyChance = it.value().value("energyChance", 0);
        row.largeHpCount = it.value().value("largeHpCount", 1);
        row.staminaBonus = it.value().value("staminaBonus", 0.0f);
        m_loot[it.key()] = row;
    }
    return true;
}

const ContentEntry* LocalContentRegistry::byContentId(const std::string& contentId) const
{
    const auto it = m_byId.find(contentId);
    if (it == m_byId.end()) {
        return nullptr;
    }
    return &m_entries[it->second];
}

ActorStats LocalContentRegistry::actor(const std::string& contentId) const
{
    const auto it = m_actors.find(contentId);
    if (it == m_actors.end()) {
        return ActorStats();
    }
    return it->second;
}

AbilityStats LocalContentRegistry::ability(const std::string& contentId) const
{
    const auto it = m_abilities.find(contentId);
    if (it == m_abilities.end()) {
        return AbilityStats();
    }
    return it->second;
}

LootRow LocalContentRegistry::loot(const std::string& kind) const
{
    const auto it = m_loot.find(kind);
    if (it == m_loot.end()) {
        return LootRow();
    }
    return it->second;
}

bool LocalContentRegistry::hasLoot(const std::string& kind) const
{
    return m_loot.count(kind) != 0;
}

int LocalContentRegistry::hunterDamage() const
{
    const AbilityStats s = ability("otr.ability.sandbox.hunter_laser");
    return s.valid && s.damage > 0 ? s.damage : 34;
}

float LocalContentRegistry::gunSpeed() const
{
    const AbilityStats s = ability("otr.ability.sandbox.gun");
    return s.valid && s.speed > 0.0f ? s.speed : 30.0f;
}

float LocalContentRegistry::hunterRange() const
{
    const AbilityStats s = ability("otr.ability.sandbox.hunter_laser");
    return s.valid && s.range > 0.0f ? s.range : 20.0f;
}

float LocalContentRegistry::hunterLife() const
{
    const AbilityStats s = ability("otr.ability.sandbox.hunter_laser");
    return s.valid && s.duration > 0.0f ? s.duration : 0.10f;
}

int LocalContentRegistry::energyPickup() const
{
    return m_energyPickup > 0 ? m_energyPickup : 3;
}

} // namespace standalone
} // namespace rc
