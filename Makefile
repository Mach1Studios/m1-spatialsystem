#! /bin/bash

# MACH1 SPATIAL SYSTEM MakeFile

# Auto-generate Makefile.variables from example if it doesn't exist
Makefile.variables:
	@if [ ! -f "Makefile.variables" ] && [ -f "Makefile.variables.example" ]; then \
		echo "Makefile.variables not found, creating from example..."; \
		cp Makefile.variables.example Makefile.variables; \
		echo "Created Makefile.variables from example"; \
		echo " Please review and update Makefile.variables with your actual values"; \
	elif [ ! -f "Makefile.variables.example" ]; then \
		echo " Neither Makefile.variables nor Makefile.variables.example found"; \
		exit 1; \
	fi

# Make sure you fill all the needed variables and paths
-include ./Makefile.variables

# getting OS type
ifeq ($(OS),Windows_NT)
	detected_OS := Windows
else
	detected_OS := $(shell uname)
endif

.PHONY: update-versions
update-versions:
ifneq ($(detected_OS),Windows)
	@chmod +x ./installer/generate_version.sh
	@./installer/generate_version.sh
endif

.PHONY: test-aax-monitor test-aax-panner test-aax-plugins test-aax-release
.PHONY: verify-aax-signing diagnose-aax

pull:
	git pull --recurse-submodules

git-submodule-clean:
	git submodule foreach --recursive git clean -x -f -d
	make pull

git-nuke:
	@echo "WARNING: This will completely nuke and re-fetch ALL submodules!"
	@echo "This is intended for users who have old submodule histories after a rewrite."
	@echo "Press Ctrl+C now if you want to cancel..."
	@sleep 3
	@echo " Deinitializing all submodules..."
	git submodule deinit --all --force || true
	@echo "Removing .git/modules to clear cached submodule references..."
	rm -rf .git/modules
	@echo " Removing submodule directories..."
	git submodule foreach --recursive 'rm -rf "$$PWD"' || true
	@echo "Re-initializing and updating all submodules from scratch..."
	git submodule update --init --recursive --force
	@echo "All submodules have been nuked and re-fetched successfully!"
	@echo "TIP: You may want to run 'make clean' and 'make setup' after this."

setup:
ifeq ($(detected_OS),Darwin)
	# Assumes you have installed Homebrew package manager
	brew install yasm cmake p7zip ninja act pre-commit launchcontrol
	npm install -g nodemon
	cd m1-transcoder && ./scripts/setup.sh
	cd m1-panner && pre-commit install
	cd m1-monitor && pre-commit install
else ifeq ($(detected_OS),Windows)
	@choco version >nul || (echo "chocolately is not working or installed" && exit 1)
	@echo "choco is installed and working"
	@choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' --apply-install-arguments-to-dependencies
	@if not exist "$(pip show pre-commit)" (pip install pre-commit)
	npm install -g nodemon
	cd m1-panner && pre-commit install
	cd m1-monitor && pre-commit install
endif

setup-codeisgning:
ifeq ($(detected_OS),Darwin)
	security find-identity -p basic -v
	xcrun notarytool store-credentials 'notarize-app' --apple-id $(APPLE_USERNAME) --team-id $(APPLE_TEAM_CODE) --password $(ALTOOL_APPPASS)
endif

setup-services: clean-services
ifeq ($(detected_OS),Darwin)
	sudo mkdir -p /Library/Application\ Support/Mach1
	sudo cp m1-orientationmanager/Resources/settings.json /Library/Application\ Support/Mach1/settings.json
	sudo cp m1-orientationmanager/Resources/com.mach1.spatial.orientationmanager.plist /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	sudo cp services/m1-system-helper/com.mach1.spatial.helper.plist /Library/LaunchAgents/com.mach1.spatial.helper.plist
endif

clean-services:
ifeq ($(detected_OS),Darwin)
	sudo rm -f /Library/Application\ Support/Mach1/settings.json
	sudo rm -f /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	sudo rm -f /Library/LaunchAgents/com.mach1.spatial.helper.plist
	killall m1-system-helper || true
	killall m1-orientationmanager || true
endif

clean:
ifeq ($(detected_OS),Darwin)
	rm -rf installer/osx/build
endif
ifeq ($(detected_OS),Windows)
	@if exist m1-monitor\\build (rmdir /s /q m1-monitor\\build)
	@if exist m1-panner\\build (rmdir /s /q m1-panner\\build)
	@if exist m1-player\\build (rmdir /s /q m1-player\\build)
	@if exist m1-transcoder\\dist (rmdir /s /q m1-transcoder\\dist)
	@if exist m1-orientationmanager\\build (rmdir /s /q m1-orientationmanager\\build)
	@if exist services\\m1-system-helper\\build (rmdir /s /q services\\m1-system-helper\\build)
