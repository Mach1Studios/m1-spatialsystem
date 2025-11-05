#! /bin/bash

# MACH1 SPATIAL SYSTEM
# postinstall services

# TODO: write a test for macos 10.7->10.9 to use load command instead of bootstrap

# Ensure log directory exists
mkdir -p "/Library/Application Support/Mach1"

if [ -f "/Library/LaunchAgents/com.mach1.spatial.helper.plist" ]
then 
    # Run postinstall actions for all logged in users.
    echo "Executing postinstall for services..." > /Library/Application\ Support/Mach1/m1-spatialsystem.log

    sudo chmod 644 /Library/LaunchAgents/com.mach1.spatial.helper.plist
    sudo chmod 644 /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
    sudo chmod 755 /Library/Application\ Support/Mach1/m1-system-helper
    sudo chmod 755 /Library/Application\ Support/Mach1/m1-orientationmanager

    # Get logged in user UIDs
    for uid in $(ps -axo uid,args | grep -i "[l]oginwindow.app" | awk '{ print $1 }')
    do
        echo "Executing postinstall for $uid" > /Library/Application\ Support/Mach1/m1-spatialsystem.log
        launchctl bootstrap gui/"$uid" /Library/LaunchAgents/com.mach1.spatial.helper.plist
        #launchctl kickstart -kp gui/"$uid"/com.mach1.spatial.helper
        launchctl bootstrap gui/"$uid" /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
    done
    
    echo "Postinstall completed for services" >> /Library/Application\ Support/Mach1/m1-spatialsystem.log
fi

exit 0