# Backend → GUI Message Mapping

## Overview

This document maps the C backend data to the React GUI's expected message format.

---

## Backend Data Sources

### Player State (`player.c`)
| Data | Variable | Description |
|------|----------|-------------|
| Player ID | `p_deck_info.player_id` | 0-indexed seat number |
| Hole Cards | `p_local_state.decoded_cards[0,1]` | Card values (0-51) |
| Game ID | `p_deck_info.game_id` | Unique game identifier |

### Betting State (`game.c` - from blockchain)
| Data | JSON Key | Description |
|------|----------|-------------|
| Current Turn | `current_turn` | Which player should act |
| Action | `action` | small_blind, big_blind, bet |
| Round | `round` | 0=preflop, 1=flop, 2=turn, 3=river |
| Pot | `pot` | Total pot in CHIPS |
| Min Amount | `min_amount` | Minimum to call |
| Player Funds | `player_funds[]` | Each player's remaining chips |

### Board Cards (`game.c`)
| Data | JSON Key | Description |
|------|----------|-------------|
| Flop | `flop[]` | 3 card values |
| Turn | `turn` | 1 card value |
| River | `river` | 1 card value |

---

## GUI Expected Messages

### 1. `backend_status`
```json
{
  "method": "backend_status",
  "backend_status": 1  // 0=not ready, 1=ready
}
```

### 2. `walletInfo`
```json
{
  "method": "walletInfo",
  "backend_status": 1,
  "balance": 100.5,        // Player's total CHIPS balance
  "addr": "RAddress...",   // Deposit address
  "table_stack_in_chips": 0.5,  // Buy-in amount
  "max_players": 2
}
```

### 3. `seats`
```json
{
  "method": "seats",
  "seats": [
    { "name": "Player1", "playing": 0, "seat": 0, "chips": 50, "empty": false },
    { "name": "Player2", "playing": 0, "seat": 1, "chips": 50, "empty": false }
  ]
}
```

### 4. `blindsInfo`
```json
{
  "method": "blindsInfo",
  "small_blind": 0.01,
  "big_blind": 0.02
}
```

### 5. `dealer`
```json
{
  "method": "dealer",
  "playerid": 0  // Dealer button position
}
```

### 6. `deal` (Hole Cards)
```json
{
  "method": "deal",
  "deal": {
    "holecards": ["Ah", "Kd"],  // Card strings
    "balance": 0.48
  }
}
```

### 7. `deal` (Board Cards - Flop)
```json
{
  "method": "deal",
  "deal": {
    "board": ["Qh", "Jc", "Ts"]
  }
}
```

### 8. `deal` (Board Cards - Turn/River)
```json
{
  "method": "deal",
  "deal": {
    "board": ["Qh", "Jc", "Ts", "9d"]  // Cumulative
  }
}
```

### 9. `betting` - Round Betting
```json
{
  "method": "betting",
  "action": "round_betting",
  "playerid": 0,
  "pot": 0.04,
  "toCall": 0.01,
  "minRaiseTo": 0.02,
  "possibilities": [0, 1, 2, 3, 4],  // Available actions
  "player_funds": [0.48, 0.49]
}
```

### 10. `betting` - Player Actions
```json
{
  "method": "betting",
  "action": "call",       // check, call, raise, fold, allin
  "playerid": 0,
  "bet_amount": 0.01
}
```

### 11. `finalInfo` (Showdown/Winner)
```json
{
  "method": "finalInfo",
  "winners": [0],
  "win_amount": 0.04,
  "showInfo": {
    "allHoleCardsInfo": [["Ah", "Kd"], ["Qh", "Jc"]],
    "boardCardInfo": ["Qh", "Jc", "Ts", "9d", "8h"]
  }
}
```

---

## Card Value to String Conversion

Backend uses 0-51, GUI expects "As", "Kh", etc.

```
Value = (suit * 13) + rank
Suits: 0=Clubs, 1=Diamonds, 2=Hearts, 3=Spades
Ranks: 0=2, 1=3, ... 8=T, 9=J, 10=Q, 11=K, 12=A
```

| Value | Card |
|-------|------|
| 0 | 2c |
| 12 | Ac |
| 13 | 2d |
| 25 | Ad |
| 26 | 2h |
| 38 | Ah |
| 39 | 2s |
| 51 | As |

