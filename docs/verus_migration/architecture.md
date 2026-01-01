# Pangea-Bet Architecture

## Overview

Pangea-Bet uses a layered architecture for blockchain communication and game state management. This document describes the design and how the layers interact.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│  Application Layer                                          │
│  dealer.c, player.c, game.c, blinder.c                      │
│                                                             │
│  Example calls:                                             │
│    poker_get_key_json("d1", T_TABLE_INFO_KEY, 0)            │
│    poker_join_table()                                       │
│    poker_list_dealers()                                     │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Poker API Layer (poker_vdxf.c)                             │
│  Poker-specific operations with key parsing                 │
│                                                             │
│  Functions:                                                 │
│    poker_get_key_str()      - Get string from poker key     │
│    poker_get_key_json()     - Get JSON from poker key       │
│    poker_append_key_json()  - Append data to poker key      │
│    poker_update_key_json()  - Update poker key data         │
│    poker_join_table()       - Join a poker table            │
│    poker_find_table()       - Find available table          │
│    poker_list_dealers()     - List registered dealers       │
│    poker_is_table_full()    - Check table capacity          │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Generic VDXF Layer (vdxf.c)                                │
│  Verus ID and ContentMultiMap operations                    │
│                                                             │
│  Functions:                                                 │
│    get_cmm()           - Get identity ContentMultiMap       │
│    update_cmm()        - Update identity ContentMultiMap    │
│    is_id_exists()      - Check if identity exists           │
│    id_canspendfor()    - Check spend permission             │
│    id_cansignfor()     - Check sign permission              │
│    get_vdxf_id()       - Convert key name to vdxfid         │
│    verus_sendcurrency_data() - Send currency with data      │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  RPC Abstraction Layer (commands.c)                         │
│  Unified interface for blockchain RPC calls                 │
│                                                             │
│  Function:                                                  │
│    chips_rpc(method, params, &result)                       │
│                                                             │
│  Internally selects transport based on configuration:       │
│    if (use_rest_api)                                        │
│      → HTTP POST via libcurl                                │
│    else                                                     │
│      → CLI execution via popen                              │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Transport Layer                                            │
│                                                             │
│  REST API (libcurl):                                        │
│    HTTP POST to http://127.0.0.1:22778                      │
│    JSON-RPC payload: {"method":"getidentity","params":[]}   │
│                                                             │
│  CLI (popen):                                               │
│    verus -chain=chips getidentity "id@" -1                  │
└─────────────────────────────────────────────────────────────┘
```

## Configuration

### RPC Credentials

RPC credentials are stored in `poker/.rpccredentials`:

```ini
[rpc]
rpc_url = http://127.0.0.1:22778
rpc_user = your_rpc_user
rpc_password = your_rpc_password
use_rest_api = yes
```

- `use_rest_api = yes`: Use HTTP REST API (recommended)
- `use_rest_api = no`: Use CLI via `verus -chain=chips`

**Note:** `poker/.rpccredentials` is in `.gitignore`. Copy from `poker/.rpccredentials.example` and fill in your credentials.

### Verus IDs and Keys

IDs and key names are configurable in `poker/config/verus_ids_keys.ini`:

```ini
[ids]
parent_id = sg777z.chips@
cashier_id = cashier.sg777z.chips.vrsc@
dealer_id = dealer.sg777z.chips.vrsc@

[keys]
key_prefix = chips.vrsc::poker.sg777z
cashiers_key = chips.vrsc::poker.sg777z.cashiers
dealers_key = chips.vrsc::poker.sg777z.dealers
```

## Key Files

| File | Purpose |
|------|---------|
| `poker/src/poker_vdxf.c` | Poker-specific API layer |
| `poker/include/poker_vdxf.h` | Poker API declarations |
| `poker/src/vdxf.c` | Generic VDXF operations |
| `poker/include/vdxf.h` | VDXF declarations and key definitions |
| `poker/src/commands.c` | RPC abstraction (`chips_rpc()`) |
| `poker/src/config.c` | Configuration parsing |

## Usage Example

### Application Code (dealer.c)

```c
#include "poker_vdxf.h"

// Get table info using poker API
cJSON *table_info = poker_get_key_json(dealer_id, T_TABLE_INFO_KEY, 0);

// Update dealer deck
cJSON *out = poker_append_key_json(table_id, deck_key, deck_data, true);

// List all dealers
cJSON *dealers = poker_list_dealers();
```

### How It Works

1. **Application calls `poker_get_key_json()`**
2. **Poker layer** calls `get_cJSON_from_id_key()` in vdxf.c
3. **VDXF layer** calls `chips_rpc("getidentity", params, &result)`
4. **RPC layer** checks configuration:
   - If `use_rest_api`: HTTP POST via libcurl
   - Else: CLI via popen
5. **Response flows back** through the layers with parsed JSON

## Benefits

1. **Abstraction**: Application code doesn't know about REST vs CLI
2. **Configuration**: Switch transport via config file, no recompile
3. **Separation**: Poker logic separate from blockchain communication
4. **Testability**: Each layer can be tested independently
5. **Maintainability**: Changes to RPC don't affect poker logic

## Related Documentation

- [IDs, Keys and Data](ids_keys_data.md) - Verus ID structure and keys
- [Game State](game_state.md) - Game state management
- [Player Joining](player_joining.md) - Player join flow
- [Verus Migration](verus_migration.md) - Overall migration plan

