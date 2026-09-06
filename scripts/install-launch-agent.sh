#!/bin/sh
set -eu

label="com.monitorbrightnesssync.agent"
binary_name="MonitorBrightnessSync"
install_dir="$HOME/.local/bin"
installed_binary="$install_dir/$binary_name"
agent_dir="$HOME/Library/LaunchAgents"
agent_plist="$agent_dir/$label.plist"
stdout_log="$HOME/Library/Logs/MonitorBrightnessSync.out.log"
stderr_log="$HOME/Library/Logs/MonitorBrightnessSync.err.log"

usage() {
    echo "Usage: $0 /path/to/MonitorBrightnessSync [MonitorBrightnessSync options]" >&2
    echo "Example: $0 ./build/Release/MonitorBrightnessSync --interval 1 --min 0.15 --max 0.85" >&2
}

escape_xml() {
    printf '%s' "$1" \
        | sed \
            -e 's/&/\&amp;/g' \
            -e 's/</\&lt;/g' \
            -e 's/>/\&gt;/g' \
            -e 's/"/\&quot;/g' \
            -e "s/'/\&apos;/g"
}

if [ "$#" -lt 1 ]; then
    usage
    exit 64
fi

source_binary="$1"
shift

if [ ! -f "$source_binary" ]; then
    echo "Binary not found: $source_binary" >&2
    exit 66
fi

if [ ! -x "$source_binary" ]; then
    echo "Binary is not executable: $source_binary" >&2
    exit 66
fi

mkdir -p "$install_dir" "$agent_dir" "$HOME/Library/Logs"
cp "$source_binary" "$installed_binary"
chmod 755 "$installed_binary"

if [ "$#" -eq 0 ]; then
    set -- --interval 1
fi

{
    printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>'
    printf '%s\n' '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "https://www.apple.com/DTDs/PropertyList-1.0.dtd">'
    printf '%s\n' '<plist version="1.0">'
    printf '%s\n' '<dict>'
    printf '%s\n' '    <key>Label</key>'
    printf '    <string>%s</string>\n' "$(escape_xml "$label")"
    printf '%s\n' '    <key>ProgramArguments</key>'
    printf '%s\n' '    <array>'
    printf '        <string>%s</string>\n' "$(escape_xml "$installed_binary")"
    for argument in "$@"; do
        printf '        <string>%s</string>\n' "$(escape_xml "$argument")"
    done
    printf '%s\n' '    </array>'
    printf '%s\n' '    <key>RunAtLoad</key>'
    printf '%s\n' '    <true/>'
    printf '%s\n' '    <key>KeepAlive</key>'
    printf '%s\n' '    <true/>'
    printf '%s\n' '    <key>StandardOutPath</key>'
    printf '    <string>%s</string>\n' "$(escape_xml "$stdout_log")"
    printf '%s\n' '    <key>StandardErrorPath</key>'
    printf '    <string>%s</string>\n' "$(escape_xml "$stderr_log")"
    printf '%s\n' '</dict>'
    printf '%s\n' '</plist>'
} > "$agent_plist"

launchctl bootout "gui/$(id -u)" "$agent_plist" 2>/dev/null || true
launchctl bootstrap "gui/$(id -u)" "$agent_plist"
launchctl kickstart -k "gui/$(id -u)/$label"

echo "Installed $installed_binary"
echo "Loaded $agent_plist"
