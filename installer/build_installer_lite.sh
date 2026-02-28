#!/bin/bash
#
# magic.RIDE Lite Installer Builder
# Creates a macOS PKG installer for the free Lite plugin
#
# Usage: ./build_installer_lite.sh [--sign "Developer ID Installer: Your Name (TEAMID)"]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build/VocalRiderLite_artefacts/Release"
INSTALLER_DIR="${SCRIPT_DIR}"
STAGING_DIR="${INSTALLER_DIR}/staging-lite"
OUTPUT_DIR="${INSTALLER_DIR}/output"

PLUGIN_NAME="magic.RIDE Lite"
VERSION="1.2.0"
IDENTIFIER="com.mbmaudio.magicride-lite"

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

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Release build not found, checking for Debug build..."
    BUILD_DIR="${PROJECT_DIR}/build/VocalRiderLite_artefacts/Debug"
    if [ ! -d "${BUILD_DIR}" ]; then
        BUILD_DIR="${PROJECT_DIR}/build_asan/VocalRiderLite_artefacts/Debug"
    fi
fi

AU_SOURCE="${BUILD_DIR}/AU/${PLUGIN_NAME}.component"
VST3_SOURCE="${BUILD_DIR}/VST3/${PLUGIN_NAME}.vst3"

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

rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/au"
mkdir -p "$STAGING_DIR/vst3"
mkdir -p "$STAGING_DIR/scripts"
mkdir -p "$OUTPUT_DIR"

echo "Copying plugins to staging area..."
cp -R "$AU_SOURCE" "$STAGING_DIR/au/"
cp -R "$VST3_SOURCE" "$STAGING_DIR/vst3/"

cat > "$STAGING_DIR/scripts/postinstall" << 'EOF'
#!/bin/bash
echo "Resetting Audio Unit cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true
touch "/Library/Audio/Plug-Ins/Components/magic.RIDE Lite.component" 2>/dev/null || true
exit 0
EOF
chmod +x "$STAGING_DIR/scripts/postinstall"

echo "Building component packages..."

pkgbuild \
    --root "$STAGING_DIR/au" \
    --install-location "/Library/Audio/Plug-Ins/Components" \
    --scripts "$STAGING_DIR/scripts" \
    --identifier "${IDENTIFIER}.au" \
    --version "$VERSION" \
    "$STAGING_DIR/au-component.pkg"

pkgbuild \
    --root "$STAGING_DIR/vst3" \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    --identifier "${IDENTIFIER}.vst3" \
    --version "$VERSION" \
    "$STAGING_DIR/vst3-component.pkg"

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

mkdir -p "$STAGING_DIR/resources"

