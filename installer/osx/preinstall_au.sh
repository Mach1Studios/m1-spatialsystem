#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Remove previous AU plugins

rm -r "/Library/Audio/Plug-Ins/Components/M1-Monitor.Component"
rm -r "/Library/Audio/Plug-Ins/Components/M1-Panner.Component"
echo "Mach1 Spatial System: Removed previously installed AU Components." > /Library/Application\ Support/Mach1/m1-spatialsystem.log

exit 0
