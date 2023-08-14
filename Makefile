#! /bin/bash

# MACH1 SPATIAL SYSTEM MakeFile

# Make sure you fill all the variables and paths
include ./Makefile.variables

pull:
	git pull --recurse-submodules

clear:
	rm -rf m1-monitor/build
	rm -rf m1-panner/build
	rm -rf m1-player/build
	rm -rf m1-orientationmanager/build
	rm -rf service/M1-SystemWatcher/build

build:
	cmake m1-monitor -Bm1-monitor/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH)
	cmake m1-panner -Bm1-panner/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH)
	cmake m1-player -Bm1-player/build
	cmake m1-orientationmanager -Bm1-orientationmanager/build
	cmake services/M1-SystemWatcher -Bservices/M1-SystemWatcher/build
	cmake --build m1-monitor/build
	cmake --build m1-panner/build
	cmake --build m1-player/build
	cmake --build m1-orientationmanager/build
	cmake --build services/M1-SystemWatcher/build

setup-dev:
ifeq ($(detected_OS),Darwin)
	cmake m1-monitor -BBm1-monitor/build -G "Xcode" -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -G "Xcode" -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build -G "Xcode"
	cmake m1-orientationmanager -Bm1-orientationmanager/build -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	cmake services/M1-SystemWatcher -Bservices/M1-SystemWatcher/build -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
else ifeq ($(detected_OS),Windows)
	cmake m1-monitor -Bm1-monitor/build -G "Visual Studio 16 2019" -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -G "Visual Studio 16 2019" -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build -G "Visual Studio 16 2019"
	cmake m1-orientationmanager -Bm1-orientationmanager/build -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
	cmake services/M1-SystemWatcher -Bservices/M1-SystemWatcher/build -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
else
	cmake m1-monitor -Bm1-monitor/build -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build
	cmake m1-orientationmanager -Bm1-orientationmanager/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
	cmake services/M1-SystemWatcher -Bservices/M1-SystemWatcher/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
endif
