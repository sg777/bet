#!/bin/bash
# Script to complete the crypto777 -> crypto rename
# Run this script from the /root/bet directory

set -e

echo "Renaming libs/crypto777 to libs/crypto..."
mv libs/crypto777 libs/crypto

echo "Removing unused files (bitcoind_RPC, SaM, ramcoder)..."
rm -f libs/crypto/bitcoind_RPC.c libs/crypto/bitcoind_RPC.o
rm -f libs/crypto/SaM.c libs/crypto/SaM.o
rm -f libs/crypto/ramcoder.c libs/crypto/ramcoder.o

echo "Cleaning old object files..."
rm -f libs/crypto/*.o libs/crypto/libcrypto777.a

echo "Rebuilding crypto library..."
cd libs/crypto && make clean && make

echo "Rebuilding poker..."
cd ../../poker && make clean && make

echo "Testing poker binary..."
./bin/bet --help | head -5

echo "Done! Crypto library renamed and poker rebuilt successfully."

echo ""
echo "Don't forget to commit the changes:"
echo "  git add -A"
echo "  git commit -m 'Rename crypto777 to crypto, remove unused utils'"
echo "  git push origin verus_test"

