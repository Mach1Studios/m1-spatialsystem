#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous videoplayer cleanup

M1SpatialSystemApplicationDir=/Applications/Mach1\ Spatial\ System
M1LegacyVideoPlayer=$M1SpatialSystemApplicationDir/M1-VideoPlayer.app

# move the old videoplayer
if [ -d "$M1LegacyVideoPlayer" ]
then
	mkdir -p /Applications/Mach1/legacy
	sudo mv $M1LegacyVideoPlayer /Applications/Mach1/legacy/M1-VideoPlayer.app
fi
