#!/bin/bash
#
# magic.RIDE Installer Builder
# Creates a macOS PKG installer for the plugin
#
# Usage: ./build_installer.sh [--sign "Developer ID Installer: Your Name (TEAMID)"]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build/VocalRider_artefacts/Release"
INSTALLER_DIR="${SCRIPT_DIR}"
STAGING_DIR="${INSTALLER_DIR}/staging"
OUTPUT_DIR="${INSTALLER_DIR}/output"

# Plugin info
PLUGIN_NAME="magic.RIDE"
VERSION="1.0.0"
IDENTIFIER="com.mbmaudio.magicride"

# Parse arguments
SIGN_IDENTITY=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --sign)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "========================================"
echo "  Building ${PLUGIN_NAME} Installer"
echo "  Version: ${VERSION}"
echo "========================================"
echo ""

# Check if Release build exists, if not try Debug
if [ ! -d "${BUILD_DIR}" ]; then
    echo "Release build not found, checking for Debug build..."
    BUILD_DIR="${PROJECT_DIR}/build/VocalRider_artefacts/Debug"
    if [ ! -d "${BUILD_DIR}" ]; then
        # Try build_asan directory
        BUILD_DIR="${PROJECT_DIR}/build_asan/VocalRider_artefacts/Debug"
    fi
fi

AU_SOURCE="${BUILD_DIR}/AU/${PLUGIN_NAME}.component"
VST3_SOURCE="${BUILD_DIR}/VST3/${PLUGIN_NAME}.vst3"

# Verify source plugins exist
if [ ! -d "$AU_SOURCE" ]; then
    echo "ERROR: AU plugin not found at: $AU_SOURCE"
    echo "Please build the plugin first with: cmake --build build --config Release"
    exit 1
fi

if [ ! -d "$VST3_SOURCE" ]; then
    echo "ERROR: VST3 plugin not found at: $VST3_SOURCE"
    echo "Please build the plugin first with: cmake --build build --config Release"
    exit 1
fi

echo "Found plugins:"
echo "  AU:   $AU_SOURCE"
echo "  VST3: $VST3_SOURCE"
echo ""

# Clean and create staging directories
rm -rf "$STAGING_DIR"
rm -rf "$OUTPUT_DIR"
mkdir -p "$STAGING_DIR/au"
mkdir -p "$STAGING_DIR/vst3"
mkdir -p "$STAGING_DIR/scripts"
mkdir -p "$OUTPUT_DIR"

# Copy plugins to staging
echo "Copying plugins to staging area..."
cp -R "$AU_SOURCE" "$STAGING_DIR/au/"
cp -R "$VST3_SOURCE" "$STAGING_DIR/vst3/"

# Create post-install script (resets AU cache)
cat > "$STAGING_DIR/scripts/postinstall" << 'EOF'
#!/bin/bash
# Post-install script for magic.RIDE
# Resets the Audio Unit cache so DAWs detect the new plugin

echo "Resetting Audio Unit cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true

# Touch the plugin to update modification date
touch "/Library/Audio/Plug-Ins/Components/magic.RIDE.component" 2>/dev/null || true

exit 0
EOF
chmod +x "$STAGING_DIR/scripts/postinstall"

# Build component packages
echo "Building component packages..."

# AU Package (installs to system Library for better DAW compatibility)
pkgbuild \
    --root "$STAGING_DIR/au" \
    --install-location "/Library/Audio/Plug-Ins/Components" \
    --scripts "$STAGING_DIR/scripts" \
    --identifier "${IDENTIFIER}.au" \
    --version "$VERSION" \
    "$STAGING_DIR/au-component.pkg"

# VST3 Package (installs to system Library for better DAW compatibility)
pkgbuild \
    --root "$STAGING_DIR/vst3" \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    --identifier "${IDENTIFIER}.vst3" \
    --version "$VERSION" \
    "$STAGING_DIR/vst3-component.pkg"

# Create distribution XML
cat > "$STAGING_DIR/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${PLUGIN_NAME}</title>
    <organization>${IDENTIFIER}</organization>
    <domains enable_localSystem="true" enable_currentUserHome="false"/>
    <options customize="never" require-scripts="false" rootVolumeOnly="false"/>
    
    <welcome file="welcome.html"/>
    <conclusion file="conclusion.html"/>
    
    <choices-outline>
        <line choice="default">
            <line choice="au"/>
            <line choice="vst3"/>
        </line>
    </choices-outline>
    
    <choice id="default"/>
    <choice id="au" visible="false">
        <pkg-ref id="${IDENTIFIER}.au"/>
    </choice>
    <choice id="vst3" visible="false">
        <pkg-ref id="${IDENTIFIER}.vst3"/>
    </choice>
    
    <pkg-ref id="${IDENTIFIER}.au" version="${VERSION}" onConclusion="none">au-component.pkg</pkg-ref>
    <pkg-ref id="${IDENTIFIER}.vst3" version="${VERSION}" onConclusion="none">vst3-component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Create resources directory
