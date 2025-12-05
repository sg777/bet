# Repository Cleanup Plan

## Current Status

### ✅ Already Properly Ignored (No Action Needed):
- `privatebet/bet` - Binary (in .gitignore)
- `privatebet/cashierd` - Binary (in .gitignore)  
- `*.o` files - Object files (in .gitignore)
- `tags` - Tags file (in .gitignore)

### ❌ Tracked But Not Needed for Build (Should Remove):

#### 1. Documentation Generation Files (Safe to Remove):
```bash
# These are Doxygen-generated files, not needed for build
git rm privatebet/Doxyfile
git rm -r privatebet/latex/
git rm Doxyfile doxygen.tag  # Root level
```

**Files to remove:**
- `privatebet/Doxyfile`
- `privatebet/latex/` (entire directory - 30+ files)
- `Doxyfile` (root)
- `doxygen.tag` (root)

#### 2. Unused Directories (Verified - Safe to Remove):

**Directories NOT used by privatebet or crypto777:**
- `basilisk/` - ✅ Verified: No includes from privatebet/crypto777
- `ccan/` - ✅ Verified: No includes from privatebet/crypto777 (only commented in Makefile)
- `datachain/` - ✅ Verified: No includes from privatebet/crypto777
- `gecko/` - ✅ Verified: No includes from privatebet/crypto777

**⚠️ KEEP:**
- `iguana/` - ❌ **USED** by `crypto777/iguana_utils.c` (includes `../iguana/iguana777.h`)

**Remove unused directories:**
```bash
git rm -r basilisk/ ccan/ datachain/ gecko/
```

#### 3. Non-Build Scripts (Keep - Deployment Script):
- `privatebet/cron.sh` - ✅ **KEEP** - This is a deployment script for monitoring/restarting cashierd service

## Recommended Cleanup Commands

### Step 1: Remove Doxygen Files (Safe)
```bash
cd /root/bet
git rm privatebet/Doxyfile
git rm -r privatebet/latex/
git rm Doxyfile doxygen.tag 2>/dev/null || true
```

### Step 2: Remove Unused Directories (Verified Safe)
```bash
# These directories are NOT used by privatebet or crypto777
# Only iguana/ is used, so we keep it
git rm -r basilisk/ ccan/ datachain/ gecko/
```

### Step 3: Keep cron.sh
```bash
# cron.sh is a deployment script - keep it
# No action needed
```

## Impact Assessment

### Files to Remove:
- **Doxygen files**: ~29 files (documentation generation, not needed for build)
- **Unused directories**: ~500+ files (basilisk, ccan, datachain, gecko - verified unused)
- **Total cleanup**: ~530+ files

### Build Verification:
After cleanup, verify build still works:
```bash
make clean
make
```

## Next Steps

1. **Review** this plan
2. **Run verification** commands to confirm unused directories
3. **Execute cleanup** commands
4. **Test build** to ensure nothing breaks
5. **Commit changes** with clear message

