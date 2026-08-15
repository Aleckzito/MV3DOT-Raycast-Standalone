#ifndef RC_LOCAL_INVENTORY_H
#define RC_LOCAL_INVENTORY_H

#include <string>
#include <unordered_map>

namespace rc {
namespace standalone {

// S7. Single backpack. JSON persist. No SQLite.
class LocalInventory {
public:
    bool load(const std::string& relativePath);
    bool save(const std::string& relativePath) const;
    void add(const std::string& contentId, int count);
    int count(const std::string& contentId) const;

private:
    std::unordered_map<std::string, int> m_stacks;
};

} // namespace standalone
} // namespace rc

#endif
