#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Uninstall 1.x.x Mach1 Spatial System

# Remove installed files from .pkg
echo "Uninstalling Mach1 Spatial System v1..."

sudo rm -rf /Applications/Mach1\ Spatial\ System
rm -rf ~/Library/Application\ Support/Mach1\ Spatial\ System

sudo rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin
sudo rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin

rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Monitor.component
rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner\(1-\>8\).component
rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner\(2-\>8\).component
rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner\(4-\>8\).component
rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner\(6-\>8\).component

rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Monitor.vst
rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner\(1-\>8\).vst
rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner\(2-\>8\).vst
rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner\(4-\>8\).vst
rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner\(6-\>8\).vst

rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3
rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner\(1-\>8\).vst3
rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner\(2-\>8\).vst3
rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner\(4-\>8\).vst3
rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner\(6-\>8\).vst3

# Remove files made by the operating system during runtime
rm -rf ~/Library/Application\ Support/M1-Transcoder
rm -rf ~/Library/Application\ Support/M1-Notifier

echo "Uninstalled Mach1 Spatial System v1"