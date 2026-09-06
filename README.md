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

Open `MonitorBrightnessSync.xcodeproj` in Xcode, select the `MonitorPlus` scheme, then build the project.

You can also build from the command line:

```sh
xcodebuild -project MonitorBrightnessSync.xcodeproj -scheme MonitorPlus -configuration Release build
```

After building, locate the `MonitorBrightnessSync` executable in Xcode's build products directory. In Xcode, you can usually find it by opening the Products group, right-clicking `MonitorBrightnessSync`, and choosing "Show in Finder".

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

Logs are written to:

```text
~/Library/Logs/MonitorBrightnessSync.out.log
~/Library/Logs/MonitorBrightnessSync.err.log
```

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
