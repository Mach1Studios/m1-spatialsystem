#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Previous M1-Transcoder cleanup

# delete the old local app data folder
if [ -d "~/Application Support/Mach1 Spatial System" ]
then 
	rm -rf "~/Application Support/Mach1 Spatial System"
fi

exit 0