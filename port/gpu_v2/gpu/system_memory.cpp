#include "system_memory.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <cstring>
#endif

namespace iss::system_memory {

std::uint64_t AvailablePhysical() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullAvailPhys : 0;
#else
    std::FILE* file = std::fopen("/proc/meminfo", "r");
    if (!file) return 0;
    char line[128];
    std::uint64_t kb = 0;
    while (std::fgets(line, sizeof(line), file)) {
        unsigned long long value = 0;
        if (std::sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
            kb = value;
            break;
        }
    }
    std::fclose(file);
    return kb << 10;
#endif
}

} // namespace iss::system_memory