LOGO_HTML='<div class="logo"><svg xmlns="http://www.w3.org/2000/svg" width="220" height="88" viewBox="0 0 599.38 239.22"><path d="M143.23,106.36h-18.45l67.18-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V36.97h30.76l66.93,69.39Z" class="logo-fill"/><path d="M239.93,115.71V36.97h77.51c15.91,0,27.64,1.56,35.19,4.68,7.55,3.12,11.32,8.53,11.32,16.24,0,4.92-1.62,8.7-4.86,11.32-3.24,2.63-7.87,4.45-13.9,5.48-6.03,1.03-13.27,1.66-21.71,1.91l.98-.98c6.15,0,11.85.17,17.1.49,5.25.33,9.84,1.07,13.78,2.21,3.94,1.15,7.01,2.91,9.23,5.29,2.21,2.38,3.32,5.66,3.32,9.84,0,5.09-1.81,9.27-5.41,12.55-3.61,3.28-8.74,5.72-15.38,7.32-6.64,1.6-14.48,2.4-23.5,2.4h-83.66ZM270.69,70.43h47.74c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94s-.97-3.22-2.89-4c-1.93-.78-5.07-1.17-9.41-1.17h-47.74v10.33ZM270.69,92.58h52.9c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94,0-1.23-.43-2.21-1.29-2.95s-2.2-1.29-4-1.66c-1.81-.37-4.14-.55-7.01-.55h-52.9v10.33Z" class="logo-fill"/><path d="M477.88,106.36h-18.45l67.17-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V36.97h30.76l66.93,69.39Z" class="logo-fill"/><path d="M37.9,200.3l41.74-62.27h27.05l42.13,62.27h-26.76l-36.1-55.75h14.5l-35.81,55.75h-26.76ZM59.7,190.87v-14.6h66.55v14.6H59.7Z" class="logo-fill"/><path d="M239.22,164.79v-26.76h24.32v31.14c0,5.51-1.02,10.2-3.06,14.06-2.04,3.86-4.88,7.04-8.51,9.54-3.63,2.5-7.82,4.43-12.55,5.79-4.74,1.36-9.81,2.32-15.23,2.87-5.42.55-10.91.83-16.49.83-5.9,0-11.61-.28-17.12-.83-5.51-.55-10.62-1.51-15.32-2.87-4.7-1.36-8.81-3.29-12.31-5.79-3.5-2.5-6.24-5.68-8.22-9.54-1.98-3.86-2.97-8.55-2.97-14.06v-31.14h24.33v26.76c0,5.19,1.27,9.15,3.79,11.87s6.14,4.59,10.85,5.59c4.7,1.01,10.36,1.51,16.98,1.51s12-.5,16.74-1.51c4.73-1,8.38-2.87,10.95-5.59,2.56-2.72,3.84-6.68,3.84-11.87Z" class="logo-fill"/><path d="M347.22,138.03c8.76,0,15.99.84,21.7,2.53,5.71,1.69,10.18,3.99,13.43,6.91s5.53,6.24,6.86,9.97c1.33,3.73,1.99,7.64,1.99,11.72s-.75,8-2.24,11.72c-1.49,3.73-3.94,7.05-7.35,9.97s-7.91,5.22-13.53,6.91c-5.61,1.69-12.57,2.53-20.87,2.53h-70.06v-62.27h70.06ZM301.49,180.84h44.76c3.63,0,6.71-.24,9.24-.73,2.53-.49,4.56-1.22,6.08-2.19,1.52-.97,2.63-2.19,3.31-3.65.68-1.46,1.02-3.16,1.02-5.11s-.34-3.65-1.02-5.11c-.68-1.46-1.78-2.68-3.31-3.65-1.52-.97-3.55-1.7-6.08-2.19-2.53-.49-5.61-.73-9.24-.73h-44.76v23.35Z" class="logo-fill"/><path d="M405.31,138.03h24.32v62.27h-24.32v-62.27Z" class="logo-fill"/><path d="M502.61,202.25c-14.14,0-25.53-1.25-34.15-3.75-8.63-2.5-14.9-6.19-18.83-11.09-3.93-4.9-5.89-10.98-5.89-18.24s1.96-13.35,5.89-18.24c3.92-4.9,10.2-8.59,18.83-11.09,8.63-2.5,20.01-3.75,34.15-3.75s25.53,1.25,34.15,3.75c8.63,2.5,14.9,6.2,18.83,11.09,3.92,4.9,5.89,10.98,5.89,18.24s-1.96,13.35-5.89,18.24c-3.93,4.9-10.2,8.6-18.83,11.09-8.63,2.5-20.01,3.75-34.15,3.75ZM502.61,183.76c6.68,0,12.46-.4,17.32-1.22,4.87-.81,8.63-2.27,11.29-4.38,2.66-2.11,3.99-5.11,3.99-9s-1.33-6.89-3.99-9c-2.66-2.11-6.42-3.57-11.29-4.38-4.86-.81-10.64-1.22-17.32-1.22s-12.54.41-17.56,1.22c-5.03.81-8.95,2.27-11.77,4.38-2.82,2.11-4.23,5.11-4.23,9s1.41,6.89,4.23,9c2.82,2.11,6.75,3.57,11.77,4.38,5.03.81,10.88,1.22,17.56,1.22Z" class="logo-fill"/></svg></div>'
LOGO_CONCLUSION='<div class="logo"><svg xmlns="http://www.w3.org/2000/svg" width="150" height="60" viewBox="0 0 599.38 239.22"><path d="M143.23,106.36h-18.45l67.18-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V36.97h30.76l66.93,69.39Z" class="logo-fill"/><path d="M239.93,115.71V36.97h77.51c15.91,0,27.64,1.56,35.19,4.68,7.55,3.12,11.32,8.53,11.32,16.24,0,4.92-1.62,8.7-4.86,11.32-3.24,2.63-7.87,4.45-13.9,5.48-6.03,1.03-13.27,1.66-21.71,1.91l.98-.98c6.15,0,11.85.17,17.1.49,5.25.33,9.84,1.07,13.78,2.21,3.94,1.15,7.01,2.91,9.23,5.29,2.21,2.38,3.32,5.66,3.32,9.84,0,5.09-1.81,9.27-5.41,12.55-3.61,3.28-8.74,5.72-15.38,7.32-6.64,1.6-14.48,2.4-23.5,2.4h-83.66ZM270.69,70.43h47.74c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94s-.97-3.22-2.89-4c-1.93-.78-5.07-1.17-9.41-1.17h-47.74v10.33ZM270.69,92.58h52.9c4.35,0,7.48-.41,9.41-1.23,1.93-.82,2.89-2.13,2.89-3.94,0-1.23-.43-2.21-1.29-2.95s-2.2-1.29-4-1.66c-1.81-.37-4.14-.55-7.01-.55h-52.9v10.33Z" class="logo-fill"/><path d="M477.88,106.36h-18.45l67.17-69.39h30.76v78.74h-30.76v-53.64l9.23,3.81-48.6,49.83h-36.91l-48.72-49.7,9.35-3.81v53.52h-30.76V36.97h30.76l66.93,69.39Z" class="logo-fill"/><path d="M37.9,200.3l41.74-62.27h27.05l42.13,62.27h-26.76l-36.1-55.75h14.5l-35.81,55.75h-26.76ZM59.7,190.87v-14.6h66.55v14.6H59.7Z" class="logo-fill"/><path d="M239.22,164.79v-26.76h24.32v31.14c0,5.51-1.02,10.2-3.06,14.06-2.04,3.86-4.88,7.04-8.51,9.54-3.63,2.5-7.82,4.43-12.55,5.79-4.74,1.36-9.81,2.32-15.23,2.87-5.42.55-10.91.83-16.49.83-5.9,0-11.61-.28-17.12-.83-5.51-.55-10.62-1.51-15.32-2.87-4.7-1.36-8.81-3.29-12.31-5.79-3.5-2.5-6.24-5.68-8.22-9.54-1.98-3.86-2.97-8.55-2.97-14.06v-31.14h24.33v26.76c0,5.19,1.27,9.15,3.79,11.87s6.14,4.59,10.85,5.59c4.7,1.01,10.36,1.51,16.98,1.51s12-.5,16.74-1.51c4.73-1,8.38-2.87,10.95-5.59,2.56-2.72,3.84-6.68,3.84-11.87Z" class="logo-fill"/><path d="M347.22,138.03c8.76,0,15.99.84,21.7,2.53,5.71,1.69,10.18,3.99,13.43,6.91s5.53,6.24,6.86,9.97c1.33,3.73,1.99,7.64,1.99,11.72s-.75,8-2.24,11.72c-1.49,3.73-3.94,7.05-7.35,9.97s-7.91,5.22-13.53,6.91c-5.61,1.69-12.57,2.53-20.87,2.53h-70.06v-62.27h70.06ZM301.49,180.84h44.76c3.63,0,6.71-.24,9.24-.73,2.53-.49,4.56-1.22,6.08-2.19,1.52-.97,2.63-2.19,3.31-3.65.68-1.46,1.02-3.16,1.02-5.11s-.34-3.65-1.02-5.11c-.68-1.46-1.78-2.68-3.31-3.65-1.52-.97-3.55-1.7-6.08-2.19-2.53-.49-5.61-.73-9.24-.73h-44.76v23.35Z" class="logo-fill"/><path d="M405.31,138.03h24.32v62.27h-24.32v-62.27Z" class="logo-fill"/><path d="M502.61,202.25c-14.14,0-25.53-1.25-34.15-3.75-8.63-2.5-14.9-6.19-18.83-11.09-3.93-4.9-5.89-10.98-5.89-18.24s1.96-13.35,5.89-18.24c3.92-4.9,10.2-8.59,18.83-11.09,8.63-2.5,20.01-3.75,34.15-3.75s25.53,1.25,34.15,3.75c8.63,2.5,14.9,6.2,18.83,11.09,3.92,4.9,5.89,10.98,5.89,18.24s-1.96,13.35-5.89,18.24c-3.93,4.9-10.2,8.6-18.83,11.09-8.63,2.5-20.01,3.75-34.15,3.75ZM502.61,183.76c6.68,0,12.46-.4,17.32-1.22,4.87-.81,8.63-2.27,11.29-4.38,2.66-2.11,3.99-5.11,3.99-9s-1.33-6.89-3.99-9c-2.66-2.11-6.42-3.57-11.29-4.38-4.86-.81-10.64-1.22-17.32-1.22s-12.54.41-17.56,1.22c-5.03.81-8.95,2.27-11.77,4.38-2.82,2.11-4.23,5.11-4.23,9s1.41,6.89,4.23,9c2.82,2.11,6.75,3.57,11.77,4.38,5.03.81,10.88,1.22,17.56,1.22Z" class="logo-fill"/></svg></div>'

