#pragma once

#include <cstdint>

namespace iss::system_memory {

/// Physical RAM available right now, in bytes (Windows: ullAvailPhys, Linux: MemAvailable).
/// 0 when it cannot be read.
std::uint64_t AvailablePhysical();

} // namespace iss::system_memory
