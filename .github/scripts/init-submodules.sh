#!/bin/bash
# Initialize submodules for CI/CD builds
# This script initializes only the submodules needed for building,
# skipping examples and demos to speed up checkout and avoid path length issues on Windows.

set -e

echo "========================================"
echo "Initializing Build Submodules"
echo "========================================"
echo ""

# Configure git for long paths (Windows compatibility)
git config --global core.longpaths true 2>/dev/null || true

# Initialize top-level submodules
echo "Initializing top-level submodules..."
git submodule update --init --depth 1 m1-monitor
git submodule update --init --depth 1 m1-panner
git submodule update --init --depth 1 m1-player
git submodule update --init --depth 1 m1-orientationmanager
git submodule update --init --depth 1 m1-transcoder
git submodule update --init --depth 1 services/m1-system-helper 2>/dev/null || true

# Function to initialize nested submodules for a project
init_project_submodules() {
    local project=$1
    echo ""
    echo "Initializing $project submodules..."
    
    cd "$project"
    
    # Core dependencies
    git submodule update --init --depth 1 JUCE 2>/dev/null || true
    git submodule update --init --depth 1 Modules/juce_murka 2>/dev/null || true
    git submodule update --init --depth 1 Modules/juce_libvlc 2>/dev/null || true
    git submodule update --init --depth 1 Modules/m1-sdk 2>/dev/null || true
    git submodule update --init --depth 1 Modules/m1_orientation_client 2>/dev/null || true
    
    # m1-sdk build dependencies (skip examples - they have deep paths)
    if [ -d "Modules/m1-sdk" ]; then
        cd Modules/m1-sdk
        git submodule update --init --depth 1 libmach1spatial/deps/glm 2>/dev/null || true
        git submodule update --init --depth 1 libmach1spatial/deps/nlohmann 2>/dev/null || true
        git submodule update --init --depth 1 libmach1spatial/deps/pugixml 2>/dev/null || true
        git submodule update --init --depth 1 libmach1spatial/deps/yaml 2>/dev/null || true
        git submodule update --init --depth 1 libmach1spatial/deps/acutest 2>/dev/null || true
        # SKIP: examples/* - These have deep paths and aren't needed for builds
        cd ../..
    fi
    
    # juce_murka dependencies
    if [ -d "Modules/juce_murka" ]; then
        cd Modules/juce_murka
        git submodule update --init --depth 1 Murka 2>/dev/null || true
        git submodule update --init --depth 1 fontstash 2>/dev/null || true
        cd ../..
    fi
    
    # juce_libvlc dependencies (m1-player only)
    if [ -d "Modules/juce_libvlc" ]; then
        cd Modules/juce_libvlc
        git submodule update --init --depth 1 vlc 2>/dev/null || true
        cd ../..
    fi
    
    # m1_orientation_client dependencies
    if [ -d "Modules/m1_orientation_client" ]; then
        cd Modules/m1_orientation_client
        git submodule update --init --depth 1 2>/dev/null || true
        cd ../..
    fi
    
    cd ..
}

# Initialize each project's submodules
init_project_submodules "m1-monitor"
init_project_submodules "m1-panner"
init_project_submodules "m1-player"

# m1-orientationmanager has simpler dependencies
echo ""
echo "Initializing m1-orientationmanager submodules..."
cd m1-orientationmanager
git submodule update --init --depth 1 2>/dev/null || true
cd ..

# m1-transcoder (electron app, skip juce_plugin deep submodules)
echo ""
echo "Initializing m1-transcoder submodules..."
cd m1-transcoder
# Only init top level, skip juce_plugin as it has the problematic deep paths
git submodule update --init --depth 1 2>/dev/null || {
    echo "  Note: Some m1-transcoder submodules skipped (not needed for electron build)"
}
cd ..

# services/m1-system-helper
if [ -d "services/m1-system-helper" ]; then
    echo ""
    echo "Initializing m1-system-helper submodules..."
    cd services/m1-system-helper
    git submodule update --init --depth 1 2>/dev/null || true
    cd ../..
fi

echo ""
echo "========================================"
echo "Submodules initialized successfully!"
echo "========================================"
echo ""
echo "Note: Example submodules (Unity, Unreal, FMOD, etc.) were skipped"
echo "      to speed up builds and avoid path length issues on Windows."

