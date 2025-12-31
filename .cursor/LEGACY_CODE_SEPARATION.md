# Legacy Code Separation Guide

## Overview

This document provides a step-by-step guide for separating legacy pub-sub and Lightning Network code into dedicated legacy files for eventual removal.

## Approach: Two-Phase Separation

### Phase 1: Separate Legacy Code (Recommended First Step)
**Why**: Allows gradual migration, easier testing, clear separation
**How**: Move legacy code to `poker/src/legacy/` directory

### Phase 2: Remove Legacy Code (After Verification)
**Why**: Clean up codebase, reduce maintenance burden
**How**: Delete legacy files after confirming Verus-only flow works

## Directory Structure

```
poker/src/legacy/
├── network_legacy.c      # Pub-sub/nanomsg stub functions
├── network_legacy.h      # Legacy network function declarations
├── payment_ln_legacy.c   # Lightning Network payment functions
└── payment_ln_legacy.h   # LN payment function declarations
```

## Step-by-Step Process

### Step 1: Create Legacy Directory

```bash
mkdir -p poker/src/legacy
```

### Step 2: Identify Legacy Functions

#### Pub-Sub/NaNomsg Functions (from network.c)
- `bet_msg_dealer_with_response_id()` - Stub function
- `bet_msg_cashier_with_response_id()` - Stub function (in cashier.c)
- `bet_msg_cashier()` - Stub function (in cashier.c)
- `bet_tcp_sock_address()` - May still be used, check first
- Any remaining socket creation code

#### Lightning Network Functions (from payment.c, client.c, commands.c)
- `bet_dcv_pay()` - Uses lightning-cli
- `bet_player_create_invoice()` - Creates LN invoice
- `bet_player_create_invoice_request()` - LN invoice request
- `bet_player_invoice_pay()` - Pays LN invoice
- `bet_player_betting_invoice()` - Betting via LN (in client.c)
- All `lightning-cli` command handling in commands.c

### Step 3: Create Legacy Header Files

#### `poker/src/legacy/network_legacy.h`
```c
#ifndef NETWORK_LEGACY_H
#define NETWORK_LEGACY_H

#include "bet.h"

// Legacy pub-sub/nanomsg functions (deprecated, being removed)
// These functions are stubs and no longer functional
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message);
cJSON *bet_msg_cashier_with_response_id(cJSON *argjson, char *cashier_ip, char *message);
cJSON *bet_msg_cashier(cJSON *argjson, char *cashier_ip, char *message);

#endif /* NETWORK_LEGACY_H */
```

#### `poker/src/legacy/payment_ln_legacy.h`
```c
#ifndef PAYMENT_LN_LEGACY_H
#define PAYMENT_LN_LEGACY_H

#include "bet.h"

// Legacy Lightning Network payment functions (deprecated, being removed)
// These functions will be removed in favor of CHIPS-only payments
int32_t bet_dcv_pay(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars);
int32_t bet_player_create_invoice(cJSON *argjson, struct privatebet_info *bet, 
                                   struct privatebet_vars *vars, char *deckid);
int32_t bet_player_create_invoice_request(cJSON *argjson, struct privatebet_info *bet, int32_t amount);
int32_t bet_player_invoice_pay(cJSON *argjson, struct privatebet_info *bet, 
                                struct privatebet_vars *vars, int amount);
int32_t bet_player_betting_invoice(cJSON *argjson, struct privatebet_info *bet, 
                                    struct privatebet_vars *vars);

#endif /* PAYMENT_LN_LEGACY_H */
```

### Step 4: Move Functions to Legacy Files

#### Example: Moving `bet_msg_dealer_with_response_id()`

**From `poker/src/network.c`:**
```c
// bet_msg_dealer_with_response_id - stub implementation (nanomsg removed, but still called)
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message)
{
    // Nanomsg removed - function no longer functional
    dlg_error("bet_msg_dealer_with_response_id: nanomsg removed - not implemented");
    return NULL;
}
```

**To `poker/src/legacy/network_legacy.c`:**
```c
#include "network_legacy.h"
#include "err.h"

// Legacy function - nanomsg/pub-sub removed, replaced by Verus ID communication
// This is a stub that returns NULL. Callers should use Verus ID updates instead.
cJSON *bet_msg_dealer_with_response_id(cJSON *argjson, char *dealer_ip, char *message)
{
    dlg_warn("bet_msg_dealer_with_response_id: Legacy pub-sub function called - use Verus IDs instead");
    // Nanomsg removed - function no longer functional
    // TODO: Replace caller with Verus ID update pattern
    return NULL;
}
```

### Step 5: Update Main Source Files

#### Remove from original files
- Delete the function implementation
- Add include for legacy header if still needed temporarily
- Add comment pointing to Verus ID alternative

