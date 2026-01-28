#!/bin/bash

echo "Building LabWork for Linux..."

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure with CMake
cmake ..
if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    exit 1
fi

# Build
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo "Executable: build/labwork"
echo ""

# Make executable
chmod +x labwork

# Return to original directory
cd ..
