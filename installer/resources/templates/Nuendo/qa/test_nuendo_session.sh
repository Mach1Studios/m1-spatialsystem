#!/bin/bash
#
# Nuendo Template QA Script
# Simple checks for verifying the Mach1 Spatial Template works correctly
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[FAIL]${NC} $1"; }

# Detect OS
case "$(uname -s)" in
    Darwin*) OS="macos" ;;
    Linux*)  OS="linux" ;;
    MINGW*|CYGWIN*|MSYS*) OS="windows" ;;
    *) OS="unknown" ;;
esac

# Check VST3 plugins are installed
check_plugins() {
    echo "Checking VST3 plugin installation..."
    
    local vst3_path
    case "$OS" in
        macos) vst3_path="/Library/Audio/Plug-Ins/VST3" ;;
        linux) vst3_path="/usr/lib/vst3" ;;
        windows) vst3_path="/c/Program Files/Common Files/VST3" ;;
    esac
    
    local ok=true
    
    if [[ -d "$vst3_path/M1-Panner.vst3" ]]; then
        log_info "M1-Panner.vst3 installed"
    else
        log_error "M1-Panner.vst3 not found at $vst3_path"
        ok=false
    fi
    
    if [[ -d "$vst3_path/M1-Monitor.vst3" ]]; then
        log_info "M1-Monitor.vst3 installed"
    else
        log_error "M1-Monitor.vst3 not found at $vst3_path"
        ok=false
    fi
    
    [[ "$ok" == "true" ]]
}

# Check template file
check_template() {
    echo "Checking template file..."
    
    local template="$TEMPLATE_DIR/Mach1 Spatial Template.npr"
    
    if [[ ! -f "$template" ]]; then
        log_error "Template not found: $template"
        return 1
    fi
    
    local size
    size=$(stat -f%z "$template" 2>/dev/null || stat -c%s "$template" 2>/dev/null)
    
    if [[ "$size" -lt 1000 ]]; then
        log_warn "Template file is very small (${size} bytes) - may be corrupt"
    else
        log_info "Template file OK (${size} bytes)"
    fi
}

# Launch Nuendo with template
launch_nuendo() {
    local template="$TEMPLATE_DIR/Mach1 Spatial Template.npr"
    
    echo "Launching Nuendo with template..."
    
    case "$OS" in
        macos)
            # Find Nuendo
            for v in 14 13 12 11; do
                if [[ -d "/Applications/Nuendo ${v}.app" ]]; then
                    log_info "Opening with Nuendo ${v}"
                    open -a "Nuendo ${v}" "$template"
                    return 0
                fi
            done
            log_error "Nuendo not found in /Applications"
            return 1
            ;;
        windows)
            start "" "$template"
            ;;
        *)
            log_error "Unsupported OS for launch"
            return 1
            ;;
    esac
}

# Main
echo ""
echo "=== Nuendo Template QA ==="
echo ""

case "${1:-}" in
    --check)
        check_plugins
        check_template
        ;;
    --launch)
        check_plugins && check_template && launch_nuendo
        ;;
    *)
        echo "Usage: $0 [--check|--launch]"
        echo ""
        echo "  --check   Verify plugins and template are installed"
        echo "  --launch  Open Nuendo with the template for manual testing"
        echo ""
        exit 0
        ;;
esac

echo ""
echo "=== Done ==="
