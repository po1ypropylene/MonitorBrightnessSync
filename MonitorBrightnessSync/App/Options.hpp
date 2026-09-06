#pragma once

#include <optional>

namespace MonitorPlus {

struct Options {
    double intervalSeconds = 0.25;
    float minimumBrightness = 0.15F;
    float maximumBrightness = 0.85F;
    bool dryRun = false;
    bool runOnce = false;
};

void printUsage(const char *executableName);
std::optional<Options> parseOptions(int argc, const char *argv[]);

} // namespace MonitorPlus
