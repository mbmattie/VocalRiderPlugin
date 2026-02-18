#!/bin/bash

# Install script for magic.RIDE plugin
# Automatically copies AU, VST3, and AAX plugins to system folders

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/VocalRider_artefacts"

# Plugin paths (Release build)
AU_SOURCE="${BUILD_DIR}/Release/AU/magic.RIDE.component"
VST3_SOURCE="${BUILD_DIR}/Release/VST3/magic.RIDE.vst3"
AAX_SOURCE="${BUILD_DIR}/Release/AAX/magic.RIDE.aaxplugin"

# System plugin directories
AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components/magic.RIDE.component"
VST3_USER_DEST="${HOME}/Library/Audio/Plug-Ins/VST3/magic.RIDE.vst3"
VST3_SYSTEM_DEST="/Library/Audio/Plug-Ins/VST3/magic.RIDE.vst3"
AAX_DEST="/Library/Application Support/Avid/Audio/Plug-Ins/magic.RIDE.aaxplugin"

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

if [ -d "${VST3_USER_DEST}" ]; then
    echo "Removing old VST3 plugin (user)..."
    rm -rf "${VST3_USER_DEST}"
fi

# Copy new plugins
echo "Installing AU plugin..."
cp -R "${AU_SOURCE}" "${AU_DEST}"
echo "  ✓ AU installed to ~/Library/Audio/Plug-Ins/Components/"

echo "Installing VST3 plugin (user location)..."
cp -R "${VST3_SOURCE}" "${VST3_USER_DEST}"
echo "  ✓ VST3 installed to ~/Library/Audio/Plug-Ins/VST3/"

# System-level VST3 install (requires sudo, for better DAW compatibility)
echo ""
echo "Installing VST3 plugin (system location for better DAW compatibility)..."
if [ -d "${VST3_SYSTEM_DEST}" ]; then
    sudo rm -rf "${VST3_SYSTEM_DEST}" 2>/dev/null || {
        echo "  ⚠ Could not remove old system VST3 (may need sudo)"
    }
fi
if sudo cp -R "${VST3_SOURCE}" "${VST3_SYSTEM_DEST}" 2>/dev/null; then
    echo "  ✓ VST3 installed to /Library/Audio/Plug-Ins/VST3/"
else
    echo "  ⚠ System VST3 install skipped (run script with sudo for full install)"
    echo "    To manually install: sudo cp -R \"${VST3_SOURCE}\" \"${VST3_SYSTEM_DEST}\""
fi

# AAX install (requires sudo, for Pro Tools)
echo ""
if [ -d "${AAX_SOURCE}" ]; then
    echo "Installing AAX plugin (for Pro Tools)..."
    if sudo mkdir -p "/Library/Application Support/Avid/Audio/Plug-Ins" 2>/dev/null; then
        if [ -d "${AAX_DEST}" ]; then
            sudo rm -rf "${AAX_DEST}" 2>/dev/null || true
        fi
        if sudo cp -R "${AAX_SOURCE}" "${AAX_DEST}" 2>/dev/null; then
            echo "  ✓ AAX installed to /Library/Application Support/Avid/Audio/Plug-Ins/"
        else
            echo "  ⚠ AAX install failed (run script with sudo for full install)"
        fi
    else
        echo "  ⚠ AAX install skipped (run script with sudo for full install)"
        echo "    To manually install: sudo cp -R \"${AAX_SOURCE}\" \"${AAX_DEST}\""
    fi
else
    echo "AAX plugin not found (skipping). Build with AAX format enabled to include it."
fi

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
