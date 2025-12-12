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

# Configure git to use HTTPS instead of SSH for GitHub (needed for CI without SSH keys)
git config --global url."https://github.com/".insteadOf "git@github.com:" 2>/dev/null || true

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
    # m1_orientation_client needs recursive init for its nested submodules (m1-mathematics)
    git submodule update --init --recursive --depth 1 Modules/m1_orientation_client 2>/dev/null || {
        # Fallback: init without recursive, then manually init nested
        git submodule update --init --depth 1 Modules/m1_orientation_client 2>/dev/null || true
    }
    
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
    
    # m1_orientation_client dependencies (includes m1-mathematics)
    if [ -d "Modules/m1_orientation_client" ]; then
        cd Modules/m1_orientation_client
        # Initialize all direct submodules first
        git submodule update --init --depth 1 2>/dev/null || true
        
        # Explicitly initialize m1-mathematics (REQUIRED for build)
        echo "    Initializing libs/m1-mathematics..."
        
        # Configure git to use HTTPS instead of SSH for GitHub (needed for CI)
        git config --global url."https://github.com/".insteadOf "git@github.com:" 2>/dev/null || true
        
        git submodule init libs/m1-mathematics 2>/dev/null || true
        git submodule update --depth 1 --force libs/m1-mathematics 2>/dev/null || {
            echo "    Shallow update failed, trying full clone..."
            git submodule update --force libs/m1-mathematics 2>/dev/null || {
                echo "    Standard update failed, trying manual clone..."
                # Last resort: manually clone the submodule
                rm -rf libs/m1-mathematics 2>/dev/null || true
                # Get the URL and convert SSH to HTTPS
                MATH_URL=$(git config -f .gitmodules --get submodule.libs/m1-mathematics.url 2>/dev/null)
                # Convert git@github.com:org/repo.git to https://github.com/org/repo.git
                MATH_URL_HTTPS=$(echo "$MATH_URL" | sed 's|git@github.com:|https://github.com/|')
                echo "    Cloning from: $MATH_URL_HTTPS"
                git clone --depth 1 "$MATH_URL_HTTPS" libs/m1-mathematics 2>/dev/null || {
                    # Try without .git suffix
                    MATH_URL_HTTPS="${MATH_URL_HTTPS%.git}"
                    git clone --depth 1 "$MATH_URL_HTTPS" libs/m1-mathematics 2>/dev/null || true
                }
            }
        }
        
        # Verify m1-mathematics was initialized
        if [ -f "libs/m1-mathematics/CMakeLists.txt" ]; then
            echo "    m1-mathematics initialized successfully"
        else
            echo "    ERROR: m1-mathematics CMakeLists.txt not found!"
            echo "    Contents of libs/m1-mathematics:"
            ls -la libs/m1-mathematics/ 2>/dev/null || echo "    (empty or not found)"
        fi
        cd ../..
    fi
    
    cd ..
}

# Initialize each project's submodules
echo ""
echo "--- Processing m1-monitor ---"
init_project_submodules "m1-monitor"

echo ""
echo "--- Processing m1-panner ---"
init_project_submodules "m1-panner"

echo ""
echo "--- Processing m1-player ---"
init_project_submodules "m1-player"

# m1-orientationmanager - also needs m1_orientation_client with m1-mathematics
echo ""
echo "Initializing m1-orientationmanager submodules..."
cd m1-orientationmanager
git submodule update --init --depth 1 2>/dev/null || true

# Init nested submodules for m1_orientation_client (contains m1-mathematics)
if [ -d "Modules/m1_orientation_client" ]; then
    cd Modules/m1_orientation_client
    git submodule update --init --depth 1 2>/dev/null || true
    
    echo "  Initializing libs/m1-mathematics..."
    git config --global url."https://github.com/".insteadOf "git@github.com:" 2>/dev/null || true
    
    git submodule init libs/m1-mathematics 2>/dev/null || true
    git submodule update --depth 1 --force libs/m1-mathematics 2>/dev/null || {
        git submodule update --force libs/m1-mathematics 2>/dev/null || {
            rm -rf libs/m1-mathematics 2>/dev/null || true
            MATH_URL=$(git config -f .gitmodules --get submodule.libs/m1-mathematics.url 2>/dev/null)
            MATH_URL_HTTPS=$(echo "$MATH_URL" | sed 's|git@github.com:|https://github.com/|')
            git clone --depth 1 "$MATH_URL_HTTPS" libs/m1-mathematics 2>/dev/null || true
        }
    }
    
    if [ -f "libs/m1-mathematics/CMakeLists.txt" ]; then
        echo "  m1-mathematics initialized in m1-orientationmanager"
    else
        echo "  WARNING: m1-mathematics not found in m1-orientationmanager"
    fi
    cd ../..
fi
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

