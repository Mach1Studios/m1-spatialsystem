#! /bin/bash

# MACH1 SPATIAL SYSTEM
# Uninstall 2.x.x Mach1 Spatial System

# Remove installed files from .pkg
sudo rm -rf /Applications/Mach1
rm -rf /Library/Application\ Support/Mach1
rm -rf /Library/LaunchAgents/com.mach1.spatial.helper.plist
rm -rf /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist

rm -rf ~/Library/Application\ Support/Mach1

sudo rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin
sudo rm -rf /Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin

rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Monitor.component
rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner.component

rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Monitor.vst
rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner.vst

rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3
rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner.vst3

# Remove files made by the operating system during runtime
rm -rf ~/Library/Application\ Support/M1-Transcoder
rm -rf ~/Library/Application\ Support/M1-Notifier
rm -f ~/Library/Application\ Support/M1-Monitor.settings
rm -f ~/Library/Application\ Support/M1-Panner.settings
