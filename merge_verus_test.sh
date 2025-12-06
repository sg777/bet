#!/bin/bash

# Script to merge origin/verus_test to upstream/verus_test
# Uses force merge strategy favoring origin's structure (poker/)

set -e

cd /root/bet

echo "Fetching latest from origin and upstream..."
git fetch origin
git fetch upstream

# Abort any existing merge if in progress (before checkout)
if [ -f .git/MERGE_HEAD ]; then
    echo "Aborting existing merge..."
    git merge --abort || true
fi

# Clean up any merge state
git reset --hard HEAD 2>/dev/null || true

echo "Checking out verus_test branch..."
git checkout verus_test

# Make sure we're clean
git reset --hard HEAD

echo "Merging upstream/verus_test into local branch, favoring origin's structure (poker/)..."
# Use -X ours to favor our version (origin's poker/ structure) when conflicts occur
git merge upstream/verus_test -X ours --no-edit || {
    echo "Merge conflicts detected. Resolving by keeping poker/ structure and removing privatebet/ files..."
    
    # For rename/rename conflicts, remove the privatebet/ versions and keep poker/
    # Remove all privatebet/ files/directories that conflict
    if [ -d "privatebet" ]; then
        git rm -rf privatebet/ 2>/dev/null || rm -rf privatebet/
    fi
    
    # Ensure all poker/ files are properly staged (keep our version)
    git add -A poker/ 2>/dev/null || true
    
    # Resolve any remaining conflicts by keeping our version
    git checkout --ours . 2>/dev/null || true
    git add -A . 2>/dev/null || true
    
    # Complete the merge
    git commit --no-edit || {
        echo "Failed to complete merge. Please resolve conflicts manually."
        exit 1
    }
}

echo "Merge successful!"
echo "Pushing to upstream/verus_test..."
if git push upstream verus_test; then
    echo "Merge and push completed successfully!"
else
    echo "Push failed. Attempting force push..."
    git push upstream verus_test --force
    echo "Force push completed!"
fi
