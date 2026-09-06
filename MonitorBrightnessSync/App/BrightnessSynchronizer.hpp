#pragma once

#include "Options.hpp"
#include "../Displays/ExternalDisplayManager.hpp"
#include "../Sensors/AmbientLightSensor.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace MonitorPlus {

class BrightnessSynchronizer {
public:
    BrightnessSynchronizer(
        const Options &options,
        AmbientLightSensor &sensor,
        ExternalDisplayManager &displayManager,
        const std::atomic_bool &isRunning
    );

    int run();

private:
    struct DisplayBrightnessBaseline {
        uint64_t displayKey;
        float initialBrightness;
    };

    std::optional<float> builtInBrightness(const AmbientLightReading &reading) const;
    bool captureBaselines(const std::vector<ExternalDisplay> &displays);
    std::optional<float> scaledBrightnessForDisplay(const ExternalDisplay &display, float currentBuiltInBrightness) const;
    float scaledBrightness(float currentBuiltInBrightness, float initialExternalBrightness) const;
    uint64_t displayKey(const ExternalDisplay &display) const;
    void sleepInterruptibly() const;
    float smoothedBrightness(float targetBrightness);

    const Options &options;
    AmbientLightSensor &sensor;
    ExternalDisplayManager &displayManager;
    const std::atomic_bool &isRunning;
    std::optional<float> currentSmoothedBrightness;
    std::optional<float> initialBuiltInBrightness;
    std::vector<DisplayBrightnessBaseline> displayBaselines;
};

} // namespace MonitorPlus
