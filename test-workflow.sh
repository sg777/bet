#!/bin/bash
# Test script to simulate the GitHub Actions workflow locally

set -e  # Exit on error

echo "=== Testing GitHub Actions Workflow Locally ==="
echo ""

# Extract git tag (simulating workflow step)
GITTAG=$(git describe --always | cut -d'-' -f1-2)
echo "Git tag: $GITTAG"
echo ""

# Clean previous builds
echo "=== Cleaning previous builds ==="
make --directory libs/crypto clean || true
make --directory poker clean || true
echo ""

# Build crypto library
echo "=== Building crypto library ==="
make --directory libs/crypto
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build crypto library"
    exit 1
fi
echo ""

# Build poker application
echo "=== Building poker application ==="
make --directory poker
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build poker application"
    exit 1
fi
echo ""

# Verify binaries exist
echo "=== Verifying binaries ==="
if [ ! -f "poker/bin/bet" ]; then
    echo "ERROR: poker/bin/bet not found"
    exit 1
fi
if [ ! -f "poker/bin/cashierd" ]; then
    echo "ERROR: poker/bin/cashierd not found"
    exit 1
fi
echo "✓ bet binary found"
echo "✓ cashierd binary found"
echo ""

# Verify config directory exists
if [ ! -d "poker/config" ]; then
    echo "ERROR: poker/config directory not found"
    exit 1
fi
echo "✓ config directory found"
echo ""

# Create release archive (simulating workflow step)
ARCHIVE_NAME="bet-linux-x86_64-${GITTAG}.tar.gz"
echo "=== Creating release archive: $ARCHIVE_NAME ==="
tar -czvf "$ARCHIVE_NAME" poker/bin/bet poker/bin/cashierd poker/config
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to create archive"
    exit 1
fi
echo ""

# Verify archive
echo "=== Verifying archive ==="
ls -lh "$ARCHIVE_NAME"
echo ""

# Test archive contents
echo "=== Testing archive contents ==="
tar -tzf "$ARCHIVE_NAME" | head -10
echo ""

echo "=== Workflow test completed successfully! ==="
echo "Archive created: $ARCHIVE_NAME"
echo ""
echo "To test the full workflow, you can:"
echo "1. Install 'act' tool: https://github.com/nektos/act"
echo "2. Run: act -j linux-build-x86_64"
echo "3. Or manually test each step as shown above"

