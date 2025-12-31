# Phase 1 & Phase 2 Migration Summary

## Completed: Legacy Code Separation & LN Removal

### Phase 1: Legacy Code Separation ✅

**Objective**: Separate legacy pub-sub and Lightning Network code into dedicated legacy files

#### Created Legacy Directory Structure
```
poker/src/legacy/
├── payment_ln_legacy.c      # Lightning Network payment functions
├── payment_ln_legacy.h      # LN payment declarations
├── network_legacy.c         # Pub-sub/nanomsg network functions
├── network_legacy.h         # Pub-sub network declarations
└── README.md                # Legacy code documentation
```

#### Moved Functions

**Lightning Network Functions** → `payment_ln_legacy.c`:
- `bet_dcv_pay()` - Dealer LN payment
- `bet_player_create_invoice()` - Player invoice creation
- `bet_player_create_invoice_request()` - Invoice request
- `bet_player_invoice_pay()` - Invoice payment
- `bet_player_betting_invoice()` - Betting via invoice (from client.c)
- Payment loop functions (stubs)

**Pub-Sub/NaNomsg Functions** → `network_legacy.c`:
- `bet_msg_dealer_with_response_id()` - Dealer communication stub
- `bet_msg_cashier_with_response_id()` - Cashier communication stub
- `bet_msg_cashier()` - Cashier message sending stub
- `bet_msg_multiple_cashiers()` - Multiple cashier messaging

#### Updated Main Source Files
- `payment.c` - Removed LN implementations, kept only CHIPS function
- `network.c` - Removed pub-sub implementations
- `cashier.c` - Removed pub-sub implementations
- `client.c` - Removed LN betting invoice implementation

#### Updated Headers
- `payment.h` - Points to legacy functions, documents migration
- `network.h` - Points to legacy functions, documents migration

#### Build System
- `Makefile` - Added legacy source files to build
- Legacy files compile successfully
- All legacy functions emit deprecation warnings

### Phase 2: Lightning Network Removal ✅

**Objective**: Remove Lightning Network configuration and simplify command handling

#### Removed Configuration
- `bet_ln_config` variable removed from `config.c`
- `bet_ln_config` parsing removed from dealer and player config
- `bet_ln_config` extern declaration removed from `common.h`
- `BET_WITH_LN` and `BET_WITHOUT_LN` constants removed from `common.h`

#### Updated Command Handling
- `commands.c` - Simplified `lightning-cli` handling to return errors with warnings
- All `lightning-cli` commands now emit deprecation warnings
- Error handling simplified (no longer attempts to execute LN commands)

#### Configuration Files
- `dealer_config.ini` - Still contains `bet_ln_config` setting (ignored, will be removed in config file cleanup)
- `player_config.ini` - Still contains `bet_ln_config` setting (ignored, will be removed in config file cleanup)

## Current State

### What's Working
✅ **Compilation**: All code compiles successfully
✅ **Legacy Code**: Isolated in `poker/src/legacy/` directory
✅ **LN Configuration**: Removed from code (config files can be cleaned separately)
✅ **Error Handling**: Simplified, LN commands return errors with warnings
✅ **Deprecation Warnings**: All legacy functions emit warnings when called

### What Remains (For Future Phases)

#### Phase 3: Replace Legacy Function Callers
- Replace calls to `bet_msg_dealer_with_response_id()` with Verus ID updates
- Replace calls to `bet_msg_cashier()` with Verus ID updates
- Replace calls to LN payment functions with CHIPS transactions
- Update `client.c` to remove `bet_player_create_invoice()` call

#### Phase 4: Remove Legacy Files
- After all callers are replaced, delete legacy files
- Remove legacy includes from source files
- Remove legacy files from Makefile

#### Phase 5: Clean Up Configuration Files
- Remove `bet_ln_config` from `dealer_config.ini`
- Remove `bet_ln_config` from `player_config.ini`
- Update configuration documentation

## Files Modified

### Source Files
- `poker/src/payment.c` - Removed LN implementations
- `poker/src/network.c` - Removed pub-sub implementations
- `poker/src/cashier.c` - Removed pub-sub implementations, added legacy include
- `poker/src/client.c` - Removed LN implementation, added legacy include
- `poker/src/config.c` - Removed `bet_ln_config` variable and parsing
- `poker/src/commands.c` - Simplified `lightning-cli` handling

### Header Files
- `poker/include/payment.h` - Updated to point to legacy functions
- `poker/include/network.h` - Updated to point to legacy functions
- `poker/include/common.h` - Removed `bet_ln_config` and LN constants

### Build System
- `poker/Makefile` - Added legacy source files to build

### New Files Created
- `poker/src/legacy/payment_ln_legacy.c` - LN payment functions
- `poker/src/legacy/payment_ln_legacy.h` - LN payment declarations
- `poker/src/legacy/network_legacy.c` - Pub-sub network functions
- `poker/src/legacy/network_legacy.h` - Pub-sub network declarations
- `poker/src/legacy/README.md` - Legacy code documentation

## Migration Pattern

### Replacing Pub-Sub Calls
**Old Pattern**:
```c
cJSON *response = bet_msg_dealer_with_response_id(argjson, dealer_ip, "message");
```

**New Pattern** (Verus ID):
```c
// Update dealer's table ID with game state
cJSON *table_state = get_game_state_info(table_id);
append_game_state(table_id, G_PLAYERS_JOINED, table_state);

// Players poll table ID
cJSON *table_state = get_game_state_info(table_id);
int32_t game_state = get_game_state(table_id);
```

### Replacing LN Payments
**Old Pattern**:
```c
bet_player_create_invoice(argjson, bet, vars, deckid);
bet_dcv_pay(argjson, bet, vars);
```

**New Pattern** (CHIPS):
```c
// Use CHIPS transaction directly
cJSON *tx = chips_transfer_funds_with_data(amount, recipient_addr, hex_data);
```

## Testing Status

✅ **Compilation**: Success
✅ **Legacy Functions**: All emit deprecation warnings
✅ **LN Commands**: Return errors with warnings
✅ **Configuration**: LN config removed from code

## Next Steps

1. **Phase 3**: Replace legacy function callers with Verus ID/CHIPS equivalents
2. **Phase 4**: Remove legacy files after verification
3. **Phase 5**: Clean up configuration files
4. **Testing**: Full integration testing with Verus-only flow

## Notes

- Legacy functions are still functional (as stubs) to maintain compilation
- All legacy functions emit deprecation warnings when called
- Configuration files still contain `bet_ln_config` but it's ignored
- Error codes `ERR_LN_*` are kept for backward compatibility but should be deprecated
- See `.cursor/VERUS_MIGRATION_PLAN.md` for detailed migration plan

