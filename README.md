# MonitorBrightnessSync

MonitorBrightnessSync keeps external display brightness in sync with the Mac's
built-in ambient light sensor (or built-in display brightness when the sensor
cannot be accessed). It runs without a window and follows changes relative to
each external display's initial brightness. Manual external brightness changes
establish a new baseline.

After the Mac wakes from system sleep, the app waits two seconds for display
services to settle, then sets each external monitor to the built-in display's
actual normalized brightness (0–1). For example, built-in brightness `0.6`
sets each external display to `0.6`. This resets the relative baseline so earlier
offsets do not carry over. Failed reads or writes retry on subsequent polls;
monitors that reconnect later are aligned when they become available.

The wake reset uses the actual built-in value, regardless of `--min` and `--max`.
Those options configure the ambient-light mapping when lux readings are used.
Equal normalized values can look different on displays with different panels.

## Requirements

- macOS and a Mac with a built-in ambient light sensor
- An external display whose brightness macOS display APIs can control
- Xcode with the macOS SDK

The background service uses a per-user LaunchAgent. It starts when you log in
and runs in your graphical session. Run the scripts as your normal user,
without `sudo`.

## Build

From the repository root:

```sh
xcodebuild \
  -project MonitorBrightnessSync.xcodeproj \
  -scheme MonitorBrightnessSync \
  -configuration Release \
  -derivedDataPath build \
  build
```

The executable is created at:

```text
build/Build/Products/Release/MonitorBrightnessSync
```

You can also select the MonitorBrightnessSync scheme in Xcode and choose
**Product > Build**. Use Release for background installation: it omits recurring
brightness progress logs. Use Debug for development.

## Install as a background service

From the repository root, run:

```sh
./scripts/install.sh
```

The installer builds Release, stops any existing service, installs the executable
and LaunchAgent, and starts the service. It starts at login and restarts if it
exits. No manual build or search through Xcode's DerivedData is needed.

Installed files:

```text
~/Library/Application Support/MonitorBrightnessSync/MonitorBrightnessSync
~/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist
```

By default, the service polls once per second (`--interval 1`). To customize it:

```sh
./scripts/install.sh --interval 0.5 --min 0.2 --max 0.8
```

The supplied options replace the service defaults. Supply those options again
when updating; running the installer without options restores `--interval 1`.
Do not use `--once` or `--help` for the background service.

## Service management

Check status:

```sh
launchctl print "gui/$(id -u)/com.monitorbrightnesssync.agent"
```

Stop until the next login (or until you start it again):

```sh
launchctl bootout "gui/$(id -u)/com.monitorbrightnesssync.agent"
```

Start after stopping:

```sh
launchctl bootstrap "gui/$(id -u)" "$HOME/Library/LaunchAgents/com.monitorbrightnesssync.agent.plist"
```

Restart a loaded service:

```sh
launchctl kickstart -k "gui/$(id -u)/com.monitorbrightnesssync.agent"
```

A stopped service is unloaded, so start it with `bootstrap` before using
`kickstart`. Sending a termination signal alone does not keep a `KeepAlive`
service stopped.

Logs:

```text
~/Library/Logs/MonitorBrightnessSync/output.log
~/Library/Logs/MonitorBrightnessSync/error.log
```

Read recent errors:

```sh
tail -n 50 "$HOME/Library/Logs/MonitorBrightnessSync/error.log"
```

Release builds do not print normal polling progress. If the service repeatedly
exits, check the error log: the app exits when the ambient light sensor is
unavailable. Stop it while diagnosing the problem.

## Update

After changing the source, run the installer again:

```sh
./scripts/install.sh
```

The build completes before the running service is stopped. The executable is
then replaced and the service started again. Include your custom runtime options
if you use them.

Existing installations keep the same service identifier. The installer replaces
the old LaunchAgent and removes the old `~/.local/bin/MonitorBrightnessSync`
executable. Old logs in `~/Library/Logs/MonitorBrightnessSync.out.log` and
`MonitorBrightnessSync.err.log` are retained.

## Uninstall

```sh
./scripts/uninstall.sh
```

This stops the service and removes the installed executable and LaunchAgent,
including the executable from the old installation location. Logs are retained.
It is safe to run the uninstaller again.

## Foreground usage

Stop the background service first if it is installed, to avoid two processes
controlling brightness at the same time. Run the local build directly:

```sh
./build/Build/Products/Release/MonitorBrightnessSync --once --dry-run
```

```text
--interval seconds   Poll interval. Default: 0.25
--min value          Minimum brightness, 0.0-1.0. Default: 0.15
--max value          Maximum brightness, 0.0-1.0. Default: 0.85
--once               Sync once and exit
--dry-run            Print the target brightness without changing displays
--help               Show help
```

## Guidance for agents

See [macOS background service conventions](docs/macos-background-services.md)
for reusable build, install, lifecycle, and documentation instructions based on
MouseScroller.

To verify the lifecycle scripts without installing a real service (requires Python 3):

```sh
python3 tests/test_service_scripts.py
```

To verify brightness syncing and sleep/wake behavior with simulated display and
power APIs (requires Python 3 and Xcode command line tools):

```sh
python3 tests/test_brightness_sync.py
```
