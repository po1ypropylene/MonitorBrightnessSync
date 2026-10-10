#include "App/BrightnessSynchronizer.hpp"
#include "Displays/BrightnessMapper.hpp"

#include <IOKit/IOMessage.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <thread>

using namespace MonitorBrightnessSync;

namespace {

struct Write {
    CGDirectDisplayID id;
    float brightness;
    bool succeeded;
};

struct Fixture {
    bool luxSensor = false;
    bool builtInAvailable = true;
    bool registrationSucceeds = true;
    bool sourceAvailable = true;
    float builtIn = 0.4F;
    uint64_t lux = 100;
    int builtInReadFailures = 0;
    int builtInReads = 0;
    int polls = 0;
    int adds = 0;
    int removes = 0;
    int deregistrations = 0;
    int closes = 0;
    int destroys = 0;
    std::string matchingClass;
    std::vector<CGDirectDisplayID> externalIDs{2, 3};
    std::map<CGDirectDisplayID, float> brightness{{2, 0.9F}, {3, 0.2F}};
    std::map<CGDirectDisplayID, int> failures;
    std::vector<Write> writes;
    std::vector<intptr_t> acknowledgements;
    IOServiceInterestCallback callback = nullptr;
    void *context = nullptr;
    std::function<void()> onPoll;

    void notify(natural_t message, intptr_t token = 0) {
        callback(context, 0, message, reinterpret_cast<void *>(token));
    }

