#!/bin/bash
#
# M1MemoryShare Test Script
# 
# This script compiles and runs the memory share reader tool to analyze
# .mem files created by M1-Panner plugins.
#
# Usage:
#   ./test_memory_share.sh              # Auto-find and analyze all .mem files
#   ./test_memory_share.sh <file.mem>   # Analyze specific file
#   ./test_memory_share.sh --watch      # Continuously monitor memory files
#   ./test_memory_share.sh --help       # Show help
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READER_SRC="$SCRIPT_DIR/read_memory_share.cpp"
READER_BIN="$SCRIPT_DIR/read_memory_share"

# Search directories for .mem files
SEARCH_DIRS=(
    "$HOME/Library/Group Containers/group.com.mach1.spatial.shared/Library/Caches/M1-Panner"
    "$HOME/Library/Caches/M1-Panner"
    "/tmp/M1-Panner"
    "/tmp"
)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}================================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================================${NC}"
}

print_success() {
    echo -e "${GREEN}$1${NC}"
}

print_warning() {
    echo -e "${YELLOW}$1${NC}"
}

print_error() {
    echo -e "${RED}$1${NC}"
}

# Compile the reader tool
compile_reader() {
    if [ ! -f "$READER_SRC" ]; then
        print_error "ERROR: Source file not found: $READER_SRC"
        exit 1
    fi
    
    # Check if binary exists and is newer than source
    if [ -f "$READER_BIN" ] && [ "$READER_BIN" -nt "$READER_SRC" ]; then
        return 0
    fi
    
    echo "Compiling memory share reader..."
    clang++ -std=c++17 -O2 -o "$READER_BIN" "$READER_SRC" 2>&1
    
    if [ $? -ne 0 ]; then
        print_error "ERROR: Compilation failed"
        exit 1
    fi
    
    print_success "Compilation successful"
    echo ""
}

# Find all .mem files
find_mem_files() {
    local files=()
    
    for dir in "${SEARCH_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            while IFS= read -r -d '' file; do
                files+=("$file")
            done < <(find "$dir" -name "M1SpatialSystem*.mem" -print0 2>/dev/null)
        fi
    done
    
    printf '%s\n' "${files[@]}"
}

# Show help
show_help() {
    echo "M1MemoryShare Test Script"
    echo ""
    echo "Usage:"
    echo "  $0                  Auto-find and analyze all .mem files"
    echo "  $0 <file.mem>       Analyze specific memory file"
    echo "  $0 --watch          Continuously monitor memory files (updates every 2s)"
    echo "  $0 --list           List found .mem files"
    echo "  $0 --clean          Remove all .mem files (for fresh testing)"
    echo "  $0 --help           Show this help"
    echo ""
    echo "Search directories:"
    for dir in "${SEARCH_DIRS[@]}"; do
        echo "  - $dir"
    done
}

# List files
list_files() {
    print_header "Searching for .mem files"
    
    local found=0
    for dir in "${SEARCH_DIRS[@]}"; do
        echo "Checking: $dir"
        if [ -d "$dir" ]; then
            local files=$(find "$dir" -name "*.mem" 2>/dev/null)
            if [ -n "$files" ]; then
                echo "$files" | while read f; do
                    local size=$(stat -f%z "$f" 2>/dev/null || stat -c%s "$f" 2>/dev/null || echo "?")
                    local mod=$(stat -f"%Sm" -t "%Y-%m-%d %H:%M:%S" "$f" 2>/dev/null || stat -c"%y" "$f" 2>/dev/null | cut -d. -f1)
                    echo "  Found: $f"
                    echo "         Size: $size bytes, Modified: $mod"
                    found=1
                done
            fi
        else
            echo "  (directory does not exist)"
        fi
    done
    
    if [ $found -eq 0 ]; then
        print_warning "No .mem files found"
        echo ""
        echo "Make sure:"
        echo "  1. M1-Panner VST3 is loaded in your DAW"
        echo "  2. The track is set to stereo (2 in / 2 out)"
        echo "  3. External spatial mixer mode is active"
    fi
}

# Clean files
clean_files() {
    print_header "Cleaning .mem files"
    
    for dir in "${SEARCH_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            local files=$(find "$dir" -name "*.mem" 2>/dev/null)
            if [ -n "$files" ]; then
                echo "Removing files in $dir:"
                echo "$files" | while read f; do
                    rm -v "$f"
                done
            fi
        fi
    done
    
    print_success "Clean complete"
}

# Watch mode
watch_mode() {
    print_header "Watch Mode - Monitoring .mem files (Ctrl+C to stop)"
    echo ""
    
    while true; do
        clear
        echo "$(date)"
        echo ""
        
        local files=$(find_mem_files)
        
        if [ -z "$files" ]; then
            print_warning "No .mem files found"
            echo ""
            echo "Waiting for M1-Panner to create memory file..."
            echo "Make sure plugin is loaded on a stereo track."
        else
            echo "$files" | head -1 | while read f; do
                "$READER_BIN" "$f"
            done
        fi
        
        echo ""
        echo -e "${BLUE}--- Refreshing in 2 seconds (Ctrl+C to stop) ---${NC}"
        sleep 2
    done
}

# Main
main() {
    case "${1:-}" in
        --help|-h)
            show_help
            exit 0
            ;;
        --list|-l)
            list_files
            exit 0
            ;;
        --clean|-c)
            clean_files
            exit 0
            ;;
        --watch|-w)
            compile_reader
            watch_mode
            exit 0
            ;;
        "")
            # Auto mode - find and analyze all files
            compile_reader
            
            local files=$(find_mem_files)
            
            if [ -z "$files" ]; then
                print_header "No .mem files found"
                echo ""
                echo "Searched in:"
                for dir in "${SEARCH_DIRS[@]}"; do
                    echo "  - $dir"
                done
                echo ""
                echo "Make sure:"
                echo "  1. M1-Panner VST3 is loaded in your DAW"
                echo "  2. The track is set to stereo (2 in / 2 out)"
                echo "  3. External spatial mixer mode is active"
                exit 1
            fi
            
            echo "$files" | while read f; do
                "$READER_BIN" "$f"
            done
            ;;
        *)
            # Specific file
            if [ ! -f "$1" ]; then
                print_error "ERROR: File not found: $1"
                exit 1
            fi
            
            compile_reader
            "$READER_BIN" "$1"
            ;;
    esac
}

main "$@"

