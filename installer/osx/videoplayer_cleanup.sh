#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous videoplayer cleanup

# move the old videoplayer
if [ -d "/Applications/Mach1 Spatial System" ]
then
	mkdir -p "/Applications/Mach1/legacy"
	mv "/Applications/Mach1 Spatial System/M1-VideoPlayer.app" "/Applications/Mach1/legacy/M1-VideoPlayer.app"
fi
