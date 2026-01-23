#!/bin/bash

# Build and Install script for magic.RIDE plugin
# Builds the plugin and automatically installs to system folders

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo ""
echo "========================================"
echo "  Building magic.RIDE plugin..."
echo "========================================"
echo ""

# Create build directory if needed
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    cmake -B build
fi

# Build the plugin
cmake --build build

# Install to system folders
echo ""
"${SCRIPT_DIR}/install_plugins.sh"

echo ""
echo "Done! You can now use the plugin in your DAW."
echo ""
