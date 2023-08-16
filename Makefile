#! /bin/bash

# MACH1 SPATIAL SYSTEM MakeFile

# Make sure you fill all the needed variables and paths
include ./Makefile.variables

# getting OS type
ifeq ($(OS),Windows_NT)
	detected_OS := Windows
else
	detected_OS := $(shell uname)
endif

pull:
	git pull --recurse-submodules

clear:
	rm -rf m1-monitor/build
	rm -rf m1-panner/build
	rm -rf m1-player/build
	rm -rf m1-orientationmanager/build
	rm -rf services/m1-watcher/build

# configure and build in one call
build:
	cmake m1-monitor -Bm1-monitor/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
	cmake --build m1-monitor/build
	cmake m1-panner -Bm1-panner/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
	cmake --build m1-panner/build
	cmake m1-player -Bm1-player/build
	cmake --build m1-player/build
	cmake m1-orientationmanager -Bm1-orientationmanager/build
	cmake --build m1-orientationmanager/build
	cmake services/m1-watcher -Bservices/m1-watcher/build
	cmake --build services/m1-watcher/build

# setup dev envs with common IDEs
setup: clear
ifeq ($(detected_OS),Darwin)
	cmake m1-monitor -Bm1-monitor/build -G "Xcode" -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -G "Xcode" -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build -G "Xcode"
	cmake m1-orientationmanager -Bm1-orientationmanager/build -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	cmake services/m1-watcher -Bservices/m1-watcher/build -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
else ifeq ($(detected_OS),Windows)
	cmake m1-monitor -Bm1-monitor/build -G "Visual Studio 16 2019" -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -G "Visual Studio 16 2019" -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build -G "Visual Studio 16 2019"
	cmake m1-orientationmanager -Bm1-orientationmanager/build -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
	cmake services/m1-watcher -Bservices/m1-watcher/build -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
else
	cmake m1-monitor -Bm1-monitor/build -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build
	cmake m1-orientationmanager -Bm1-orientationmanager/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
	cmake services/m1-watcher -Bservices/m1-watcher/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
endif
