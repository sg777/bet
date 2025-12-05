#!/bin/bash
# Setup script to install git hooks that configure repository settings
# This script can be run manually OR it will be called automatically during installation
#
# WHO RUNS THIS:
# - Option 1: Each developer runs it once after cloning: ./scripts/setup-git-hooks.sh
# - Option 2: It's called automatically by tools/install-pangea.sh (if integrated)
# - Option 3: Developers can add it to their local git config manually

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GIT_HOOKS_DIR="$REPO_ROOT/.git/hooks"
GITHOOKS_DIR="$REPO_ROOT/.githooks"

# Check if we're in a git repository
if [ ! -d "$REPO_ROOT/.git" ]; then
    echo "Warning: Not in a git repository. Skipping git hooks setup."
    exit 0
fi

echo "Setting up git hooks..."

# Create .git/hooks directory if it doesn't exist
mkdir -p "$GIT_HOOKS_DIR"

# Install post-checkout hook to set git config
if [ -f "$GITHOOKS_DIR/post-checkout" ]; then
    cp "$GITHOOKS_DIR/post-checkout" "$GIT_HOOKS_DIR/post-checkout"
    chmod +x "$GIT_HOOKS_DIR/post-checkout"
    echo "✓ Installed post-checkout hook"
    
    # Run it once to set the config immediately
    "$GIT_HOOKS_DIR/post-checkout"
    echo "✓ Configured git to ignore dirty submodules in diffs"
else
    echo "Warning: .githooks/post-checkout not found"
    # Fallback: set config directly
    git config diff.ignoreSubmodules dirty 2>/dev/null || true
    echo "✓ Configured git to ignore dirty submodules in diffs (fallback)"
fi

echo "Git hooks setup complete!"

