#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous videoplayer cleanup

# move the old videoplayer
if [ -d "/Applications/Mach1 Spatial System/M1-VideoPlayer.app" ]
then
	mkdir -p "/Applications/Mach1/legacy"
	sudo mv "/Applications/Mach1 Spatial System/M1-VideoPlayer.app" "/Applications/Mach1/legacy/M1-VideoPlayer.app"
	echo "Mach1 Spatial System: Found version 1 installed and migrated legacy M1-VideoPlayer app to new install location." > /Library/Application\ Support/Mach1/m1-spatialsystem.log
fi

exit 0