else
	rm -rf m1-monitor/build
	rm -rf m1-panner/build
	rm -rf m1-player/build
	rm -rf m1-transcoder/dist
	rm -rf m1-orientationmanager/build
	rm -rf services/m1-system-helper/build
endif

clean-dev:
ifeq ($(detected_OS),Darwin)
	rm -rf installer/osx/build
endif
ifeq ($(detected_OS),Windows)
	@if exist m1-monitor\\build-dev (rmdir /s /q m1-monitor\\build-dev)
	@if exist m1-panner\\build-dev (rmdir /s /q m1-panner\\build-dev)
	@if exist m1-player\\build-dev (rmdir /s /q m1-player\\build-dev)
	@if exist m1-transcoder\\dist (rmdir /s /q m1-transcoder\\dist)
	@if exist m1-orientationmanager\\build-dev (rmdir /s /q m1-orientationmanager\\build-dev)
	@if exist services\\m1-system-helper\\build-dev (rmdir /s /q services\\m1-system-helper\\build-dev)
else
	rm -rf m1-monitor/build-dev
	rm -rf m1-panner/build-dev
	rm -rf m1-player/build-dev
	rm -rf m1-transcoder/dist
	rm -rf m1-orientationmanager/build-dev
	rm -rf services/m1-system-helper/build-dev
endif

clean-installs:
ifeq ($(detected_OS),Darwin)
	sudo rm -rf /Applications/Mach1\ Spatial\ System
	sudo rm -rf /Applications/Mach1
	sudo rm -rf /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist
	sudo rm -rf /Library/LaunchAgents/com.mach1.spatial.helper.plist
	sudo rm -rf /Library/Application\ Support/Mach1
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
else ifeq ($(detected_OS),Windows)
	@echo "WARNING: You must run this command as Administrator to remove system files!"
	@takeown /f "$(ProgramFiles)\Mach1" /r /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Mach1" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramFiles)\Mach1" (rmdir /s /q "$(ProgramFiles)\Mach1")
	@takeown /f "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Monitor.aaxplugin" /r /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Monitor.aaxplugin" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Monitor.aaxplugin" (rmdir /s /q "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Monitor.aaxplugin")
	@takeown /f "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Panner.aaxplugin" /r /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Panner.aaxplugin" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Panner.aaxplugin" (rmdir /s /q "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-Panner.aaxplugin")
	@takeown /f "$(ProgramFiles)\Steinberg\VstPlugins\M1-Monitor.dll" /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Steinberg\VstPlugins\M1-Monitor.dll" /grant %USERNAME%:F >nul 2>&1
	@if exist "$(ProgramFiles)\Steinberg\VstPlugins\M1-Monitor.dll" (del /f /q "$(ProgramFiles)\Steinberg\VstPlugins\M1-Monitor.dll")
	@takeown /f "$(ProgramFiles)\Steinberg\VstPlugins\M1-Panner.dll" /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Steinberg\VstPlugins\M1-Panner.dll" /grant %USERNAME%:F >nul 2>&1
	@if exist "$(ProgramFiles)\Steinberg\VstPlugins\M1-Panner.dll" (del /f /q "$(ProgramFiles)\Steinberg\VstPlugins\M1-Panner.dll")
	@takeown /f "$(ProgramFiles)\Common Files\VST3\M1-Monitor.vst3" /r /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Common Files\VST3\M1-Monitor.vst3" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramFiles)\Common Files\VST3\M1-Monitor.vst3" (rmdir /s /q "$(ProgramFiles)\Common Files\VST3\M1-Monitor.vst3")
	@takeown /f "$(ProgramFiles)\Common Files\VST3\M1-Panner.vst3" /r /d y >nul 2>&1
	@icacls "$(ProgramFiles)\Common Files\VST3\M1-Panner.vst3" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramFiles)\Common Files\VST3\M1-Panner.vst3" (rmdir /s /q "$(ProgramFiles)\Common Files\VST3\M1-Panner.vst3")
	@takeown /f "$(ProgramData)\Mach1" /r /d y >nul 2>&1
	@icacls "$(ProgramData)\Mach1" /grant %USERNAME%:F /t >nul 2>&1
	@if exist "$(ProgramData)\Mach1" (rmdir /s /q "$(ProgramData)\Mach1")
endif

# Add these helper functions at the top of the Makefile
define kill_port
	@lsof -ti:$(1) | xargs kill -9 2>/dev/null || true
endef

# Force target to always run
FORCE:

clean-docs-ports:
ifeq ($(detected_OS),Darwin)
	@lsof -ti:$(docs_port) | xargs kill -9 2>/dev/null || true
