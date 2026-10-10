#pragma once

#include "Options.hpp"
#include "../Displays/ExternalDisplayManager.hpp"
#include "../Sensors/AmbientLightSensor.hpp"
#include "../System/SystemPowerMonitor.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace MonitorBrightnessSync {

class BrightnessSynchronizer {
public:
    BrightnessSynchronizer(
        const Options &options,
        AmbientLightSensor &sensor,
        ExternalDisplayManager &displayManager,
        SystemPowerMonitor &powerMonitor,
        const std::atomic_bool &isRunning
    );

    int run();

private:
    struct DisplayBrightnessBaseline {
        uint64_t displayKey;
        float builtInBrightness;
        float externalBrightness;
        std::optional<float> lastAppliedBrightness;
        uint64_t wakeGeneration = 0;
    };

    std::optional<float> builtInBrightness(const AmbientLightReading &reading) const;
    bool captureBaselines(const std::vector<ExternalDisplay> &displays);
    DisplayBrightnessBaseline *baselineForDisplay(const ExternalDisplay &display);
    float scaledBrightness(float currentBuiltInBrightness, const DisplayBrightnessBaseline &baseline) const;
    uint64_t displayKey(const ExternalDisplay &display) const;
    void sleepInterruptibly() const;
    float smoothedBrightness(float targetBrightness);

    const Options &options;
    AmbientLightSensor &sensor;
    ExternalDisplayManager &displayManager;
    SystemPowerMonitor &powerMonitor;
    const std::atomic_bool &isRunning;
    std::optional<float> currentSmoothedBrightness;
    std::optional<float> initialBuiltInBrightness;
    std::vector<DisplayBrightnessBaseline> displayBaselines;
};

}
