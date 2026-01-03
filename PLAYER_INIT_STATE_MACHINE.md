# Player Initialization State Machine

## Overview
Implemented a proper state machine for player initialization flow to provide better visibility to the GUI and maintain clean separation between CLI/auto mode and GUI mode.

## State Definitions

```c
// Player initialization states (before entering game loop)
#define P_INIT_WALLET_READY  1  // Wallet + ID verified (can read blockchain)
#define P_INIT_TABLE_FOUND   2  // Found table (have table info)
#define P_INIT_WAIT_JOIN     3  // Waiting for user approval (GUI only)
#define P_INIT_JOINING       4  // Executing payin_tx (blocking)
#define P_INIT_JOINED        5  // Successfully joined (have seat)
#define P_INIT_DECK_READY    6  // Deck loaded/initialized
#define P_INIT_IN_GAME       7  // In game loop
```

## State Flow

### CLI/Auto Mode (Sequential, No Pauses)
```
START
  ↓
bet_parse_verus_player() → P_INIT_WALLET_READY (1)
  ↓
poker_find_table() → P_INIT_TABLE_FOUND (2)
  ↓
poker_join_table() → P_INIT_JOINING (4) → P_INIT_JOINED (5)
  ↓
player_init_deck() → P_INIT_DECK_READY (6)
  ↓
while(1) handle_game_state_player() → P_INIT_IN_GAME (7)
```

### GUI Mode (Future Enhancement - Can Add Pause at State 3)
```
START
  ↓
bet_parse_verus_player() → P_INIT_WALLET_READY (1)
  ↓  [sends table_info to GUI]
poker_find_table() → P_INIT_TABLE_FOUND (2)
  ↓  [GUI shows table info, available seats]
[OPTIONAL PAUSE: P_INIT_WAIT_JOIN (3)]
  ↓  [user clicks seat, sends join_table]
poker_join_table() → P_INIT_JOINING (4)
  ↓  [GUI shows "Joining table... 10-30 seconds"]
  → P_INIT_JOINED (5)
  ↓  [GUI clears notice]
player_init_deck() → P_INIT_DECK_READY (6)
  ↓
while(1) handle_game_state_player() → P_INIT_IN_GAME (7)
```

## Implementation Details

### Backend Changes

#### 1. State Definitions (`poker/include/player.h`)
- Added state constants (P_INIT_WALLET_READY through P_INIT_IN_GAME)
- Added global variable: `extern int32_t player_init_state`
- Added helper: `const char *player_init_state_str(int32_t state)`

#### 2. State Tracking (`poker/src/player.c`)
- Implemented `player_init_state_str()` to convert state numbers to readable strings
- Global variable `int32_t player_init_state = 0`

#### 3. GUI Communication (`poker/src/client.c`)
- New function: `void send_init_state_to_gui(int32_t state)`
  - Updates global `player_init_state`
  - Sends JSON message with `method: "player_init_state"`, `state`, `state_name`
  - Only sends if in GUI mode (`g_betting_mode == BET_MODE_GUI`)
  - Uses colored logging: `\033[34m[► TO GUI]\033[0m`

#### 4. State Updates in `handle_verus_player()` (`poker/src/player.c`)

**State 1 - Wallet Ready (line ~687):**
```c
if ((retval = bet_parse_verus_player()) != OK) {
    return retval;
}
send_init_state_to_gui(P_INIT_WALLET_READY);  // ← NEW
```

**State 2 - Table Found (line ~708):**
```c
dlg_info("Table found");
print_struct_table(&player_t);
send_init_state_to_gui(P_INIT_TABLE_FOUND);  // ← NEW
```

**State 4 - Joining (line ~713):**
```c
if (!already_joined) {
    send_init_state_to_gui(P_INIT_JOINING);  // ← NEW
    if ((retval = poker_join_table()) != OK) {
        return retval;
    }
```

**State 5 - Joined (line ~720):**
```c
    dlg_info("Table Joined");
    send_init_state_to_gui(P_INIT_JOINED);  // ← NEW
}
```

**State 6 - Deck Ready (line ~840):**
```c
}
send_init_state_to_gui(P_INIT_DECK_READY);  // ← NEW
```

**State 7 - In Game (line ~845):**
```c
send_init_state_to_gui(P_INIT_IN_GAME);  // ← NEW
dlg_info("Player init state: %s - entering game loop", player_init_state_str(P_INIT_IN_GAME));
while (1) {
    retval = handle_game_state_player(player_config.table_id);
```

### GUI Changes

#### 1. Message Interface (`src/components/Game/onMessage.ts`)
Added to `IMessage` interface:
```typescript
state?: number;
state_name?: string;
```

#### 2. Message Handler (`src/components/Game/onMessage.ts`)
Added new case in `onMessage_player`:
```typescript
case "player_init_state":
  console.log(`[BACKEND STATE] Player init state: ${message.state} - ${message.state_name}`);
  switch (message.state) {
    case 1: // P_INIT_WALLET_READY
      console.log("  ✓ Wallet and ID ready, loading table info...");
      break;
    case 2: // P_INIT_TABLE_FOUND
      console.log("  ✓ Table found");
      break;
    case 3: // P_INIT_WAIT_JOIN
      console.log("  ⏳ Waiting for user to select seat...");
      break;
    case 4: // P_INIT_JOINING
      console.log("  📤 Joining table, executing payin transaction...");
      setNotice("Joining table... Please wait 10-30 seconds for transaction to confirm.", dispatch);
      break;
    case 5: // P_INIT_JOINED
      console.log("  ✓ Successfully joined table");
      clearNotice(dispatch);
      break;
    case 6: // P_INIT_DECK_READY
      console.log("  ✓ Deck initialized");
      break;
    case 7: // P_INIT_IN_GAME
      console.log("  ✓ Entered game loop");
      clearNotice(dispatch);
      break;
  }
  break;
```