mkdir -p "$STAGING_DIR/resources"

# Create welcome HTML
cat > "$STAGING_DIR/resources/welcome.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            padding: 20px;
            line-height: 1.6;
            color: #333;
        }
        h1 { color: #7B5CAD; margin-bottom: 10px; }
        .version { color: #666; font-size: 14px; margin-bottom: 20px; }
        ul { margin-left: 20px; }
        .note { 
            background: #f5f5f5; 
            padding: 12px; 
            border-radius: 6px; 
            margin-top: 20px;
            font-size: 13px;
        }
    </style>
</head>
<body>
    <h1>magic.RIDE</h1>
    <div class="version">Version ${VERSION}</div>
    
    <p>Welcome! This installer will install the magic.RIDE vocal rider plugin.</p>
    
    <p><strong>What will be installed:</strong></p>
    <ul>
        <li>Audio Unit (AU) plugin</li>
        <li>VST3 plugin</li>
    </ul>
    
    <p>The plugins will be installed to the system Library folder for maximum DAW compatibility. An administrator password may be required.</p>
    
    <div class="note">
        <strong>After installation:</strong> Restart your DAW or rescan plugins to see magic.RIDE.
    </div>
</body>
</html>
EOF

# Create conclusion HTML
cat > "$STAGING_DIR/resources/conclusion.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            padding: 20px;
            line-height: 1.6;
            color: #333;
        }
        h1 { color: #4CAF50; margin-bottom: 10px; }
        .path {
            background: #f5f5f5;
            padding: 8px 12px;
            border-radius: 4px;
            font-family: monospace;
            font-size: 12px;
            margin: 5px 0;
        }
        .tip {
            background: #e8f5e9;
            padding: 12px;
            border-radius: 6px;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <h1>Installation Complete!</h1>
    
    <p>magic.RIDE has been successfully installed.</p>
    
    <p><strong>Installation locations:</strong></p>
    <div class="path">/Library/Audio/Plug-Ins/Components/magic.RIDE.component</div>
    <div class="path">/Library/Audio/Plug-Ins/VST3/magic.RIDE.vst3</div>
    
    <div class="tip">
        <strong>Next steps:</strong>
        <ol>
            <li>Open your DAW (Logic Pro, Ableton, etc.)</li>
            <li>Rescan plugins if needed</li>
            <li>Find magic.RIDE in your plugin list</li>
        </ol>
    </div>
    
    <p style="margin-top: 20px; color: #666;">Thank you for installing magic.RIDE!</p>
</body>
</html>
EOF

# Build the final product package
echo "Building final installer package..."

FINAL_PKG="${OUTPUT_DIR}/${PLUGIN_NAME}-${VERSION}-Installer.pkg"

productbuild \
    --distribution "$STAGING_DIR/distribution.xml" \
    --resources "$STAGING_DIR/resources" \
    --package-path "$STAGING_DIR" \
    "$FINAL_PKG"

# Sign the package if identity provided
if [ -n "$SIGN_IDENTITY" ]; then
    echo "Signing package with: $SIGN_IDENTITY"
    SIGNED_PKG="${OUTPUT_DIR}/${PLUGIN_NAME}-${VERSION}-Installer-signed.pkg"
    productsign --sign "$SIGN_IDENTITY" "$FINAL_PKG" "$SIGNED_PKG"
    rm "$FINAL_PKG"
    mv "$SIGNED_PKG" "$FINAL_PKG"
    echo "Package signed successfully!"
fi

# Clean up staging
rm -rf "$STAGING_DIR"

echo ""
echo "========================================"
echo "  Installer built successfully!"
echo "========================================"
echo ""
echo "Output: $FINAL_PKG"
echo "Size: $(du -h "$FINAL_PKG" | cut -f1)"
echo ""

if [ -z "$SIGN_IDENTITY" ]; then
    echo "NOTE: Package is unsigned. To sign and notarize:"
    echo ""
    echo "1. Sign the package:"
    echo "   ./build_installer.sh --sign \"Developer ID Installer: Your Name (TEAMID)\""
    echo ""
    echo "2. Notarize with Apple:"
    echo "   xcrun notarytool submit \"$FINAL_PKG\" --apple-id YOUR_EMAIL --team-id TEAMID --password APP_SPECIFIC_PASSWORD --wait"
    echo ""
    echo "3. Staple the notarization ticket:"
    echo "   xcrun stapler staple \"$FINAL_PKG\""
    echo ""
fi
