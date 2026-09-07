# MonitorBrightnessSync

MonitorBrightnessSync is a small macOS command-line utility that keeps external display brightness in sync with the Mac's built-in ambient light sensor.

The app runs in the background, polls the ambient light sensor, maps the current light level to a brightness range, and applies that brightness to connected external displays.

## Requirements

- macOS
- A Mac with a built-in ambient light sensor
- At least one external display whose brightness can be controlled by macOS display APIs
- Xcode, to build the app from source

## Usage

Run the binary directly:

```sh
MonitorBrightnessSync [options]
```

Options:

```text
--interval seconds   Poll interval. Default: 0.25
--min value          Minimum brightness, 0.0-1.0. Default: 0.15
--max value          Maximum brightness, 0.0-1.0. Default: 0.85
--once               Sync once and exit
--dry-run            Print the target brightness without changing displays
--help               Show help
```

Example:

```sh
MonitorBrightnessSync --interval 1 --min 0.2 --max 0.9
```

## Build

Open `MonitorBrightnessSync.xcodeproj` in Xcode, select the `MonitorBrightnessSync` scheme, then build the project.

### Release Binary

Use the Release configuration for the binary you install as a background service. Release builds omit the recurring brightness sync progress logs that are useful while debugging but noisy when the app runs continuously.

From the repository root, build with:

```sh
xcodebuild -project MonitorBrightnessSync.xcodeproj -scheme MonitorBrightnessSync -configuration Release build
```

The executable is written under Xcode's DerivedData build products directory, usually:

```text
~/Library/Developer/Xcode/DerivedData/MonitorBrightnessSync-*/Build/Products/Release/MonitorBrightnessSync
```

In Xcode, you can also locate it from the Products group by right-clicking `MonitorBrightnessSync` and choosing "Show in Finder".

For local debugging, build the Debug configuration from Xcode or run:

```sh
xcodebuild -project MonitorBrightnessSync.xcodeproj -scheme MonitorBrightnessSync -configuration Debug build
```

## Install as a Background Service

The repository includes an installer script that copies the built executable to `~/.local/bin/MonitorBrightnessSync`, creates a per-user LaunchAgent, and starts it with `launchctl`.

From the repository root, run:

```sh
./scripts/install-launch-agent.sh /path/to/MonitorBrightnessSync
```

For example:

```sh
./scripts/install-launch-agent.sh ~/Library/Developer/Xcode/DerivedData/MonitorBrightnessSync-*/Build/Products/Release/MonitorBrightnessSync
```

By default, the LaunchAgent runs the app with:

```sh
--interval 1
```

To pass different runtime options, add them after the binary path:

```sh
./scripts/install-launch-agent.sh /path/to/MonitorBrightnessSync --interval 0.5 --min 0.2 --max 0.8
```

The LaunchAgent writes stdout and stderr to:

```text
~/Library/Logs/MonitorBrightnessSync.out.log
~/Library/Logs/MonitorBrightnessSync.err.log
```

When installed from a Release binary, normal polling progress is not printed to those files.

## Manage the LaunchAgent

Stop the background service:

```sh
launchctl bootout "gui/$(id -u)" ~/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist
```

Start it again:

```sh
launchctl bootstrap "gui/$(id -u)" ~/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist
```

Restart it:

```sh
launchctl kickstart -k "gui/$(id -u)/com.monitorbrightnesssync.agent"
```

Uninstall it:

```sh
launchctl bootout "gui/$(id -u)" ~/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist 2>/dev/null || true
rm -f ~/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist
rm -f ~/.local/bin/MonitorBrightnessSync
```

## Notes

- The app exits if the ambient light sensor is unavailable.
- It must keep running to continuously sync brightness, so installing it as a LaunchAgent is the recommended setup.
- `--once` is useful for testing but should not be used for the background LaunchAgent.
