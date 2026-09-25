#include "BrightnessSynchronizer.hpp"

#include "../Displays/BrightnessMapper.hpp"

#include <IOKit/IOReturn.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace MonitorBrightnessSync {

namespace {

#if DEBUG
constexpr bool isDebugBuild = true;
#else
constexpr bool isDebugBuild = false;
#endif

}

BrightnessSynchronizer::BrightnessSynchronizer(
    const Options &options,
    AmbientLightSensor &sensor,
    ExternalDisplayManager &displayManager,
    const std::atomic_bool &isRunning
) : options(options),
    sensor(sensor),
    displayManager(displayManager),
    isRunning(isRunning) {
}

int BrightnessSynchronizer::run() {
    std::vector<ExternalDisplay> displays = displayManager.displays();
    if (displays.empty()) {
        std::cerr << "No external displays found.\n"
                  << displayManager.diagnosticMessage() << "\n";
        return EXIT_FAILURE;
    }

    if (!captureBaselines(displays)) {
        displayManager.releaseDisplayServices(displays);
        return EXIT_FAILURE;
    }

    displayManager.releaseDisplayServices(displays);

    if constexpr (isDebugBuild) {
        const size_t displayCount = displays.size();
        std::cout << "Syncing " << displayCount << " external display"
                  << (displayCount == 1 ? "" : "s")
                  << " every " << options.intervalSeconds << "s"
                  << (options.dryRun ? " (dry run)" : "")
                  << ". Press Ctrl-C to stop.\n";
    }

    do {
        displays = displayManager.displays();
        const std::optional<AmbientLightReading> reading = sensor.read();
        if (!reading.has_value()) {
            std::cerr << "Failed to read ambient light signal.\n";
            displayManager.releaseDisplayServices(displays);
            sleepInterruptibly();
            continue;
        }

        const std::optional<float> currentBuiltInBrightness = builtInBrightness(*reading);
        if (!currentBuiltInBrightness.has_value()) {
            std::cerr << "Failed to normalize ambient light signal.\n";
            displayManager.releaseDisplayServices(displays);
            sleepInterruptibly();
            continue;
        }

        if constexpr (isDebugBuild) {
            std::cout << std::fixed << std::setprecision(2)
                      << (reading->kind == AmbientLightReadingKind::Lux ? "Lux: " : "Built-in brightness: ")
                      << reading->value
                      << ", initial built-in: " << *initialBuiltInBrightness << "\n";
        }

        for (const ExternalDisplay &display : displays) {
            DisplayBrightnessBaseline *baseline = baselineForDisplay(display);
            if (baseline == nullptr) {
                std::cerr << "  " << display.name << ": no startup brightness baseline\n";
                continue;
            }

            constexpr float manualAdjustmentTolerance = 0.01F;
            const std::optional<float> observedBrightness = displayManager.brightness(display);
            if (baseline->lastAppliedBrightness.has_value()
                && observedBrightness.has_value()
                && std::abs(*observedBrightness - *baseline->lastAppliedBrightness) > manualAdjustmentTolerance) {
                baseline->builtInBrightness = *currentBuiltInBrightness;
                baseline->externalBrightness = *observedBrightness;
                baseline->lastAppliedBrightness.reset();

                if constexpr (isDebugBuild) {
                    std::cout << "  " << display.name << ": keeping manual brightness "
                              << *observedBrightness << " and updating baseline\n";
                }
                continue;
            }

            const float brightness = scaledBrightness(*currentBuiltInBrightness, *baseline);

            if (options.dryRun) {
                std::cout << "  " << display.name << ": would set brightness to " << brightness << "\n";
                continue;
            }

            const IOReturn result = displayManager.setBrightness(display, brightness);
            if (result != kIOReturnSuccess) {
                std::cerr << "  " << display.name << ": failed to set brightness (0x"
                          << std::hex << result << std::dec << ")\n";
            } else {
                baseline->lastAppliedBrightness = brightness;
                if constexpr (isDebugBuild) {
                    std::cout << "  " << display.name << ": updated to " << brightness << "\n";
                }
            }
        }

        displayManager.releaseDisplayServices(displays);

        if (!options.runOnce) {
            sleepInterruptibly();
        }
    } while (isRunning && !options.runOnce);

    return EXIT_SUCCESS;
}

