#! /bin/bash

# MACH1 SPATIAL SYSTEM
# postinstall services

LOG_DIR="/Library/Application Support/Mach1"
LOG_FILE="$LOG_DIR/m1-spatialsystem.log"
INSTALL_DIR="/Library/Application Support/Mach1"
LAUNCH_AGENT_DIR="/Library/LaunchAgents"
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

fix_permissions()
{
    [ -f "$HELPER_PLIST" ] && chmod 644 "$HELPER_PLIST"
    [ -f "$ORIENTATION_PLIST" ] && chmod 644 "$ORIENTATION_PLIST"

    if [ -d "$INSTALL_DIR/m1-system-helper.app" ]; then
        chmod -R 755 "$INSTALL_DIR/m1-system-helper.app"
    elif [ -f "$INSTALL_DIR/m1-system-helper" ]; then
        chmod 755 "$INSTALL_DIR/m1-system-helper"
    fi

    [ -f "$INSTALL_DIR/m1-orientationmanager" ] && chmod 755 "$INSTALL_DIR/m1-orientationmanager"
}

bootstrap_label_for_user()
{
    local uid="$1"
    local label="$2"
    local plist="$3"

    [ -f "$plist" ] || return 0

    launchctl bootout "gui/${uid}" "$plist" >/dev/null 2>&1 || true

    if launchctl bootstrap "gui/${uid}" "$plist" >/dev/null 2>&1; then
        log "Bootstrapped ${label} for uid ${uid}"
        return 0
    fi

    if launchctl kickstart -kp "gui/${uid}/${label}" >/dev/null 2>&1; then
        log "Kickstarted existing ${label} for uid ${uid}"
        return 0
    fi

    if launchctl asuser "$uid" launchctl start "$label" >/dev/null 2>&1; then
        log "Started existing ${label} for uid ${uid}"
        return 0
    fi

    log "WARNING: Failed to bootstrap ${label} for uid ${uid}"
    return 1
}

log "Executing postinstall for services..."
fix_permissions

for uid in $(logged_in_uids); do
    bootstrap_label_for_user "$uid" "$HELPER_LABEL" "$HELPER_PLIST" || true
    bootstrap_label_for_user "$uid" "$ORIENTATION_LABEL" "$ORIENTATION_PLIST" || true
done

log "Postinstall completed for services"
exit 0