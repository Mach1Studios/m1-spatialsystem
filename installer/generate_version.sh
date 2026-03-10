#!/bin/bash

COMMIT_VERSION=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --commit-version)
            COMMIT_VERSION=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Generate version with git hash for a specific directory
generate_version() {
    local dir=$1
    local base_version=$2
    
    if [ -d "$dir/.git" ]; then
        # Get the short hash
        local hash=$(cd $dir && git rev-parse --short HEAD)
        local date=$(cd $dir && git show -s --date=format:'%Y%m%d' --format=%cd)
        # Use the full base version and append date
        echo "$base_version.$date"
    elif [ -d "./.git/modules/$dir" ]; then
        # Get the short hash
        local hash=$(cd $dir && git rev-parse --short HEAD)
        local date=$(cd $dir && git show -s --date=format:'%Y%m%d' --format=%cd)
        # Use the full base version and append date
        echo "$base_version.$date"
    else
        echo "$base_version"
    fi
}

# Update version files for all components
update_versions() {
    # Read the central base version
    local base_version=$(cat ./VERSION)
    
    # Update m1-panner version
    generate_version "m1-panner" "$base_version" > ./m1-panner/VERSION
    
    # Update m1-monitor version
    generate_version "m1-monitor" "$base_version" > ./m1-monitor/VERSION

    # Update m1-player version
    generate_version "m1-player" "$base_version" > ./m1-player/VERSION
    
    # Update m1-orientationmanager version
    generate_version "m1-orientationmanager" "$base_version" > ./m1-orientationmanager/VERSION
    
    # Update m1-system-helper version
    generate_version "." "$base_version" > ./services/m1-system-helper/VERSION

    # Update m1-transcoder version
    local transcoder_version=$(generate_version "m1-transcoder" "$base_version")
    echo "$transcoder_version" > ./m1-transcoder/VERSION
    
    # Sync transcoder version to package.json for electron-builder
    if [ -f "./m1-transcoder/package.json" ]; then
        # Use node to update package.json version while preserving formatting
        node -e "
            const fs = require('fs');
            const pkg = JSON.parse(fs.readFileSync('./m1-transcoder/package.json', 'utf8'));
            pkg.version = '$transcoder_version';
            fs.writeFileSync('./m1-transcoder/package.json', JSON.stringify(pkg, null, 2) + '\n');
        " 2>/dev/null || \
        # Fallback to sed if node is not available
        sed -i.bak 's/"version": "[^"]*"/"version": "'$transcoder_version'"/' ./m1-transcoder/package.json
    fi
}