else ifeq ($(detected_OS),Windows)
	@for /f "tokens=5" %%a in ('netstat -aon ^| findstr :$(docs_port)') do taskkill /PID %%a /F 2>nul || exit 0
else
	@lsof -ti:$(docs_port) | xargs kill -9 2>/dev/null || true
endif

clean-docs-dist:
ifeq ($(detected_OS),Windows)
	@if exist installer\resources\docs\dist (rmdir /s /q installer\resources\docs\dist)
else
	@echo "Cleaning documentation dist folder..."
	rm -rf installer/resources/docs/dist
endif

# Documentation deployment commands
docs-build: clean-docs-dist FORCE
	@echo "Building documentation..."
ifeq ($(detected_OS),Windows)
	cd installer\resources\docs && node build-docs.js
else
	cd installer/resources/docs && node build-docs.js
endif

docs-deploy: docs-build
	@echo "Deploying documentation to spatialsystem.mach1.tech..."
	aws s3 sync installer/resources/docs/dist/ s3://$(docs_s3_bucket_name)/ \
		--cache-control "public, max-age=3600"
	@echo "Invalidating CloudFront cache..."
	@aws cloudfront create-invalidation \
		--distribution-id $(docs_cloudfront_id) \
		--paths "/*" \
		--output json \
		--query 'Invalidation.{Status:Status,CreateTime:CreateTime,Id:Id}' || (echo "CloudFront invalidation failed" && exit 1)
	@echo "Documentation deployed to spatialsystem.mach1.tech"

docs-stage: docs-build
	@echo "Deploying documentation to staging..."
	aws s3 sync installer/resources/docs/dist/ s3://$(docs_s3_stage_bucket_name)/ \
		--cache-control "no-cache"
	@echo "Invalidating CloudFront cache..."
	@aws cloudfront create-invalidation \
		--distribution-id $(docs_stage_cloudfront_id) \
		--paths "/*" \
		--output json \
		--query 'Invalidation.{Status:Status,CreateTime:CreateTime,Id:Id}' || (echo "CloudFront invalidation failed" && exit 1)
	@echo "Documentation deployed to staging.spatialsystem.mach1.tech"

docs-local: clean-docs-ports docs-build
	@echo "Starting local documentation server..."
	cd installer/resources/docs/dist && python3 -m http.server $(docs_port)
	@echo "Documentation available at http://localhost:$(docs_port)"

docs-verify:
	@echo "🔍 Verifying documentation deployment..."
	@echo "Testing CloudFront direct URL..."
	@curl -I -s https://$(shell aws cloudfront get-distribution --id $(docs_cloudfront_id) --query 'Distribution.DomainName' --output text) | head -1
	@echo "Testing custom domain URL..."
	@curl -I -s https://spatialsystem.mach1.tech | head -1 || echo "Custom domain not accessible"
	@echo "DNS lookup for spatialsystem.mach1.tech:"
	@dig spatialsystem.mach1.tech CNAME +short || nslookup spatialsystem.mach1.tech

# configure for debug and setup dev envs with common IDEs
dev: clean-dev dev-monitor dev-panner dev-player dev-orientationmanager dev-system-helper dev-transcoder

dev-monitor:
ifeq ($(detected_OS),Darwin)
	cmake m1-monitor -Bm1-monitor/build-dev -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
else
	cmake m1-monitor -Bm1-monitor/build-dev -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_STANDALONE=ON
endif

dev-panner:
ifeq ($(detected_OS),Darwin)
	cmake m1-panner -Bm1-panner/build-dev -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
else
	cmake m1-panner -Bm1-panner/build-dev -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_STANDALONE=ON
endif

dev-player:
ifeq ($(detected_OS),Darwin)
	cmake m1-player -Bm1-player/build-dev -G "Xcode"
else
	cmake m1-player -Bm1-player/build-dev
endif

dev-orientationmanager:
ifeq ($(detected_OS),Darwin)
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -G "Xcode" -DENABLE_DEBUG_EMULATOR_DEVICE=ON -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	sudo mkdir -p /Library/Application\ Support/Mach1
	sudo cp m1-orientationmanager/Resources/settings.json /Library/Application\ Support/Mach1/settings.json
else ifeq ($(detected_OS),Windows)
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -DENABLE_DEBUG_EMULATOR_DEVICE=ON -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
else
	cmake m1-orientationmanager -Bm1-orientationmanager/build-dev -DENABLE_DEBUG_EMULATOR_DEVICE=ON -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
endif

dev-oscclient:
ifeq ($(detected_OS),Darwin)
	cmake m1-orientationmanager/osc_client -Bm1-orientationmanager/osc_client/build-dev -G "Xcode"
else
	cmake m1-orientationmanager/osc_client -Bm1-orientationmanager/osc_client/build-dev
