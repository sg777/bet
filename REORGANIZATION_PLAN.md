# Repository Reorganization Plan

## Current Structure Issues
- `privatebet/` name doesn't clearly indicate it's the poker source code
- `docs/` contains generated HTML files mixed with actual documentation
- Supporting directories scattered at root level
- Unclear separation between source, dependencies, and generated files

## Proposed Clean Structure

```
bet/
├── src/                    # Main poker source code (rename from privatebet/)
│   ├── src/               # Source files
│   ├── include/           # Header files
│   ├── config/            # Configuration files
│   └── Makefile
│
├── docs/                   # Documentation (cleaned)
│   ├── protocol/          # Protocol documentation
│   ├── verus_migration/   # Verus migration docs
│   └── README.md          # Main docs README
│
├── libs/                   # Internal libraries/dependencies
│   ├── crypto777/         # Crypto library
│   └── iguana/            # Iguana library
│
├── external/               # External submodules (keep as is)
│   ├── jsmn/
│   ├── nng/
│   ├── libwebsockets/
│   └── ...
│
├── includes/               # Additional header files (keep as is)
│   ├── curl/
│   └── openssl/
│
├── tools/                  # Build and installation tools
│   ├── install-pangea.sh
│   ├── install_cmake.sh
│   └── ...
│
├── scripts/                # Utility scripts
│   ├── setup-git-hooks.sh
│   └── ...
│
├── assets/                 # Assets (keep as is)
├── logos/                  # Logos (keep as is)
├── log/                    # Logging (keep as is)
│
├── Makefile               # Root Makefile
├── README.md              # Main README
└── .gitignore
```

## Reorganization Steps

### Step 1: Clean up docs/ directory
- Remove generated HTML/JS/CSS files from Doxygen
- Keep only: `protocol/`, `verus_migration/`, and actual `.md` files
- Move generated docs to `docs/generated/` or remove entirely

### Step 2: Rename privatebet/ to src/
- Rename `privatebet/` → `src/`
- Update Makefile references
- Update README.md paths
- Update any script references

### Step 3: Organize dependencies
- Move `crypto777/` → `libs/crypto777/`
- Move `iguana/` → `libs/iguana/`
- Update Makefile paths

### Step 4: Update all references
- Makefile paths
- README.md documentation paths
- Script paths
- Configuration file paths

## Files to Update

### Makefiles:
- Root `Makefile` - update `privatebet` → `src`, `crypto777` → `libs/crypto777`
- `src/Makefile` - update relative paths if needed

### Documentation:
- `README.md` - update all `privatebet/` references to `src/`
- Any scripts referencing paths

### Scripts:
- `tools/install-pangea.sh` - update paths
- `scripts/*.sh` - update paths if needed

## Benefits
1. **Clearer naming**: `src/` clearly indicates source code
2. **Better organization**: Dependencies grouped in `libs/`
3. **Cleaner docs**: Only actual documentation, no generated files
4. **Standard structure**: Follows common project layout conventions

## Migration Impact
- Build process: Update Makefile paths
- Documentation: Update README paths
- Scripts: Update any hardcoded paths
- CI/CD: Update any path references

