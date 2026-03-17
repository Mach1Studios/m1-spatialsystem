#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous M1-Transcoder cleanup

LOG_DIR="/Library/Application Support/Mach1"
LOG_FILE="$LOG_DIR/m1-spatialsystem.log"

mkdir -p "$LOG_DIR"

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

console_user=$(stat -f %Su /dev/console 2>/dev/null)
if [ -n "$console_user" ] && [ "$console_user" != "root" ]; then
    user_home=$(resolve_user_home "$console_user")
else
    user_home=""
fi

# delete the old local app data folder
if [ -n "$user_home" ] && [ -d "$user_home/Library/Application Support/Mach1 Spatial System" ]
then 
	rm -rf "$user_home/Library/Application Support/Mach1 Spatial System"
	echo "Mach1 Spatial System: Removed legacy user data folder." > "$LOG_FILE"
fi

exit 0