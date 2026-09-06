#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/IOKitLib.h>

#include <string>

namespace MonitorPlus {

struct ExternalDisplay {
    CGDirectDisplayID id = 0;
    io_service_t service = IO_OBJECT_NULL;
    std::string name;
    bool releasesService = false;
};

} // namespace MonitorPlus
