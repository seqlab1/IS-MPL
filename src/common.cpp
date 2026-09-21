#include "common.h"

#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#include <cstdio>
#endif

bool g_debug = false;

#if defined(_WIN32)

MemUsage getMemUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    MemUsage m{0, 0};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m.workingKB = pmc.WorkingSetSize / 1024;
        m.peakKB    = pmc.PeakWorkingSetSize / 1024;
    }
    return m;
}

#else

static size_t readVmRSSKB() {
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            size_t valueKB = 0;
            std::string unit;
            iss >> key >> valueKB >> unit;
            return valueKB;
        }
    }
    return 0;
}

MemUsage getMemUsage() {
    MemUsage m{0, 0};
    m.workingKB = readVmRSSKB();
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        m.peakKB = (size_t)ru.ru_maxrss;
    }
    return m;
}

#endif
