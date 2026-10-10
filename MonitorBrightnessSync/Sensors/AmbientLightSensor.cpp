#include "AmbientLightSensor.hpp"

#include "../System/CFString.hpp"

#include <mach/mach.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <vector>

namespace MonitorBrightnessSync {

namespace {

constexpr mach_port_t mainPortDefault = MACH_PORT_NULL;
constexpr std::array sensorServiceClasses = {
    "AppleLMUController",
    "AppleSPUALSSunfishDriver",
    "AppleSPUALSSealionDriver",
    "AppleSPUALSSealionAccessoryDriver"
};

}

AmbientLightSensor::AmbientLightSensor(const SystemAPIs &apis) : apis(apis) {
    for (const char *serviceClass : sensorServiceClasses) {
        io_service_t service = apis.IOServiceGetMatchingService(
            mainPortDefault,
            apis.IOServiceMatching(serviceClass)
        );

        if (service == IO_OBJECT_NULL) {
            continue;
        }

        const kern_return_t result = apis.IOServiceOpen(service, mach_task_self(), 0, &connection);
        apis.IOObjectRelease(service);

        if (result == KERN_SUCCESS) {
            serviceClassName = serviceClass;
            diagnostic = "Using ambient light sensor service: " + serviceClassName;
            return;
        }

        std::ostringstream message;
        message << "Found " << serviceClass << " but IOServiceOpen failed: 0x"
                << std::hex << result;
        diagnostic = message.str();
        connection = IO_OBJECT_NULL;
    }

    backlightService = apis.IOServiceGetMatchingService(
        mainPortDefault,
        apis.IOServiceMatching("AppleARMBacklight")
    );

    if (backlightService != IO_OBJECT_NULL) {
        diagnostic = "Using built-in display brightness as the ambient-light fallback.";
        return;
    }

    if (diagnostic == "Ambient light sensor has not been initialized.") {
        diagnostic = "No supported ambient-light IOService class or AppleARMBacklight fallback was found.";
    }
}

AmbientLightSensor::~AmbientLightSensor() {
    if (connection != IO_OBJECT_NULL) {
        apis.IOServiceClose(connection);
    }
    if (backlightService != IO_OBJECT_NULL) {
        apis.IOObjectRelease(backlightService);
    }
}

bool AmbientLightSensor::isAvailable() const {
    return connection != IO_OBJECT_NULL || backlightService != IO_OBJECT_NULL;
}

const std::string &AmbientLightSensor::diagnosticMessage() const {
    return diagnostic;
}

std::optional<AmbientLightReading> AmbientLightSensor::read() const {
    if (connection != IO_OBJECT_NULL) {
        return readLux();
    }
    if (backlightService != IO_OBJECT_NULL) {
        return readBuiltInBrightness();
    }

    return std::nullopt;
}

std::optional<AmbientLightReading> AmbientLightSensor::readLux() const {
    if (connection == IO_OBJECT_NULL) {
        return std::nullopt;
    }

    uint64_t outputValues[2] = {};
    uint32_t outputValueCount = 2;

    const kern_return_t result = apis.IOConnectCallMethod(
        connection,
        0,
        nullptr,
        0,
        nullptr,
        0,
        outputValues,
        &outputValueCount,
        nullptr,
        nullptr
    );

    if (result != KERN_SUCCESS || outputValueCount < 2) {
        return std::nullopt;
    }

    return AmbientLightReading{
        .kind = AmbientLightReadingKind::Lux,
        .value = (static_cast<double>(outputValues[0]) + static_cast<double>(outputValues[1])) / 2.0
    };
}

std::optional<AmbientLightReading> AmbientLightSensor::readBuiltInBrightness() const {
    if (const std::optional<AmbientLightReading> reading = readBuiltInDisplayServicesBrightness()) {
        return reading;
    }

    // Also works when an ambient-light service is active and no backlight
    // registry service was retained during initialization.
    if (const auto displayID = builtInDisplayID()) {
        const io_service_t service = apis.CGDisplayIOServicePort(*displayID);
        CFString brightnessKey(apis, kIODisplayBrightnessKey);
        float brightness = 0.0F;
        if (service != IO_OBJECT_NULL && brightnessKey.get() != nullptr
            && apis.IODisplayGetFloatParameter(service, 0, brightnessKey.get(), &brightness) == kIOReturnSuccess
            && std::isfinite(brightness)) {
            return AmbientLightReading{
                .kind = AmbientLightReadingKind::BuiltInBrightness,
                .value = std::clamp(static_cast<double>(brightness), 0.0, 1.0)
            };
        }
    }

    if (backlightService == IO_OBJECT_NULL) {
        return std::nullopt;
    }

    CFString displayParametersKey(apis, "IODisplayParameters");
    if (displayParametersKey.get() == nullptr) {
        return std::nullopt;
    }

    CFTypeRef parameters = apis.IORegistryEntryCreateCFProperty(
        backlightService,
        displayParametersKey.get(),
        nullptr,
        0
    );
    if (parameters == nullptr) {
        return std::nullopt;
    }

    std::optional<AmbientLightReading> reading;
    if (apis.CFGetTypeID(parameters) == apis.CFDictionaryGetTypeID()) {
        CFDictionaryRef parameterDictionary = static_cast<CFDictionaryRef>(parameters);
        CFString brightnessKey(apis, "brightness");
        CFDictionaryRef brightness = static_cast<CFDictionaryRef>(
            apis.CFDictionaryGetValue(parameterDictionary, brightnessKey.get())
        );

        if (brightness != nullptr && apis.CFGetTypeID(brightness) == apis.CFDictionaryGetTypeID()) {
            const std::optional<double> value = numberValue(brightness, "value");
            const std::optional<double> maximum = numberValue(brightness, "max");

            if (value.has_value() && maximum.has_value()
                && std::isfinite(*value) && std::isfinite(*maximum) && *maximum > 0.0) {
                reading = AmbientLightReading{
                    .kind = AmbientLightReadingKind::BuiltInBrightness,
                    .value = std::clamp(*value / *maximum, 0.0, 1.0)
                };
            }
        }
    }

    apis.CFRelease(parameters);
    return reading;
}

std::optional<AmbientLightReading> AmbientLightSensor::readBuiltInDisplayServicesBrightness() const {
    if (apis.DisplayServicesGetBrightness == nullptr) {
        return std::nullopt;
    }

    const std::optional<CGDirectDisplayID> displayID = builtInDisplayID();
    if (!displayID.has_value()) {
        return std::nullopt;
    }

    float brightness = 0.0F;
    if (apis.DisplayServicesGetBrightness(*displayID, &brightness) != 0 || !std::isfinite(brightness)) {
        return std::nullopt;
    }

    return AmbientLightReading{
        .kind = AmbientLightReadingKind::BuiltInBrightness,
        .value = std::clamp(static_cast<double>(brightness), 0.0, 1.0)
    };
}

std::optional<CGDirectDisplayID> AmbientLightSensor::builtInDisplayID() const {
    auto findBuiltInDisplay = [this](SystemAPIs::CGGetOnlineDisplayListFunction listDisplays) -> std::optional<CGDirectDisplayID> {
        uint32_t displayCount = 0;
        if (listDisplays(0, nullptr, &displayCount) != kCGErrorSuccess || displayCount == 0) {
            return std::nullopt;
        }

        std::vector<CGDirectDisplayID> displayIDs(displayCount);
        if (listDisplays(displayCount, displayIDs.data(), &displayCount) != kCGErrorSuccess) {
            return std::nullopt;
        }

        for (uint32_t index = 0; index < displayCount; ++index) {
            if (apis.CGDisplayIsBuiltin(displayIDs[index])) {
                return displayIDs[index];
            }
        }

        return std::nullopt;
    };

    if (const std::optional<CGDirectDisplayID> displayID = findBuiltInDisplay(apis.CGGetOnlineDisplayList)) {
        return displayID;
    }

    return findBuiltInDisplay(apis.CGGetActiveDisplayList);
}

std::optional<double> AmbientLightSensor::numberValue(CFDictionaryRef dictionary, const char *key) const {
    CFString numberKey(apis, key);
    if (numberKey.get() == nullptr) {
        return std::nullopt;
    }

    CFTypeRef value = apis.CFDictionaryGetValue(dictionary, numberKey.get());
    if (value == nullptr || apis.CFGetTypeID(value) != apis.CFNumberGetTypeID()) {
        return std::nullopt;
    }

    double number = 0.0;
    if (!apis.CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberDoubleType, &number)) {
        return std::nullopt;
    }

    return number;
}

}