## Key Design Principles

### 1. Sequential, Not Looped
- Init states are **one-time sequential actions**
- Only the game state (after init) loops continuously
- No need to poll init states repeatedly

### 2. Minimal Code Disruption
- States are inserted at existing checkpoints
- No refactoring of core logic
- CLI/auto mode runs unchanged (just logs states)

### 3. GUI Mode Extensibility
- Currently auto-proceeds through all states
- **State 3 (P_INIT_WAIT_JOIN)** is reserved for future GUI pause point
- To add pause:
  ```c
  send_init_state_to_gui(P_INIT_TABLE_FOUND);
  
  if (g_betting_mode == BET_MODE_GUI) {
      send_init_state_to_gui(P_INIT_WAIT_JOIN);
      pthread_cond_wait(&join_cond, &join_mutex);  // Wait for GUI
  }
  
  send_init_state_to_gui(P_INIT_JOINING);
  ```

### 4. Clear State Meanings
Each state represents a **meaningful checkpoint**:
- **WALLET_READY**: Can read blockchain data
- **TABLE_FOUND**: Have table info to display
- **WAIT_JOIN**: (Reserved) User can click seat
- **JOINING**: Transaction in progress (show spinner)
- **JOINED**: Transaction confirmed, have seat
- **DECK_READY**: Crypto keys ready
- **IN_GAME**: Game loop active

## Message Protocol

### Backend → GUI Message Format
```json
{
  "method": "player_init_state",
  "state": 4,
  "state_name": "JOINING"
}
```

### State to GUI Action Mapping
| State | State Name | GUI Action |
|-------|------------|------------|
| 1 | WALLET_READY | Log progress |
| 2 | TABLE_FOUND | Log progress |
| 3 | WAIT_JOIN | (Reserved) Show seat selection |
| 4 | JOINING | Show notice: "Joining table... 10-30 seconds" |
| 5 | JOINED | Clear notice |
| 6 | DECK_READY | Log progress |
| 7 | IN_GAME | Clear notice, ready for game |

## Testing

### Backend Logs
```bash
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui
```

Expected output:
```
Player init state: WALLET_READY
[► TO GUI] Player state: WALLET_READY
Player init state: TABLE_FOUND
[► TO GUI] Player state: TABLE_FOUND
Player init state: JOINING
[► TO GUI] Player state: JOINING
Player init state: JOINED
[► TO GUI] Player state: JOINED
Player init state: DECK_READY
[► TO GUI] Player state: DECK_READY
Player init state: IN_GAME - entering game loop
[► TO GUI] Player state: IN_GAME
```

### GUI Console Logs
```
[BACKEND STATE] Player init state: 1 - WALLET_READY
  ✓ Wallet and ID ready, loading table info...
[BACKEND STATE] Player init state: 2 - TABLE_FOUND
  ✓ Table found
[BACKEND STATE] Player init state: 4 - JOINING
  📤 Joining table, executing payin transaction...
[BACKEND STATE] Player init state: 5 - JOINED
  ✓ Successfully joined table
[BACKEND STATE] Player init state: 6 - DECK_READY
  ✓ Deck initialized
[BACKEND STATE] Player init state: 7 - IN_GAME
  ✓ Entered game loop
```

## Future Enhancements

### 1. Add GUI Approval Pause (State 3)
Modify `handle_verus_player()` after `P_INIT_TABLE_FOUND`:
```c
send_init_state_to_gui(P_INIT_TABLE_FOUND);

if (g_betting_mode == BET_MODE_GUI) {
    // Wait for user to select seat
    extern pthread_mutex_t gui_join_mutex;
    extern pthread_cond_t gui_join_cond;
    
    send_init_state_to_gui(P_INIT_WAIT_JOIN);
    dlg_info("Waiting for GUI to approve join...");
    
    pthread_mutex_lock(&gui_join_mutex);
    pthread_cond_wait(&gui_join_cond, &gui_join_mutex);
    pthread_mutex_unlock(&gui_join_mutex);
}

send_init_state_to_gui(P_INIT_JOINING);
```

GUI sends `join_table` → Backend signals condition variable → Proceeds to JOINING state.

### 2. Add Rejoin State
If player reconnects to existing game:
- Add `P_INIT_REJOINING` state
- Skip JOINING/JOINED states
- Go directly to loading deck from DB

### 3. Progress Percentage
Each state could map to percentage:
- WALLET_READY: 14%
- TABLE_FOUND: 28%
- JOINING: 42%
- JOINED: 57%
- DECK_READY: 71%
- IN_GAME: 100%

## Files Modified

### Backend
- `poker/include/player.h` - State definitions, declarations
- `poker/src/player.c` - State string function, state updates in `handle_verus_player()`
- `poker/include/gui.h` - Declaration for `send_init_state_to_gui()`
- `poker/src/client.c` - Implementation of `send_init_state_to_gui()`

### GUI
- `src/components/Game/onMessage.ts` - Message handler for `player_init_state`, interface update

## Compilation
```bash
cd /root/bet/poker && make
```
✅ Compiles without errors or warnings.

