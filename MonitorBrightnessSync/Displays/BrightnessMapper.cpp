#include "BrightnessMapper.hpp"

#include <algorithm>
#include <cmath>

namespace MonitorBrightnessSync {

float brightnessForLux(double lux, const Options &options) {
    constexpr double brightOfficeLux = 500.0;
    const double normalized = std::clamp(
        std::log1p(std::max(0.0, lux)) / std::log1p(brightOfficeLux),
        0.0,
        1.0
    );
    const double range = options.maximumBrightness - options.minimumBrightness;

    return static_cast<float>(options.minimumBrightness + (normalized * range));
}

}
