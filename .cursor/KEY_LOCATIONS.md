# Key Locations Quick Reference

## Entry Points

### Main Application
- **`poker/src/bet.c`** - Main entry point, command-line handling
  - `main()` function
  - Command routing (player, dealer, cashier modes)

### Player Mode
- **`poker/src/player.c`** - Player node implementation
  - `bet_player_*` functions
  - `bet_player_frontend_loop()` - Main player loop

### Dealer Mode
- **`poker/src/dealer.c`** - Dealer node implementation
  - `bet_dcv_*` functions (Dealer Control Variables)
  - `bet_bvv_*` functions (Bet Verification Variables)

### Cashier Mode
- **`poker/src/cashier.c`** - Cashier node implementation
  - `bet_process_*` functions
  - Transaction processing

## Core Game Logic

### Game State
- **`poker/src/game.c`** - Game state management
- **`poker/src/poker.c`** - Poker game logic
- **`poker/src/states.c`** - State machine
- **`poker/src/cards.c`** - Card operations (renamed from cards777.c)

### Card Operations
- **`poker/src/cards.c`** - Card handling, encryption, decryption
- **`poker/src/deck.c`** - Deck management
- **`poker/src/blinder.c`** - Card blinding operations

## Networking

### Client/Server
- **`poker/src/client.c`** - Client-side networking, WebSocket client
- **`poker/src/host.c`** - Server-side networking, WebSocket server
- **`poker/src/network.c`** - Network protocol implementation

### WebSocket
- WebSocket handling in `client.c` and `host.c`
- Port: 9000 (GUI communication)
- Port: 1234 (Web server)

## Cryptography

### Crypto Functions
- **`libs/crypto/curve25519.c`** - Curve25519 operations
- **`includes/curve25519.h`** - Crypto function declarations
- **Key functions**: `crypto_account_*` (renamed from `acct777_*`)

### Key Operations
- `crypto_account_pubkey()` - Public key derivation
- `crypto_account_sign()` - Message signing
- `crypto_account_validate()` - Signature validation
- `curve25519_shared()` - Shared secret computation

## Payment & Transactions

### Payment Processing
- **`poker/src/payment.c`** - Payment operations
- **`poker/src/cashier.c`** - Cashier transaction processing
- CHIPS RPC calls
- Lightning Network integration (optional)

### Transaction Types
- Payin transactions
- Payout transactions
- In-game transactions
- Dispute resolution

## Storage & Database

### Database Operations
- **`poker/src/storage.c`** - SQLite3 operations
- Database schema management
- Transaction history
- Game state persistence

## Verus/VDXF Integration

### VDXF Operations
- **`poker/src/vdxf.c`** - Verus ID and VDXF protocol
- Identity management
- Table registration
- Dealer registration
- Content retrieval

## Configuration

### Config Files
- **`poker/config/`** - All configuration files
  - Player configuration
  - Dealer configuration
  - Cashier configuration
  - CHIPS configuration

### Config Parsing
- **`poker/src/config.c`** - Configuration loading
- Uses iniparser library
- Configuration validation

## Headers & Definitions

### Common Definitions
- **`poker/include/common.h`** - Common constants, game constants
  - `CARDS_*` constants
  - Port definitions
  - Game constants

### Error Definitions
- **`poker/include/err.h`** - Error codes and error handling
  - `bet_error_t` enum
  - `bet_err_str()` function

### Main Headers
- **`poker/include/bet.h`** - Main application header
- **`poker/include/player.h`** - Player module header
- **`poker/include/dealer.h`** - Dealer module header
- **`poker/include/cashier.h`** - Cashier module header

## Build System

### Makefiles
- **`poker/Makefile`** - Main build configuration
- **Root `Makefile`** - Top-level build

### Build Output
- **`poker/bin/`** - Compiled binaries
- **`poker/build/`** - Object files

## Documentation

The doc tree follows [Diátaxis](https://diataxis.fr) (tutorials / guides /
reference / explanation / archive). Top-level index: `docs/README.md`.

### Tutorials (follow-along walkthroughs)
- **`docs/tutorials/cli-auto-vrsctest.md`** — local VRSCTEST regtest bring-up
- **`docs/tutorials/community-quickstart.md`** — sanity-check workflow for new contributors
- **`docs/tutorials/revoke-recovery.md`** — registering an identity with revocation/recovery authorities

### Guides (task-oriented how-tos)
- **`docs/guides/build-from-source.md`** — packages, submodules, `make -j`
- **`docs/guides/gui-simulator.md`** — running `tools/gui_simulator.js`