# Update installer version files
update_installer_versions() {
    local panner_ver=$(cat ./m1-panner/VERSION)
    local player_ver=$(cat ./m1-player/VERSION)
    local monitor_ver=$(cat ./m1-monitor/VERSION)
    local om_ver=$(cat ./m1-orientationmanager/VERSION)
    local helper_ver=$(cat ./services/m1-system-helper/VERSION)
    local transcoder_ver=$(cat ./m1-transcoder/VERSION)

    local major=$(echo $panner_ver | cut -d. -f1)
    local minor=$(echo $panner_ver | cut -d. -f2)
    local base_version=$major.$minor

    # Update Windows installer script
    if [ -f "./installer/win/installer.iss" ]; then
        sed -i.bak "s/AppVersion=.*/AppVersion=$panner_ver/" ./installer/win/installer.iss
        sed -i.bak "s/VersionInfoVersion=.*/VersionInfoVersion=$panner_ver/" ./installer/win/installer.iss
    fi

    # Update macOS installer version strings
    if [ -f "./installer/osx/Mach1 Spatial System Installer.pkgproj" ]; then
        local pkgproj="./installer/osx/Mach1 Spatial System Installer.pkgproj"
        local temp_file="temp_pkgproj.xml"
        
        # Create a copy to work with
        cp "$pkgproj" "$temp_file"
        
        # Update versions based on identifiers
        perl -0777 -i -pe '
            # Helper function to preserve package structure and only update version
            sub update_pkg_version {
                my ($pkg_content, $new_version) = @_;
                $pkg_content =~ s/(<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/$1$new_version$2/;
                return $pkg_content;
            }

            # Find and update complete package blocks based on identifier
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.monitor\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$monitor_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.panner\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$panner_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.player\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$player_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.orientationmanager\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$om_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.helper\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$helper_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.transcoder\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$transcoder_ver'")/egs;
            
            # Update plugin installer packages (AAX, VST2, VST3, AU installers)
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.plugins\.aax\.installer<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$panner_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.plugins\.vst2\.installer<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$panner_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.plugins\.vst3\.installer<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$panner_ver'")/egs;
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.plugins\.au\.installer<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$panner_ver'")/egs;
        ' "$temp_file"

        # Verify the changes look good before moving file
        if [ -s "$temp_file" ] && grep -q "<string>" "$temp_file"; then
            mv "$temp_file" "$pkgproj"
            echo "Updated package versions in $pkgproj"
            echo "base_version: $base_version"
            echo "Monitor: $monitor_ver"
            echo "Panner: $panner_ver"
            echo "Player: $player_ver"
            echo "OrientationManager: $om_ver"
            echo "Helper: $helper_ver"
            echo "Transcoder: $transcoder_ver"
        else
            echo "Error: Failed to update package versions or XML structure is incorrect"
            rm "$temp_file"
            return 1
        fi
    fi
}

commit_and_push_version_in_repo() {
    local repo_path="$1"
    local version_path="$2"
    local label="$3"

    if ! git -C "$repo_path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "$label: not a git work tree, skipping automatic VERSION commit."
        return 0
    fi

    if ! git -C "$repo_path" diff --cached --quiet --ignore-submodules --; then
        echo "$label: staged changes already exist, skipping automatic VERSION commit."
        return 0
    fi

    if git -C "$repo_path" diff --quiet -- "$version_path"; then
        echo "$label: $version_path is unchanged, skipping automatic VERSION commit."
        return 0
    fi

    git -C "$repo_path" add -- "$version_path"

    if git -C "$repo_path" diff --cached --quiet -- "$version_path"; then
        echo "$label: no staged VERSION change detected, skipping automatic VERSION commit."
        return 0
    fi

    git -C "$repo_path" commit -m "version bump"
    git -C "$repo_path" push origin HEAD
}

commit_and_push_versions_if_requested() {
    if [[ "$COMMIT_VERSION" -ne 1 ]]; then
        return 0
    fi

    commit_and_push_version_in_repo "." "VERSION" "root"
    commit_and_push_version_in_repo "m1-panner" "VERSION" "m1-panner"
    commit_and_push_version_in_repo "m1-monitor" "VERSION" "m1-monitor"
    commit_and_push_version_in_repo "m1-player" "VERSION" "m1-player"
    commit_and_push_version_in_repo "m1-orientationmanager" "VERSION" "m1-orientationmanager"
    commit_and_push_version_in_repo "m1-transcoder" "VERSION" "m1-transcoder"
}

# Main execution
echo "Updating component versions..."
update_versions

echo "Updating installer versions..."
update_installer_versions

# Clean up backup files
find . -name "*.bak" -type f -delete

# Print current versions
echo "Current versions:"
echo "m1-panner: $(cat ./m1-panner/VERSION)"
echo "m1-player: $(cat ./m1-player/VERSION)"
echo "m1-monitor: $(cat ./m1-monitor/VERSION)"
echo "m1-orientationmanager: $(cat ./m1-orientationmanager/VERSION)"
echo "m1-system-helper: $(cat ./services/m1-system-helper/VERSION)"
echo "m1-transcoder: $(cat ./m1-transcoder/VERSION)"

commit_and_push_versions_if_requested