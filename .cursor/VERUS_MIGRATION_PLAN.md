# Verus Migration Plan

## Overview

Pangea-Bet is migrating from a hybrid architecture (CHIPS + Lightning Network + Pub-Sub sockets) to a pure Verus-based architecture using:
- **Verus IDs** for all inter-node communication (replacing pub-sub sockets)
- **CHIPS on Verus** for all payments (replacing Lightning Network)
- **WebSockets** for GUI communication (unchanged)

## Current State Analysis

### Legacy Components to Remove

#### 1. Pub-Sub/NaNomsg Communication
**Status**: Mostly removed, but stub functions remain
- **Location**: Throughout `poker/src/` files
- **Evidence**: 100+ "Nanomsg removed" comments
- **Remaining**: Stub functions like `bet_msg_dealer_with_response_id()`, `bet_msg_cashier()`
- **Ports**: 7797, 7798, 7901, 7902 (no longer used)

#### 2. Lightning Network Payments
**Status**: Partially deprecated, still has code paths
- **Location**: `poker/src/payment.c`, `poker/src/client.c`, `poker/src/commands.c`
- **Evidence**: `bet_ln_config` variable, `lightning-cli` command calls
- **Functions**: `bet_player_create_invoice()`, `bet_dcv_pay()`, etc.
- **Error codes**: `ERR_LN_*` (49-64) - can be deprecated

### New Architecture Components

#### Verus ID Communication
**Status**: Already implemented
- **Location**: `poker/src/vdxf.c`
- **Functions**: `updateidentity()`, `getidentitycontent()`, `get_game_state()`, `append_game_state()`
- **Pattern**: Poll-based updates to Verus IDs
- **Documentation**: `docs/verus_migration/`

## Migration Strategy

### Phase 1: Separate Legacy Code (Current Phase)

#### 1.1 Create Legacy Directory Structure
```
poker/src/legacy/
├── network_legacy.c      # Pub-sub/nanomsg code
├── network_legacy.h      # Legacy network headers
├── payment_ln_legacy.c  # Lightning Network payment code
└── payment_ln_legacy.h  # LN payment headers
```

#### 1.2 Move Legacy Functions
- **Pub-Sub functions** → `network_legacy.c`
  - `bet_nanosock()` (if still exists)
  - `bet_msg_dealer_with_response_id()` (stub)
  - `bet_msg_cashier_with_response_id()` (stub)
  - `bet_msg_cashier()` (stub)
  - Any remaining socket creation/binding code

- **LN Payment functions** → `payment_ln_legacy.c`
  - `bet_dcv_pay()` (uses lightning-cli)
  - `bet_player_create_invoice()`
  - `bet_player_create_invoice_request()`
  - `bet_player_invoice_pay()`
  - `bet_player_betting_invoice()` (in client.c)
  - All `lightning-cli` command handling

#### 1.3 Update Main Code
- Replace legacy function calls with Verus ID equivalents
- Add `#ifdef LEGACY_SUPPORT` guards around legacy includes
- Update error handling to remove LN error paths

### Phase 2: Remove Lightning Network (Next Phase)

#### 2.1 Remove LN Configuration
- Remove `bet_ln_config` from `config.c`
- Remove LN config from `player.ini`, `dealer.ini`
- Remove `BET_WITH_LN` / `BET_WITHOUT_LN` conditionals

#### 2.2 Remove LN Functions
- Delete `payment_ln_legacy.c` after confirming no dependencies
- Remove `lightning-cli` command handling from `commands.c`
- Remove LN error codes from `err.h` (or mark as deprecated)

#### 2.3 Update Payment Flow
- All payments use CHIPS transactions only
- Remove invoice creation/payment logic
- Simplify payment functions to CHIPS-only

### Phase 3: Remove Pub-Sub (Final Phase)

#### 3.1 Verify Verus ID Communication
- Ensure all game state updates use Verus IDs
- Verify player/dealer/cashier communication via IDs
- Test reconnection scenarios

#### 3.2 Remove Pub-Sub Code
- Delete `network_legacy.c` after verification
- Remove port bindings (7797, 7798, 7901, 7902)
- Remove `ERR_NNG_*` error codes

#### 3.3 Clean Up Network Module
- Simplify `network.c` to only handle WebSockets
- Remove socket-related includes
- Update documentation

