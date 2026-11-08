// SPDX-License-Identifier: GPL-2.0-or-later
// iss: which compute shaders a hung submission ran, for GPUs without buffer markers.
//
// The GPU black box (gpu_blackbox.h) needs VK_AMD_buffer_marker; Intel and NVIDIA have no
// markers, so a GPU hang there only said "readback still waiting after 2 s". Every recorded
// compute dispatch is noted here with its queue tick; when a readback waits too long, the
// dispatches of the ticks the GPU has not finished are printed (HANG_DISPATCHES), so a hang
// points at a short list of shaders. Costs one ring write per dispatch. ISS_HANG_TRACE=0 off.
#pragma once

#include <array>
#include <utility>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace iss::hang_trace {

struct Dispatch {
    std::atomic<std::uint64_t> stamp{}; ///< sequence + 1 once written
    std::uint64_t tick{};
    std::uint64_t hash{};
    std::uint32_t queue{};
    std::uint32_t x{}, y{}, z{};
};

inline constexpr std::uint32_t RingSize = 8192;
inline std::array<Dispatch, RingSize> ring{};
inline std::atomic<std::uint64_t> next{};

inline bool Enabled() {
    static const bool on = [] {
        const char* p = std::getenv("ISS_HANG_TRACE");
        return !(p && p[0] == '0');
    }();
    return on;
}

inline void Note(std::uint64_t tick, std::uint64_t hash, std::uint32_t queue, std::uint32_t x,
                 std::uint32_t y, std::uint32_t z) {
    if (!Enabled()) return;
    const std::uint64_t seq = next.fetch_add(1, std::memory_order_relaxed);
    auto& d = ring[seq % RingSize];
    d.stamp.store(0, std::memory_order_relaxed);
    d.tick = tick;
    d.hash = hash;
    d.queue = queue;
    d.x = x;
    d.y = y;
    d.z = z;
    d.stamp.store(seq + 1, std::memory_order_release);
}

/// ISS_SKIP_DISPATCH=<hex hash>[,<hex hash>...] (up to 16): true for those shaders.
inline bool SkipDispatch(std::uint64_t hash) {
    static const auto list = [] {
        std::array<std::uint32_t, 16> hashes{};
        std::uint32_t n = 0;
        if (const char* p = std::getenv("ISS_SKIP_DISPATCH")) {
            while (*p && n < hashes.size()) {
                char* end = nullptr;
                const unsigned long long value = std::strtoull(p, &end, 16);
                if (end == p) break;
                hashes[n++] = static_cast<std::uint32_t>(value);
                p = *end == ',' ? end + 1 : end;
            }
            for (std::uint32_t i = 0; i < n; ++i)
                std::fprintf(stderr, "HANG_SKIP_DISPATCH shader=%08x\n", hashes[i]);
        }
        return std::pair{hashes, n};
    }();
    for (std::uint32_t i = 0; i < list.second; ++i)
        if (list.first[i] == static_cast<std::uint32_t>(hash)) return true;
    return false;
}

/// ISS_SUBMIT_EVERY_DISPATCH=N: true every N graphics-queue dispatches (0: never).
inline bool SubmitDue() {
    static const std::uint32_t every = [] {
        const char* p = std::getenv("ISS_SUBMIT_EVERY_DISPATCH");
        return p ? static_cast<std::uint32_t>(std::strtoul(p, nullptr, 10)) : 0u;
    }();
    if (every == 0) return false;
    static thread_local std::uint32_t count = 0;
    if (++count < every) return false;
    count = 0;
    return true;
}

/// The dispatches noted on `queue` with done < tick <= upto, grouped by shader (at most 96).
inline void DumpRange(std::uint32_t queue, std::uint64_t done, std::uint64_t upto) {
    if (!Enabled()) return;
    struct Row {
        std::uint64_t hash, first_tick;
        std::uint32_t count, x, y, z;
    };
    std::array<Row, 96> rows{};
    std::uint32_t num_rows = 0, total = 0;
    const std::uint64_t end = next.load(std::memory_order_acquire);
    const std::uint64_t begin = end > RingSize ? end - RingSize : 0;
    for (std::uint64_t seq = begin; seq < end; ++seq) {
        const auto& d = ring[seq % RingSize];
        if (d.stamp.load(std::memory_order_acquire) != seq + 1) continue;
        if (d.queue != queue || d.tick <= done || d.tick > upto) continue;
        ++total;
        std::uint32_t i = 0;
        while (i < num_rows && rows[i].hash != d.hash) ++i;
        if (i == num_rows) {
            if (num_rows == rows.size()) continue;
            rows[num_rows++] = Row{d.hash, d.tick, 0, d.x, d.y, d.z};
        }
        ++rows[i].count;
    }
    std::fprintf(stderr, "HANG_DISPATCHES queue=%u ticks %llu..%llu: %u dispatches, %u shaders\n",
                 queue, (unsigned long long)(done + 1), (unsigned long long)upto, total, num_rows);
    for (std::uint32_t i = 0; i < num_rows; ++i)
        std::fprintf(stderr, "  shader=%08llx count=%u first_tick=%llu groups=%ux%ux%u\n",
                     (unsigned long long)rows[i].hash, rows[i].count,
                     (unsigned long long)rows[i].first_tick, rows[i].x, rows[i].y, rows[i].z);
}

/// First the tick the GPU is stuck in (done + 1: the shortest list of suspects), then every
/// unfinished tick. ISS_SUBMIT_EVERY=N (draws per submission) makes the ticks smaller.
inline void Dump(std::uint32_t queue, std::uint64_t done, std::uint64_t upto) {
    DumpRange(queue, done, done + 1);
    if (upto > done + 1) DumpRange(queue, done, upto);
}

} // namespace iss::hang_trace