cat > "$STAGING_DIR/resources/welcome.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="color-scheme" content="light dark">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            padding: 20px;
            line-height: 1.6;
            color: #333;
            background-color: #fff;
        }
        .logo { text-align: center; margin-bottom: 16px; }
        .logo-fill { fill: #1E1E2E; }
        h1 { color: #7B5CAD; margin-bottom: 4px; text-align: center; }
        .version { color: #666; font-size: 14px; margin-bottom: 20px; text-align: center; }
        .badge { color: #7B5CAD; font-weight: bold; }
        ul { margin-left: 20px; }
        .note { 
            background: #f5f5f5; 
            padding: 12px; 
            border-radius: 6px; 
            margin-top: 20px;
            font-size: 13px;
        }
        @media (prefers-color-scheme: dark) {
            body { color: #e0e0e0; background-color: #1e1e1e; }
            h1 { color: #b08cde; }
            .version { color: #a0a0a0; }
            .badge { color: #b08cde; }
            .note { background: #2d2d2d; color: #e0e0e0; }
            .logo-fill { fill: #ffffff; }
        }
    </style>
</head>
<body>
    ${LOGO_HTML}
    <h1>magic.RIDE Lite</h1>
    <div class="version">Version ${VERSION} — <span class="badge">Free</span></div>
    
    <p>Welcome! This installer will install the magic.RIDE Lite vocal rider plugin.</p>
    
    <p><strong>What will be installed:</strong></p>
    <ul>
        <li>Audio Unit (AU) plugin</li>
        <li>VST3 plugin</li>
    </ul>
    
    <p>The plugins will be installed to the system Library folder for maximum DAW compatibility. An administrator password may be required.</p>
    
    <div class="note">
        <strong>After installation:</strong> Restart your DAW or rescan plugins to see magic.RIDE Lite.
    </div>
</body>
</html>
EOF

cat > "$STAGING_DIR/resources/conclusion.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="color-scheme" content="light dark">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            padding: 20px;
            line-height: 1.6;
            color: #333;
            background-color: #fff;
        }
        .logo { text-align: center; margin-bottom: 12px; }
        .logo-fill { fill: #1E1E2E; }
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
        .upgrade {
            background: #f3e5f5;
            padding: 12px;
            border-radius: 6px;
            margin-top: 16px;
        }
        .thanks { margin-top: 20px; color: #666; text-align: center; }
        @media (prefers-color-scheme: dark) {
            body { color: #e0e0e0; background-color: #1e1e1e; }
            h1 { color: #66bb6a; }
            .path { background: #2d2d2d; color: #e0e0e0; }
            .tip { background: #1b3d1f; color: #e0e0e0; }
            .upgrade { background: #2d1f3d; color: #e0e0e0; }
            p, .thanks { color: #e0e0e0; }
            .logo-fill { fill: #ffffff; }
        }
    </style>
</head>
<body>
    ${LOGO_CONCLUSION}
    <h1>Installation Complete!</h1>
    
    <p>magic.RIDE Lite has been successfully installed.</p>
    
    <p><strong>Installation locations:</strong></p>
    <div class="path">/Library/Audio/Plug-Ins/Components/magic.RIDE Lite.component</div>
    <div class="path">/Library/Audio/Plug-Ins/VST3/magic.RIDE Lite.vst3</div>
    
    <div class="tip">
        <strong>Next steps:</strong>
        <ol>
            <li>Open your DAW (Logic Pro, Ableton, etc.)</li>
            <li>Rescan plugins if needed</li>
            <li>Find magic.RIDE Lite in your plugin list</li>
        </ol>
    </div>
    
    <div class="upgrade">
        <strong>Want more?</strong> Upgrade to magic.RIDE for the full feature set including Pro Tools AAX support.
    </div>
    
    <p class="thanks">Thank you for installing magic.RIDE Lite!<br/>MBM Audio</p>
</body>
</html>
EOF

echo "Building final installer package..."

FINAL_PKG="${OUTPUT_DIR}/${PLUGIN_NAME}-${VERSION}-Installer.pkg"

productbuild \
    --distribution "$STAGING_DIR/distribution.xml" \
    --resources "$STAGING_DIR/resources" \
    --package-path "$STAGING_DIR" \
    "$FINAL_PKG"

if [ -n "$SIGN_IDENTITY" ]; then
    echo "Signing package with: $SIGN_IDENTITY"
    SIGNED_PKG="${OUTPUT_DIR}/${PLUGIN_NAME}-${VERSION}-Installer-signed.pkg"
    productsign --sign "$SIGN_IDENTITY" "$FINAL_PKG" "$SIGNED_PKG"
    rm "$FINAL_PKG"
    mv "$SIGNED_PKG" "$FINAL_PKG"
    echo "Package signed successfully!"
fi

rm -rf "$STAGING_DIR"

echo ""
echo "========================================"
echo "  Lite Installer built successfully!"
echo "========================================"
echo ""
echo "Output: $FINAL_PKG"
echo "Size: $(du -h "$FINAL_PKG" | cut -f1)"
echo ""

if [ -z "$SIGN_IDENTITY" ]; then
    echo "NOTE: Package is unsigned. To sign and notarize:"
    echo ""
    echo "1. Sign the package:"
    echo "   ./build_installer_lite.sh --sign \"Developer ID Installer: Your Name (TEAMID)\""
    echo ""
    echo "2. Notarize with Apple:"
    echo "   xcrun notarytool submit \"$FINAL_PKG\" --apple-id YOUR_EMAIL --team-id TEAMID --password APP_SPECIFIC_PASSWORD --wait"
    echo ""
    echo "3. Staple the notarization ticket:"
    echo "   xcrun stapler staple \"$FINAL_PKG\""
    echo ""
fi
