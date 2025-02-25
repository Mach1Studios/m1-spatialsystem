#! /bin/sh

# MACH1 SPATIAL SYSTEM
# Uninstall 2.x.x Mach1 Spatial System

# Remove installed files from .pkg
/bin/echo "Uninstalling Mach1 Spatial System v2..."

# Stop any running processes
sudo pkill -f "M1-"

# Remove main application directories
sudo /bin/rm -rf /Applications/Mach1
sudo /bin/rm -rf /Library/Application\ Support/Mach1

# Unload launch agents
LAUNCH_AGENTS=(
    "com.mach1.spatial.helper"
    "com.mach1.spatial.orientationmanager"
)

for agent in "${LAUNCH_AGENTS[@]}"; do
    if [ -f "/Library/LaunchAgents/${agent}.plist" ]; then
        sudo /bin/launchctl unload "/Library/LaunchAgents/${agent}.plist"
    fi
done

# Remove launch agents
sudo /bin/rm -f /Library/LaunchAgents/com.mach1.spatial.*.plist

# Remove Application Support files
/bin/rm -rf ~/Library/Application\ Support/Mach1
/bin/rm -rf ~/Library/Application\ Support/M1-*

# Remove plugins
PLUGIN_PATHS=(
    "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin"
    "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"
    "~/Library/Audio/Plug-Ins/Components/M1-Monitor.component"
    "~/Library/Audio/Plug-Ins/Components/M1-Panner.component"
    "~/Library/Audio/Plug-Ins/VST/M1-Monitor.vst"
    "~/Library/Audio/Plug-Ins/VST/M1-Panner.vst"
    "~/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3"
    "~/Library/Audio/Plug-Ins/VST3/M1-Panner.vst3"
)

for plugin in "${PLUGIN_PATHS[@]}"; do
    sudo /bin/rm -rf "$plugin"
done

# Remove cache and preferences
/bin/rm -rf ~/Library/Caches/com.mach1.*
/bin/rm -rf ~/Library/Preferences/com.mach1.*
/bin/rm -rf ~/Library/Saved\ Application\ State/com.mach1.*

# Remove runtime files
/bin/rm -rf ~/Library/Application\ Support/M1-Transcoder
/bin/rm -rf ~/Library/Application\ Support/M1-Notifier
/bin/rm -f ~/Library/Application\ Support/M1-Monitor.settings
/bin/rm -f ~/Library/Application\ Support/M1-Panner.settings

# Forget all Mach1 packages
IDENTIFIERS=(
    "com.mach1.spatial.plugins.aax.installer"
    "com.mach1.spatial.plugins.au.installer"
    "com.mach1.spatial.orientationmanager.installer"
    "com.mach1.spatial.player.installer"
    "com.mach1.spatial.transcoder.installer"
    "com.mach1.spatial.plugins.vst3.installer"
    "com.mach1.spatial.plugins.vst2.installer"
    "com.mach1.spatial.installer"              # Main installer
    "com.mach1.spatial.helper.installer"       # Helper installer
    "com.mach1.spatial.monitor.installer"      # Monitor installer
    "com.mach1.spatial.panner.installer"       # Panner installer
    "com.mach1.spatial.notifier.installer"     # Notifier installer
)

for identifier in "${IDENTIFIERS[@]}"; do
    sudo /usr/sbin/pkgutil --forget "$identifier"
done

# Reset specific permissions
sudo tccutil reset All com.mach1.spatial.panner
sudo tccutil reset All com.mach1.spatial.monitor
sudo tccutil reset All com.mach1.spatial.player

# Clean up logs
sudo /bin/rm -f /var/log/mach1*.log

/bin/echo "Uninstalled Mach1 Spatial System v2"

exit 0