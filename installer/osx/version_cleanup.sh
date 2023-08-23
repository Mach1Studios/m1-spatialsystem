#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous version cleanup

M1SpatialSystemApplicationDir=/Applications/Mach1\ Spatial\ System
M1LocalDataDir=~/Application\ Support/Mach1\ Spatial\ System
M1LegacyVideoPlayer=$M1SpatialSystemApplicationDir/M1-VideoPlayer.app

# move the old videoplayer
if [ -d "$M1LegacyVideoPlayer" ]
then
	mkdir -p /Applications/Mach1/legacy
	mv $M1LegacyVideoPlayer /Applications/Mach1/legacy/M1-VideoPlayer.app
fi

# delete the old app folder
if [ -d "$M1SpatialSystemApplicationDir" ]
then 
	rm -Rf $M1SpatialSystemApplicationDir
fi

# delete the old local app data folder
# if [ -d "$M1LocalDataDir" ]
# then 
# 	rm -Rf $M1LocalDataDir
# fi