endif

dev-system-helper:
ifeq ($(detected_OS),Darwin)
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -G "Xcode" -DCMAKE_INSTALL_PREFIX="/Library/Application Support/Mach1"
	sudo mkdir -p /Library/Application\ Support/Mach1
	sudo cp m1-orientationmanager/Resources/settings.json /Library/Application\ Support/Mach1/settings.json
else ifeq ($(detected_OS),Windows)
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -DCMAKE_INSTALL_PREFIX="\Documents and Settings\All Users\Application Data\Mach1"
else
	cmake services/m1-system-helper -Bservices/m1-system-helper/build-dev -DCMAKE_INSTALL_PREFIX="/opt/Mach1"
endif

dev-transcoder:
ifeq ($(detected_OS),Windows)
	cd m1-transcoder && scripts\setup.sh && npm install
else 
	cd m1-transcoder && ./scripts/setup.sh && npm install
endif

dev-transcoder-plugin:
ifeq ($(detected_OS),Darwin)
	cmake m1-transcoder/juce_plugin -Bm1-transcoder/juce_plugin/build-dev -G "Xcode" -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DBUILD_STANDALONE=ON
else
	cmake m1-transcoder/juce_plugin -Bm1-transcoder/juce_plugin/build-dev -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_STANDALONE=ON
endif

overlay-debug:
	cd m1-panner/Resources/overlay_debug && ./run_simulator.sh --title "Avid Video Engine"

# run configure first
package: update-versions build docs-build codesign notarize test-aax-release installer-pkg

# clean and configure for release
configure: clean update-versions
	cmake m1-monitor -Bm1-monitor/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
	cmake m1-panner -Bm1-panner/build -DBUILD_VST3=ON -DBUILD_AAX=ON -DBUILD_AU=ON -DBUILD_VST=ON -DVST2_PATH=$(VST2_PATH) -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
ifeq ($(detected_OS),Darwin)
	cmake m1-player -Bm1-player/build -G "Xcode"
else
	cmake m1-player -Bm1-player/build
endif
	cmake m1-orientationmanager -Bm1-orientationmanager/build
	cmake services/m1-system-helper -Bservices/m1-system-helper/build
ifeq ($(detected_OS),Darwin)
	cd m1-transcoder && ./scripts/setup.sh && npm install
else ifeq ($(detected_OS),Windows)
	cd m1-transcoder && npm install
endif

# Build targets for individual components
build-monitor:
	cmake --build m1-monitor/build --config "Release"

build-panner:
	cmake --build m1-panner/build --config "Release"

build-player:
	cmake --build m1-player/build --config "Release"

build-orientationmanager:
	cmake --build m1-orientationmanager/build --config "Release"

build-system-helper:
	cmake --build services/m1-system-helper/build --config "Release"

build-transcoder:
ifeq ($(detected_OS),Darwin)
	cd m1-transcoder && npm run package-mac
else ifeq ($(detected_OS),Windows)
	cd m1-transcoder && npm run package-win
endif

# Build configured release
build: build-monitor build-panner build-player build-orientationmanager build-system-helper build-transcoder

codesign:
ifeq ($(detected_OS),Darwin)
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(MONITOR_FREE_GUID)" --signid $(APPLE_CODESIGN_ID) --in m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin --out m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin --autoinstall on
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/AU/M1-Monitor.component
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/VST/M1-Monitor.vst
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-monitor/build/M1-Monitor_artefacts/VST3/M1-Monitor.vst3
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(PANNER_FREE_GUID)" --signid $(APPLE_CODESIGN_ID) --in m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin --out m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin --autoinstall on
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/AU/M1-Panner.component
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/VST/M1-Panner.vst
	codesign --force --sign $(APPLE_CODESIGN_CODE) --timestamp m1-panner/build/M1-Panner_artefacts/VST3/M1-Panner.vst3
	codesign -v --force -o runtime --entitlements m1-player/Resources/M1-Player.entitlements --sign $(APPLE_CODESIGN_CODE) --timestamp m1-player/build/M1-Player_artefacts/Release/M1-Player.app
	codesign -v --force -o runtime --entitlements m1-orientationmanager/Resources/entitlements.mac.plist --sign $(APPLE_CODESIGN_CODE) --timestamp m1-orientationmanager/build/m1-orientationmanager_artefacts/m1-orientationmanager
	codesign -v --force -o runtime --entitlements services/m1-system-helper/entitlements.mac.plist --sign $(APPLE_CODESIGN_CODE) --timestamp services/m1-system-helper/build/m1-system-helper_artefacts/m1-system-helper
