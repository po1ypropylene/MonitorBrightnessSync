#pragma once

#include "ExternalDisplay.hpp"
#include "../System/SystemAPIs.hpp"

#include <IOKit/IOReturn.h>

#include <optional>
#include <string>
#include <vector>

namespace MonitorBrightnessSync {

class ExternalDisplayManager {
public:
    explicit ExternalDisplayManager(const SystemAPIs &apis);

    std::vector<ExternalDisplay> displays() const;
    std::string diagnosticMessage() const;
    void releaseDisplayServices(const std::vector<ExternalDisplay> &displays) const;
    std::optional<float> brightness(const ExternalDisplay &display) const;
    IOReturn setBrightness(const ExternalDisplay &display, float brightness) const;

private:
    using DisplayListFunction = CGError (*)(uint32_t, CGDirectDisplayID *, uint32_t *);

    std::vector<CGDirectDisplayID> displayIDs(DisplayListFunction listDisplays) const;
    std::optional<std::string> displayName(io_service_t service) const;
    std::optional<std::string> stringProperty(io_service_t service, const char *key) const;
    std::vector<ExternalDisplay> externalDisplays(
        const std::vector<CGDirectDisplayID> &displayIDs,
        std::string *diagnostic
    ) const;
    std::vector<ExternalDisplay> registryDisplays(std::string *diagnostic) const;
    bool isLikelyBuiltInDisplay(const std::string &name, const std::optional<std::string> &prefsKey) const;

    const SystemAPIs &apis;
};

}
