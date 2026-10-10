#pragma once

#include "../System/SystemAPIs.hpp"

#include <IOKit/IOKitLib.h>

#include <optional>
#include <string>

namespace MonitorBrightnessSync {

enum class AmbientLightReadingKind {
    Lux,
    BuiltInBrightness
};

struct AmbientLightReading {
    AmbientLightReadingKind kind;
    double value;
};

class AmbientLightSensor {
public:
    explicit AmbientLightSensor(const SystemAPIs &apis);
    AmbientLightSensor(const AmbientLightSensor &) = delete;
    AmbientLightSensor &operator=(const AmbientLightSensor &) = delete;
    ~AmbientLightSensor();

    bool isAvailable() const;
    const std::string &diagnosticMessage() const;
    std::optional<AmbientLightReading> read() const;
    std::optional<AmbientLightReading> readBuiltInBrightness() const;

private:
    std::optional<AmbientLightReading> readLux() const;
    std::optional<AmbientLightReading> readBuiltInDisplayServicesBrightness() const;
    std::optional<CGDirectDisplayID> builtInDisplayID() const;
    std::optional<double> numberValue(CFDictionaryRef dictionary, const char *key) const;

    const SystemAPIs &apis;
    io_connect_t connection = IO_OBJECT_NULL;
    io_service_t backlightService = IO_OBJECT_NULL;
    std::string diagnostic = "Ambient light sensor has not been initialized.";
    std::string serviceClassName;
};

}