else ifeq ($(detected_OS),Windows)
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(MONITOR_FREE_GUID)" --signid $(WIN_SIGNTOOL_ID) --in m1-monitor/build/M1-Monitor_artefacts/Release/AAX/M1-Monitor.aaxplugin --out m1-monitor/build/M1-Monitor_artefacts/Release/AAX/M1-Monitor.aaxplugin --autoinstall on
	$(WRAPTOOL) sign --verbose --account $(PACE_ACCOUNT) --wcguid "$(PANNER_FREE_GUID)" --signid $(WIN_SIGNTOOL_ID) --in m1-panner/build/M1-Panner_artefacts/Release/AAX/M1-Panner.aaxplugin --out m1-panner/build/M1-Panner_artefacts/Release/AAX/M1-Panner.aaxplugin --autoinstall on
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

# Plugin validation with pluginval
download-pluginval:
ifeq ($(detected_OS),Darwin)
	@if [ ! -f "./pluginval.app/Contents/MacOS/pluginval" ]; then \
		echo "Downloading pluginval for macOS..."; \
		curl -LO "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip"; \
		unzip -o pluginval_macOS.zip; \
		rm pluginval_macOS.zip; \
		echo "pluginval downloaded and extracted"; \
	else \
		echo "pluginval already exists"; \
	fi
else ifeq ($(detected_OS),Windows)
	@if not exist "pluginval.exe" ( \
		echo "Downloading pluginval for Windows..." && \
		curl -LO "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" && \
		7z x -aoa pluginval_Windows.zip && \
		del pluginval_Windows.zip && \
		echo "pluginval downloaded and extracted" \
	) else ( \
		echo "pluginval already exists" \
	)
endif

# Helper function to check AAX validator is configured
check-aax-validator:
	@if [ -z "$(AAX_VALIDATOR_PATH)" ]; then \
		echo "ERROR: AAX_VALIDATOR_PATH not set in Makefile.variables"; \
		echo "Please set AAX_VALIDATOR_PATH to the path of the dsh tool."; \
		echo "Example: /path/to/aax-validator-dsh-.../CommandLineTools/dsh"; \
		exit 1; \
	elif [ ! -f "$(AAX_VALIDATOR_PATH)" ]; then \
		echo "ERROR: AAX validator not found at: $(AAX_VALIDATOR_PATH)"; \
		echo "Please check your AAX_VALIDATOR_PATH in Makefile.variables"; \
		exit 1; \
	else \
		echo "Using AAX validator at: $(AAX_VALIDATOR_PATH)"; \
	fi

test-monitor: dev-monitor download-pluginval
	@echo "Building M1-Monitor in development mode..."
	cmake --build m1-monitor/build-dev --config "Debug"
	@echo "Testing M1-Monitor plugin..."
ifeq ($(detected_OS),Darwin)
	./pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --repeat 2 --verbose --validate "m1-monitor/build-dev/M1-Monitor_artefacts/Debug/VST3/M1-Monitor.vst3"
else ifeq ($(detected_OS),Windows)
	./pluginval.exe --strictness-level 10 --repeat 2 --verbose --skip-gui-tests --validate "m1-monitor/build-dev/M1-Monitor_artefacts/Debug/VST3/M1-Monitor.vst3"
endif
	@echo "M1-Monitor validation completed"

test-panner: dev-panner download-pluginval
	@echo "Building M1-Panner in development mode..."
	cmake --build m1-panner/build-dev --config "Debug"
	@echo "Testing M1-Panner plugin..."
ifeq ($(detected_OS),Darwin)
	./pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --repeat 2 --verbose --validate "m1-panner/build-dev/M1-Panner_artefacts/Debug/VST3/M1-Panner.vst3"
else ifeq ($(detected_OS),Windows)
	./pluginval.exe --strictness-level 10 --repeat 2 --verbose --skip-gui-tests --validate "m1-panner/build-dev/M1-Panner_artefacts/Debug/VST3/M1-Panner.vst3"
endif
	@echo "M1-Panner validation completed"

test-plugins: test-monitor test-panner
	@echo "All plugin validations completed successfully!"

# AAX-specific validation and diagnostics
verify-aax-signing:
	@echo "=== Verifying AAX Plugin Code Signing ==="
