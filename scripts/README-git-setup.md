# Git Configuration Setup

## Purpose
This automatically configures git to ignore dirty submodules in `git diff` output, so that changes in third-party libraries (like `external/libwebsockets`) don't clutter your diffs.

## Who Needs to Run This?

### For New Developers (One-Time Setup)

After cloning the repository, run:
```bash
cd bet
./scripts/setup-git-hooks.sh
```

**What happens:**
1. The script installs a git hook (`.git/hooks/post-checkout`)
2. The hook automatically sets `git config diff.ignoreSubmodules dirty`
3. This configuration persists for this repository

### After Setup (Automatic)

Once the hook is installed, it runs automatically whenever you:
- `git checkout <branch>`
- `git pull` (in some cases)
- Switch branches

You don't need to do anything - it's automatic!

## Manual Alternative

If you prefer not to use hooks, you can manually set it once:
```bash
git config diff.ignoreSubmodules dirty
```

## What This Does

- **Before:** `git diff` shows changes in submodules like `external/libwebsockets`
- **After:** `git diff` ignores dirty submodules, showing only your actual code changes

This is a **local repository setting** - it doesn't need to be committed or pushed.