    bool wrote(CGDirectDisplayID id, float value) const {
        for (const auto &write : writes) {
            if (write.id == id && write.succeeded && std::abs(write.brightness - value) < 0.0001F) {
                return true;
            }
        }
        return false;
    }
};

Fixture *fixture = nullptr;

SystemAPIs simulatedAPIs(Fixture &state) {
    fixture = &state;
    SystemAPIs apis;
    // Load symbols, but replace every API that accesses hardware or power state.
    assert(apis.load());
    apis.CGGetOnlineDisplayList = apis.CGGetActiveDisplayList = [](uint32_t capacity, CGDirectDisplayID *ids, uint32_t *count) {
        *count = static_cast<uint32_t>(fixture->externalIDs.size() + 1);
        if (capacity > 0) {
            assert(capacity >= *count);
            ids[0] = 1;
            std::copy(fixture->externalIDs.begin(), fixture->externalIDs.end(), ids + 1);
        }
        return kCGErrorSuccess;
    };
    apis.CGDisplayIsBuiltin = [](CGDirectDisplayID id) -> boolean_t { return id == 1; };
    apis.CGDisplayIOServicePort = [](CGDirectDisplayID) -> io_service_t { return IO_OBJECT_NULL; };
    apis.IOServiceMatching = [](const char *name) -> CFMutableDictionaryRef {
        fixture->matchingClass = name;
        return nullptr;
    };
    apis.IOServiceGetMatchingService = [](mach_port_t, CFDictionaryRef) -> io_service_t {
        if (fixture->luxSensor && fixture->matchingClass == "AppleLMUController") {
            return 11;
        }
        return !fixture->luxSensor && fixture->matchingClass == "AppleARMBacklight" ? 12 : 0;
    };
    apis.IOServiceOpen = [](io_service_t, task_port_t, uint32_t, io_connect_t *connection) {
        *connection = 13;
        return KERN_SUCCESS;
    };
    apis.IOObjectRelease = [](io_object_t) { return KERN_SUCCESS; };
    apis.IOServiceClose = [](io_connect_t) {
        ++fixture->closes;
        return KERN_SUCCESS;
    };
    apis.IORegistryEntryCreateCFProperty = [](io_registry_entry_t, CFStringRef, CFAllocatorRef, IOOptionBits) -> CFTypeRef {
        return nullptr;
    };
    apis.IODisplayGetFloatParameter = [](io_service_t, IOOptionBits, CFStringRef, float *) { return kIOReturnNotReady; };
    apis.DisplayServicesGetBrightness = [](CGDirectDisplayID id, float *value) {
        if (id == 1) {
            ++fixture->builtInReads;
            if (!fixture->builtInAvailable) {
                ++fixture->builtInReadFailures;
                return 1;
            }
            *value = fixture->builtIn;
        } else {
            *value = fixture->brightness.at(id);
        }
        return 0;
    };
    apis.DisplayServicesSetBrightness = [](CGDirectDisplayID id, float value) {
        assert(id != 1);
        const bool success = fixture->failures[id] == 0;
        fixture->writes.push_back({id, value, success});
        if (!success) {
            --fixture->failures[id];
            return 1;
        }
        fixture->brightness[id] = value;
        return 0;
    };
    apis.IOConnectCallMethod = [](mach_port_t, uint32_t, const uint64_t *, uint32_t,
                                  const void *, size_t, uint64_t *values, uint32_t *count, void *, size_t *) {
        values[0] = values[1] = fixture->lux;
        *count = 2;
        return KERN_SUCCESS;
    };
    apis.IORegisterForSystemPower = [](void *context, IONotificationPortRef *port,
                                      IOServiceInterestCallback callback, io_object_t *notifier) -> io_connect_t {
        if (!fixture->registrationSucceeds) {
            return IO_OBJECT_NULL;
        }
        fixture->context = context;
        fixture->callback = callback;
        *port = reinterpret_cast<IONotificationPortRef>(1);
        *notifier = 14;
        return 15;
    };
    apis.IONotificationPortGetRunLoopSource = [](IONotificationPortRef) -> CFRunLoopSourceRef {
        return fixture->sourceAvailable ? reinterpret_cast<CFRunLoopSourceRef>(1) : nullptr;
    };
    apis.CFRunLoopAddSource = [](CFRunLoopRef, CFRunLoopSourceRef, CFRunLoopMode) { ++fixture->adds; };
    apis.CFRunLoopRemoveSource = [](CFRunLoopRef, CFRunLoopSourceRef, CFRunLoopMode) { ++fixture->removes; };
    apis.CFRunLoopRunInMode = [](CFRunLoopMode, CFTimeInterval timeout, Boolean) {
        if (timeout == 0) {
            ++fixture->polls;
            assert(fixture->polls < 200);
            if (fixture->onPoll) {
                fixture->onPoll();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::duration<double>(timeout));
        }
        return kCFRunLoopRunTimedOut;
    };
    apis.IOAllowPowerChange = [](io_connect_t, intptr_t token) {
        fixture->acknowledgements.push_back(token);
        return kIOReturnSuccess;
    };
    apis.IODeregisterForSystemPower = [](io_object_t *notifier) {
        ++fixture->deregistrations;
        *notifier = IO_OBJECT_NULL;
        return kIOReturnSuccess;
    };
    apis.IONotificationPortDestroy = [](IONotificationPortRef) { ++fixture->destroys; };
    return apis;
}

void testWakeAndRelativeSync() {
    Fixture state;
    auto apis = simulatedAPIs(state);
    std::atomic_bool running = true;
    Options options;
    options.intervalSeconds = 0.001;
    int stage = 0;
    {
        AmbientLightSensor sensor(apis);
        ExternalDisplayManager displays(apis);
        SystemPowerMonitor power(apis);
        assert(power.isAvailable());
        state.onPoll = [&] {
            if (stage == 0 && state.writes.size() == 2) {
                // Preserve each startup brightness until sleep. A returning monitor
                // has a different ID and a brightness unrelated to the old baseline.
                assert(state.wrote(2, 0.9F) && state.wrote(3, 0.2F));
                state.notify(kIOMessageCanSystemSleep, 5);
                state.notify(kIOMessageSystemWillSleep, 6);
                assert(!power.canAdjustBrightness());
                state.builtIn = 0.6F;
                state.builtInAvailable = false;
                state.externalIDs = {9};
                state.brightness[9] = 0.05F;
                state.brightness[3] = 1.0F;
                state.failures[9] = 1;
                state.notify(kIOMessageSystemHasPoweredOn);
                assert(!power.canAdjustBrightness());
                stage = 1;
            } else if (stage == 1 && state.builtInReadFailures > 0) {
                state.builtInAvailable = true;
                stage = 2;
            } else if (stage == 2 && state.wrote(9, 0.6F)) {
                // The other monitor returns later and must receive its own reset.
                state.externalIDs = {9, 3};
                stage = 3;
            } else if (stage == 3 && state.wrote(3, 0.6F)) {
                state.builtIn = 0.3F;
                stage = 4;
            } else if (stage == 4 && state.wrote(9, 0.3F) && state.wrote(3, 0.3F)) {
                state.brightness[9] = 0.8F;
                stage = 5;
            } else if (stage == 5) {
                // The preceding poll adopted the manual external adjustment.
                state.builtIn = 0.15F;
                stage = 6;
            } else if (stage == 6 && state.wrote(9, 0.4F) && state.wrote(3, 0.15F)) {
                state.notify(kIOMessageSystemWillSleep, 7);
                state.builtIn = 0.0F;
                state.notify(kIOMessageSystemHasPoweredOn);
                stage = 7;
            } else if (stage == 7 && state.wrote(9, 0.0F) && state.wrote(3, 0.0F)) {
                state.notify(kIOMessageSystemWillSleep, 8);
                state.builtIn = 1.0F;
                state.notify(kIOMessageSystemHasPoweredOn);
                stage = 8;
            } else if (stage == 8 && state.wrote(9, 1.0F) && state.wrote(3, 1.0F)) {
                running = false;
                stage = 9;
            }
        };
        BrightnessSynchronizer sync(options, sensor, displays, power, running);
        assert(sync.run() == 0);
        assert(stage == 9 && power.wakeGeneration() == 3);
        assert(state.acknowledgements == std::vector<intptr_t>({5, 6, 7, 8}));
        int failedWrites = 0;
        for (const auto &write : state.writes) {
            failedWrites += !write.succeeded;
        }
        assert(failedWrites == 1);
    }
    assert(state.adds == 1 && state.removes == 1);
    assert(state.deregistrations == 1 && state.destroys == 1 && state.closes == 1);
}

void testLuxWake(bool dryRun) {
    Fixture state;
    state.luxSensor = true;
    state.builtIn = 0.8F;
    auto apis = simulatedAPIs(state);
    std::atomic_bool running = true;
    Options options;
    options.minimumBrightness = 0.2F;
    options.maximumBrightness = 0.25F;
    options.intervalSeconds = 0.001;
    options.dryRun = dryRun;
    std::ostringstream output;
    auto *previous = std::cout.rdbuf(output.rdbuf());
    {
        AmbientLightSensor sensor(apis);
        ExternalDisplayManager displays(apis);
        SystemPowerMonitor power(apis);
        assert(sensor.read()->kind == AmbientLightReadingKind::Lux);
        assert(brightnessForLux(state.lux, options) < 0.3F);
        state.onPoll = [&] {
            if (state.polls == 1) {
                state.notify(kIOMessageSystemHasPoweredOn);
            } else if (state.builtInReads > 0) {
                running = false;
            }
        };
        BrightnessSynchronizer sync(options, sensor, displays, power, running);
        assert(sync.run() == 0);
    }
    std::cout.rdbuf(previous);
    if (dryRun) {
        assert(state.writes.empty());
        assert(output.str().find("would align brightness after wake to 0.8") != std::string::npos);
        assert(state.brightness[2] == 0.9F && state.brightness[3] == 0.2F);
    } else {
        assert(state.wrote(2, 0.8F) && state.wrote(3, 0.8F));
    }
}

void testPowerRegistrationFailures() {
    Fixture failed;
    failed.registrationSucceeds = false;
    auto apis = simulatedAPIs(failed);
    {
        SystemPowerMonitor power(apis);
        assert(!power.isAvailable());
    }
    assert(failed.deregistrations == 0 && failed.closes == 0 && failed.destroys == 0);

    Fixture missingSource;
    missingSource.sourceAvailable = false;
    apis = simulatedAPIs(missingSource);
    {
        SystemPowerMonitor power(apis);
        assert(!power.isAvailable());
    }
    assert(missingSource.deregistrations == 1 && missingSource.closes == 1 && missingSource.destroys == 1);
}

void testInvalidBuiltInReadingAndFallback() {
    Fixture state;
    auto apis = simulatedAPIs(state);
    AmbientLightSensor sensor(apis);
    state.builtIn = std::numeric_limits<float>::quiet_NaN();
    assert(!sensor.readBuiltInBrightness().has_value());
    apis.DisplayServicesGetBrightness = nullptr;
    apis.CGDisplayIOServicePort = [](CGDirectDisplayID id) -> io_service_t { return id == 1 ? 20 : 0; };
    apis.IODisplayGetFloatParameter = [](io_service_t, IOOptionBits, CFStringRef, float *value) {
        *value = 0.7F;
        return kIOReturnSuccess;
    };
    assert(std::abs(sensor.readBuiltInBrightness()->value - 0.7) < 0.0001);
}

}

int main() {
    testPowerRegistrationFailures();
    testInvalidBuiltInReadingAndFallback();
    testWakeAndRelativeSync();
    testLuxWake(false);
    testLuxWake(true);
    std::cout << "PASS: wake alignment, retries, reconnects, baseline reset, manual adjustments, endpoints, lux, dry run, and power cleanup\n";
}