ifeq ($(detected_OS),Darwin)
	@echo "\n--- M1-Monitor AAX Plugin ---"
	@if [ -f "m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin/Contents/MacOS/M1-Monitor" ]; then \
		codesign -dvvv m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin 2>&1 | grep -E "(Authority|TeamIdentifier|Signature|Format)"; \
		echo "Checking PACE wrapping:"; \
		WRAP_OUTPUT=$$($(WRAPTOOL) verify --verbose --in m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin 2>&1); \
		echo "$$WRAP_OUTPUT"; \
		if echo "$$WRAP_OUTPUT" | grep -q "signed, but not wrapped"; then \
			echo ""; \
			echo "CRITICAL WARNING: Plugin is signed but NOT PACE-wrapped!"; \
			echo "   AAX plugins must be wrapped to load in Pro Tools."; \
			echo "   Check your codesign target - WRAPTOOL sign step may have failed."; \
		elif echo "$$WRAP_OUTPUT" | grep -q "verification failed\|error"; then \
			echo "ERROR: PACE verification failed"; \
		else \
			echo "✓ Plugin is properly wrapped"; \
		fi \
	else \
		echo "ERROR: M1-Monitor AAX plugin not found. Run 'make build-monitor' first."; \
		exit 1; \
	fi
	@echo "\n--- M1-Panner AAX Plugin ---"
	@if [ -f "m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin/Contents/MacOS/M1-Panner" ]; then \
		codesign -dvvv m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin 2>&1 | grep -E "(Authority|TeamIdentifier|Signature|Format)"; \
		echo "Checking PACE wrapping:"; \
		WRAP_OUTPUT=$$($(WRAPTOOL) verify --verbose --in m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin 2>&1); \
		echo "$$WRAP_OUTPUT"; \
		if echo "$$WRAP_OUTPUT" | grep -q "signed, but not wrapped"; then \
			echo ""; \
			echo "CRITICAL WARNING: Plugin is signed but NOT PACE-wrapped!"; \
			echo "   AAX plugins must be wrapped to load in Pro Tools."; \
			echo "   Check your codesign target - WRAPTOOL sign step may have failed."; \
		elif echo "$$WRAP_OUTPUT" | grep -q "verification failed\|error"; then \
			echo "ERROR: PACE verification failed"; \
		else \
			echo "✓ Plugin is properly wrapped"; \
		fi \
	else \
		echo "ERROR: M1-Panner AAX plugin not found. Run 'make build-panner' first."; \
		exit 1; \
	fi
else ifeq ($(detected_OS),Windows)
	@echo "\n--- M1-Monitor AAX Plugin ---"
	@if exist "m1-monitor\build\M1-Monitor_artefacts\Release\AAX\M1-Monitor.aaxplugin" ( \
		$(WRAPTOOL) verify --verbose --in m1-monitor\build\M1-Monitor_artefacts\Release\AAX\M1-Monitor.aaxplugin || echo "WARNING: PACE verification failed" \
	) else ( \
		echo "ERROR: M1-Monitor AAX plugin not found. Run 'make build-monitor' first." && exit 1 \
	)
	@echo "\n--- M1-Panner AAX Plugin ---"
	@if exist "m1-panner\build\M1-Panner_artefacts\Release\AAX\M1-Panner.aaxplugin" ( \
		$(WRAPTOOL) verify --verbose --in m1-panner\build\M1-Panner_artefacts\Release\AAX\M1-Panner.aaxplugin || echo "WARNING: PACE verification failed" \
	) else ( \
		echo "ERROR: M1-Panner AAX plugin not found. Run 'make build-panner' first." && exit 1 \
	)
endif
	@echo "\n=== Code Signing Verification Complete ==="

test-aax-monitor: dev-monitor check-aax-validator
	@echo "Building M1-Monitor in development mode..."
	cmake --build m1-monitor/build-dev --config "Debug"
	@echo "Testing M1-Monitor AAX plugin..."
ifeq ($(detected_OS),Darwin)
	@if [ -f "m1-monitor/build-dev/M1-Monitor_artefacts/Debug/AAX/M1-Monitor.aaxplugin/Contents/MacOS/M1-Monitor" ]; then \
		echo "Running PACE AAX Validator (dsh) on M1-Monitor..."; \
		$(AAX_VALIDATOR_PATH) m1-monitor/build-dev/M1-Monitor_artefacts/Debug/AAX/M1-Monitor.aaxplugin || echo "WARNING: AAX validation had issues"; \
	else \
		echo "ERROR: M1-Monitor AAX plugin not found at m1-monitor/build-dev/M1-Monitor_artefacts/Debug/AAX/M1-Monitor.aaxplugin"; \
		exit 1; \
	fi
else ifeq ($(detected_OS),Windows)
	@if exist "m1-monitor\build-dev\M1-Monitor_artefacts\Debug\AAX\M1-Monitor.aaxplugin" ( \
		echo "Running PACE AAX Validator (dsh) on M1-Monitor..." && \
		$(AAX_VALIDATOR_PATH) m1-monitor\build-dev\M1-Monitor_artefacts\Debug\AAX\M1-Monitor.aaxplugin || echo "WARNING: AAX validation had issues" \
	) else ( \
		echo "ERROR: M1-Monitor AAX plugin not found" && exit 1 \
	)
endif
	@echo "M1-Monitor AAX validation completed"

