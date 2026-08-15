#ifndef RC_LOCAL_QUEST_LOG_H
#define RC_LOCAL_QUEST_LOG_H

#include <string>
#include <unordered_map>
#include <vector>

namespace rc {
namespace standalone {

struct QuestDef {
    std::string contentId;
    std::string title;
    std::string journalEntryId;
    int killsNeeded = 1;
};

struct VocationDef {
    int id = 0;
    std::string name;
    std::string contentId;
};

// S9/S10. Storage JSON local. Autoridad C++; Luau solo se entera por hooks.
class LocalQuestLog {
public:
    bool load();
    bool save() const;

    bool onKill(int killCount);
    bool isDone(const std::string& contentId) const;
    std::string status(const std::string& contentId) const;

    int vocationId() const { return m_vocationId; }
    const char* vocationName() const;
    bool trySetVocation(int id);

    const std::string& lastEvent() const { return m_lastEvent; }

private:
    bool loadQuestFile(const std::string& path);
    bool loadVocations(const std::string& path);
    bool loadStorage(const std::string& path);

    std::vector<QuestDef> m_quests;
    std::vector<VocationDef> m_vocations;
    std::unordered_map<std::string, std::string> m_flags;
    int m_vocationId = 0;
    std::string m_storagePath;
    std::string m_lastEvent;
};

} // namespace standalone
} // namespace rc

#endif
