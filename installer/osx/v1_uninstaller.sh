#!/bin/bash

# MACH1 SPATIAL SYSTEM
# Uninstall 1.x.x Mach1 Spatial System

LOG_DIR="/Library/Application Support/Mach1"
LOG_FILE="$LOG_DIR/m1-spatialsystem.log"

mkdir -p "$LOG_DIR"

log()
{
    echo "$1" >> "$LOG_FILE"
}

resolve_user_home()
{
    local username="$1"
    local user_home=""

    if [ -z "$username" ] || [ "$username" = "root" ]; then
        return 0
    fi

    user_home=$(dscl . -read "/Users/$username" NFSHomeDirectory 2>/dev/null | awk 'NR==1 { print $2 }')

    if [ -z "$user_home" ]; then
        user_home=$(id -P "$username" 2>/dev/null | awk -F: 'NR==1 { print $9 }')
    fi

    if [ -z "$user_home" ]; then
        user_home=$(eval "printf '%s' ~$username" 2>/dev/null)
        if [ "$user_home" = "~$username" ]; then
            user_home=""
        fi
    fi

    printf '%s\n' "$user_home"
}

remove_path()
{
    local target="$1"

    [ -n "$target" ] || return 0
    rm -rf "$target"
}

bundle_version()
{
    local bundle_path="$1"
    local plist_path="$bundle_path/Contents/Info.plist"
    local version=""

    [ -f "$plist_path" ] || return 0

    version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$plist_path" 2>/dev/null)
    if [ -z "$version" ]; then
        version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$plist_path" 2>/dev/null)
    fi

    printf '%s\n' "$version"
}

remove_bundle_if_legacy()
{
    local bundle_path="$1"
    local version=""

    [ -e "$bundle_path" ] || return 0

    version=$(bundle_version "$bundle_path")
    case "$version" in
        1|1.*)
            remove_path "$bundle_path"
            log "Removed legacy v1 bundle at $bundle_path"
            ;;
        "")
            log "Skipped bundle with unknown version at $bundle_path"
            ;;
        *)
            log "Preserved non-v1 bundle at $bundle_path (version $version)"
            ;;
    esac
}

console_user=$(stat -f %Su /dev/console 2>/dev/null)
if [ -n "$console_user" ] && [ "$console_user" != "root" ]; then
    user_home=$(resolve_user_home "$console_user")
else
    user_home=""
fi

log "Uninstalling Mach1 Spatial System v1..."
echo "Uninstalling Mach1 Spatial System v1..."

remove_path "/Applications/Mach1 Spatial System"

SYSTEM_SHARED_BUNDLES=(
    "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin"
    "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"
)

for plugin in "${SYSTEM_SHARED_BUNDLES[@]}"; do
    remove_bundle_if_legacy "$plugin"
done

if [ -n "$user_home" ]; then
    USER_SHARED_BUNDLES=(
        "$user_home/Library/Application Support/Mach1 Spatial System"
        "$user_home/Library/Audio/Plug-Ins/Components/M1-Monitor.component"
        "$user_home/Library/Audio/Plug-Ins/VST/M1-Monitor.vst"
        "$user_home/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3"
    )
    USER_UNIQUE_LEGACY_PATHS=(
        "$user_home/Library/Audio/Plug-Ins/Components/M1-Panner(1->8).component"
        "$user_home/Library/Audio/Plug-Ins/Components/M1-Panner(2->8).component"
        "$user_home/Library/Audio/Plug-Ins/Components/M1-Panner(4->8).component"
        "$user_home/Library/Audio/Plug-Ins/Components/M1-Panner(6->8).component"
        "$user_home/Library/Audio/Plug-Ins/VST/M1-Panner(1->8).vst"
        "$user_home/Library/Audio/Plug-Ins/VST/M1-Panner(2->8).vst"
        "$user_home/Library/Audio/Plug-Ins/VST/M1-Panner(4->8).vst"
        "$user_home/Library/Audio/Plug-Ins/VST/M1-Panner(6->8).vst"
        "$user_home/Library/Audio/Plug-Ins/VST3/M1-Panner(1->8).vst3"
        "$user_home/Library/Audio/Plug-Ins/VST3/M1-Panner(2->8).vst3"
        "$user_home/Library/Audio/Plug-Ins/VST3/M1-Panner(4->8).vst3"
        "$user_home/Library/Audio/Plug-Ins/VST3/M1-Panner(6->8).vst3"
        "$user_home/Library/Application Support/M1-Transcoder"
        "$user_home/Library/Application Support/M1-Notifier"
    )

    for path in "${USER_SHARED_BUNDLES[@]}"; do
        case "$path" in
            *"/Mach1 Spatial System")
                remove_path "$path"
                ;;
            *)
                remove_bundle_if_legacy "$path"
                ;;
        esac
    done

    for path in "${USER_UNIQUE_LEGACY_PATHS[@]}"; do
        remove_path "$path"
    done
fi

log "Uninstalled Mach1 Spatial System v1"
echo "Uninstalled Mach1 Spatial System v1"

exit 0