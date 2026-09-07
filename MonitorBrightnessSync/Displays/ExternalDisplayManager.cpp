#include "ExternalDisplayManager.hpp"

#include "../System/CFString.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <mach/mach.h>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace MonitorBrightnessSync {

namespace {

constexpr IOOptionBits nilOptions = 0;
constexpr mach_port_t mainPortDefault = MACH_PORT_NULL;

}

ExternalDisplayManager::ExternalDisplayManager(const SystemAPIs &apis) : apis(apis) {
}

std::vector<ExternalDisplay> ExternalDisplayManager::displays() const {
    std::vector<ExternalDisplay> onlineDisplays = externalDisplays(displayIDs(apis.CGGetOnlineDisplayList), nullptr);
    if (!onlineDisplays.empty()) {
        return onlineDisplays;
    }

    std::vector<ExternalDisplay> activeDisplays = externalDisplays(displayIDs(apis.CGGetActiveDisplayList), nullptr);
    if (!activeDisplays.empty()) {
        return activeDisplays;
    }

    return registryDisplays(nullptr);
}

std::string ExternalDisplayManager::diagnosticMessage() const {
    std::ostringstream message;
    std::vector<CGDirectDisplayID> onlineIDs = displayIDs(apis.CGGetOnlineDisplayList);
    std::vector<CGDirectDisplayID> activeIDs = displayIDs(apis.CGGetActiveDisplayList);

    message << "Online displays reported by CoreGraphics: " << onlineIDs.size() << ".";

    std::string onlineDetails;
    externalDisplays(onlineIDs, &onlineDetails);
    if (!onlineDetails.empty()) {
        message << "\nOnline display details:\n" << onlineDetails;
    }

    message << "\nActive displays reported by CoreGraphics: " << activeIDs.size() << ".";
    std::string activeDetails;
    externalDisplays(activeIDs, &activeDetails);
    if (!activeDetails.empty()) {
        message << "\nActive display details:\n" << activeDetails;
    }

    std::string registryDetails;
    std::vector<ExternalDisplay> registryDisplayList = registryDisplays(&registryDetails);
    releaseDisplayServices(registryDisplayList);
    message << "\nIODisplayConnect entries reported by IORegistry: " << registryDisplayList.size() << ".";
    if (!registryDetails.empty()) {
        message << "\nIORegistry display details:\n" << registryDetails;
    }

    return message.str();
}

void ExternalDisplayManager::releaseDisplayServices(const std::vector<ExternalDisplay> &displays) const {
    for (const ExternalDisplay &display : displays) {
        if (display.releasesService && display.service != IO_OBJECT_NULL) {
            apis.IOObjectRelease(display.service);
        }
    }
}

std::optional<float> ExternalDisplayManager::brightness(const ExternalDisplay &display) const {
    if (display.id != 0 && apis.DisplayServicesGetBrightness != nullptr) {
        float brightness = 0.0F;
        if (apis.DisplayServicesGetBrightness(display.id, &brightness) == 0) {
            return std::clamp(brightness, 0.0F, 1.0F);
        }
    }

    if (display.service == IO_OBJECT_NULL) {
        return std::nullopt;
    }

    CFString brightnessKey(apis, kIODisplayBrightnessKey);
    if (brightnessKey.get() == nullptr) {
        return std::nullopt;
    }

    float brightness = 0.0F;
    if (apis.IODisplayGetFloatParameter(display.service, nilOptions, brightnessKey.get(), &brightness) != kIOReturnSuccess) {
        return std::nullopt;
    }

    return std::clamp(brightness, 0.0F, 1.0F);
}

std::vector<CGDirectDisplayID> ExternalDisplayManager::displayIDs(DisplayListFunction listDisplays) const {
    uint32_t displayCount = 0;
    listDisplays(0, nullptr, &displayCount);

    if (displayCount == 0) {
        return {};
    }

    std::vector<CGDirectDisplayID> displayIDs(displayCount);
    CGError error = listDisplays(displayCount, displayIDs.data(), &displayCount);
    if (error != kCGErrorSuccess) {
        std::cerr << "Failed to enumerate displays: " << error << "\n";
        return {};
    }

    displayIDs.resize(displayCount);
    return displayIDs;
}

std::vector<ExternalDisplay> ExternalDisplayManager::externalDisplays(
    const std::vector<CGDirectDisplayID> &displayIDs,
    std::string *diagnostic
) const {
    std::vector<ExternalDisplay> externalDisplays;

    for (CGDirectDisplayID displayID : displayIDs) {
        const bool isBuiltin = apis.CGDisplayIsBuiltin(displayID);
        io_service_t service = apis.CGDisplayIOServicePort(displayID);
        const std::string name = service == IO_OBJECT_NULL
            ? "Unknown display"
            : displayName(service).value_or("External display");

        if (diagnostic != nullptr) {
            std::ostringstream line;
            line << "  id=" << displayID
                 << ", name=" << name
                 << ", builtIn=" << (isBuiltin ? "yes" : "no")
                 << ", ioService=" << service << "\n";
            diagnostic->append(line.str());
        }

        if (isBuiltin) {
            continue;
        }

        externalDisplays.push_back({
            .id = displayID,
            .service = service,
            .name = name,
            .releasesService = false
        });
    }

    return externalDisplays;
}