### Reference (look-it-up facts)
- **`docs/reference/glossary.md`** — terminology
- **`docs/reference/dealer-config.md` / `player-config.md` / `cashier-config.md`** — per-role INI fields
- **`docs/reference/gui-message-formats.md`** — full GUI ↔ backend WebSocket spec
- **`docs/reference/gui-quick-reference.md`** — one-page cheat sheet
- **`docs/reference/gui-backend-mapping.md`** — which C file emits which message
- **`docs/reference/tx-types.md`** — on-chain transaction types
- **`docs/reference/rpc-dependency.md`** — Verus RPCs `bet` relies on
- **`docs/reference/game-states.md`** — `enum game_state` reference
- **`docs/reference/vdxf-keys.md`** — every CMM key, per identity
- **`docs/reference/cli-print.md`** — `./bet print*` shell commands
- **`docs/reference/player-join-flow.md`** — payin + dealer poll + cashier verify

### Explanation (architectural background)
- **`docs/explanation/verus-overview.md`** — how `bet` uses Verus
- **`docs/explanation/architecture.md`** — layered structure of the codebase
- **`docs/explanation/identity-tree.md`** — aggregator / operational / per-table IDs
- **`docs/explanation/adhoc-multisig.md`** — `primaryaddresses` + `minimumsignatures`
- **`docs/explanation/node-communication.md`** — CMM-only inter-node model
- **`docs/explanation/deck-shuffling.md`** — multi-pass Curve25519 shuffle
- **`docs/explanation/getidentitycontent.md`** — CMM reconstruction from `height_start`
- **`docs/explanation/player-rejoin.md`** — disconnect / reconnect semantics

### Archive
- **`docs/archive/`** — pre-Verus / pre-migration historical docs
  (LN-era protocol, removed features, dated milestones).

## External Dependencies

### Libraries
- **`external/iniparser/`** - INI file parsing
- **`external/libwebsockets/`** - WebSocket library
- **`external/dlg/`** - Logging library

### System Libraries
- SQLite3 (`-lsqlite3`)
- OpenSSL (`-lssl -lcrypto`)
- libcurl (`-lcurl`)
- libevent (`-levent`)

## Helper Functions

### JSON Helpers
- `jstr()` - Get string from JSON
- `jint()` - Get integer from JSON
- `jdouble()` - Get double from JSON
- Located in various source files (check includes)

### String Helpers
- `bits256_str()` - Convert bits256 to string
- `hexstr_to_str()` - Hex string conversion
- Located in crypto/utility files

## Testing & Debugging

### Test Code
- **`poker/src/test.c`** - Test functions
- Unit tests for various modules

### Logging
- **`poker/include/macrologger.h`** - Logging macros
- `dlg_error()`, `dlg_info()`, `dlg_warn()`, `dlg_debug()`

## Ports Reference

### Network Ports
- **7797** - Dealer pub-sub
- **7798** - Dealer push-pull
- **7901** - Cashier pub-sub
- **7902** - Cashier push-pull
- **7903** - Dealer BVV pub-sub
- **7904** - Dealer BVV push-pull
- **9000** - WebSocket (GUI)
- **1234** - Web server (GUI hosting)

## Constants Reference

### Game Constants (`common.h`)
- `CARDS_MAXCARDS = 10`
- `CARDS_MAXPLAYERS = 9`
- `CARDS_MAXROUNDS = 4`
- `hand_size = 7`
- `BET_PLAYERTIMEOUT = 15`

### Blockchain Constants
- `satoshis = 100000000`
- `default_chips_tx_fee = 0.0001`

## Common Patterns

### Finding Functions
- Player functions: Search for `bet_player_` in `player.c`
- Dealer functions: Search for `bet_dcv_` or `bet_bvv_` in `dealer.c`
- Cashier functions: Search for `bet_process_` in `cashier.c`
- Crypto functions: Search for `crypto_account_` in `curve25519.c`

### Finding Error Handling
- Error codes: `poker/include/err.h`
- Error messages: `poker/src/err.c` - `bet_err_str()`

### Finding Memory Management
- Look for `calloc`/`malloc` patterns
- Check for corresponding `free`/`cJSON_Delete` calls
- Verify error path cleanup

## Quick Search Tips

### Find Function Definition
```bash
grep -r "function_name" poker/src/
```

### Find Function Usage
```bash
grep -r "function_name(" poker/
```

### Find Error Code
```bash
grep "ERR_CODE_NAME" poker/include/err.h
```

### Find Constant Definition
```bash
grep "CONSTANT_NAME" poker/include/common.h
```

