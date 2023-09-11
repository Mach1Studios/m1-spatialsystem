#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous version cleanup

# delete the old app folder
if [ -d "/Applications/Mach1 Spatial System" ]
then 
	rm -rf "/Applications/Mach1 Spatial System"
fi

# delete the old local app data folder
# if [ -d "~/Application Support/Mach1 Spatial System" ]
# then 
# 	rm -rf "~/Application Support/Mach1 Spatial System"
# fi