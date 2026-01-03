# Player GUI WebSocket Protocol

This document describes the JSON messages sent from the player backend to the GUI over WebSocket.

## Connection

- **Port**: Configured in player INI file (default: `9001` for player 1, `9002` for player 2)
- **Protocol**: WebSocket (ws://)

---

## Messages from Backend to GUI

### 1. `backend_status`

Sent when GUI first connects via WebSocket.

```json
{
    "method": "backend_status",
    "backend_status": 1,
    "message": "Backend ready!"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"backend_status"` |
| `backend_status` | number | `1` = ready, `0` = not ready |
| `message` | string | Human-readable status message |

**GUI Action**: If `backend_status == 1`, GUI can proceed to request table info.

---

### 2. `table_info`

Response to GUI's `table_info` request. Provides player wallet info and configured table details.

```json
{
    "method": "table_info",
    "addr": "RA7fucUzBCZyCGTd5wppUQ9SGzBgbmDqk7",
    "balance": 132.52420000,
    "backend_status": 1,
    "max_players": 2,
    "table_stack_in_chips": 50,
    "table_id": "t1",
    "dealer_id": "d1",
    "occupied_seats": [
        {"seat": 0, "player_id": "p1"},
        {"seat": 1, "player_id": "p2"}
    ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"table_info"` |
| `addr` | string | Player's CHIPS wallet address |
| `balance` | number | Current CHIPS balance |
| `backend_status` | number | `1` = ready, `0` = not ready |
| `max_players` | number | Maximum players allowed at table |
| `table_stack_in_chips` | number | Table stake in CHIPS |
| `table_id` | string | Configured table ID from INI file |
| `dealer_id` | string | Configured dealer ID from INI file |
| `occupied_seats` | array | Array of occupied seat objects |

**Occupied Seat Object:**
| Field | Type | Description |
|-------|------|-------------|
| `seat` | number | Seat number (0-indexed) |
| `player_id` | string | Verus ID of player in that seat |

**GUI Action**: Display table info, show available seats, enable "Join Table" button.

---

### 3. `player_init_state`

Progress updates during player initialization. Sent at each stage of the join/game flow.

```json
{
    "method": "player_init_state",
    "state": 5,
    "state_name": "JOINED",
    "player_id": "p1.sg777z.chips.vrsc@"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"player_init_state"` |
| `state` | number | Numeric state code (see table below) |
| `state_name` | string | Human-readable state name |
| `player_id` | string | *(Only for JOINED state)* Player's Verus ID |

#### State Codes

All states are sent to GUI via `player_init_state` messages.

| State | Value | Sent to GUI? | Extra Fields | Description |
|-------|-------|--------------|--------------|-------------|
| `WALLET_READY` | 1 | ⚠️ Maybe* | - | Wallet + Verus ID verified |
| `TABLE_FOUND` | 2 | ✅ Yes | - | Table found on blockchain |
| `WAIT_JOIN` | 3 | ✅ Yes | - | Waiting for user to click join |
| `JOINING` | 4 | ✅ Yes | - | Payin TX executing (10-30s) |
| `JOINED` | 5 | ✅ Yes | `player_id` | Player has seat |
| `DECK_READY` | 6 | ✅ Yes | - | Deck shuffling complete |
| `IN_GAME` | 7 | ✅ Yes | - | Game loop active |

> ⚠️ **Note on WALLET_READY**: This state is sent very early, often BEFORE the GUI WebSocket connects. The GUI may not receive this message. GUI should request `table_info` after receiving `backend_status` instead of waiting for `WALLET_READY`.

#### Detailed State Descriptions

##### 1. WALLET_READY (state=1)
- **When sent**: Immediately after backend verifies wallet and Verus ID
- **GUI receives?**: Often NO - sent before WebSocket connects
- **What happens next**: Backend waits for GUI to send `table_info`
- **GUI action**: N/A (use `backend_status` message instead)

##### 2. TABLE_FOUND (state=2)
- **When sent**: After `poker_find_table()` succeeds
- **GUI receives?**: YES
- **What happens next**: Immediately followed by WAIT_JOIN
- **GUI action**: Update UI with table details

##### 3. WAIT_JOIN (state=3)  
- **When sent**: After TABLE_FOUND, before backend blocks waiting
- **GUI receives?**: YES
- **What happens next**: Backend BLOCKS waiting for `join_table` message
- **GUI action**: Show "Join Table" button, wait for user click

##### 4. JOINING (state=4)
- **When sent**: After GUI sends `join_table`, before payin TX
- **GUI receives?**: YES
- **What happens next**: Backend executes blockchain payin (10-30 seconds)
- **GUI action**: Show loading spinner "Processing payin transaction..."

##### 5. JOINED (state=5)
- **When sent**: After payin TX confirmed and player added to table
- **GUI receives?**: YES
- **Extra field**: `player_id` - Player's Verus ID (use as display name)
- **What happens next**: Deck initialization begins
- **GUI action**: Show seat assignment, display player name

##### 6. DECK_READY (state=6)
- **When sent**: After player's deck shuffling data stored on blockchain
- **GUI receives?**: YES
- **What happens next**: Waiting for other players/dealer to complete
- **GUI action**: Show "Waiting for other players..."

##### 7. IN_GAME (state=7)
- **When sent**: When all players ready and game loop starts
- **GUI receives?**: YES
- **What happens next**: Game events (cards, betting, etc.)
- **GUI action**: Transition to poker table UI

#### State Flow Diagram

```
[Backend starts]
      │
      ▼
WALLET_READY (1) ──── Sent to GUI (may miss if GUI not connected yet)
      │
      ▼
[Backend WAITS for GUI "table_info" request]
      │
      ▼ [GUI sends table_info]
      │
TABLE_FOUND (2) ───── Sent to GUI ✓
      │
      ▼
WAIT_JOIN (3) ─────── Sent to GUI ✓
      │
      ▼
[Backend WAITS for GUI "join_table" request]
      │
      ▼ [GUI sends join_table]
      │
JOINING (4) ───────── Sent to GUI ✓
      │
      ▼ [Payin TX executing... 10-30 seconds]
      │
JOINED (5) ────────── Sent to GUI ✓ (includes player_id)
      │
      ▼ [Deck initialization...]
      │
DECK_READY (6) ────── Sent to GUI ✓
      │
      ▼ [Waiting for all players...]
      │
IN_GAME (7) ───────── Sent to GUI ✓
      │
      ▼
[Game loop - betting, cards, etc.]
```

---

### 4. `join_ack`

Immediate acknowledgment when GUI sends `join_table` request.

```json
{
    "method": "join_ack",
    "status": "approved",
    "message": "Join approved, proceeding with payin transaction"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"join_ack"` |
| `status` | string | `"approved"` or `"rejected"` |
| `message` | string | Human-readable status message |

**GUI Action**: Show confirmation, transition to loading state while waiting for `JOINING` → `JOINED`.

---

### 5. `error`

Sent when an error occurs during initialization.

```json
{
    "method": "error",
    "error": "No tables are available",
    "message": "Failed to find table"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"error"` |
| `error` | string | Error code/description |
| `message` | string | Human-readable error context |

**GUI Action**: Display error message to user, optionally offer retry.

---

## Messages from GUI to Backend

### 1. `table_info` (Request)

Request current table information. Triggers backend to find table.

```json
{
    "method": "table_info"
}
```

**Response**: Backend sends `table_info` response (see above).

---

### 2. `join_table`

Request to join the table. Only valid when backend is in `WAIT_JOIN` state.

```json
{
    "method": "join_table",
    "seat": 0
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"join_table"` |
| `seat` | number | *(Optional)* Preferred seat number |

**Response**: Backend sends `join_ack`, then progresses through states.

---

### 3. `backend_status` (Request)

Request current backend status.

```json
{
    "method": "backend_status"
}
```

**Response**: Backend sends `backend_status` response.

---

### 4. `betting`

Submit a betting action during gameplay.

```json
{
    "method": "betting",
    "action": "raise",
    "amount": 0.01
}
```

| Field | Type | Description |
|-------|------|-------------|
| `method` | string | Always `"betting"` |
| `action` | string | One of: `"fold"`, `"check"`, `"call"`, `"raise"`, `"allin"` |
| `amount` | number | *(For raise only)* Amount to raise |

---

## Example Session Flow

```
1. GUI connects to ws://localhost:9001
2. Backend → GUI: backend_status {backend_status: 1}
3. GUI → Backend: {method: "table_info"}
4. Backend → GUI: table_info {table_id: "t1", ...}
5. Backend → GUI: player_init_state {state: 2, state_name: "TABLE_FOUND"}
6. Backend → GUI: player_init_state {state: 3, state_name: "WAIT_JOIN"}
7. [User clicks "Join Table" button]
8. GUI → Backend: {method: "join_table", seat: 0}
9. Backend → GUI: join_ack {status: "approved"}
10. Backend → GUI: player_init_state {state: 4, state_name: "JOINING"}
11. [~10-30 seconds for blockchain transaction]
12. Backend → GUI: player_init_state {state: 5, state_name: "JOINED", player_id: "p1@"}
13. Backend → GUI: player_init_state {state: 6, state_name: "DECK_READY"}
14. Backend → GUI: player_init_state {state: 7, state_name: "IN_GAME"}
15. [Game loop - betting actions, card reveals, etc.]
```

---

## Notes

- All messages are JSON objects sent as WebSocket text frames
- The backend uses libwebsockets for WebSocket communication
- Messages are logged with `[► TO GUI]` prefix in backend logs
- Incoming messages are logged with `[◄ FROM GUI]` prefix

