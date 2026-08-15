#ifndef RC_LOCAL_CONTENT_REGISTRY_H
#define RC_LOCAL_CONTENT_REGISTRY_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rc {
namespace standalone {

struct ContentAlias {
    std::string namespaceName;
    std::string id;
    std::string purpose;
};

struct ContentEntry {
    std::string contentId;
    uint16_t runtimeId = 0;
    std::string kind;
    std::string status;
    std::string displayName;
    std::string definitionPath;
    std::vector<ContentAlias> externalAliases;
};

struct ActorStats {
    int maxHp = 0;
    float speed = 0.0f;
    int attackDamage = 0;
    float cruiseAlt = 0.0f;
    int segmentHp = 0;
    bool valid = false;
};

struct AbilityStats {
    int damage = 0;
    float speed = 0.0f;
    float range = 0.0f;
    float duration = 0.0f;
    int energyCost = 0;
    bool homing = false;
    bool valid = false;
};

struct SpawnPoint {
    std::string kind;
    std::string contentId;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// S3. playerSpawn del meta del mundo. valid=false => default C++.
struct WorldSpawn {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool valid = false;
};

struct LootRow {
    int exp = 0;
    int smallHpChance = 0;
    int largeHpChance = 0;
    int energyChance = 0;
    int largeHpCount = 1;
    float staminaBonus = 0.0f;
};

struct NpcSpawn {
    std::string contentId;
    std::string name;
    std::string talkId;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool valid = false;
};

// S2. Catalog + actor/ability JSON. WARN on miss; defaults stay in C++.
class LocalContentRegistry {
public:
    // metaRelative vacio = meta del pack sandbox. Se pasa explicito cuando el
    // mapa lo trae al lado (<mapa>.meta.json), para que un mapa suelto cargue
    // sus propios spawns sin tocar archivos versionados.
    bool loadAll(const std::string& metaRelative = std::string());

    const ContentEntry* byContentId(const std::string& contentId) const;
    ActorStats actor(const std::string& contentId) const;
    AbilityStats ability(const std::string& contentId) const;
    LootRow loot(const std::string& kind) const;
    bool hasLoot(const std::string& kind) const;
    const std::vector<SpawnPoint>& spawns() const { return m_spawns; }
    bool hasSpawns() const { return !m_spawns.empty(); }
    const std::vector<NpcSpawn>& npcs() const { return m_npcSpawns; }

    // S3. Mapa de voxeles declarado por el meta + spawn del jugador.
    const std::string& voxelWorldPath() const { return m_voxelWorld; }
    const WorldSpawn& playerSpawn() const { return m_playerSpawn; }

    int hunterDamage() const;
    float gunSpeed() const;
    float hunterRange() const;
    float hunterLife() const;
    int energyPickup() const;

    size_t catalogSize() const { return m_entries.size(); }

    // Ruta real de un recurso data/: repo root, exe dir o CWD. Devuelve la
    // relativa tal cual si no existe en ninguno.
    static std::string resolve(const std::string& relative);

private:
    bool loadCatalog(const std::string& path);
    bool loadDefinition(const ContentEntry& entry);
    bool loadMeta(const std::string& path);
    bool loadLoot(const std::string& path);

    std::vector<ContentEntry> m_entries;
    std::unordered_map<std::string, size_t> m_byId;
    std::unordered_map<std::string, ActorStats> m_actors;
    std::unordered_map<std::string, AbilityStats> m_abilities;
    std::vector<SpawnPoint> m_spawns;
    std::vector<NpcSpawn> m_npcSpawns;
    std::string m_voxelWorld;
    WorldSpawn m_playerSpawn;
    std::unordered_map<std::string, LootRow> m_loot;
    int m_energyPickup = 3;
};

} // namespace standalone
} // namespace rc

#endif
