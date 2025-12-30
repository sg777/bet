#!/bin/bash
# Complete script to rename crypto777 to crypto and remove unused files

set -e

cd /root/bet

echo "Step 1: Renaming libs/crypto777 to libs/crypto..."
if [ -d "libs/crypto777" ]; then
    mv libs/crypto777 libs/crypto
    echo "✓ Renamed directory"
else
    echo "✗ libs/crypto777 not found!"
    exit 1
fi

echo ""
echo "Step 2: Removing unused files (bitcoind_RPC, SaM, ramcoder)..."
cd libs/crypto
rm -f bitcoind_RPC.c bitcoind_RPC.o
rm -f SaM.c SaM.o  
rm -f ramcoder.c ramcoder.o
echo "✓ Removed unused files"

echo ""
echo "Step 3: Cleaning old build artifacts..."
rm -f *.o libcrypto777.a libcrypto.a
echo "✓ Cleaned build artifacts"

echo ""
echo "Step 4: Building crypto library..."
make clean
make
if [ -f "libcrypto.a" ]; then
    echo "✓ Crypto library built successfully"
else
    echo "✗ Failed to build crypto library!"
    exit 1
fi

cd ../../poker

echo ""
echo "Step 5: Building poker..."
make clean
make

if [ -f "bin/bet" ]; then
    echo "✓ Poker built successfully"
    echo ""
    echo "Step 6: Testing poker binary..."
    ./bin/bet --help 2>&1 | head -5
    echo ""
    echo "✓ All done! Crypto library renamed and poker rebuilt."
else
    echo "✗ Failed to build poker!"
    exit 1
fi

echo ""
echo "Don't forget to commit:"
echo "  git add -A"
echo "  git commit -m 'Rename crypto777 to crypto, remove unused utils'"
echo "  git push origin verus_test"

