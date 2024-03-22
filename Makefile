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

setup-codeisgning:
ifeq ($(detected_OS),Darwin)
	security find-identity -p basic -v
	xcrun notarytool store-credentials 'notarize-app' --apple-id $(APPLE_USERNAME) --team-id $(APPLE_TEAM_CODE) --password $(ALTOOL_APPPASS)
endif 

clear:
ifeq ($(detected_OS),Darwin)
	rm -rf installer/osx/build
endif
ifeq ($(detected_OS),Windows)
	-del /F /S /Q m1-monitor\build
	-del /F /S /Q m1-panner\build
	-del /F /S /Q m1-player\build
	-del /F /S /Q m1-transcoder\dist
	-del /F /S /Q m1-orientationmanager\build
	-del /F /S /Q services\m1-system-helper\build
else
	rm -rf m1-monitor/build
	rm -rf m1-panner/build
	rm -rf m1-player/build
	rm -rf m1-transcoder/dist
	rm -rf m1-orientationmanager/build
	rm -rf services/m1-system-helper/build
endif

clear-dev:
ifeq ($(detected_OS),Darwin)
	rm -rf installer/osx/build
endif
ifeq ($(detected_OS),Windows)
	-del /F /S /Q m1-monitor\build-dev
	-del /F /S /Q m1-panner\build-dev
	-del /F /S /Q m1-player\build-dev
	-del /F /S /Q m1-transcoder\dist
	-del /F /S /Q m1-orientationmanager\build-dev
	-del /F /S /Q services\m1-system-helper\build-dev
else
	rm -rf m1-monitor/build-dev
	rm -rf m1-panner/build-dev
	rm -rf m1-player/build-dev
	rm -rf m1-transcoder/dist
	rm -rf m1-orientationmanager/build-dev
	rm -rf services/m1-system-helper/build-dev
endif

clear-installs:
ifeq ($(detected_OS),Darwin)
	sudo rm -rf /Applications/Mach1\ Spatial\ System/M1-Transcoder.app
	sudo rm -rf /Applications/Mach1\ Spatial\ System/M1-VideoPlayer.app
	sudo rm -rf /Applications/Mach1/M1-Transcoder.app
	sudo rm -rf /Applications/Mach1/M1-Player.app
	sudo rm -rf /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	sudo rm -rf /Library/LaunchAgents/com.mach1.spatial.helper.plist
	sudo rm -rf /Library/Application Support/Mach1/m1-orientationmanager
	sudo rm -rf /Library/Application Support/Mach1/m1-system-helper
	# removing global
	sudo rm -rf "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin"
	sudo rm -rf "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"
	sudo rm -rf "/Library/Audio/Plug-Ins/VST/M1-Monitor.vst"
	sudo rm -rf "/Library/Audio/Plug-Ins/VST/M1-Panner.vst"
	sudo rm -rf "/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3"
	sudo rm -rf "/Library/Audio/Plug-Ins/VST3/M1-Panner.vst3"
	sudo rm -rf "/Library/Audio/Plug-Ins/Components/M1-Monitor.component"
	sudo rm -rf "/Library/Audio/Plug-Ins/Components/M1-Panner.component"
	# removing local user
	rm -rf ~/Library/Application\ Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin
	rm -rf ~/Library/Application\ Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin
	rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Monitor.vst
	rm -rf ~/Library/Audio/Plug-Ins/VST/M1-Panner.vst
	rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Monitor.vst3
	rm -rf ~/Library/Audio/Plug-Ins/VST3/M1-Panner.vst3
	rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Monitor.component
	rm -rf ~/Library/Audio/Plug-Ins/Components/M1-Panner.component
endif

# configure for debug and setup dev envs with common IDEs
dev: clear-dev
ifeq ($(detected_OS),Darwin)
	cmake m1-monitor -Bm1-monitor/build-dev -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build-dev -G "Xcode"
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	cmake m1-orientationmanager/osc_client -Bm1-orientationmanager/osc_client/build-dev -G "Xcode"
	cd m1-transcoder && ./scripts/setup.sh && npm install
