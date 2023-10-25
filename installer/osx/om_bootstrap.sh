#! /bin/bash

# MACH1 SPATIAL SYSTEM
# postinstall orientationmanager

# TODO: write a test for macos 10.7->10.9 to use load command instead of bootstrap

if [ -f "/Library/LaunchAgents/com.mach1.spatial.watcher.plist" ]
then 
	#launchctl load /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	echo "Bootstrapping the watcher utility as user $(id -u)"
	launchctl bootstrap gui/$(id -u) /Library/LaunchAgents/com.mach1.spatial.watcher.plist
fi