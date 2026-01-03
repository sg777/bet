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

| State | Value | Description | GUI Action |
|-------|-------|-------------|------------|
| `WALLET_READY` | 1 | Wallet + Verus ID verified | Show "Loading table info..." |
| `TABLE_FOUND` | 2 | Table found on blockchain | Show table details |
| `WAIT_JOIN` | 3 | Waiting for user to join | Show "Join Table" button, wait for user click |
| `JOINING` | 4 | Executing payin transaction | Show spinner: "Processing payin..." (takes 10-30s) |
| `JOINED` | 5 | Successfully joined, have seat | Show seat assignment, use `player_id` as name |
| `DECK_READY` | 6 | Deck initialized | Show "Preparing game..." |
| `IN_GAME` | 7 | In active game loop | Transition to poker table UI |

#### State Flow Diagram

```
WALLET_READY (1)
      │
      ▼ [GUI sends table_info]
TABLE_FOUND (2)
      │
      ▼
WAIT_JOIN (3) ◄── Backend waits here for GUI
      │
      ▼ [GUI sends join_table]
JOINING (4) ◄── Payin TX executing (blockchain operation)
      │
      ▼
JOINED (5) ◄── Player has seat, includes player_id
      │
      ▼
DECK_READY (6)
      │
      ▼
IN_GAME (7) ◄── Game loop active
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

