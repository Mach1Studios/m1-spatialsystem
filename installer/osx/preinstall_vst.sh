#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Remove previous VST plugins

rm -r "/Library/Audio/Plug-Ins/VST/M1-Monitor.vst"
rm -r "/Library/Audio/Plug-Ins/VST/M1-Panner.vst"
echo "Mach1 Spatial System: Removed previously installed VSTs." > /Library/Application\ Support/Mach1/m1-spatialsystem.log

exit 0