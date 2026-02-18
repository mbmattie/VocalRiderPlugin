#!/bin/bash
#
# magic.RIDE Code Signing and Notarization Script
#
# Prerequisites:
# 1. Apple Developer Account ($99/year)
# 2. Developer ID certificates installed in Keychain
# 3. App-specific password for notarization
#
# Usage: ./sign_and_notarize.sh
#
# Environment variables (set these or edit the script):
#   DEVELOPER_ID_APP       - "Developer ID Application: Your Name (TEAMID)"
#   DEVELOPER_ID_INSTALLER - "Developer ID Installer: Your Name (TEAMID)"
#   APPLE_ID               - Your Apple ID email
#   TEAM_ID                - Your 10-character Team ID
#   APP_PASSWORD           - App-specific password (generate at appleid.apple.com)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build/VocalRider_artefacts/Release"

# ============================================
# CONFIGURE THESE VALUES
# ============================================
DEVELOPER_ID_APP="${DEVELOPER_ID_APP:-Developer ID Application: Your Name (TEAMID)}"
DEVELOPER_ID_INSTALLER="${DEVELOPER_ID_INSTALLER:-Developer ID Installer: Your Name (TEAMID)}"
APPLE_ID="${APPLE_ID:-your@email.com}"
TEAM_ID="${TEAM_ID:-XXXXXXXXXX}"
APP_PASSWORD="${APP_PASSWORD:-xxxx-xxxx-xxxx-xxxx}"
# ============================================

PLUGIN_NAME="magic.RIDE"
VERSION="1.0.0"

# Check if Release build exists, if not try Debug
if [ ! -d "${BUILD_DIR}" ]; then
    BUILD_DIR="${PROJECT_DIR}/build/VocalRider_artefacts/Debug"
    if [ ! -d "${BUILD_DIR}" ]; then
        BUILD_DIR="${PROJECT_DIR}/build_asan/VocalRider_artefacts/Debug"
    fi
fi

AU_PATH="${BUILD_DIR}/AU/${PLUGIN_NAME}.component"
VST3_PATH="${BUILD_DIR}/VST3/${PLUGIN_NAME}.vst3"
AAX_PATH="${BUILD_DIR}/AAX/${PLUGIN_NAME}.aaxplugin"

echo "========================================"
echo "  Signing and Notarizing ${PLUGIN_NAME}"
echo "========================================"
echo ""

# Verify plugins exist
if [ ! -d "$AU_PATH" ] || [ ! -d "$VST3_PATH" ]; then
    echo "ERROR: Plugins not found. Build first!"
    exit 1
fi

# Function to sign a plugin bundle
sign_plugin() {
    local plugin_path="$1"
    local plugin_name=$(basename "$plugin_path")
    
    echo "Signing: $plugin_name"
    
    # Sign all nested code first (frameworks, dylibs, etc.)
    find "$plugin_path" -name "*.dylib" -o -name "*.framework" | while read nested; do
        codesign --force --timestamp --options runtime \
            --sign "$DEVELOPER_ID_APP" "$nested" 2>/dev/null || true
    done
    
    # Sign the main bundle
    codesign --force --timestamp --options runtime \
        --sign "$DEVELOPER_ID_APP" \
        "$plugin_path"
    
    # Verify
    codesign --verify --deep --strict "$plugin_path"
    echo "  ✓ Signed and verified"
}

# Sign plugins
echo "Step 1: Signing plugins..."
sign_plugin "$AU_PATH"
sign_plugin "$VST3_PATH"
if [ -d "$AAX_PATH" ]; then
    sign_plugin "$AAX_PATH"
    echo "  Note: AAX also requires PACE/iLok signing for Pro Tools. See docs/AAX_PRO_TOOLS_GUIDE.md"
else
    echo "  AAX plugin not found (skipping)"
fi
echo ""

# Build installer
echo "Step 2: Building signed installer..."
"${SCRIPT_DIR}/build_installer.sh" --sign "$DEVELOPER_ID_INSTALLER"
echo ""

INSTALLER_PKG="${SCRIPT_DIR}/output/${PLUGIN_NAME}-${VERSION}-Installer.pkg"

# Notarize
echo "Step 3: Submitting for notarization..."
echo "This may take several minutes..."

xcrun notarytool submit "$INSTALLER_PKG" \
    --apple-id "$APPLE_ID" \
    --team-id "$TEAM_ID" \
    --password "$APP_PASSWORD" \
    --wait

echo ""

# Staple
echo "Step 4: Stapling notarization ticket..."
xcrun stapler staple "$INSTALLER_PKG"

echo ""
echo "========================================"
echo "  Success!"
echo "========================================"
echo ""
echo "Your signed and notarized installer is ready:"
echo "  $INSTALLER_PKG"
echo ""
echo "Users can now install without any security warnings!"
