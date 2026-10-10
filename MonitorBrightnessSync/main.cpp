#include "App/BrightnessSynchronizer.hpp"
#include "App/Options.hpp"
#include "Displays/ExternalDisplayManager.hpp"
#include "Sensors/AmbientLightSensor.hpp"
#include "System/SystemAPIs.hpp"
#include "System/SystemPowerMonitor.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

std::atomic_bool isRunning = true;

void handleSignal(int) {
    isRunning = false;
}

}

int main(int argc, const char *argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    
    const std::optional<MonitorBrightnessSync::Options> parsedOptions = MonitorBrightnessSync::parseOptions(argc, argv);
    if (!parsedOptions.has_value()) {
        MonitorBrightnessSync::printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    
    MonitorBrightnessSync::SystemAPIs apis;
    if (!apis.load()) {
        std::cerr << "Failed to load required macOS display and IOKit APIs.\n";
        return EXIT_FAILURE;
    }
    
    MonitorBrightnessSync::AmbientLightSensor sensor(apis);
    if (!sensor.isAvailable()) {
        std::cerr << "Ambient light sensor unavailable: " << sensor.diagnosticMessage() << "\n";
        return EXIT_FAILURE;
    }
    
    MonitorBrightnessSync::ExternalDisplayManager displayManager(apis);
    MonitorBrightnessSync::SystemPowerMonitor powerMonitor(apis);
    if (!powerMonitor.isAvailable()) {
        std::cerr << "Failed to register for system sleep/wake notifications.\n";
        return EXIT_FAILURE;
    }
    MonitorBrightnessSync::BrightnessSynchronizer synchronizer(
                                                     *parsedOptions,
                                                     sensor,
                                                     displayManager,
                                                     powerMonitor,
                                                     isRunning
                                                     );
    
    return synchronizer.run();
}
