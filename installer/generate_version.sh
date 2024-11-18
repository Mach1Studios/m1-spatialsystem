#!/bin/bash

# Generate version with git hash for a specific directory
generate_version() {
    local dir=$1
    local base_version=$2
    
    if [ -d "$dir/.git" ]; then
        # Get the short hash
        local hash=$(cd $dir && git rev-parse --short HEAD)
        local date=$(cd $dir && git show -s --date=format:'%Y%m%d' --format=%cd)
        # Split base version into major.minor
        local major=$(echo $base_version | cut -d. -f1)
        local minor=$(echo $base_version | cut -d. -f2)
        # Generate new version without newlines
        echo -n "$major.$minor.$date"
    elif [ -d "./.git/modules/$dir" ]; then
        # Get the short hash
        local hash=$(cd $dir && git rev-parse --short HEAD)
        local date=$(cd $dir && git show -s --date=format:'%Y%m%d' --format=%cd)
        # Split base version into major.minor
        local major=$(echo $base_version | cut -d. -f1)
        local minor=$(echo $base_version | cut -d. -f2)
        # Generate new version without newlines
        echo -n "$major.$minor.$date"
    else
        echo -n "$base_version"
    fi
}

# Update version files for all components
update_versions() {
    # Update m1-panner version
    generate_version "m1-panner" $(cat ./m1-panner/VERSION) > ./m1-panner/VERSION
    
    # Update m1-monitor version
    generate_version "m1-monitor" $(cat ./m1-monitor/VERSION) > ./m1-monitor/VERSION

    # Update m1-player version
    generate_version "m1-player" $(cat ./m1-player/VERSION) > ./m1-player/VERSION
    
    # Update m1-orientationmanager version
    generate_version "m1-orientationmanager" $(cat ./m1-orientationmanager/VERSION) > ./m1-orientationmanager/VERSION
    
    # Update m1-system-helper version
    generate_version "." $(cat ./services/m1-system-helper/VERSION) > ./services/m1-system-helper/VERSION
}

# Update installer version files
update_installer_versions() {
    local panner_ver=$(cat ./m1-panner/VERSION)
    local player_ver=$(cat ./m1-player/VERSION)
    local monitor_ver=$(cat ./m1-monitor/VERSION)
    local om_ver=$(cat ./m1-orientationmanager/VERSION)
    local helper_ver=$(cat ./services/m1-system-helper/VERSION)

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
            s/(<key>IDENTIFIER<\/key>\s*<string>com\.mach1\.spatial\.transcoder\.[^<]*<\/string>.*?<key>VERSION<\/key>\s*<string>)[^<]*(<\/string>)/update_pkg_version($&, "'$player_ver'")/egs;
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
        else
            echo "Error: Failed to update package versions or XML structure is incorrect"
            rm "$temp_file"
            return 1
        fi
    fi
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