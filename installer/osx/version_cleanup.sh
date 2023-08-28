#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous version cleanup

M1SpatialSystemApplicationDir=/Applications/Mach1\ Spatial\ System
M1LocalDataDir=\~/Application\ Support/Mach1\ Spatial\ System

# delete the old app folder
if [ -d "$M1SpatialSystemApplicationDir" ]
then 
	sudo rm -Rf $M1SpatialSystemApplicationDir
fi

# delete the old local app data folder
# if [ -d "$M1LocalDataDir" ]
# then 
# 	sudo rm -Rf $M1LocalDataDir
# fi