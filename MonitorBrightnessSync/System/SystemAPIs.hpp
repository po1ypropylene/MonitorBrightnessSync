#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <mach/mach.h>

namespace MonitorBrightnessSync {

struct SystemAPIs {
    using CFDictionaryGetCountFunction = CFIndex (*)(CFDictionaryRef);
    using CFDictionaryGetKeysAndValuesFunction = void (*)(CFDictionaryRef, const void **, const void **);
    using CFDictionaryGetTypeIDFunction = CFTypeID (*)();
    using CFDictionaryGetValueFunction = const void *(*)(CFDictionaryRef, const void *);
    using CFGetTypeIDFunction = CFTypeID (*)(CFTypeRef);
    using CFNumberGetTypeIDFunction = CFTypeID (*)();
    using CFNumberGetValueFunction = Boolean (*)(CFNumberRef, CFNumberType, void *);
    using CFReleaseFunction = void (*)(CFTypeRef);
    using CFStringCreateWithCStringFunction = CFStringRef (*)(CFAllocatorRef, const char *, CFStringEncoding);
    using CFStringGetCStringFunction = Boolean (*)(CFStringRef, char *, CFIndex, CFStringEncoding);
    using CFStringGetTypeIDFunction = CFTypeID (*)();

    using CGDisplayIOServicePortFunction = io_service_t (*)(CGDirectDisplayID);
    using CGDisplayIsBuiltinFunction = boolean_t (*)(CGDirectDisplayID);
    using CGGetActiveDisplayListFunction = CGError (*)(uint32_t, CGDirectDisplayID *, uint32_t *);
    using CGGetOnlineDisplayListFunction = CGError (*)(uint32_t, CGDirectDisplayID *, uint32_t *);
    using CGSSetDisplayBrightnessFunction = CGError (*)(CGDirectDisplayID, float);
    using DisplayServicesGetBrightnessFunction = int (*)(CGDirectDisplayID, float *);
    using DisplayServicesSetBrightnessFunction = int (*)(CGDirectDisplayID, float);

    using IOConnectCallMethodFunction = kern_return_t (*)(
        mach_port_t,
        uint32_t,
        const uint64_t *,
        uint32_t,
        const void *,
        size_t,
        uint64_t *,
        uint32_t *,
        void *,
        size_t *
    );
    using IODisplayCreateInfoDictionaryFunction = CFDictionaryRef (*)(io_service_t, IOOptionBits);
    using IODisplayGetFloatParameterFunction = IOReturn (*)(io_service_t, IOOptionBits, CFStringRef, float *);
    using IODisplaySetFloatParameterFunction = IOReturn (*)(io_service_t, IOOptionBits, CFStringRef, float);
    using IOIteratorNextFunction = io_object_t (*)(io_iterator_t);
    using IOObjectReleaseFunction = kern_return_t (*)(io_object_t);
    using IORegistryEntryCreateCFPropertyFunction = CFTypeRef (*)(
        io_registry_entry_t,
        CFStringRef,
        CFAllocatorRef,
        IOOptionBits
    );
    using IOServiceCloseFunction = kern_return_t (*)(io_connect_t);
    using IOServiceGetMatchingServicesFunction = kern_return_t (*)(mach_port_t, CFDictionaryRef, io_iterator_t *);
    using IOServiceGetMatchingServiceFunction = io_service_t (*)(mach_port_t, CFDictionaryRef);
    using IOServiceMatchingFunction = CFMutableDictionaryRef (*)(const char *);
    using IOServiceOpenFunction = kern_return_t (*)(io_service_t, task_port_t, uint32_t, io_connect_t *);

    void *coreFoundation = nullptr;
    void *coreGraphics = nullptr;
    void *displayServices = nullptr;
    void *ioKit = nullptr;

    CFDictionaryGetCountFunction CFDictionaryGetCount = nullptr;
    CFDictionaryGetKeysAndValuesFunction CFDictionaryGetKeysAndValues = nullptr;
    CFDictionaryGetTypeIDFunction CFDictionaryGetTypeID = nullptr;
    CFDictionaryGetValueFunction CFDictionaryGetValue = nullptr;
    CFGetTypeIDFunction CFGetTypeID = nullptr;
    CFNumberGetTypeIDFunction CFNumberGetTypeID = nullptr;
    CFNumberGetValueFunction CFNumberGetValue = nullptr;
    CFReleaseFunction CFRelease = nullptr;
    CFStringCreateWithCStringFunction CFStringCreateWithCString = nullptr;
    CFStringGetCStringFunction CFStringGetCString = nullptr;
    CFStringGetTypeIDFunction CFStringGetTypeID = nullptr;

    CGDisplayIOServicePortFunction CGDisplayIOServicePort = nullptr;
    CGDisplayIsBuiltinFunction CGDisplayIsBuiltin = nullptr;
    CGGetActiveDisplayListFunction CGGetActiveDisplayList = nullptr;
    CGGetOnlineDisplayListFunction CGGetOnlineDisplayList = nullptr;
    CGSSetDisplayBrightnessFunction CGSSetDisplayBrightness = nullptr;
    DisplayServicesGetBrightnessFunction DisplayServicesGetBrightness = nullptr;
    DisplayServicesSetBrightnessFunction DisplayServicesSetBrightness = nullptr;

    IOConnectCallMethodFunction IOConnectCallMethod = nullptr;
    IODisplayCreateInfoDictionaryFunction IODisplayCreateInfoDictionary = nullptr;
    IODisplayGetFloatParameterFunction IODisplayGetFloatParameter = nullptr;
    IODisplaySetFloatParameterFunction IODisplaySetFloatParameter = nullptr;
    IOIteratorNextFunction IOIteratorNext = nullptr;
    IOObjectReleaseFunction IOObjectRelease = nullptr;
    IORegistryEntryCreateCFPropertyFunction IORegistryEntryCreateCFProperty = nullptr;
    IOServiceCloseFunction IOServiceClose = nullptr;
    IOServiceGetMatchingServicesFunction IOServiceGetMatchingServices = nullptr;
    IOServiceGetMatchingServiceFunction IOServiceGetMatchingService = nullptr;
    IOServiceMatchingFunction IOServiceMatching = nullptr;
    IOServiceOpenFunction IOServiceOpen = nullptr;

    bool load();

private:
    template <typename Function>
    bool resolve(void *handle, const char *symbol, Function &function);
    
    template <typename Function>
    void resolveOptional(void *handle, const char *symbol, Function &function);
};

}