test-aax-panner: dev-panner check-aax-validator
	@echo "Building M1-Panner in development mode..."
	cmake --build m1-panner/build-dev --config "Debug"
	@echo "Testing M1-Panner AAX plugin..."
ifeq ($(detected_OS),Darwin)
	@if [ -f "m1-panner/build-dev/M1-Panner_artefacts/Debug/AAX/M1-Panner.aaxplugin/Contents/MacOS/M1-Panner" ]; then \
		echo "Running PACE AAX Validator (dsh) on M1-Panner..."; \
		$(AAX_VALIDATOR_PATH) m1-panner/build-dev/M1-Panner_artefacts/Debug/AAX/M1-Panner.aaxplugin || echo "WARNING: AAX validation had issues"; \
	else \
		echo "ERROR: M1-Panner AAX plugin not found at m1-panner/build-dev/M1-Panner_artefacts/Debug/AAX/M1-Panner.aaxplugin"; \
		exit 1; \
	fi
else ifeq ($(detected_OS),Windows)
	@if exist "m1-panner\build-dev\M1-Panner_artefacts\Debug\AAX\M1-Panner.aaxplugin" ( \
		echo "Running PACE AAX Validator (dsh) on M1-Panner..." && \
		$(AAX_VALIDATOR_PATH) m1-panner\build-dev\M1-Panner_artefacts\Debug\AAX\M1-Panner.aaxplugin || echo "WARNING: AAX validation had issues" \
	) else ( \
		echo "ERROR: M1-Panner AAX plugin not found" && exit 1 \
	)
endif
	@echo "M1-Panner AAX validation completed"

test-aax-plugins: test-aax-monitor test-aax-panner
	@echo "All AAX plugin validations completed!"

test-aax-release: check-aax-validator verify-aax-signing
	@echo "=== Testing Release AAX Plugins ==="
ifeq ($(detected_OS),Darwin)
	@echo "\n--- Validating M1-Monitor Release AAX ---"; \
	$(AAX_VALIDATOR_PATH) m1-monitor/build/M1-Monitor_artefacts/AAX/M1-Monitor.aaxplugin || echo "WARNING: M1-Monitor AAX validation had issues"; \
	echo "\n--- Validating M1-Panner Release AAX ---"; \
	$(AAX_VALIDATOR_PATH) m1-panner/build/M1-Panner_artefacts/AAX/M1-Panner.aaxplugin || echo "WARNING: M1-Panner AAX validation had issues"
else ifeq ($(detected_OS),Windows)
	@echo "\n--- Validating M1-Monitor Release AAX ---"
	@$(AAX_VALIDATOR_PATH) m1-monitor\build\M1-Monitor_artefacts\Release\AAX\M1-Monitor.aaxplugin || echo "WARNING: M1-Monitor AAX validation had issues"
	@echo "\n--- Validating M1-Panner Release AAX ---"
	@$(AAX_VALIDATOR_PATH) m1-panner\build\M1-Panner_artefacts\Release\AAX\M1-Panner.aaxplugin || echo "WARNING: M1-Panner AAX validation had issues"
endif
	@echo "\n=== Release AAX Plugin Validation Complete ==="

diagnose-aax:
	@echo "=== AAX Plugin Diagnostic Report ==="
ifeq ($(detected_OS),Darwin)
	@echo "\n--- System Information ---"
	@echo "macOS Version: $$(sw_vers -productVersion)"
	@echo "Architecture: $$(uname -m)"
	@echo "Pro Tools Installed: $$(if [ -d '/Applications/Pro Tools.app' ]; then echo 'Yes'; else echo 'No'; fi)"
	@echo "\n--- AAX Plugin Directories ---"
	@echo "Global AAX Plugins:"
	@ls -la "/Library/Application Support/Avid/Audio/Plug-Ins/" 2>/dev/null | grep -E "(M1-Monitor|M1-Panner)" || echo "No Mach1 plugins found"
	@echo "\nUser AAX Plugins:"
	@ls -la "$$HOME/Library/Application Support/Avid/Audio/Plug-Ins/" 2>/dev/null | grep -E "(M1-Monitor|M1-Panner)" || echo "No Mach1 plugins found"
	@echo "\n--- Code Signing Status ---"
	@for plugin in "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin" \
	               "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin" \
	               "$$HOME/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin" \
	               "$$HOME/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"; do \
		if [ -e "$$plugin" ]; then \
			echo "\nChecking: $$plugin"; \
			codesign -dv "$$plugin" 2>&1 | head -5; \
			echo "Valid signature: $$(codesign -v "$$plugin" 2>&1 && echo 'YES' || echo 'NO')"; \
			spctl -a -t install "$$plugin" 2>&1 | head -2; \
		fi \
	done
	@echo "\n--- PACE iLok Status ---"
	@if [ -d "/Applications/iLok License Manager.app" ]; then \
		echo "iLok License Manager: Installed"; \
		ps aux | grep -i ilok | grep -v grep | head -3 || echo "iLok daemon not running"; \
	else \
		echo "iLok License Manager: NOT INSTALLED"; \
	fi
	@echo "\n--- Gatekeeper & Quarantine ---"
	@for plugin in "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Monitor.aaxplugin" \
	               "/Library/Application Support/Avid/Audio/Plug-Ins/M1-Panner.aaxplugin"; do \
		if [ -e "$$plugin" ]; then \
			echo "\nQuarantine attributes for $$plugin:"; \
			xattr -l "$$plugin" 2>/dev/null | grep "com.apple.quarantine" || echo "No quarantine flag"; \
		fi \
	done
	@echo "\n--- Recommendations ---"
	@echo "1. If plugins have quarantine flags, run: sudo xattr -rd com.apple.quarantine '/Library/Application Support/Avid/Audio/Plug-Ins/M1-*.aaxplugin'"
	@echo "2. Ensure iLok License Manager is installed and running"
	@echo "3. Verify PACE signing with: $(WRAPTOOL) verify --in <plugin.aaxplugin>"
	@echo "4. Check Pro Tools logs: ~/Library/Logs/Avid/"
	@echo "5. Try clearing Pro Tools cache: ~/Library/Caches/com.avid.*"
