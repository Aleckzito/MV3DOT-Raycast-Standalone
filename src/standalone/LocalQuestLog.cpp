#include "LocalQuestLog.h"
#include "LocalContentRegistry.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace rc {
namespace standalone {

bool LocalQuestLog::load()
{
    m_quests.clear();
    m_vocations.clear();
    m_flags.clear();
    m_vocationId = 0;
    m_lastEvent.clear();
    loadQuestFile(LocalContentRegistry::resolve("data/quests/first_blood.json"));
    loadVocations(LocalContentRegistry::resolve("data/vocations/vocations_sandbox.json"));
    m_storagePath = LocalContentRegistry::resolve("data/player/sandbox_storage.json");
    loadStorage(m_storagePath);
    std::cout << "[quest] loaded defs=" << m_quests.size()
              << " vocations=" << m_vocations.size()
              << " vocationId=" << m_vocationId << "\n";
    return !m_quests.empty();
}

bool LocalQuestLog::loadQuestFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception& ex) {
        std::cerr << "[quest] parse: " << ex.what() << "\n";
        return false;
    }
    if (!doc.is_object()) {
        return false;
    }
    QuestDef q;
    q.contentId = doc.value("contentId", "");
    q.title = doc.value("title", "Quest");
    q.journalEntryId = doc.value("journalEntryId", q.contentId);
    q.killsNeeded = 1;
    if (doc.contains("completeOn") && doc["completeOn"].is_object()) {
        q.killsNeeded = doc["completeOn"].value("kills", 1);
    }
    if (!q.contentId.empty()) {
        m_quests.push_back(q);
        if (m_flags.find(q.contentId) == m_flags.end()) {
            const std::string st = doc.value("status", "idle");
            m_flags[q.contentId] = (st == "active" || st == "done") ? st : "idle";
        }
    }
    return true;
}

bool LocalQuestLog::loadVocations(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception&) {
        return false;
    }
    if (!doc.contains("vocations") || !doc["vocations"].is_array()) {
        return false;
    }
    for (const nlohmann::json& row : doc["vocations"]) {
        if (!row.is_object()) {
            continue;
        }
        VocationDef v;
        v.id = row.value("id", 0);
        v.name = row.value("name", "None");
        v.contentId = row.value("contentId", "");
        m_vocations.push_back(v);
    }
    return true;
}

bool LocalQuestLog::loadStorage(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception&) {
        return false;
    }
    m_vocationId = doc.value("vocationId", 0);
    if (doc.contains("flags") && doc["flags"].is_object()) {
        for (auto it = doc["flags"].begin(); it != doc["flags"].end(); ++it) {
            if (it.value().is_string()) {
                m_flags[it.key()] = it.value().get<std::string>();
            }
        }
    }
    return true;
}

bool LocalQuestLog::save() const
{
    nlohmann::json flags = nlohmann::json::object();
    for (auto it = m_flags.begin(); it != m_flags.end(); ++it) {
        flags[it->first] = it->second;
    }
    nlohmann::json doc;
    doc["schema"] = "otraycast.player.storage.v1";
    doc["version"] = 1;
    doc["vocationId"] = m_vocationId;
    doc["flags"] = flags;
    std::string path = m_storagePath;
    if (path.empty()) {
        path = LocalContentRegistry::resolve("data/player/sandbox_storage.json");
    }
    const std::filesystem::path outPath(path);
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

bool LocalQuestLog::onKill(int killCount)
{
    m_lastEvent.clear();
    bool changed = false;
    for (size_t i = 0; i < m_quests.size(); ++i) {
        const QuestDef& q = m_quests[i];
        std::string& st = m_flags[q.contentId];
        if (st.empty() || st == "idle") {
            st = "active";
            changed = true;
        }
        if (st == "done") {
            continue;
        }
        if (killCount >= q.killsNeeded) {
            st = "done";
            m_lastEvent = q.title;
            changed = true;
            std::cout << "[quest] DONE " << q.contentId << " (" << q.title << ")\n";
        }
    }
    if (changed) {
        save();
    }
    return !m_lastEvent.empty();
}

bool LocalQuestLog::isDone(const std::string& contentId) const
{
    return status(contentId) == "done";
}

std::string LocalQuestLog::status(const std::string& contentId) const
{
    const auto it = m_flags.find(contentId);
    if (it == m_flags.end()) {
        return "idle";
    }
    return it->second;
}

const char* LocalQuestLog::vocationName() const
{
    for (size_t i = 0; i < m_vocations.size(); ++i) {
        if (m_vocations[i].id == m_vocationId) {
            return m_vocations[i].name.c_str();
        }
    }
    return "None";
}

bool LocalQuestLog::trySetVocation(int id)
{
    if (m_vocationId != 0) {
        return false;
    }
    bool known = false;
    for (size_t i = 0; i < m_vocations.size(); ++i) {
        if (m_vocations[i].id == id) {
            known = true;
            break;
        }
    }
    if (!known || id == 0) {
        return false;
    }
    m_vocationId = id;
    save();
    std::cout << "[vocation] set " << vocationName() << " id=" << id << "\n";
    return true;
}

} // namespace standalone
} // namespace rc
