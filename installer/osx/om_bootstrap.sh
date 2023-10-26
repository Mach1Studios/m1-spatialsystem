#! /bin/bash

# MACH1 SPATIAL SYSTEM
# postinstall orientationmanager

# TODO: write a test for macos 10.7->10.9 to use load command instead of bootstrap

if [ -f "/Library/LaunchAgents/com.mach1.spatial.watcher.plist" ]
then 
	#launchctl load /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	#echo "Bootstrapping the watcher utility as user $(id -u)"
	#launchctl bootstrap gui/$(id -u) /Library/LaunchAgents/com.mach1.spatial.watcher.plist

    # Run postinstall actions for root.
    echo "Executing postinstall..."
    # Add commands to execute in system context here.

    # Run postinstall actions for all logged in users.
    for pid_uid in $(ps -axo pid,uid,args | grep -i "com.mach1.spatial.watcher" | awk '{print $1 "," $2}'); do
        pid=$(echo $pid_uid | cut -d, -f1)
        uid=$(echo $pid_uid | cut -d, -f2)
        # Replace echo with e.g. launchctl load.
        echo "Executing postinstall for $uid"
        echo "launchctl bsexec "$pid" chroot -u "$uid""
        launchctl bsexec "$pid" chroot -u "$uid"
    done

fi