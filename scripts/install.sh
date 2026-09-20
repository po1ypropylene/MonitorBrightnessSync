#!/bin/zsh
set -euo pipefail

readonly serviceLabel="com.monitorbrightnesssync.agent"
readonly projectDirectory="$(cd "$(dirname "$0")/.." && pwd)"
readonly buildDirectory="$projectDirectory/build"
readonly installationDirectory="$HOME/Library/Application Support/MonitorBrightnessSync"
readonly executableDestination="$installationDirectory/MonitorBrightnessSync"
readonly launchAgentPath="$HOME/Library/LaunchAgents/$serviceLabel.plist"
readonly logDirectory="$HOME/Library/Logs/MonitorBrightnessSync"
readonly serviceDomain="gui/$(id -u)"

executableSource="$buildDirectory/Build/Products/Release/MonitorBrightnessSync"
buildFromSource=true
if [[ "${1:-}" == --binary ]]; then
    if (( $# < 2 )); then
        echo "Usage: $0 [--binary /path/to/MonitorBrightnessSync] [runtime options]" >&2
        exit 64
    fi
    executableSource="$2"
    buildFromSource=false
    shift 2
fi
for argument in "$@"; do
    if [[ "$argument" == --once || "$argument" == --help ]]; then
        echo "$argument is for foreground use, not a continuously running service." >&2
        exit 64
    fi
done
if (( $# == 0 )); then
    set -- --interval 1
fi

if $buildFromSource; then
    echo "Building MonitorBrightnessSync..."
    xcodebuild \
        -project "$projectDirectory/MonitorBrightnessSync.xcodeproj" \
        -scheme MonitorBrightnessSync \
        -configuration Release \
        -derivedDataPath "$buildDirectory" \
        build
fi
if [[ ! -f "$executableSource" || ! -x "$executableSource" ]]; then
    echo "Executable not found or not executable: $executableSource" >&2
    exit 66
fi

mkdir -p "$installationDirectory" "$HOME/Library/LaunchAgents" "$logDirectory"
# Prepare and validate the new plist before stopping an existing service.
temporaryPlist="$(mktemp "$HOME/Library/LaunchAgents/.MonitorBrightnessSync.XXXXXX")"
trap 'rm -f "$temporaryPlist"' EXIT
plutil -create xml1 "$temporaryPlist"
plutil -insert Label -string "$serviceLabel" "$temporaryPlist"
plutil -insert ProgramArguments -xml '<array/>' "$temporaryPlist"
plutil -insert ProgramArguments.0 -string "$executableDestination" "$temporaryPlist"
argumentIndex=1
for argument in "$@"; do
    plutil -insert "ProgramArguments.$argumentIndex" -string "$argument" "$temporaryPlist"
    argumentIndex=$((argumentIndex + 1))
done
plutil -insert RunAtLoad -bool true "$temporaryPlist"
plutil -insert KeepAlive -bool true "$temporaryPlist"
plutil -insert StandardOutPath -string "$logDirectory/output.log" "$temporaryPlist"
plutil -insert StandardErrorPath -string "$logDirectory/error.log" "$temporaryPlist"
plutil -lint "$temporaryPlist"

launchctl bootout "$serviceDomain/$serviceLabel" 2>/dev/null || true
install -m 755 "$executableSource" "$executableDestination"
mv -f "$temporaryPlist" "$launchAgentPath"
# Remove the executable installed by the previous installer after replacement.
rm -f "$HOME/.local/bin/MonitorBrightnessSync"
launchctl enable "$serviceDomain/$serviceLabel"
launchctl bootstrap "$serviceDomain" "$launchAgentPath"
launchctl kickstart -k "$serviceDomain/$serviceLabel"

echo "MonitorBrightnessSync is installed and will start whenever you log in."
echo "Executable: $executableDestination"
echo "Status: launchctl print $serviceDomain/$serviceLabel"
echo "Logs: $logDirectory"