std::vector<ExternalDisplay> ExternalDisplayManager::registryDisplays(std::string *diagnostic) const {
    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t result = apis.IOServiceGetMatchingServices(
        mainPortDefault,
        apis.IOServiceMatching("IODisplayConnect"),
        &iterator
    );

    if (result != KERN_SUCCESS) {
        if (diagnostic != nullptr) {
            std::ostringstream line;
            line << "  failed to enumerate IODisplayConnect services: 0x"
                 << std::hex << result << std::dec << "\n";
            diagnostic->append(line.str());
        }
        return {};
    }

    if (iterator == IO_OBJECT_NULL) {
        if (diagnostic != nullptr) {
            diagnostic->append("  no IODisplayConnect services were returned\n");
        }
        return {};
    }

    std::vector<ExternalDisplay> displays;
    while (true) {
        io_service_t service = apis.IOIteratorNext(iterator);
        if (service == IO_OBJECT_NULL) {
            break;
        }

        const std::string name = displayName(service).value_or("External display");
        const std::optional<std::string> prefsKey = stringProperty(service, "IODisplayPrefsKey");
        const bool isBuiltIn = isLikelyBuiltInDisplay(name, prefsKey);

        if (diagnostic != nullptr) {
            std::ostringstream line;
            line << "  name=" << name
                 << ", builtIn=" << (isBuiltIn ? "yes" : "no")
                 << ", ioService=" << service;
            if (prefsKey.has_value()) {
                line << ", prefsKey=" << *prefsKey;
            }
            line << "\n";
            diagnostic->append(line.str());
        }

        if (isBuiltIn) {
            apis.IOObjectRelease(service);
            continue;
        }

        displays.push_back({
            .id = 0,
            .service = service,
            .name = name,
            .releasesService = true
        });
    }

    apis.IOObjectRelease(iterator);
    return displays;
}

IOReturn ExternalDisplayManager::setBrightness(const ExternalDisplay &display, float brightness) const {
    if (display.id != 0 && apis.DisplayServicesSetBrightness != nullptr) {
        const int result = apis.DisplayServicesSetBrightness(display.id, brightness);
        if (result == 0) {
            return kIOReturnSuccess;
        }
    }

    if (display.service == IO_OBJECT_NULL) {
        return kIOReturnNoDevice;
    }

    CFString brightnessKey(apis, kIODisplayBrightnessKey);
    if (brightnessKey.get() == nullptr) {
        return kIOReturnError;
    }

    return apis.IODisplaySetFloatParameter(
        display.service,
        nilOptions,
        brightnessKey.get(),
        brightness
    );
}

std::optional<std::string> ExternalDisplayManager::displayName(io_service_t service) const {
    CFDictionaryRef info = apis.IODisplayCreateInfoDictionary(service, kIODisplayOnlyPreferredName);
    if (info == nullptr) {
        return std::nullopt;
    }

    std::optional<std::string> name;
    CFString productNameKey(apis, kDisplayProductName);
    if (productNameKey.get() == nullptr) {
        apis.CFRelease(info);
        return std::nullopt;
    }

    CFDictionaryRef localizedNames = static_cast<CFDictionaryRef>(
        apis.CFDictionaryGetValue(info, productNameKey.get())
    );

    if (localizedNames != nullptr && apis.CFGetTypeID(localizedNames) == apis.CFDictionaryGetTypeID()) {
        CFIndex count = apis.CFDictionaryGetCount(localizedNames);
        if (count > 0) {
            std::vector<const void *> values(static_cast<size_t>(count));
            apis.CFDictionaryGetKeysAndValues(localizedNames, nullptr, values.data());

            CFStringRef displayName = static_cast<CFStringRef>(values.front());
            if (displayName != nullptr && apis.CFGetTypeID(displayName) == apis.CFStringGetTypeID()) {
                char buffer[256] = {};
                if (apis.CFStringGetCString(displayName, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                    name = buffer;
                }
            }
        }
    }

    apis.CFRelease(info);
    return name;
}

std::optional<std::string> ExternalDisplayManager::stringProperty(io_service_t service, const char *key) const {
    CFString propertyKey(apis, key);
    if (propertyKey.get() == nullptr) {
        return std::nullopt;
    }

    CFTypeRef property = apis.IORegistryEntryCreateCFProperty(
        service,
        propertyKey.get(),
        nullptr,
        0
    );
    if (property == nullptr) {
        return std::nullopt;
    }

    std::optional<std::string> value;
    if (apis.CFGetTypeID(property) == apis.CFStringGetTypeID()) {
        char buffer[512] = {};
        if (apis.CFStringGetCString(static_cast<CFStringRef>(property), buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            value = buffer;
        }
    }

    apis.CFRelease(property);
    return value;
}

bool ExternalDisplayManager::isLikelyBuiltInDisplay(
    const std::string &name,
    const std::optional<std::string> &prefsKey
) const {
    if (name == "Color LCD" || name.find("Built-in") != std::string::npos) {
        return true;
    }

    return prefsKey.has_value()
        && (prefsKey->find("AppleBacklightDisplay") != std::string::npos
            || prefsKey->find("AppleDisplay") != std::string::npos);
}

}
