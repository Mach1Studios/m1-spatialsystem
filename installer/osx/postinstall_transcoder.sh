#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous M1-Transcoder cleanup

# delete the old local app data folder
if [ -d "~/Application Support/Mach1 Spatial System" ]
then 
	rm -rf "~/Application Support/Mach1 Spatial System"
	echo "Mach1 Spatial System: Removed legacy user data folder." > /Library/Application\ Support/Mach1/m1-spatialsystem.log
fi

exit 0