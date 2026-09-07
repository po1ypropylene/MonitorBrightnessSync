#pragma once

#include "SystemAPIs.hpp"

#include <CoreFoundation/CoreFoundation.h>

namespace MonitorBrightnessSync {

class CFString {
public:
    CFString(const SystemAPIs &apis, const char *value);
    CFString(const CFString &) = delete;
    CFString &operator=(const CFString &) = delete;
    ~CFString();

    CFStringRef get() const;

private:
    const SystemAPIs &apis;
    CFStringRef string = nullptr;
};

}
