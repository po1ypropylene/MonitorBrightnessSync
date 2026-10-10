#include "SystemPowerMonitor.hpp"

#include <IOKit/IOMessage.h>

namespace MonitorBrightnessSync {

SystemPowerMonitor::SystemPowerMonitor(const SystemAPIs &apis)
    : apis(apis), runLoopMode(apis, "MonitorBrightnessSyncPower") {
    if (runLoopMode.get() == nullptr) {
        return;
    }

    connection = apis.IORegisterForSystemPower(this, &notificationPort, handlePowerMessage, &notifier);
    if (connection == IO_OBJECT_NULL || notificationPort == nullptr) {
        return;
    }

    source = apis.IONotificationPortGetRunLoopSource(notificationPort);
    if (source != nullptr) {
        runLoop = apis.CFRunLoopGetCurrent();
        apis.CFRunLoopAddSource(runLoop, source, runLoopMode.get());
    }
}

SystemPowerMonitor::~SystemPowerMonitor() {
    if (runLoop != nullptr && source != nullptr) {
        apis.CFRunLoopRemoveSource(runLoop, source, runLoopMode.get());
    }
    if (notifier != IO_OBJECT_NULL) {
        apis.IODeregisterForSystemPower(&notifier);
    }
    if (connection != IO_OBJECT_NULL) {
        apis.IOServiceClose(connection);
    }
    if (notificationPort != nullptr) {
        apis.IONotificationPortDestroy(notificationPort);
    }
}

bool SystemPowerMonitor::isAvailable() const {
    return runLoop != nullptr && source != nullptr;
}

void SystemPowerMonitor::processEvents(double timeoutSeconds) const {
    if (isAvailable()) {
        apis.CFRunLoopRunInMode(runLoopMode.get(), timeoutSeconds, true);
    }
}

bool SystemPowerMonitor::canAdjustBrightness() const {
    return !sleeping && std::chrono::steady_clock::now() >= readyAt;
}

uint64_t SystemPowerMonitor::wakeGeneration() const {
    return generation;
}

void SystemPowerMonitor::handlePowerMessage(void *context, io_service_t, natural_t message, void *argument) {
    auto &monitor = *static_cast<SystemPowerMonitor *>(context);
    switch (message) {
        case kIOMessageCanSystemSleep:
            monitor.apis.IOAllowPowerChange(monitor.connection, reinterpret_cast<intptr_t>(argument));
            break;
        case kIOMessageSystemWillSleep:
            monitor.sleeping = true;
            monitor.apis.IOAllowPowerChange(monitor.connection, reinterpret_cast<intptr_t>(argument));
            break;
        case kIOMessageSystemWillNotSleep:
            monitor.sleeping = false;
            break;
        case kIOMessageSystemHasPoweredOn:
            monitor.sleeping = false;
            ++monitor.generation;
            // Let display services restore brightness and reconnect external monitors.
            monitor.readyAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            break;
        default:
            break;
    }
}

}
