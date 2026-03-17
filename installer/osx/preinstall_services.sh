#! /bin/bash

# MACH1 SPATIAL SYSTEM
# preinstall services

LOG_DIR="/Library/Application Support/Mach1"
LOG_FILE="$LOG_DIR/m1-spatialsystem.log"
INSTALL_DIR="/Library/Application Support/Mach1"
LAUNCH_AGENT_DIR="/Library/LaunchAgents"
LEGACY_LAUNCH_AGENT_DIR="/Library/Application Support/LaunchAgents"
LEGACY_APP_DIR="/Applications/Mach1 Spatial System"
HELPER_LABEL="com.mach1.spatial.helper"
ORIENTATION_LABEL="com.mach1.spatial.orientationmanager"
HELPER_PLIST="$LAUNCH_AGENT_DIR/${HELPER_LABEL}.plist"
ORIENTATION_PLIST="$LAUNCH_AGENT_DIR/${ORIENTATION_LABEL}.plist"

mkdir -p "$LOG_DIR"
: > "$LOG_FILE"

log()
{
    echo "$1" >> "$LOG_FILE"
}

logged_in_uids()
{
    ps -axo uid=,args= | awk '/[l]oginwindow.app/ { print $1 }' | sort -u
}

user_home_for_uid()
{
    local uid="$1"
    local username
    local user_home=""

    username=$(dscl . -search /Users UniqueID "$uid" 2>/dev/null | awk 'NR==1 { print $1 }')
    if [ -z "$username" ]; then
        username=$(id -nu "$uid" 2>/dev/null)
    fi

    if [ -n "$username" ]; then
        user_home=$(dscl . -read "/Users/$username" NFSHomeDirectory 2>/dev/null | awk 'NR==1 { print $2 }')
    fi

    if [ -z "$user_home" ] && [ -n "$username" ]; then
        user_home=$(id -P "$username" 2>/dev/null | awk -F: 'NR==1 { print $9 }')
    fi

    if [ -z "$user_home" ] && [ -n "$username" ]; then
        user_home=$(eval "printf '%s' ~$username" 2>/dev/null)
        if [ "$user_home" = "~$username" ]; then
            user_home=""
        fi
    fi

    printf '%s\n' "$user_home"
}

bootout_label_for_users()
{
    local label="$1"
    local plist="$2"
    local uid

    for uid in $(logged_in_uids); do
        log "Stopping ${label} for uid ${uid}"
        launchctl bootout "gui/${uid}" "$plist" >/dev/null 2>&1 \
            || launchctl kill TERM "gui/${uid}/${label}" >/dev/null 2>&1 \
            || true
    done
}

log "Executing preinstall for services..."

if [ -f "$HELPER_PLIST" ]; then
    bootout_label_for_users "$HELPER_LABEL" "$HELPER_PLIST"
fi

if [ -f "$ORIENTATION_PLIST" ]; then
    bootout_label_for_users "$ORIENTATION_LABEL" "$ORIENTATION_PLIST"
fi

rm -f "$INSTALL_DIR/m1-orientationmanager"
rm -f "$INSTALL_DIR/m1-system-helper"
rm -rf "$INSTALL_DIR/m1-system-helper.app"
rm -f "$LAUNCH_AGENT_DIR/${ORIENTATION_LABEL}.plist"
rm -f "$LAUNCH_AGENT_DIR/${HELPER_LABEL}.plist"
rm -f "$LEGACY_LAUNCH_AGENT_DIR/${ORIENTATION_LABEL}.plist"
rm -f "$LEGACY_LAUNCH_AGENT_DIR/${HELPER_LABEL}.plist"
rm -f "$LEGACY_APP_DIR/m1-system-helper"
rm -rf "$LEGACY_APP_DIR/m1-system-helper.app"

for uid in $(logged_in_uids); do
    user_home=$(user_home_for_uid "$uid")
    if [ -n "$user_home" ] && [ -d "$user_home/Library/Application Support/Mach1 Spatial System" ]; then
        rm -rf "$user_home/Library/Application Support/Mach1 Spatial System"
        log "Removed legacy user data folder for uid ${uid}"
    fi
done

rmdir "$LEGACY_APP_DIR" >/dev/null 2>&1 || true

log "Mach1 Spatial System: Removed previously installed service payloads and legacy helper paths."
exit 0