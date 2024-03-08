#! /bin/sh

# MACH1 SPATIAL SYSTEM
# Uninstall 2.x.x Mach1 Spatial System

# Remove installed files from .pkg
/bin/echo "Uninstalling Mach1 Spatial System v2..."

sudo /bin/rm -rf /Applications/Mach1
/bin/rm -rf /Library/Application\ Support/Mach1

if [ -f /Library/LaunchAgents/com.mach1.spatial.helper.plist ]; then
	sudo /bin/launchctl unload /Library/LaunchAgents/com.mach1.spatial.helper.plist
fi

if [ -f /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist ]; then
	sudo /bin/launchctl unload /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
fi

## remove launchagents
/bin/rm -f /Library/LaunchAgents/com.mach1.spatial.helper.plist
/bin/rm -f /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist

## Remove Application Support files
/bin/rm -rf ~/Library/Application\ Support/Mach1

## Remove plugins
sudo /bin/rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin
sudo /bin/rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin
/bin/rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Monitor.component
/bin/rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner.component
/bin/rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Monitor.vst
/bin/rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner.vst
/bin/rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3
/bin/rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner.vst3

# Remove files made by the operating system during runtime
/bin/rm -rf ~/Library/Application\ Support/M1-Transcoder
/bin/rm -rf ~/Library/Application\ Support/M1-Notifier
/bin/rm -f ~/Library/Application\ Support/M1-Monitor.settings
/bin/rm -f ~/Library/Application\ Support/M1-Panner.settings

## Forget we ever got installed
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.plugins.aax.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.plugins.au.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.orientationmanager.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.player.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.transcoder.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.plugins.vst3.installer
sudo /usr/sbin/pkgutil --forget com.mach1.spatial.plugins.vst2.installer

/bin/echo "Uninstalled Mach1 Spatial System v2"

exit 0