#!/bin/bash

# Install script for magic.RIDE plugin
# Automatically copies AU and VST3 plugins to system folders

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/VocalRider_artefacts"

# Plugin paths (Release build)
AU_SOURCE="${BUILD_DIR}/Release/AU/magic.RIDE.component"
VST3_SOURCE="${BUILD_DIR}/Release/VST3/magic.RIDE.vst3"

# System plugin directories
AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components/magic.RIDE.component"
VST3_DEST="${HOME}/Library/Audio/Plug-Ins/VST3/magic.RIDE.vst3"

echo ""
echo "========================================"
echo "  Installing magic.RIDE plugins..."
echo "========================================"
echo ""

# Check if build artifacts exist
if [ ! -d "${AU_SOURCE}" ]; then
    echo "ERROR: AU plugin not found at ${AU_SOURCE}"
    echo "Please build the project first: cmake --build build"
    exit 1
fi

if [ ! -d "${VST3_SOURCE}" ]; then
    echo "ERROR: VST3 plugin not found at ${VST3_SOURCE}"
    echo "Please build the project first: cmake --build build"
    exit 1
fi

# Create plugin directories if they don't exist
mkdir -p "${HOME}/Library/Audio/Plug-Ins/Components"
mkdir -p "${HOME}/Library/Audio/Plug-Ins/VST3"

# Remove old plugins if they exist
if [ -d "${AU_DEST}" ]; then
    echo "Removing old AU plugin..."
    rm -rf "${AU_DEST}"
fi

if [ -d "${VST3_DEST}" ]; then
    echo "Removing old VST3 plugin..."
    rm -rf "${VST3_DEST}"
fi

# Copy new plugins
echo "Installing AU plugin..."
cp -R "${AU_SOURCE}" "${AU_DEST}"
echo "  ✓ AU installed to ~/Library/Audio/Plug-Ins/Components/"

echo "Installing VST3 plugin..."
cp -R "${VST3_SOURCE}" "${VST3_DEST}"
echo "  ✓ VST3 installed to ~/Library/Audio/Plug-Ins/VST3/"

# Reset Audio Unit cache (forces macOS to rescan)
echo ""
echo "Resetting Audio Unit cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "  ✓ Audio Unit cache reset"

echo ""
echo "========================================"
echo "  Installation complete!"
echo "========================================"
echo ""
echo "Plugins are now available in your DAW."
echo "You may need to rescan plugins in your DAW."
echo ""
