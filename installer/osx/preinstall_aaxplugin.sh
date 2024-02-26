#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Remove previous AAX plugins

rm -r "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin"
rm -r "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"
echo "Mach1 Spatial System: Removed previously installed AAXs." > /Library/Application\ Support/Mach1/m1-spatialsystem.log

exit 0
