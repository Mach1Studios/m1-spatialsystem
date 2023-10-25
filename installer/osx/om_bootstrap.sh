#! /bin/bash

# MACH1 SPATIAL SYSTEM
# postinstall orientationmanager

# TODO: write a test for macos 10.7->10.9 to use load command instead of bootstrap

#launchctl load /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
launchctl bootstrap gui/$UID /Library/LaunchAgents/com.mach1.spatial.watcher.plist
