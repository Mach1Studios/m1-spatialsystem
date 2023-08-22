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

setup-codeisgning:
ifeq ($(detected_OS),Darwin)
	security find-identity -p basic -v
	xcrun notarytool store-credentials 'notarize-app' --apple-id $(APPLE_ID) --team-id $(APPLE_TEAM_CODE)
endif 

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
dev: clear
ifeq ($(detected_OS),Darwin)
	cmake m1-monitor -Bm1-monitor/build -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
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
	cmake m1-monitor -Bm1-monitor/build -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build
	cmake m1-orientationmanager -Bm1-orientationmanager/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
	cmake services/m1-watcher -Bservices/m1-watcher/build -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
endif

codesign:
ifeq ($(detected_OS),Darwin)
	codesign --deep --force --options=runtime --entitlements m1-player/entitlements.plist --sign $(APPLE_TEAM_CODE) --timestamp m1-player/build/M1-Player_artefacts/M1-Player.app
	#codesign --deep --force --options=runtime --entitlements m1-transcoder/entitlements.plist --sign $(APPLE_TEAM_CODE) --timestamp m1-transcoder/build/M1-Transcoder.app
	#codesign --deep --force --options=runtime --entitlements m1-orientationmanager/entitlements.plist --sign $(APPLE_TEAM_CODE) --timestamp m1-orientationmanager/build/M1-OrientationManager_artefacts/M1-OrientationManager
	#codesign --deep --force --options=runtime --entitlements services/m1-watcher/entitlements.plist --sign $(APPLE_TEAM_CODE) --timestamp services/m1-watcher/build/M1-SystemWatcher_artefacts/M1-SystemWatcher
endif

package:
ifeq ($(detected_OS),Darwin)
	packagesbuild -v installer/osx/Mach1\ Spatial\ System\ Installer.pkgproj
	mkdir -p installer/osx/build/signed
	productsign --sign $(APPLE_CODESIGN_INSTALLER_ID) "installer/osx/build/Mach1 Spatial System Installer.pkg" "installer/osx/build/signed/Mach1 Spatial System Installer.pkg"
	xcrun notarytool submit --wait --keychain-profile 'notarize-app' --apple-id $(APPLE_USERNAME) --password $(ALTOOL_APPPASS) --team-id $(APPLE_TEAM_CODE) "installer/osx/build/signed/Mach1 Spatial System Installer.pkg";
	xcrun stapler staple installer/osx/build/signed/Mach1\ Spatial\ System\ Installer.pkg
endif