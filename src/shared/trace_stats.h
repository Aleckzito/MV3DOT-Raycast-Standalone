#pragma once

#include "trace_log.h"

#include <atomic>
#include <chrono>
#include <cstdint>

#ifdef ENABLE_TRACE
#include <sstream>
#endif

namespace rc {

// Aggregated counters for hot-path telemetry (no per-cell logging).
struct TraceStats {
    std::atomic<uint64_t> chunkDeltasQueued {0};
    std::atomic<uint64_t> chunkDeltasApplied {0};
    std::atomic<uint64_t> fullSnapshotCopies {0};
    std::atomic<uint64_t> chunksQueuedForRebuild {0};
    std::atomic<uint64_t> chunkCacheHits {0};
    std::atomic<uint64_t> chunkCacheMisses {0};

    void noteChunkCacheHit()
    {
#ifdef ENABLE_TRACE
        chunkCacheHits.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void noteChunkCacheMiss()
    {
#ifdef ENABLE_TRACE
        chunkCacheMisses.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void noteChunkQueued()
    {
#ifdef ENABLE_TRACE
        chunkDeltasQueued.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void noteChunkApplied()
    {
#ifdef ENABLE_TRACE
        chunkDeltasApplied.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void noteFullSnapshotCopy()
    {
#ifdef ENABLE_TRACE
        fullSnapshotCopies.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void noteChunkRebuildQueued()
    {
#ifdef ENABLE_TRACE
        chunksQueuedForRebuild.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    void maybeReportStreaming(const char* sys, std::chrono::steady_clock::time_point& lastReport)
    {
#ifdef ENABLE_TRACE
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport < std::chrono::seconds(1)) {
            return;
        }
        lastReport = now;
        LOG_TRACE(
            sys,
            "streaming queued=" << chunkDeltasQueued.load(std::memory_order_relaxed)
                                << " applied=" << chunkDeltasApplied.load(std::memory_order_relaxed)
                                << " fullCopies="
                                << fullSnapshotCopies.load(std::memory_order_relaxed)
                                << " rebuildQ="
                                << chunksQueuedForRebuild.load(std::memory_order_relaxed)
                                << " cacheHit="
                                << chunkCacheHits.load(std::memory_order_relaxed)
                                << " cacheMiss="
                                << chunkCacheMisses.load(std::memory_order_relaxed));
#else
        (void)sys;
        (void)lastReport;
#endif
    }
};

inline TraceStats& traceStats()
{
    static TraceStats stats;
    return stats;
}

} // namespace rc
