# Verus Test Branch - Comprehensive Overview

## Branch Purpose

The `verus_test` branch is a **migration branch** that transitions Pangea-Bet from a hybrid architecture to a pure Verus-based architecture. This branch implements Verus ID (VDXF) communication to replace socket-based communication and removes Lightning Network dependencies.

## Current Architecture Status

### ✅ What's Implemented

1. **Verus ID Communication System**
   - Core VDXF functions implemented in `poker/src/vdxf.c`
   - Functions for reading/writing game state to Verus IDs
   - Key-value storage system using Verus contentmultimap (CMM)
   - ID creation and management (`sg777z.chips.vrsc@`, `dealer.sg777z.chips.vrsc@`, `cashier.sg777z.chips.vrsc@`)

2. **Code Refactoring & Cleanup**
   - Removed unused directories (basilisk, datachain, gecko, logos, assets, ccan)
   - Renamed `crypto777` → `crypto` directory
   - Renamed `cards777` → `cards` files
   - Fixed buffer overflows, memory leaks, and code safety issues
   - Improved error handling throughout

3. **Documentation**
   - Comprehensive GUI message format documentation (`docs/protocol/GUI_MESSAGE_FORMATS.md`)
   - Verus migration documentation (`docs/verus_migration/`)
   - Refactoring samples showing before/after code improvements

4. **CHIPS-Only Payments**
   - Payment system refactored to use CHIPS transactions
   - Lightning Network functions stubbed out (return errors)

### ⚠️ What's Partially Implemented

1. **Legacy Code Removal**
   - **Pub-Sub/NaNomsg**: Mostly removed, but stub functions remain
     - Functions like `bet_msg_dealer_with_response_id()` are stubs
     - Ports 7797, 7798, 7901, 7902 defined but not actively used
   - **Lightning Network**: Functions deprecated but still in codebase
     - Functions return `ERR_LN` errors
     - Configuration variables may still exist

2. **Verus ID Integration**
   - Core functions exist but may not be fully integrated everywhere
   - Some communication paths may still reference old socket-based methods

### ❌ What Needs Work

1. **Complete Legacy Code Removal** (See `.cursor/VERUS_MIGRATION_PLAN.md`)
   - Phase 1: Move legacy functions to `poker/src/legacy/` directory
   - Phase 2: Remove Lightning Network code completely
   - Phase 3: Remove all pub-sub/nanomsg code

2. **TODOs Found in Code**
   - `vdxf.c:59`: More sophisticated key type determination system
   - `vdxf.c:336`: Multi-table support (currently single table only)
   - `vdxf.c:458`: Handle case where TX succeeds but verus_pid not added to table
   - `player.c:104,143,195`: Error handling and rejoin logic
   - `dealer.c:276,283`: Automated dealer registration, multi-table support
   - `game.c:140`: Block wait time for dealer actions

3. **Testing & Verification**
   - Verify all game flows work with Verus IDs only
   - Test player reconnection scenarios
   - Test dealer/cashier communication via Verus IDs
   - Performance testing of Verus ID polling

## Key Files & Directories

### Core Implementation
- **`poker/src/vdxf.c`** - Verus ID communication (1498 lines)
- **`poker/include/vdxf.h`** - VDXF function declarations and key definitions
- **`poker/src/payment.c`** - Payment functions (CHIPS-only, LN stubbed)
- **`poker/src/network.c`** - Network functions (pub-sub stubbed)

### Configuration
- **`poker/config/verus_dealer.ini`** - Verus dealer configuration
- **`poker/config/verus_player.ini`** - Verus player configuration
- **`poker/config/blockchain_config.ini`** - Blockchain settings

### Documentation
- **`docs/verus_migration/`** - Verus migration documentation
- **`docs/protocol/GUI_MESSAGE_FORMATS.md`** - GUI WebSocket API docs
- **`.cursor/VERUS_MIGRATION_PLAN.md`** - Detailed migration plan

## Verus ID System Architecture

### ID Hierarchy
```
sg777z.chips.vrsc@ (root)
├── dealer.sg777z.chips.vrsc@
│   └── <dealer_name>.sg777z.chips.vrsc@
│       └── Keys: t_table_info, t_player_info, t_game_info, etc.
├── cashier.sg777z.chips.vrsc@
│   └── Keys: chips.vrsc::poker.cashiers
└── <player_name>.sg777z.chips.vrsc@
    └── Keys: player_deck, etc.
```

### Key Naming Convention
- Format: `chips.vrsc::poker.<key_name>`
- Examples:
  - `chips.vrsc::poker.cashiers` - Cashier list
  - `chips.vrsc::poker.dealers` - Dealer list
  - `chips.vrsc::poker.t_table_info` - Table information
  - `chips.vrsc::poker.t_player_info` - Player information
  - `chips.vrsc::poker.t_game_info` - Game state

### Communication Pattern
1. **Dealer** updates table ID with game state
2. **Players** poll table ID to get updates
3. **Cashiers** read/write to cashiers ID for coordination
4. All updates are blockchain transactions (cost: tx_fee per update)

## Recent Changes (Last 20 Commits)

1. **GUI improvements**: Independent threads, config path fixes, WebSocket fixes
2. **Code safety**: Fixed buffer overflows, memory leaks
3. **Cleanup**: Removed unused directories and files
4. **Refactoring**: Improved JSON parsing, config loading, error handling
5. **Verus fixes**: Fixed segmentation faults in Verus RPC commands
6. **Chain name**: Replaced `chips777` with `chips`

## Migration Status

### Current Phase: **Phase 1 - Legacy Code Separation** (In Progress)

According to `.cursor/VERUS_MIGRATION_PLAN.md`:
- ✅ Verus ID communication implemented
- ⚠️ Legacy code still present (stubs)
- ❌ Legacy code not yet moved to separate directory
- ❌ Lightning Network code not fully removed
- ❌ Pub-sub code not fully removed

### Next Steps

1. **Immediate**: Review and understand current Verus ID implementation
2. **Short-term**: Complete Phase 1 - Move legacy code to `poker/src/legacy/`
3. **Medium-term**: Complete Phase 2 - Remove Lightning Network
4. **Long-term**: Complete Phase 3 - Remove pub-sub, full Verus ID migration

## Important Notes

1. **WebSocket Variables**: Variables used by UI via WebSockets should NOT be removed even if they appear unused
2. **Error Codes**: `ERR_LN_*` (49-64) are deprecated but still defined
3. **Ports**: Old pub-sub ports (7797, 7798, 7901, 7902) are defined but not actively used
4. **Chain**: Uses `chips10sec` chain (10-second block time)
5. **Costs**: Each Verus ID update costs a transaction fee (~0.0001 CHIPS)

## Testing Requirements

Before considering this branch production-ready:
- [ ] Full game flow test (join → play → payout)
- [ ] Player reconnection test
- [ ] Multi-player game test
- [ ] Dealer disconnection/recovery test
- [ ] Cashier coordination test
- [ ] Performance test (Verus ID polling frequency)
- [ ] Error handling test (failed transactions, network issues)

## Resources

- **Migration Plan**: `.cursor/VERUS_MIGRATION_PLAN.md`
- **Architecture Docs**: `.cursor/ARCHITECTURE.md`
- **Code Locations**: `.cursor/KEY_LOCATIONS.md`
- **Verus Migration**: `docs/verus_migration/verus_migration.md`
- **GUI API**: `docs/protocol/GUI_MESSAGE_FORMATS.md`


