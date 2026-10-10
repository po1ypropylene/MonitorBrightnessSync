#include "SystemAPIs.hpp"

#include <dlfcn.h>
#include <iostream>

namespace MonitorBrightnessSync {

bool SystemAPIs::load() {
    coreFoundation = dlopen(
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
        RTLD_NOW
    );
    coreGraphics = dlopen(
        "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics",
        RTLD_NOW
    );
    displayServices = dlopen(
        "/System/Library/PrivateFrameworks/DisplayServices.framework/DisplayServices",
        RTLD_NOW
    );
    ioKit = dlopen(
        "/System/Library/Frameworks/IOKit.framework/IOKit",
        RTLD_NOW
    );

    if (coreFoundation == nullptr || coreGraphics == nullptr || ioKit == nullptr) {
        return false;
    }

    const bool requiredAPIsLoaded = resolve(coreFoundation, "CFDictionaryGetCount", CFDictionaryGetCount)
        && resolve(coreFoundation, "CFDictionaryGetKeysAndValues", CFDictionaryGetKeysAndValues)
        && resolve(coreFoundation, "CFDictionaryGetTypeID", CFDictionaryGetTypeID)
        && resolve(coreFoundation, "CFDictionaryGetValue", CFDictionaryGetValue)
        && resolve(coreFoundation, "CFGetTypeID", CFGetTypeID)
        && resolve(coreFoundation, "CFNumberGetTypeID", CFNumberGetTypeID)
        && resolve(coreFoundation, "CFNumberGetValue", CFNumberGetValue)
        && resolve(coreFoundation, "CFRelease", CFRelease)
        && resolve(coreFoundation, "CFStringCreateWithCString", CFStringCreateWithCString)
        && resolve(coreFoundation, "CFStringGetCString", CFStringGetCString)
        && resolve(coreFoundation, "CFStringGetTypeID", CFStringGetTypeID)
        && resolve(coreFoundation, "CFRunLoopGetCurrent", CFRunLoopGetCurrent)
        && resolve(coreFoundation, "CFRunLoopAddSource", CFRunLoopAddSource)
        && resolve(coreFoundation, "CFRunLoopRemoveSource", CFRunLoopRemoveSource)
        && resolve(coreFoundation, "CFRunLoopRunInMode", CFRunLoopRunInMode)
        && resolve(coreGraphics, "CGDisplayIOServicePort", CGDisplayIOServicePort)
        && resolve(coreGraphics, "CGDisplayIsBuiltin", CGDisplayIsBuiltin)
        && resolve(coreGraphics, "CGGetActiveDisplayList", CGGetActiveDisplayList)
        && resolve(coreGraphics, "CGGetOnlineDisplayList", CGGetOnlineDisplayList)
        && resolve(coreGraphics, "CGSSetDisplayBrightness", CGSSetDisplayBrightness)
        && resolve(ioKit, "IOConnectCallMethod", IOConnectCallMethod)
        && resolve(ioKit, "IODisplayCreateInfoDictionary", IODisplayCreateInfoDictionary)
        && resolve(ioKit, "IODisplayGetFloatParameter", IODisplayGetFloatParameter)
        && resolve(ioKit, "IODisplaySetFloatParameter", IODisplaySetFloatParameter)
        && resolve(ioKit, "IOIteratorNext", IOIteratorNext)
        && resolve(ioKit, "IOObjectRelease", IOObjectRelease)
        && resolve(ioKit, "IORegistryEntryCreateCFProperty", IORegistryEntryCreateCFProperty)
        && resolve(ioKit, "IOServiceClose", IOServiceClose)
        && resolve(ioKit, "IOServiceGetMatchingServices", IOServiceGetMatchingServices)
        && resolve(ioKit, "IOServiceGetMatchingService", IOServiceGetMatchingService)
        && resolve(ioKit, "IOServiceMatching", IOServiceMatching)
        && resolve(ioKit, "IOServiceOpen", IOServiceOpen)
        && resolve(ioKit, "IORegisterForSystemPower", IORegisterForSystemPower)
        && resolve(ioKit, "IODeregisterForSystemPower", IODeregisterForSystemPower)
        && resolve(ioKit, "IOAllowPowerChange", IOAllowPowerChange)
        && resolve(ioKit, "IONotificationPortGetRunLoopSource", IONotificationPortGetRunLoopSource)
        && resolve(ioKit, "IONotificationPortDestroy", IONotificationPortDestroy);

    if (displayServices != nullptr) {
        resolveOptional(displayServices, "DisplayServicesGetBrightness", DisplayServicesGetBrightness);
        resolveOptional(displayServices, "DisplayServicesSetBrightness", DisplayServicesSetBrightness);
    }

    return requiredAPIsLoaded;
}

template <typename Function>
bool SystemAPIs::resolve(void *handle, const char *symbol, Function &function) {
    function = reinterpret_cast<Function>(dlsym(handle, symbol));
    if (function == nullptr) {
        std::cerr << "Missing system symbol: " << symbol << "\n";
        return false;
    }

    return true;
}

template <typename Function>
void SystemAPIs::resolveOptional(void *handle, const char *symbol, Function &function) {
    function = reinterpret_cast<Function>(dlsym(handle, symbol));
}

}
