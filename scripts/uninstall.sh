#!/bin/zsh
set -euo pipefail

readonly serviceLabel="com.monitorbrightnesssync.agent"
readonly installationDirectory="$HOME/Library/Application Support/MonitorBrightnessSync"
readonly launchAgentPath="$HOME/Library/LaunchAgents/$serviceLabel.plist"
readonly serviceDomain="gui/$(id -u)"

launchctl bootout "$serviceDomain/$serviceLabel" 2>/dev/null || true
rm -f "$launchAgentPath"
rm -f "$installationDirectory/MonitorBrightnessSync"
rm -f "$HOME/.local/bin/MonitorBrightnessSync"
rmdir "$installationDirectory" 2>/dev/null || true

echo "MonitorBrightnessSync has been uninstalled."
echo "Logs remain in $HOME/Library/Logs/MonitorBrightnessSync."
echo "Older installations may also have MonitorBrightnessSync.out.log and MonitorBrightnessSync.err.log in ~/Library/Logs."
