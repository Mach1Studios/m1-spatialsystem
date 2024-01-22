#! /bin/bash

# MACH1 SPATIAL SYSTEM
# preinstall services

if [ -f "/Library/LaunchAgents/com.mach1.spatial.helper.plist" ]
then 
    echo "Executing preinstall for services..."
    for uid in $(ps -axo uid,args | grep -i "[l]oginwindow.app" | awk '{ print $1 }')
    do
    	echo "Testing user $uid"
    	if launchctl asuser "$uid" launchctl list 'com.mach1.spatial.helper' &> /dev/null; then
    	    echo "Executing preinstall for $uid"
    		launchctl bootout gui/"$uid" '/Library/LaunchAgents/com.mach1.spatial.helper.plist'
           	launchctl stop com.mach1.spatial.orientationmanager
    	fi
    done

	rm -f "/Library/Application Support/Mach1/m1-orientationmanager"
	rm -f "/Library/Application Support/Mach1/m1-system-helper"
	rm "/Library/Application Support/LaunchAgents/com.mach1.spatial.orientationmanager.plist"
	rm "/Library/Application Support/LaunchAgents/com.mach1.spatial.helper.plist"
fi

# TODO: Figure out ideal way to handle deprecating old system (not safe to remove in case the user wants to switch back)
# delete the old local app data folder
# if [ -d "~/Application Support/Mach1 Spatial System" ]
# then 
# 	rm -rf "~/Application Support/Mach1 Spatial System"
# fi

exit 0