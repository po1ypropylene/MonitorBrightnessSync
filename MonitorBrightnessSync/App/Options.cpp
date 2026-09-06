#include "Options.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace MonitorPlus {
namespace {

std::optional<double> parseDouble(const char *value) {
    char *end = nullptr;
    const double parsed = std::strtod(value, &end);

    if (end == value || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }

    return parsed;
}

} // namespace

void printUsage(const char *executableName) {
    std::cout
        << "Usage: " << executableName << " [options]\n\n"
        << "Sync external display brightness to the MacBook ambient light sensor.\n\n"
        << "Options:\n"
        << "  --interval seconds   Poll interval. Default: 0.25\n"
        << "  --min value          Minimum brightness, 0.0-1.0. Default: 0.15\n"
        << "  --max value          Maximum brightness, 0.0-1.0. Default: 0.85\n"
        << "  --once               Sync once and exit\n"
        << "  --dry-run            Print the target brightness without changing displays\n"
        << "  --help               Show this help\n";
}

std::optional<Options> parseOptions(int argc, const char *argv[]) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        auto readValue = [&](const char *name) -> std::optional<double> {
            if (index + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return std::nullopt;
            }

            ++index;
            const std::optional<double> parsed = parseDouble(argv[index]);
            if (!parsed.has_value()) {
                std::cerr << "Invalid value for " << name << ": " << argv[index] << "\n";
            }

            return parsed;
        };

        if (argument == "--help") {
            printUsage(argv[0]);
            std::exit(EXIT_SUCCESS);
        } else if (argument == "--interval") {
            const std::optional<double> value = readValue("--interval");
            if (!value.has_value() || *value <= 0.0) {
                std::cerr << "--interval must be greater than 0\n";
                return std::nullopt;
            }
            options.intervalSeconds = *value;
        } else if (argument == "--min") {
            const std::optional<double> value = readValue("--min");
            if (!value.has_value() || *value < 0.0 || *value > 1.0) {
                std::cerr << "--min must be between 0 and 1\n";
                return std::nullopt;
            }
            options.minimumBrightness = static_cast<float>(*value);
        } else if (argument == "--max") {
            const std::optional<double> value = readValue("--max");
            if (!value.has_value() || *value < 0.0 || *value > 1.0) {
                std::cerr << "--max must be between 0 and 1\n";
                return std::nullopt;
            }
            options.maximumBrightness = static_cast<float>(*value);
        } else if (argument == "--dry-run") {
            options.dryRun = true;
        } else if (argument == "--once") {
            options.runOnce = true;
        } else {
            std::cerr << "Unknown option: " << argument << "\n";
            return std::nullopt;
        }
    }

    if (options.minimumBrightness > options.maximumBrightness) {
        std::cerr << "--min must be less than or equal to --max\n";
        return std::nullopt;
    }

    return options;
}

} // namespace MonitorPlus
