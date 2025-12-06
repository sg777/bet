# Repository Cleanup Analysis

## Summary
This document identifies files and directories that are **NOT** part of the build process and can be removed to make the repository lean.

## Build Process Overview

### What the Makefile Actually Uses:
1. **`privatebet/`** - Main source code
   - `src/*.c` - Source files (listed in Makefile)
   - `include/*.h` - Header files
   - `Makefile` - Build configuration
   - `scripts/build_dir_tree.sh` - Build script
   - `config/` - Configuration files (runtime, not build)

2. **`crypto777/`** - Dependency library (built via `make --directory crypto777`)

3. **`external/`** - External dependencies (submodules)
   - `jsmn`, `nng`, `libwebsockets`, `dlg`, `cmake`, `iniparser`

4. **`includes/`** - Additional header files (likely curl headers)

## Files/Directories NOT Needed for Build

### In `privatebet/` directory:

#### 1. **Build Outputs** (should be in `.gitignore`, not in repo):
- `privatebet/bet` - Compiled binary
- `privatebet/cashierd` - Compiled binary  
- `privatebet/src/*.o` - Object files
- `privatebet/tags` - Generated tags file (ctags/etags)

#### 2. **Documentation Generation Files** (not needed for build):
- `privatebet/Doxyfile` - Doxygen configuration
- `privatebet/latex/` - Entire directory (Doxygen-generated LaTeX files)
  - All `.tex` files
  - All `.dot` files (graphviz)
  - All `.md5` files
  - `Makefile` in latex/

#### 3. **Non-Build Scripts**:
- `privatebet/cron.sh` - Cron script (not part of build process)

### In Root Directory:

#### 1. **Documentation Generation**:
- `Doxyfile` - Root Doxygen config
- `doxygen.tag` - Generated Doxygen tags

#### 2. **Potentially Unused Directories** (need verification):
- `basilisk/` - Not referenced in Makefile
- `ccan/` - Only commented out in Makefile (legacy?)
- `datachain/` - Not referenced in Makefile
- `gecko/` - Not referenced in Makefile
- `iguana/` - Not referenced in Makefile

#### 3. **Documentation/Assets** (may be needed for users, not build):
- `assets/` - Images/assets (for documentation?)
- `logos/` - Logo files (for documentation?)
- `docs/` - Documentation (needed for users, but not for build)

## Recommended Actions

### High Priority (Safe to Remove):
1. **Remove build outputs from git tracking:**
   ```bash
   git rm --cached privatebet/bet privatebet/cashierd privatebet/tags
   git rm --cached privatebet/src/*.o
   ```

2. **Add to `.gitignore`:**
   ```
   # Build outputs
   privatebet/bet
   privatebet/cashierd
   privatebet/*.o
   privatebet/src/*.o
   privatebet/tags
   ```

3. **Remove Doxygen-generated files:**
   ```bash
   git rm -r privatebet/latex/
   git rm privatebet/Doxyfile
   git rm Doxyfile doxygen.tag
   ```

### Medium Priority (Verify First):
1. **Check if these directories are used:**
   - `basilisk/`, `ccan/`, `datachain/`, `gecko/`, `iguana/`
   - Search codebase for includes/references
   - If unused, remove them

2. **`privatebet/cron.sh`:**
   - If it's a deployment/runtime script, keep it
   - If it's just an example, remove it

### Low Priority (Keep for Users):
- `docs/` - Keep (needed for documentation)
- `assets/`, `logos/` - Keep if used in docs/README

## Verification Commands

To verify if directories are actually used:

```bash
# Check for includes/references
grep -r "basilisk\|ccan\|datachain\|gecko\|iguana" privatebet/src/ privatebet/include/

# Check if files are tracked but not used
git ls-files | grep -E "(basilisk|ccan|datachain|gecko|iguana)" | head -10
```

## Next Steps

1. Review this analysis
2. Run verification commands for questionable directories
3. Create `.gitignore` entries for build outputs
4. Remove confirmed unused files
5. Test build after cleanup to ensure nothing breaks

