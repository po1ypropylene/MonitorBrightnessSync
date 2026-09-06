#include "App/BrightnessSynchronizer.hpp"
#include "App/Options.hpp"
#include "Displays/ExternalDisplayManager.hpp"
#include "Sensors/AmbientLightSensor.hpp"
#include "System/SystemAPIs.hpp"

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
    
    const std::optional<MonitorPlus::Options> parsedOptions = MonitorPlus::parseOptions(argc, argv);
    if (!parsedOptions.has_value()) {
        MonitorPlus::printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    
    MonitorPlus::SystemAPIs apis;
    if (!apis.load()) {
        std::cerr << "Failed to load required macOS display and IOKit APIs.\n";
        return EXIT_FAILURE;
    }
    
    MonitorPlus::AmbientLightSensor sensor(apis);
    if (!sensor.isAvailable()) {
        std::cerr << "Ambient light sensor unavailable: " << sensor.diagnosticMessage() << "\n";
        return EXIT_FAILURE;
    }
    
    MonitorPlus::ExternalDisplayManager displayManager(apis);
    MonitorPlus::BrightnessSynchronizer synchronizer(
                                                     *parsedOptions,
                                                     sensor,
                                                     displayManager,
                                                     isRunning
                                                     );
    
    return synchronizer.run();
}
