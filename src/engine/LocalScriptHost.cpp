#include "LocalScriptHost.h"

#include "DataRoot.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#if defined(RC_HAS_LUAU)
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#endif

namespace rc {
namespace standalone {

namespace {

std::string readScript(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string resolveScript(const std::string& relative)
{
    // Misma raiz que el contenido: los .luau no pueden venir de otro arbol data/.
    const std::string found = dataPath(relative);
    return found.empty() ? relative : found;
}

} // namespace

LocalScriptHost::LocalScriptHost()
    : m_L(nullptr)
    , m_ready(false)
    , m_heartbeat(0.0f)
{
}

LocalScriptHost::~LocalScriptHost()
{
#if defined(RC_HAS_LUAU)
    if (m_L != nullptr) {
        lua_close(m_L);
        m_L = nullptr;
    }
#endif
}

int LocalScriptHost::hostLog(lua_State* L)
{
#if defined(RC_HAS_LUAU)
    const char* msg = luaL_optstring(L, 1, "");
    std::cout << "[luau] " << (msg != nullptr ? msg : "") << "\n";
#else
    (void)L;
#endif
    return 0;
}

bool LocalScriptHost::init()
{
#if !defined(RC_HAS_LUAU)
    std::cerr << "[script] WARN Luau not linked\n";
    m_ready = false;
    return false;
#else
    if (m_L != nullptr) {
        lua_close(m_L);
        m_L = nullptr;
    }
    m_L = luaL_newstate();
    if (m_L == nullptr) {
        std::cerr << "[script] WARN luaL_newstate failed\n";
        return false;
    }
    luaL_openlibs(m_L);
    hardenSandbox();
    registerHost();
    luaL_sandbox(m_L);
    luaL_sandboxthread(m_L);
    registerHost();
    m_ready = true;
    std::cout << "[script] Luau sandbox ready\n";
    return true;
#endif
}

void LocalScriptHost::hardenSandbox()
{
#if defined(RC_HAS_LUAU)
    static const char* kBlocked[] = {
        "io", "os", "package", "debug", "require", "dofile",
        "loadfile", "loadstring", "load", "getfenv", "setfenv"
    };
    for (int i = 0; i < 11; ++i) {
        lua_pushnil(m_L);
        lua_setglobal(m_L, kBlocked[i]);
    }
#endif
}

void LocalScriptHost::registerHost()
{
#if defined(RC_HAS_LUAU)
    lua_newtable(m_L);
    lua_pushcfunction(m_L, &LocalScriptHost::hostLog, "log");
    lua_setfield(m_L, -2, "log");
    lua_setglobal(m_L, "host");
#endif
}

bool LocalScriptHost::loadScriptFile(const std::string& relativePath)
{
#if !defined(RC_HAS_LUAU)
    (void)relativePath;
    return false;
#else
    const std::string path = resolveScript(relativePath);
    const std::string src = readScript(path);
    if (src.empty()) {
        std::cerr << "[script] WARN missing " << relativePath << "\n";
        return false;
    }
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(src.c_str(), src.size(), nullptr, &bytecodeSize);
    if (bytecode == nullptr) {
        std::cerr << "[script] compile failed " << relativePath << "\n";
        return false;
    }
    const int loadStatus = luau_load(m_L, relativePath.c_str(), bytecode, bytecodeSize, 0);
    std::free(bytecode);
    if (loadStatus != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        std::cerr << "[script] load " << relativePath << ": "
                  << (err != nullptr ? err : "?") << "\n";
        lua_pop(m_L, 1);
        return false;
    }
    const int callStatus = lua_pcall(m_L, 0, 0, 0);
    if (callStatus != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        std::cerr << "[script] run " << relativePath << ": "
                  << (err != nullptr ? err : "?") << "\n";
        lua_pop(m_L, 1);
        return false;
    }
    std::cout << "[script] loaded " << relativePath << "\n";
    return true;
#endif
}

bool LocalScriptHost::loadManifest(const std::string& relativePath)
{
#if !defined(RC_HAS_LUAU)
    (void)relativePath;
    return false;
#else
    if (!m_ready || m_L == nullptr) {
        return false;
    }
    const std::string path = resolveScript(relativePath);
    const std::string text = readScript(path);
    if (text.empty()) {
        std::cerr << "[script] WARN manifest missing " << relativePath << "\n";
        return false;
    }
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        std::cerr << "[script] manifest parse: " << ex.what() << "\n";
        return false;
    }
    if (!doc.contains("scripts") || !doc["scripts"].is_array()) {
        return false;
    }
    struct Row {
        std::string name;
        std::string path;
        int phase;
        bool enabled;
        std::vector<std::string> depends;
    };
    std::vector<Row> rows;
    for (const nlohmann::json& s : doc["scripts"]) {
        if (!s.is_object()) {
            continue;
        }
        Row row;
        row.name = s.value("name", "");
        row.path = s.value("path", "");
        row.phase = s.value("phase", 8);
        row.enabled = s.value("enabled", true);
        if (s.contains("dependsOn") && s["dependsOn"].is_array()) {
            for (const nlohmann::json& d : s["dependsOn"]) {
                if (d.is_string()) {
                    row.depends.push_back(d.get<std::string>());
                }
            }
        }
        if (row.enabled && !row.path.empty()) {
            rows.push_back(row);
        }
    }
    for (size_t a = 0; a < rows.size(); ++a) {
        size_t best = a;
        for (size_t b = a + 1; b < rows.size(); ++b) {
            if (rows[b].phase < rows[best].phase) {
                best = b;
            }
        }
        const Row tmp = rows[a];
        rows[a] = rows[best];
        rows[best] = tmp;
    }
    int loaded = 0;
    for (const Row& row : rows) {
        if (loadScriptFile(row.path)) {
            loaded += 1;
        }
    }
    std::cout << "[script] manifest loaded " << loaded << "/" << rows.size() << "\n";
    return loaded > 0;
#endif
}

void LocalScriptHost::fireHook(const std::string& name, const std::string& kind)
{
#if !defined(RC_HAS_LUAU)
    (void)name;
    (void)kind;
    return;
#else
    if (!m_ready || m_L == nullptr || name.empty()) {
        return;
    }
    lua_getglobal(m_L, "otr");
    if (!lua_istable(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }
    lua_getfield(m_L, -1, "hooks");
    lua_remove(m_L, -2);
    if (!lua_istable(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }
    lua_getfield(m_L, -1, name.c_str());
    lua_remove(m_L, -2);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }
    lua_createtable(m_L, 0, 2);
    lua_pushstring(m_L, kind.c_str());
    lua_setfield(m_L, -2, "kind");
    lua_pushstring(m_L, name.c_str());
    lua_setfield(m_L, -2, "hook");
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        std::cerr << "[script] hook " << name << ": " << (err != nullptr ? err : "?") << "\n";
        lua_pop(m_L, 1);
    }
#endif
}

bool LocalScriptHost::processTalk(const std::string& who, const std::string& text)
{
#if !defined(RC_HAS_LUAU)
    (void)who;
    (void)text;
    return false;
#else
    if (!m_ready || m_L == nullptr || who.empty()) {
        return false;
    }
    lua_getglobal(m_L, "otr");
    if (!lua_istable(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    lua_getfield(m_L, -1, "talk");
    lua_remove(m_L, -2);
    if (!lua_istable(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    lua_getfield(m_L, -1, who.c_str());
    lua_remove(m_L, -2);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    lua_pushstring(m_L, text.c_str());
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        std::cerr << "[script] talk " << who << ": " << (err != nullptr ? err : "?") << "\n";
        lua_pop(m_L, 1);
        return false;
    }
    return true;
#endif
}

void LocalScriptHost::tick(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    m_heartbeat += dt;
    if (m_heartbeat < 60.0f) {
        return;
    }
    m_heartbeat = 0.0f;
    fireHook("onHeartbeat", "world");
}

} // namespace standalone
} // namespace rc