else ifeq ($(detected_OS),Windows)
	cmake m1-monitor -Bm1-monitor/build-dev -G "Visual Studio 16 2019" -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -G "Visual Studio 16 2019" -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build-dev -G "Visual Studio 16 2019"
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -G "Visual Studio 16 2019" -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
	cmake m1-orientationmanager/osc_client -Bm1-orientationmanager/osc_client/build-dev -G "Visual Studio 16 2019"
	cd m1-transcoder && ./scripts/setup.sh && npm install
else
	cmake m1-monitor -Bm1-monitor/build-dev -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-player -Bm1-player/build-dev
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
	cmake m1-orientationmanager/osc_client -Bm1-orientationmanager/osc_client/build-dev
	cd m1-transcoder && ./scripts/setup.sh && npm install
endif

dev-sdk:
ifeq ($(detected_OS),Darwin)
	rm -rf m1-monitor/build-dev
	rm -rf m1-panner/build-dev
	cmake m1-monitor -Bm1-monitor/build-dev -G "Xcode" -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -G "Xcode" -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
else ifeq ($(detected_OS),Windows)
	-del /F /S /Q m1-monitor\build
	-del /F /S /Q m1-panner\build
	cmake m1-monitor -Bm1-monitor/build-dev -G "Visual Studio 16 2019" -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -G "Visual Studio 16 2019" -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