## Implementation Details

### Verus ID Communication Pattern

#### Current Pattern (Already Implemented)
```c
// Dealer updates table ID
cJSON *table_state = get_game_state_info(table_id);
append_game_state(table_id, G_PLAYERS_JOINED, table_state);

// Player polls table ID
cJSON *table_state = get_game_state_info(table_id);
int32_t game_state = get_game_state(table_id);
handle_game_state_player(table_id);
```

#### Functions to Use
- `get_game_state_info(id)` - Read state from Verus ID
- `append_game_state(id, state, data)` - Update state in Verus ID
- `update_cmm_from_id_key_data_cJSON()` - Update ID content
- `get_cJSON_from_id_key()` - Read ID content

### Payment Flow (Verus-Only)

#### Current: CHIPS Transactions
```c
// Payin: Player → Cashier
chips_send_raw_tx(payin_tx);

// Payout: Cashier → Player  
chips_send_raw_tx(payout_tx);
```

#### Remove: Lightning Network
- No invoice creation
- No channel establishment
- No LN payment processing

## File-by-File Changes

### Files Requiring Changes

#### High Priority
1. **`poker/src/payment.c`**
   - Move LN functions to `payment_ln_legacy.c`
   - Remove `bet_dcv_pay()` (uses lightning-cli)
   - Simplify to CHIPS-only

2. **`poker/src/client.c`**
   - Remove `bet_player_betting_invoice()` (LN invoice)
   - Remove LN channel establishment code
   - Remove `lightning-cli` calls

3. **`poker/src/commands.c`**
   - Remove `lightning-cli` command handling
   - Remove LN-related command processing

4. **`poker/src/network.c`**
   - Move stub functions to `network_legacy.c`
   - Remove nanomsg-related code

5. **`poker/src/config.c`**
   - Remove `bet_ln_config` variable
   - Remove LN config parsing

#### Medium Priority
6. **`poker/src/host.c`**
   - Remove LN balance/address fields
   - Remove LN channel checks

7. **`poker/src/states.c`**
   - Remove LN payment logging
   - Simplify to CHIPS-only

8. **`poker/include/payment.h`**
   - Remove LN function declarations
   - Update to CHIPS-only interface

#### Low Priority (Cleanup)
9. **`poker/src/err.c`**
   - Mark `ERR_LN_*` as deprecated
   - Keep for backward compatibility initially

10. **`poker/include/err.h`**
    - Document deprecated error codes
    - Plan removal in future version

## Testing Strategy

### Phase 1 Testing
- [ ] Verify legacy code compiles in separate files
- [ ] Ensure no broken function calls
- [ ] Test that Verus ID communication still works

### Phase 2 Testing
- [ ] Test CHIPS-only payment flow
- [ ] Verify no LN code paths are executed
- [ ] Test game flow without LN

### Phase 3 Testing
- [ ] Test full game flow with Verus IDs only
- [ ] Test player reconnection scenarios
- [ ] Test dealer/cashier communication
- [ ] Performance test Verus ID polling

## Rollback Plan

### If Issues Arise
1. Keep legacy files in `poker/src/legacy/` for reference
2. Use `#ifdef LEGACY_SUPPORT` to re-enable if needed
3. Maintain git history for easy rollback
4. Document any issues encountered

## Documentation Updates

### Files to Update
1. **`.cursorrules`** - Update architecture section
2. **`.cursor/ARCHITECTURE.md`** - Remove pub-sub/LN references
3. **`.cursor/KEY_LOCATIONS.md`** - Update network locations
4. **`README.md`** - Update setup instructions
5. **`docs/protocol/`** - Update protocol docs

## Timeline Estimate

- **Phase 1** (Legacy Separation): 2-3 days
- **Phase 2** (LN Removal): 1-2 days  
- **Phase 3** (Pub-Sub Removal): 1-2 days
- **Testing & Documentation**: 2-3 days

**Total**: ~1-2 weeks

## Success Criteria

- [ ] No pub-sub/nanomsg code in main source files
- [ ] No Lightning Network code in main source files
- [ ] All communication via Verus IDs
- [ ] All payments via CHIPS on Verus
- [ ] GUI communication via WebSockets (unchanged)
- [ ] All tests passing
- [ ] Documentation updated

