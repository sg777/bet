# Pangea-Bet Architecture Overview

## System Architecture

### High-Level Components

```
┌─────────────────────────────────────────────────────────────┐
│                    Pangea-Bet Application                  │
│                    (poker/src/bet.c)                        │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
    ┌───▼───┐          ┌────▼────┐        ┌────▼────┐
    │Player │          │ Dealer  │        │ Cashier │
    │ Node  │          │  Node   │        │  Node   │
    └───┬───┘          └────┬────┘        └────┬────┘
        │                   │                   │
        └───────────────────┼───────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
    ┌───▼───┐          ┌────▼────┐        ┌────▼────┐
    │ CHIPS │          │   LN    │        │  GUI    │
    │Block- │          │(Optional)│       │WebSocket│
    │ chain │          └─────────┘        └─────────┘
    └───────┘
```

## Core Modules

### 1. Player Module (`poker/src/player.c`)
**Purpose**: Handles player node operations
- Connects to dealer nodes
- Manages player state and game participation
- Handles card decryption and game actions
- WebSocket communication with GUI

**Key Functions**:
- `bet_player_*` - Player-specific functions
- `bet_player_frontend_loop()` - Main player loop

**Dependencies**:
- `game.c` - Game logic
- `client.c` - Network client
- `cards.c` - Card operations
- `payment.c` - Payment processing

### 2. Dealer Module (`poker/src/dealer.c`)
**Purpose**: Manages dealer node operations
- Hosts poker tables
- Manages player connections
- Coordinates game flow
- Handles deck shuffling and card distribution

**Key Functions**:
- `bet_dcv_*` - Dealer control variable functions
- `bet_bvv_*` - Bet verification variable functions

**Dependencies**:
- `host.c` - Hosting and networking
- `game.c` - Game state management
- `blinder.c` - Card blinding operations
- `dealer_registration.c` - Dealer registration with Verus

### 3. Cashier Module (`poker/src/cashier.c`)
**Purpose**: Manages funds and transactions
- Processes payins and payouts
- Validates transactions
- Manages multi-signature addresses
- Handles disputes

**Key Functions**:
- `bet_process_*` - Transaction processing
- `bet_check_cashiers_status()` - Cashier validation
- `bet_reverse_disputed_tx()` - Dispute resolution

**Dependencies**:
- `payment.c` - Payment operations
- `storage.c` - Database operations
- `vdxf.c` - Verus ID operations

### 4. Game Logic (`poker/src/game.c`, `poker/src/poker.c`)
**Purpose**: Core poker game mechanics
- Game state management
- Betting rounds
- Hand evaluation
- Winner determination

**Key Functions**:
- Game state transitions
- Betting logic
- Hand ranking

**Dependencies**:
- `cards.c` - Card operations
- `states.c` - State machine

### 5. Network Module (`poker/src/network.c`, `poker/src/client.c`)
**Purpose**: Inter-node communication
- Pub-sub messaging (NNG, previously used)
- WebSocket server (libwebsockets)
- HTTP client (libcurl)

**Key Functions**:
- Network message handling
- Connection management
- Protocol implementation

**Dependencies**:
- `host.c` - Hosting services
- External: libwebsockets, libcurl

### 6. Cryptography (`libs/crypto/curve25519.c`)
**Purpose**: Cryptographic operations
- Key generation and management
- Signing and verification
- Shared secret computation
- Message encryption/decryption

**Key Functions**:
- `crypto_account_*` - Account operations
- `curve25519_*` - Curve25519 operations
- `vcalc_sha256*` - SHA256 hashing

**Dependencies**:
- `includes/curve25519.h` - Header definitions

### 7. Storage Module (`poker/src/storage.c`)
**Purpose**: Local data persistence
- SQLite3 database operations
- Game state storage
- Transaction history
- Configuration persistence

**Key Functions**:
- Database queries
- Transaction management
- Data retrieval

**Dependencies**:
- SQLite3 library

### 8. VDXF/Verus Module (`poker/src/vdxf.c`)
**Purpose**: Verus blockchain integration
- Verus ID operations
- VDXF protocol handling
- Identity management
- Table and dealer registration

**Key Functions**:
- `bet_vdxf_*` - VDXF operations
- Identity verification
- Content retrieval

**Dependencies**:
- Verus RPC (via curl)

## Data Flow

### Player Joining Flow
```
1. Player → Dealer: Join request
2. Dealer → Player: Table info
3. Player → Cashier: Payin transaction
4. Cashier → Player: Payin confirmation
5. Player → Dealer: Payin proof
6. Dealer → Player: Added to table
```

### Game Flow
```
1. Deck Shuffling (all players + dealer)
2. Card Distribution (encrypted shares)
3. Betting Rounds (pre-flop, flop, turn, river)
4. Showdown (card decryption)
5. Winner Determination
6. Payout Processing
```

### Payment Flow
```
1. Payin: Player → Cashier (CHIPS transaction)
2. Game: In-game actions (optional LN)
3. Payout: Cashier → Player (CHIPS transaction)
```

## Communication Protocols

### Inter-Node Communication
- **Pub-Sub**: Dealer ↔ Players (port 7797)
- **Push-Pull**: Dealer ↔ Players (port 7798)
- **Pub-Sub**: Cashier ↔ All (port 7901)
- **Push-Pull**: Cashier ↔ All (port 7902)

### GUI Communication
- **WebSocket**: GUI ↔ Node (port 9000)
- **HTTP**: GUI hosting (port 1234)

### Blockchain Communication
- **RPC**: Application ↔ CHIPS daemon (JSON-RPC)
- **RPC**: Application ↔ Lightning daemon (JSON-RPC, optional)

## State Management

### Game States
Defined in `poker/src/states.c`:
- Table states (empty, full, active)
- Player states (waiting, playing, disconnected)
- Game phases (shuffling, betting, showdown)

### State Transitions
- Managed by state machine in `states.c`
- Events trigger state changes
- State validation before transitions

## Security Considerations

### Mental Poker
- Cards encrypted with shared secrets
- No single party can see cards before reveal
- Cryptographic verification of all operations

### Transaction Security
- Multi-signature addresses for cashiers
- Transaction validation before processing
- Dispute resolution mechanism

### Network Security
- WebSocket encryption (WSS)
- Message authentication
- Identity verification via Verus IDs

## Configuration Management

### Configuration Files
- `poker/config/player.ini` - Player settings
- `poker/config/dealer.ini` - Dealer settings
- `poker/config/cashier.ini` - Cashier settings
- `poker/config/chips.conf` - CHIPS daemon config

### Configuration Loading
- Parsed by `poker/src/config.c`
- Uses iniparser library
- Validated on startup

## Error Handling Architecture

### Error Propagation
```
Function → Error Code → Caller → Handler → Log/Response
```

### Error Categories
1. **Gameplay Errors** (1-32): Game logic issues
2. **Blockchain Errors** (33-64): CHIPS/LN issues
3. **System Errors** (65+): Memory, network, parsing

### Error Recovery
- Transaction rollback for failed operations
- State restoration on errors
- Dispute mechanism for unresolved issues

