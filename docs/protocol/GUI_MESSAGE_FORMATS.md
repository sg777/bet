# GUI Message Formats Documentation

This document provides a comprehensive reference for all JSON message formats used in communication between the Pangea-Bet backend and the GUI frontend via WebSocket.

## Table of Contents

1. [Connection Details](#connection-details)
2. [Common Wallet Messages](#common-wallet-messages)
3. [Dealer Node Messages](#dealer-node-messages)
4. [Player Node Messages](#player-node-messages)
5. [Cashier Node Messages](#cashier-node-messages)
6. [Message Flow Diagrams](#message-flow-diagrams)

---

## Connection Details

### WebSocket Connection
- **Port**: `9000` (default, defined as `gui_ws_port` in `common.h`)
- **Protocol**: WebSocket (ws:// or wss://)
- **Connection URL**: `ws://<backend_ip>:9000`

### Node Types
- **Dealer (DCV)**: Hosts tables, manages game flow
- **Player**: Joins tables, plays games
- **Cashier**: Handles wallet operations (can also act as BVV)

---

## Common Wallet Messages

These messages are supported by **all node types** (Dealer, Player, Cashier).

### Get Balance Info

**Request:**
```json
{
  "method": "get_bal_info"
}
```

**Response:**
```json
{
  "method": "bal_info",
  "chips_bal": 123.45678900
}
```

**Fields:**
- `method` (string, required): Always `"bal_info"` in response
- `chips_bal` (number, required): Current CHIPS balance in the wallet

---

### Get Address Info

**Request:**
```json
{
  "method": "get_addr_info"
}
```

**Response:**
```json
{
  "method": "addr_info",
  "chips_addr": "Rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

**Fields:**
- `method` (string, required): Always `"addr_info"` in response
- `chips_addr` (string, required): New CHIPS address generated for receiving funds

---

### Withdraw Request

**Request:**
```json
{
  "method": "withdrawRequest"
}
```

**Response:**
```json
{
  "method": "withdrawResponse",
  "balance": 123.45678900,
  "tx_fee": 0.00010000,
  "addrs": [
    {
      "address": "Rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
      "label": "default"
    }
  ]
}
```

**Fields:**
- `method` (string, required): Always `"withdrawResponse"` in response
- `balance` (number, required): Current wallet balance
- `tx_fee` (number, required): Transaction fee required for withdrawal
- `addrs` (array, required): List of addresses owned by the wallet that can receive withdrawals

---

### Withdraw

**Request:**
```json
{
  "method": "withdraw",
  "amount": 10.5,
  "addr": "Rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

**Response:**
```json
{
  "method": "withdrawInfo",
  "tx": {
    "txid": "abc123...",
    "hex": "01000000...",
    "fee": 0.00010000
  }
}
```

**Fields:**
- `method` (string, required): Always `"withdrawInfo"` in response
- `amount` (number, required): Amount to withdraw (must be > 0)
- `addr` (string, required): Address to send funds to (must be owned by wallet)
- `tx` (object, required): Transaction details including `txid`, `hex`, and `fee`

**Note**: Withdrawals are restricted to addresses owned by the backend wallet for security.

---

## Dealer Node Messages

### Game Info

**Request:**
```json
{
  "method": "game"
}
```

**Response:**
```json
{
  "method": "game",
  "game": {
    "tocall": 0,
    "seats": 2,
    "pot": [0],
    "gametype": "NL Hold'em<br>Blinds: 3/6"
  }
}
```

**Fields:**
- `tocall` (number, optional): Amount to call
- `seats` (number, required): Number of seats at the table
- `pot` (array, required): Array of pot values (main pot and side pots)
- `gametype` (string, required): Game type description (e.g., "NL Hold'em<br>Blinds: 3/6")

---

### Seats Info

**Request:**
```json
{
  "method": "seats"
}
```

**Response:**
```json
{
  "method": "seats",
  "seats": [
    {
      "playerid": 0,
      "name": "Player1",
      "chips": 1000,
      "status": "active"
    },
    {
      "playerid": 1,
      "name": "Player2",
      "chips": 1000,
      "status": "active"
    }
  ]
}
```

**Fields:**
- `seats` (array, required): Array of seat information
  - `playerid` (number, required): Player ID (0-based)
  - `name` (string, required): Player name/identifier
  - `chips` (number, required): Player's chip count
  - `status` (string, required): Player status (e.g., "active", "folded", "allin")

---

### Chat

**Request:**
```json
{
  "method": "chat",
  "message": "Hello everyone!"
}
```

**Response:** (Echoed back or broadcast to all players)

**Fields:**
- `message` (string, required): Chat message text

---

### Reset

**Request:**
```json
{
  "method": "reset"
}
```

**Response:** (No specific response, game state is reset)

**Note**: Resets all dealer control variables and game state.

---

## Player Node Messages

### Backend Status

**Request:**
```json
{
  "method": "backend_status"
}
```

**Response:**
```json
{
  "method": "backend_status",
  "backend_status": 1
}
```

**Fields:**
- `backend_status` (number, required): Status code
  - `0` = `backend_not_ready`
  - `1` = `backend_ready`

---

### Betting

**Request:**
```json
{
  "method": "betting",
  "round": 0,
  "action": 4,
  "amount": 10.5
}
```

**Response:** (Game state updates sent via other messages)

**Fields:**
- `round` (number, required): Betting round
  - `0` = preflop
  - `1` = flop
  - `2` = turn
  - `3` = river
- `action` (number, required): Betting action
  - `1` = small_blind
  - `2` = big_blind
  - `3` = check
  - `4` = call
  - `5` = raise
  - `6` = allin
  - `7` = fold
- `amount` (number, required): Bet amount (for raise/call actions)

---

### Player Join

**Request:**
```json
{
  "method": "player_join",
  "table_id": "abc123...",
  "player_id": "player_verus_id@"
}
```

**Response:**
```json
{
  "method": "join_info",
  "playerid": 0,
  "table_id": "abc123...",
  "status": "joined"
}
```

**Fields:**
- `table_id` (string, required): Table identifier
- `player_id` (string, required): Player Verus ID
- `playerid` (number, required): Assigned player ID (0-based)
- `status` (string, required): Join status

---

### Reset

**Request:**
```json
{
  "method": "reset"
}
```

**Response:** (Game state reset)

---

### Sit Out

**Request:**
```json
{
  "method": "sitout",
  "value": 1
}
```

**Response:** (No specific response)

**Fields:**
- `value` (number, required): `1` to sit out, `0` to sit in

---

### Wallet Info

**Request:**
```json
{
  "method": "walletInfo"
}
```

**Response:**
```json
{
  "method": "walletInfo",
  "addr": "Rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
  "balance": 123.45678900,
  "backend_status": 1,
  "max_players": 9,
  "table_stack_in_chips": 200,
  "table_id": "abc123...",
  "tx_fee": 0.00010000
}
```

**Fields:**
- `addr` (string, required): Wallet address
- `balance` (number, required): Current balance
- `backend_status` (number, required): Backend status (0 or 1)
- `max_players` (number, required): Maximum players at table
- `table_stack_in_chips` (number, required): Table stack size in chips
- `table_id` (string, required): Current table ID
- `tx_fee` (number, required): Transaction fee

---

### Warning

**Sent from backend (unsolicited):**
```json
{
  "method": "warning",
  "warning_num": 0
}
```

**Fields:**
- `warning_num` (number, required): Warning code
  - `0` = `seat_already_taken`
  - `1` = `insufficient_funds`
  - `2` = `table_is_full`

---

### Game State Messages (Sent from Backend)

#### Init Deck
```json
{
  "method": "init_d",
  "deckid": "abc123...",
  "cardprods": [...],
  "dcvblindcards": [...]
}
```

#### Init Player
```json
{
  "method": "init",
  "playerid": 0,
  "deckid": "abc123...",
  "cardprods": [...]
}
```

#### Join Response
```json
{
  "method": "join_res",
  "playerid": 0,
  "status": "joined"
}
```

#### Turn Info
```json
{
  "method": "turn",
  "playerid": 0,
  "cardid": 5,
  "card_type": 1
}
```

#### Game Info
```json
{
  "method": "game_info",
  "round": 0,
  "pot": 100,
  "tocall": 10
}
```

#### Final Info
```json
{
  "method": "finalInfo",
  "winners": [0, 1],
  "payouts": [50, 50]
}
```

---

## Cashier Node Messages

Cashier nodes support **only the common wallet messages** listed above:
- `get_bal_info`
- `get_addr_info`
- `withdrawRequest`
- `withdraw`

Cashier nodes do not have game-specific functionality in the GUI.

---

## Message Flow Diagrams

### Dealer Initialization Flow

```
GUI → Backend: {"method": "game"}
Backend → GUI: {"method": "game", "game": {...}}

GUI → Backend: {"method": "seats"}
Backend → GUI: {"method": "seats", "seats": [...]}
```

### Player Join Flow

```
GUI → Backend: {"method": "player_join", ...}
Backend → GUI: {"method": "join_info", ...}

GUI → Backend: {"method": "walletInfo"}
Backend → GUI: {"method": "walletInfo", ...}
```

### Betting Flow

```
GUI → Backend: {"method": "betting", "round": 0, "action": 4, "amount": 10}
Backend → GUI: {"method": "game_info", "round": 0, "pot": 100, ...}
Backend → GUI: {"method": "turn", "playerid": 0, ...}
```

### Wallet Operations Flow

```
GUI → Backend: {"method": "get_bal_info"}
Backend → GUI: {"method": "bal_info", "chips_bal": 123.45}

GUI → Backend: {"method": "withdrawRequest"}
Backend → GUI: {"method": "withdrawResponse", "balance": 123.45, "addrs": [...]}

GUI → Backend: {"method": "withdraw", "amount": 10, "addr": "Rxxx..."}
Backend → GUI: {"method": "withdrawInfo", "tx": {...}}
```

---

## Error Handling

### Invalid Method
If an unknown method is sent, the backend will log a warning but may not send an error response. The GUI should handle timeouts appropriately.

### Invalid Parameters
If required parameters are missing or invalid:
- Withdraw operations return `null` response
- Other operations may return error messages or fail silently

### Connection Status
- Monitor WebSocket connection status
- Handle reconnection logic
- Backend may send `warning` messages for various conditions

---

## Implementation Notes

### WebSocket Write Functions
- **Dealer**: Uses `bet_dcv_lws_write()` → `lws_write()`
- **Player**: Uses `player_lws_write()` → WebSocket write
- **Cashier**: Uses `lws_write()` directly

### Common Wallet Handler
All wallet operations use common functions from `payment.c`:
- `bet_wallet_get_bal_info()`
- `bet_wallet_get_addr_info()`
- `bet_wallet_withdraw_request()`
- `bet_wallet_withdraw()`

### Node-Specific Handlers
- **Dealer**: `bet_dcv_frontend()` in `host.c`
- **Player**: `bet_player_frontend()` in `client.c`
- **Cashier**: `bet_cashier_frontend()` in `cashier.c`

---

## Version Information

- **Backend Version**: Current (Verus-only architecture)
- **Lightning Network**: Removed (CHIPS-only payments)
- **Pub-Sub Communication**: Removed (Verus ID-based communication)
- **Last Updated**: 2024

---

## Additional Resources

- [Protocol Documentation](./README.md)
- [Game Flow](./method_game.md)
- [Seats Management](./method_seats.md)
- [Transaction Types](./tx_types.md)