---

## Action Possibilities (GUI Constants)

```javascript
// From GUI's lib/constants.ts
export const Possibilities = {
  fold: 0,
  call: 1,
  raise: 2,
  check: 3,
  allin: 4,
  bet: 5,
  // ...
};
```

---

## Implementation Status

### ✅ Phase 1: Backend → GUI (One-Way) - IMPLEMENTED

#### New Files Added
- `poker/src/gui.c` - GUI message building and sending
- `poker/include/gui.h` - GUI function declarations

#### Implemented Functions
```c
// Card conversion
const char *card_value_to_string(int32_t card_value);
cJSON *cards_to_json_array(int32_t *cards, int32_t count);

// Message sending
void gui_send_message(cJSON *message);

// Message builders
cJSON *gui_build_backend_status(int32_t status);
cJSON *gui_build_wallet_info(double balance, const char *addr, double stack_amount, int32_t max_players);
cJSON *gui_build_seats_info(int32_t num_players, double *chips, int32_t *connected);
cJSON *gui_build_blinds_info(double small_blind, double big_blind);
cJSON *gui_build_dealer_info(int32_t dealer_seat);
cJSON *gui_build_deal_holecards(int32_t card1, int32_t card2, double balance);
cJSON *gui_build_deal_board(int32_t *board_cards, int32_t count);
cJSON *gui_build_betting_round(int32_t playerid, double pot, double to_call, double min_raise, ...);
cJSON *gui_build_betting_action(int32_t playerid, const char *action, double bet_amount);
cJSON *gui_build_final_info(int32_t *winners, int32_t winner_count, double win_amount, ...);
```

#### Integration Points in player.c
- **Card reveal**: Sends `deal` message with holecards (after both are revealed)
- **Board cards**: Sends `deal` message with board array (flop/turn/river)
- **Betting turn**: Sends `betting` message with pot, toCall, minRaise, and player_funds

#### Logging
All GUI messages are logged with `[GUI]` prefix for debugging:
```
[src/gui.c:gui_send_message:62] [GUI] {"method":"deal","deal":{"holecards":["Ac","Kh"],"balance":0.48}}
```

### 🔄 Phase 2: GUI → Backend (Two-Way) - PENDING

Still needed:
1. Handle WebSocket incoming messages in `player.c`
2. Parse GUI action messages (fold/call/raise)
3. Call `player_write_betting_action()` from WebSocket handler
4. In GUI mode (`BET_MODE_GUI`), wait for WebSocket action instead of auto-action

### 🔄 Phase 3: GUI Updates - PENDING

Still needed:
1. Update GUI WebSocket URL to connect to backend (port 9000)
2. Test message compatibility
3. Add action sending from GUI

---

## WebSocket Configuration

Each node type uses a dedicated port (single bidirectional connection):

| Node Type | Port | Constant |
|-----------|------|----------|
| Dealer    | 9000 | `DEFAULT_DEALER_WS_PORT` |
| Player    | 9001 | `DEFAULT_PLAYER_WS_PORT` |
| Cashier   | 9002 | `DEFAULT_CASHIER_WS_PORT` |

This allows running multiple node types on the same machine without port conflicts.

## Testing GUI Mode

Run player with `--gui` flag:
```bash
./bin/bet player config/verus_player_p1.ini --gui
```

This enables:
- WebSocket server on port 9001 (player-specific)
- GUI message sending via WebSocket
- Waiting for GUI action during betting (instead of auto-action)
- Logging all messages with `[GUI]` prefix

### Connection from GUI
```javascript
// Connect to player backend
const playerWs = new WebSocket('ws://localhost:9001');

// Connect to dealer backend  
const dealerWs = new WebSocket('ws://localhost:9000');
```

---

## Files Modified

### Backend (C)
- `poker/src/gui.c` - NEW: GUI message building and sending
- `poker/include/gui.h` - NEW: GUI function declarations
- `poker/src/player.c` - Added GUI message integration
- `poker/src/bet.c` - Added `--gui` command line flag
- `poker/Makefile` - Added gui.c to build

### GUI (React/TypeScript) - TO BE MODIFIED
- `src/components/Game/onMessage.ts` - Adjust message handling
- `src/config/` - Backend URL configuration
- `src/hooks/` - WebSocket connection setup