else
	rm -rf m1-monitor/build-dev
	rm -rf m1-panner/build-dev
	cmake m1-monitor -Bm1-monitor/build-dev -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
	cmake m1-panner -Bm1-panner/build-dev -DEXTERNAL_M1SDK_PATH=$(M1SDK_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
endif

# run configure first
package: build codesign notarize installer-pkg

# clear and configure for release
configure: clear
	cmake m1-monitor -Bm1-monitor/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
	cmake m1-panner -Bm1-panner/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DAAX_PATH=$(AAX_PATH) -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
ifeq ($(detected_OS),Darwin)
	cmake m1-player -Bm1-player/build -G "Xcode"
else
	cmake m1-player -Bm1-player/build
endif
	cmake m1-orientationmanager -Bm1-orientationmanager/build
	cmake services/m1-system-helper -Bservices/m1-system-helper/build
	cd m1-transcoder && ./scripts/setup.sh && npm install

build: 
	cmake --build m1-monitor/build --config "Release"
	cmake --build m1-panner/build --config "Release"
	cmake --build m1-player/build --config "Release"
	cmake --build m1-orientationmanager/build --config "Release"
	cmake --build services/m1-system-helper/build --config "Release"
ifeq ($(detected_OS),Darwin)
	cd m1-transcoder && npm run package-mac
else ifeq ($(detected_OS),Windows)
	cd m1-transcoder && npm run package-win
endif

codesign:
ifeq ($(detected_OS),Darwin)
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin
	$(WRAPTOOL) sign --verbose --account $(PACE_ID) --wcguid "$(M1_GLOBAL_GUID)" --signid $(APPLE_CODESIGN_ID) --in m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin --out m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/AU/M1-Monitor.component
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/VST/M1-Monitor.vst
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/VST3/M1-Monitor.vst3
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin
	$(WRAPTOOL) sign --verbose --account $(PACE_ID) --wcguid "$(M1_GLOBAL_GUID)" --signid $(APPLE_CODESIGN_ID) --in m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin --out m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/AU/M1-Panner.component
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/VST/M1-Panner.vst
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/VST3/M1-Panner.vst3
	codesign -v --force -o runtime --entitlements m1-player/Resources/M1-Player.entitlements --sign $(APPLE_CODESIGN_CODE) --timestamp m1-player/build/M1-Player_artefacts/Release/M1-Player.app
	codesign -v --force -o runtime --entitlements m1-orientationmanager/Resources/entitlements.mac.plist --sign $(APPLE_CODESIGN_CODE) --timestamp m1-orientationmanager/build/m1-orientationmanager_artefacts/m1-orientationmanager
	codesign -v --force -o runtime --entitlements services/m1-system-helper/entitlements.mac.plist --sign $(APPLE_CODESIGN_CODE) --timestamp services/m1-system-helper/build/m1-system-helper_artefacts/m1-system-helper
else ifeq ($(detected_OS),Windows)
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(M1_GLOBAL_GUID)" --signid $(WIN_SIGNTOOL_ID) --in m1-monitor/build/M1-Monitor_artefacts/Release/AAX/M1-Monitor.aaxplugin --out m1-monitor/build/M1-Monitor_artefacts/Release/AAX/M1-Monitor.aaxplugin
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(M1_GLOBAL_GUID)" --signid $(WIN_SIGNTOOL_ID) --in m1-panner/build/M1-Panner_artefacts/Release/AAX/M1-Panner.aaxplugin --out m1-panner/build/M1-Panner_artefacts/Release/AAX/M1-Panner.aaxplugin
endif

# codesigning and notarizing m1-transcoder is done via electron-builder
notarize:
ifeq ($(detected_OS),Darwin)
	# m1-player
	ditto -c -k --keepParent "$(PWD)/m1-player/build/M1-Player_artefacts/Release/M1-Player.app" "$(PWD)/m1-player/build/M1-Player_artefacts/Release/M1-Player.app.zip"
	#cd "m1-player/build/M1-Player_artefacts" && zip -qr M1-Player.app.zip M1-Player.app -x "*.DS_Store"
	./installer/osx/macos_utilities.sh -f M1-Player -e .app -z .zip -p m1-player/build/M1-Player_artefacts/Release -k 'notarize-app' --apple-id $(APPLE_USERNAME) --apple-app-pass $(ALTOOL_APPPASS) -t $(APPLE_TEAM_CODE)
	# m1-orientationmanager
	#ditto -c -k --keepParent "$(PWD)/m1-orientationmanager/build/m1-orientationmanager_artefacts/m1-orientationmanager" "$(PWD)/m1-orientationmanager/build/m1-orientationmanager_artefacts/m1-orientationmanager.zip"
	#cd "m1-orientationmanager/build/m1-orientationmanager_artefacts" && zip -qr m1-orientationmanager.zip m1-orientationmanager -x "*.DS_Store"
	#./installer/osx/macos_utilities.sh -f m1-orientationmanager -z .zip -p m1-orientationmanager/build/m1-orientationmanager_artefacts -k 'notarize-app' --apple-id $(APPLE_USERNAME) --apple-app-pass $(ALTOOL_APPPASS) -t $(APPLE_TEAM_CODE)
	# m1-system-helper
	#ditto -c -k --keepParent "$(PWD)/services/m1-system-helper/build/m1-system-helper/m1-system-helper" "$(PWD)/services/m1-system-helper/build/m1-system-helper_artefacts/m1-system-helper.zip"
	#cd "services/m1-system-helper/build/m1-system-helper" && zip -qr m1-system-helper.zip m1-system-helper -x "*.DS_Store"
	#./installer/osx/macos_utilities.sh -f m1-system-helper -z .zip -p services/m1-system-helper/build/m1-system-helper_artefacts -k 'notarize-app' --apple-id $(APPLE_USERNAME) --apple-app-pass $(ALTOOL_APPPASS) -t $(APPLE_TEAM_CODE)
endif

installer-pkg:
ifeq ($(detected_OS),Darwin)
	packagesbuild -v installer/osx/Mach1\ Spatial\ System\ Installer.pkgproj
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp installer/osx/build/Mach1\ Spatial\ System\ Installer.pkg
	mkdir -p installer/osx/build/signed
	productsign --sign $(APPLE_CODESIGN_INSTALLER_ID) "installer/osx/build/Mach1 Spatial System Installer.pkg" "installer/osx/build/signed/Mach1 Spatial System Installer.pkg"
	xcrun notarytool submit --wait --keychain-profile 'notarize-app' --apple-id $(APPLE_USERNAME) --password $(ALTOOL_APPPASS) --team-id $(APPLE_TEAM_CODE) "installer/osx/build/signed/Mach1 Spatial System Installer.pkg"
	xcrun stapler staple installer/osx/build/signed/Mach1\ Spatial\ System\ Installer.pkg
else ifeq ($(detected_OS),Windows)
	$(WIN_INNO_PATH) "-Ssigntool=$(WIN_SIGNTOOL_PATH) sign -f $(WIN_CODESIGN_CERT_PATH) -p $(WIN_SIGNTOOL_PASS) -t http://timestamp.digicert.com $$f" "${CURDIR}/installer/win/installer.iss"
endif