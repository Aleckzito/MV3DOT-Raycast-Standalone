#ifndef RC_LOCAL_SCRIPT_HOST_H
#define RC_LOCAL_SCRIPT_HOST_H

#include <string>

struct lua_State;

namespace rc {
namespace standalone {

// S5. Luau sandbox in-process. No sockets. Missing VM = no-op WARN.
class LocalScriptHost {
public:
    LocalScriptHost();
    ~LocalScriptHost();

    LocalScriptHost(const LocalScriptHost&) = delete;
    LocalScriptHost& operator=(const LocalScriptHost&) = delete;

    bool init();
    bool loadManifest(const std::string& relativePath);
    void fireHook(const std::string& name, const std::string& kind = std::string());
    bool processTalk(const std::string& who, const std::string& text);
    void tick(float dt);
    bool ready() const { return m_ready; }

private:
    bool loadScriptFile(const std::string& relativePath);
    void hardenSandbox();
    void registerHost();
    static int hostLog(lua_State* L);

    lua_State* m_L;
    bool m_ready;
    float m_heartbeat;
};

} // namespace standalone
} // namespace rc

#endif