std::optional<float> BrightnessSynchronizer::builtInBrightness(const AmbientLightReading &reading) const {
    if (reading.kind == AmbientLightReadingKind::BuiltInBrightness) {
        return std::clamp(static_cast<float>(reading.value), 0.0F, 1.0F);
    }

    return brightnessForLux(reading.value, options);
}

bool BrightnessSynchronizer::captureBaselines(const std::vector<ExternalDisplay> &displays) {
    const std::optional<AmbientLightReading> reading = sensor.read();
    if (!reading.has_value()) {
        std::cerr << "Failed to read initial ambient light signal.\n";
        return false;
    }

    initialBuiltInBrightness = builtInBrightness(*reading);
    if (!initialBuiltInBrightness.has_value()) {
        std::cerr << "Failed to normalize initial ambient light signal.\n";
        return false;
    }

    displayBaselines.clear();
    for (const ExternalDisplay &display : displays) {
        const std::optional<float> brightness = displayManager.brightness(display);
        if (!brightness.has_value()) {
            std::cerr << "Unable to read initial brightness for " << display.name
                      << "; cannot scale this display safely.\n";
            continue;
        }

        displayBaselines.push_back({
            .displayKey = displayKey(display),
            .builtInBrightness = *initialBuiltInBrightness,
            .externalBrightness = *brightness,
            .lastAppliedBrightness = std::nullopt
        });

        if constexpr (isDebugBuild) {
            std::cout << std::fixed << std::setprecision(2)
                      << "Baseline " << display.name
                      << ": built-in " << *initialBuiltInBrightness
                      << ", external " << *brightness << "\n";
        }
    }

    if (displayBaselines.empty()) {
        std::cerr << "No external display brightness baselines could be read.\n";
        return false;
    }

    return true;
}

BrightnessSynchronizer::DisplayBrightnessBaseline *BrightnessSynchronizer::baselineForDisplay(
    const ExternalDisplay &display
) {
    const uint64_t key = displayKey(display);
    for (DisplayBrightnessBaseline &baseline : displayBaselines) {
        if (baseline.displayKey == key) {
            return &baseline;
        }
    }

    return nullptr;
}

float BrightnessSynchronizer::scaledBrightness(
    float currentBuiltInBrightness,
    const DisplayBrightnessBaseline &baseline
) const {
    constexpr float epsilon = 0.001F;
    const float initialBuiltIn = std::clamp(baseline.builtInBrightness, 0.0F, 1.0F);
    const float currentBuiltIn = std::clamp(currentBuiltInBrightness, 0.0F, 1.0F);
    const float initialExternal = std::clamp(baseline.externalBrightness, 0.0F, 1.0F);

    if (currentBuiltIn >= initialBuiltIn) {
        if (initialBuiltIn >= 1.0F - epsilon) {
            return initialExternal;
        }

        const float progressTowardMaximum = (currentBuiltIn - initialBuiltIn) / (1.0F - initialBuiltIn);
        return std::clamp(
            initialExternal + ((1.0F - initialExternal) * progressTowardMaximum),
            0.0F,
            1.0F
        );
    }

    if (initialBuiltIn <= epsilon) {
        return initialExternal;
    }

    const float progressTowardMinimum = currentBuiltIn / initialBuiltIn;
    return std::clamp(initialExternal * progressTowardMinimum, 0.0F, 1.0F);
}

uint64_t BrightnessSynchronizer::displayKey(const ExternalDisplay &display) const {
    if (display.id != 0) {
        return static_cast<uint64_t>(display.id);
    }

    return static_cast<uint64_t>(display.service);
}

void BrightnessSynchronizer::sleepInterruptibly() const {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration<double>(options.intervalSeconds);

    while (isRunning && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

float BrightnessSynchronizer::smoothedBrightness(float targetBrightness) {
    const float brightness = currentSmoothedBrightness.has_value()
        ? ((*currentSmoothedBrightness * 0.70F) + (targetBrightness * 0.30F))
        : targetBrightness;
    currentSmoothedBrightness = brightness;

    return brightness;
}

}
