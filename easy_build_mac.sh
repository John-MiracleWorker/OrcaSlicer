#!/bin/bash

# Easy Build Script for OrcaSlicer on macOS
# This script helps set up the environment and build OrcaSlicer from source.

set -e

echo "============================================="
echo "   OrcaSlicer Mac Installer / Builder"
echo "============================================="

# 1. Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew not found. Please install Homebrew first:"
    echo "  /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

# 2. Install Dependencies
echo "Checking dependencies..."
brew install cmake git ninja autoconf automake libtool zstd gettext

# 3. Build OrcaSlicer
echo "Starting Build Process..."
echo "This may take a while as it builds all dependencies first."

# Determine architecture
ARCH=$(uname -m)
echo "Architecture: $ARCH"

# Run the official build script
# Assuming this script is in the root directory
chmod +x ./build_release_macos.sh
./build_release_macos.sh -a "$ARCH"

echo "============================================="
echo "   Build Complete!"
echo "============================================="
echo "You can find the application in:"
echo "  build/$ARCH/OrcaSlicer/OrcaSlicer.app"
echo ""
echo "To run it:"
echo "  open build/$ARCH/OrcaSlicer/OrcaSlicer.app"
