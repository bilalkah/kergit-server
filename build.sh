#!/bin/bash
set -e

# Build third-party dependencies
cd third_party/uWebSockets/uSockets

echo "[INFO] Building uSockets static library..."
make
cd - > /dev/null

echo "[INFO] uSockets build complete." 

# Clean previous build for a fresh start
rm -rf build

# Create build directory and build
mkdir -p build
cd build

cmake -G Ninja ..
ninja

echo "[INFO] All components built successfully." 