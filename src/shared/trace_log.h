#pragma once

#include <string>
#include <string_view>

#ifdef ENABLE_TRACE
#include <sstream>
#endif

namespace rc {

#ifdef ENABLE_TRACE
constexpr std::string_view stripPath(std::string_view path)
{
    const size_t pos = path.find_last_of("\\/");
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
}

void traceLogLine(std::string line);

#define LOG_TRACE(sys, msg)                                                                          \
    do {                                                                                             \
        std::ostringstream _traceSs;                                                                 \
        _traceSs << "[" << (sys) << "][LINE:" << __LINE__ << "][FILE:" << stripPath(__FILE__)        \
                 << "] " << (msg) << "\n";                                                           \
        traceLogLine(_traceSs.str());                                                                \
    } while (false)
#else
#define LOG_TRACE(sys, msg)                                                                          \
    do {                                                                                             \
    } while (false)
#endif

} // namespace rc
