#pragma once

#include "CFString.hpp"

#include <chrono>
#include <cstdint>

namespace MonitorBrightnessSync {

// Notifications and brightness updates run on the same thread.
class SystemPowerMonitor {
public:
    explicit SystemPowerMonitor(const SystemAPIs &apis);
    SystemPowerMonitor(const SystemPowerMonitor &) = delete;
    SystemPowerMonitor &operator=(const SystemPowerMonitor &) = delete;
    ~SystemPowerMonitor();

    bool isAvailable() const;
    void processEvents(double timeoutSeconds) const;
    bool canAdjustBrightness() const;
    uint64_t wakeGeneration() const;

private:
    static void handlePowerMessage(void *context, io_service_t service, natural_t message, void *argument);

    const SystemAPIs &apis;
    CFString runLoopMode;
    CFRunLoopRef runLoop = nullptr;
    CFRunLoopSourceRef source = nullptr;
    IONotificationPortRef notificationPort = nullptr;
    io_connect_t connection = IO_OBJECT_NULL;
    io_object_t notifier = IO_OBJECT_NULL;
    bool sleeping = false;
    uint64_t generation = 0;
    std::chrono::steady_clock::time_point readyAt{};
};

}