else ifeq ($(detected_OS),Windows)
	@echo "\n--- System Information ---"
	@systeminfo | findstr /C:"OS Name" /C:"OS Version"
	@echo "\n--- AAX Plugin Directories ---"
	@if exist "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\" ( \
		dir "$(ProgramFiles)\Common Files\Avid\Audio\Plug-Ins\M1-*.aaxplugin" /s /b 2>nul || echo "No Mach1 plugins found" \
	) else ( \
		echo "AAX plugin directory does not exist" \
	)
	@echo "\n--- PACE iLok Status ---"
	@if exist "$(ProgramFiles)\iLok License Manager\" ( \
		echo "iLok License Manager: Installed" \
	) else ( \
		echo "iLok License Manager: NOT INSTALLED" \
	)
	@echo "\n--- Recommendations ---"
	@echo "1. Ensure iLok License Manager is installed and running"
	@echo "2. Verify PACE signing with: $(WRAPTOOL) verify --in <plugin.aaxplugin>"
	@echo "3. Check Pro Tools logs in: %%APPDATA%%\Avid\"
	@echo "4. Run Pro Tools as Administrator once to register plugins"
endif
	@echo "\n=== End Diagnostic Report ==="

clean-pluginval:
ifeq ($(detected_OS),Darwin)
	rm -rf pluginval.app
else ifeq ($(detected_OS),Windows)
	del pluginval.exe
endif
	@echo "pluginval cleaned up"

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

deploy-installer:
ifeq ($(detected_OS),Darwin)
	@echo "### DEPLOYING TO S3 ###"
	@if [ ! -f "installer/osx/build/signed/Mach1 Spatial System Installer.pkg" ]; then \
		echo "Installer not found. Please run 'make installer-pkg' first."; \
		exit 1; \
	fi
	@read -p "Version: " version; \
	if [ -z "$$version" ]; then \
		echo "Version cannot be empty"; \
		exit 1; \
	fi; \
	aws s3 cp "installer/osx/build/signed/Mach1 Spatial System Installer.pkg" \
		"s3://mach1-releases/$$version/Mach1 Spatial System Installer.pkg" \
		--profile mach1 \
		--content-disposition "Mach1 Spatial System Installer.pkg"; \
	echo "Installer deployed to s3://mach1-releases/$$version/"; \
	echo "REMINDER: Update the Avid Store Submission per version update!"
else ifeq ($(detected_OS),Windows)
	@echo "### DEPLOYING TO S3 ###"
	@if not exist "installer\win\Output\Mach1 Spatial System Installer.exe" ( \
		echo "Installer not found. Please run 'make installer-pkg' first." && \
		exit 1 \
	)
	@powershell -Command "$$version = Read-Host 'Version'; if ($$version -eq '') { Write-Host 'Version cannot be empty'; exit 1 } else { aws s3 cp 'installer\win\Output\Mach1 Spatial System Installer.exe' \"s3://mach1-releases/$$version/Mach1 Spatial System Installer.exe\" --profile mach1 --content-disposition \"Mach1 Spatial System Installer.exe\"; Write-Host \"Installer deployed to s3://mach1-releases/$$version/\"; Write-Host \"REMINDER: Update the Avid Store Submission per version update!\" }"
else
	@echo "Installer deployment is not supported on this platform"
endif