#### Example: `poker/src/network.c`
```c
#include "network.h"
#include "bet.h"
// ... other includes ...

// Legacy pub-sub functions moved to poker/src/legacy/network_legacy.c
// Use Verus ID communication instead (see vdxf.c)

// bet_nanosock removed - nanomsg/nano sockets no longer used
// bet_send_data removed - nanomsg/nano sockets no longer used
```

### Step 6: Update Makefile

Add legacy files to build (temporarily, until removed):

```makefile
# In poker/Makefile
LEGACY_SOURCES := $(SRC_DIR)/legacy/network_legacy.c \
                  $(SRC_DIR)/legacy/payment_ln_legacy.c

# Add to OBJFILES if needed for compilation
# Or compile separately for reference
```

### Step 7: Replace Callers with Verus ID Pattern

#### Old Pattern (Pub-Sub)
```c
cJSON *response = bet_msg_dealer_with_response_id(argjson, dealer_ip, "message");
```

#### New Pattern (Verus ID)
```c
// Update dealer's table ID with game state
cJSON *table_state = get_game_state_info(table_id);
append_game_state(table_id, G_PLAYERS_JOINED, table_state);

// Players poll table ID
cJSON *table_state = get_game_state_info(table_id);
int32_t game_state = get_game_state(table_id);
```

### Step 8: Remove LN Configuration

#### From `poker/src/config.c`
```c
// Remove or comment out
// int32_t bet_ln_config = 0;

// Remove from config parsing
// if (-1 != iniparser_getboolean(ini, "dealer:bet_ln_config", -1)) {
//     bet_ln_config = iniparser_getboolean(ini, "dealer:bet_ln_config", -1);
// }
```

#### From config files
- Remove `bet_ln_config` from `player.ini`, `dealer.ini`

### Step 9: Update Error Handling

#### Mark LN errors as deprecated
```c
// In poker/src/err.c
case ERR_LN:
    dlg_warn("ERR_LN: Lightning Network deprecated, use CHIPS only");
    return "Lightning Network deprecated";
```

#### Remove LN error paths
- Replace `ERR_LN_*` checks with `ERR_CHIPS_*` or appropriate Verus errors
- Update error messages to reflect Verus-only architecture

## Verification Checklist

### Before Moving Code
- [ ] Identify all legacy functions
- [ ] Find all callers of legacy functions
- [ ] Document Verus ID replacement pattern for each

### After Moving Code
- [ ] Code compiles without errors
- [ ] All legacy functions in legacy/ directory
- [ ] Main source files don't contain legacy implementations
- [ ] Legacy functions marked with deprecation warnings

### Before Removing Legacy Code
- [ ] All callers replaced with Verus ID pattern
- [ ] No references to legacy functions in main code
- [ ] Tests pass with Verus-only flow
- [ ] Documentation updated

## Common Patterns

### Pattern 1: Replacing Pub-Sub Message Sending

**Old:**
```c
bet_msg_dealer_with_response_id(argjson, dealer_ip, "join_table");
```

**New:**
```c
// Player updates their player ID
cJSON *player_info = create_player_join_info();
update_cmm_from_id_key_data_cJSON(player_id, get_vdxf_id(PLAYER_INFO_KEY), player_info, true);

// Dealer polls player IDs
cJSON *player_info = get_cJSON_from_id_key(player_id, PLAYER_INFO_KEY, 0);
```

### Pattern 2: Replacing LN Invoice Payment

**Old:**
```c
bet_player_create_invoice(argjson, bet, vars, deckid);
bet_dcv_pay(argjson, bet, vars);
```

**New:**
```c
// Use CHIPS transaction directly
cJSON *tx = chips_transfer_funds_with_data(amount, recipient_addr, hex_data);
```

### Pattern 3: Replacing LN Channel Establishment

**Old:**
```c
bet_establish_ln_channel_with_dealer(bet, dealer_info);
```

**New:**
```c
// No channel needed - use direct CHIPS transactions
// Funds are managed via payin/payout transactions
```

## Testing Strategy

### Unit Tests
- Test Verus ID read/write operations
- Test CHIPS transaction creation
- Verify no legacy function calls in new code paths

### Integration Tests
- Full game flow with Verus IDs only
- Payment flow with CHIPS only
- Player reconnection via Verus IDs

### Regression Tests
- Ensure existing functionality still works
- Verify GUI communication unchanged
- Check error handling paths

## Rollback Plan

If issues arise:
1. Keep legacy files in `poker/src/legacy/` for reference
2. Use `#ifdef LEGACY_SUPPORT` to conditionally compile
3. Maintain git history for easy rollback
4. Document any issues in migration notes

## Timeline

- **Week 1**: Separate legacy code into legacy/ directory
- **Week 2**: Replace callers with Verus ID pattern
- **Week 3**: Testing and verification
- **Week 4**: Remove legacy code (if all tests pass)

## Notes

- Keep legacy code until Verus-only flow is fully tested
- Document all replacements in code comments
- Update `.cursorrules` and architecture docs as you go
- Commit frequently with clear messages

