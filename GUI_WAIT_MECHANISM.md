# GUI Wait Mechanism - Backend Synchronization

## Problem Statement
When backend starts with `--gui` flag, it was proceeding with table join (payin_tx) automatically without waiting for GUI connection or user approval. This caused:
1. Backend might execute payin_tx before GUI even connects
2. No user control over when to join
3. GUI receives updates after the fact

## Solution: Conditional Wait at P_INIT_WAIT_JOIN

### Implementation Overview
Backend now **waits** after finding table (P_INIT_TABLE_FOUND) if GUI mode is enabled. It enters P_INIT_WAIT_JOIN state and blocks until GUI sends `join_table` message.

---

## Backend Changes

### 1. Synchronization Primitives (`poker/include/common.h`)

```c
// GUI synchronization for player join approval
#include <pthread.h>
extern pthread_mutex_t gui_join_mutex;
extern pthread_cond_t gui_join_cond;
extern int gui_join_approved;  // 0 = waiting, 1 = approved
```

### 2. Variable Definitions (`poker/src/bet.c`)

```c
// GUI synchronization for player join approval
pthread_mutex_t gui_join_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gui_join_cond = PTHREAD_COND_INITIALIZER;
int gui_join_approved = 0;  // 0 = waiting, 1 = approved
```

### 3. Wait Logic (`poker/src/player.c` - `handle_verus_player()`)

**After P_INIT_TABLE_FOUND:**
```c
// State 2: Found table, have table info
send_init_state_to_gui(P_INIT_TABLE_FOUND);
dlg_info("Player init state: %s", player_init_state_str(P_INIT_TABLE_FOUND));

// If GUI mode, wait for GUI to approve join
if (g_betting_mode == BET_MODE_GUI) {
    // State 3: Waiting for GUI approval
    send_init_state_to_gui(P_INIT_WAIT_JOIN);
    dlg_info("Player init state: %s - waiting for GUI approval...", 
             player_init_state_str(P_INIT_WAIT_JOIN));
    
    // Wait for GUI to send join_table message
    pthread_mutex_lock(&gui_join_mutex);
    while (gui_join_approved == 0) {
        pthread_cond_wait(&gui_join_cond, &gui_join_mutex);
    }
    pthread_mutex_unlock(&gui_join_mutex);
    
    dlg_info("GUI join approved, proceeding to join table");
}

// Continue with joining...
```

### 4. Signal Logic (`poker/src/client.c` - `bet_player_frontend()`)

**When GUI sends `join_table`:**
```c
cases("join_table")
    dlg_info("Received join_table command from GUI");
    
    // Signal backend thread to proceed with join
    pthread_mutex_lock(&gui_join_mutex);
    gui_join_approved = 1;
    pthread_cond_signal(&gui_join_cond);
    pthread_mutex_unlock(&gui_join_mutex);
    
    // Send acknowledgment to GUI
    cJSON *join_ack = cJSON_CreateObject();
    cJSON_AddStringToObject(join_ack, "method", "join_ack");
    cJSON_AddStringToObject(join_ack, "status", "approved");
    cJSON_AddStringToObject(join_ack, "message", "Join approved, proceeding with payin transaction");
    player_lws_write(join_ack);
    cJSON_Delete(join_ack);
    break;
```

---

## GUI Changes

### 1. State Tracking (`src/store/initialState.ts`)

Added to initial state:
```typescript
playerInitState: 0,  // Tracks backend init state
```

Added to IState interface:
```typescript
interface IState {
  // ...
  backendStatus: number;
  playerInitState: number;  // NEW
  balance: number;
  // ...
}
```

### 2. Message Handler (`src/components/Game/onMessage.ts`)

```typescript
case "player_init_state":
  console.log(`[BACKEND STATE] Player init state: ${message.state} - ${message.state_name}`);
  
  // Update player init state in store
  updateStateValue("playerInitState", message.state, dispatch);
  
  // Update GUI based on state
  switch (message.state) {
    case 1: // P_INIT_WALLET_READY
      console.log("  ✓ Wallet and ID ready, loading table info...");
      break;
    case 2: // P_INIT_TABLE_FOUND
      console.log("  ✓ Table found");
      setNotice("Table found! Select a seat to join.", dispatch);
      break;
    case 3: // P_INIT_WAIT_JOIN
      console.log("  ⏳ Waiting for user to select seat...");
      setNotice("Click on an available seat to join the table", dispatch);
      break;
    case 4: // P_INIT_JOINING
      console.log("  📤 Joining table, executing payin transaction...");
      setNotice("Joining table... Please wait 10-30 seconds...", dispatch);
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

---

## Flow Comparison

### Before (Auto-join):
```
Backend starts
  ↓
bet_parse_verus_player() → P_INIT_WALLET_READY (send to GUI if connected)
  ↓
poker_find_table() → P_INIT_TABLE_FOUND (send to GUI if connected)
  ↓
poker_join_table() → P_INIT_JOINING (payin_tx executes immediately!)
  ↓
...continues...
```

**Problem:** payin_tx happens regardless of GUI state!

### After (GUI Wait):
```
Backend starts
  ↓
bet_parse_verus_player() → P_INIT_WALLET_READY
  ↓ [sends to GUI]
poker_find_table() → P_INIT_TABLE_FOUND
  ↓ [sends to GUI]
IF GUI MODE:
  P_INIT_WAIT_JOIN
  ↓ [sends to GUI]
  pthread_cond_wait() ← **BLOCKS HERE**
  ↓
  [GUI user clicks seat]
  ↓
  [GUI sends join_table]
  ↓
  pthread_cond_signal() ← **WAKES UP**
  ↓
poker_join_table() → P_INIT_JOINING (now user-approved!)
  ↓
...continues...
```

**Benefit:** Backend waits for user action!

---

## Handling GUI Connection Failures

### Scenario 1: GUI Not Connected Yet

**Symptom:** `send_init_state_to_gui()` writes fail silently.

**Current Behavior:**
- `player_lws_write()` in `client.c` returns early if no WebSocket connection
- Backend reaches `pthread_cond_wait()` and blocks
- When GUI connects later, user sees current state
- User clicks seat → backend wakes up

**Result:** ✅ Works correctly - backend waits indefinitely for GUI

### Scenario 2: GUI Disconnects During Wait

**Symptom:** Backend is blocked at `pthread_cond_wait()`, GUI dies.

**Current Behavior:**
- Backend remains blocked (no timeout)
- Manual intervention required (Ctrl+C)

**Future Enhancement:**
Use `pthread_cond_timedwait()` with timeout:
```c
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 300;  // 5 minute timeout

int rc = pthread_cond_timedwait(&gui_join_cond, &gui_join_mutex, &ts);
if (rc == ETIMEDOUT) {
    dlg_error("GUI join approval timeout after 5 minutes");
    return ERR_TIMEOUT;
}
```

---

## CLI/Auto Mode Compatibility

**CLI Mode (`--cli` or default):**
```c
if (g_betting_mode == BET_MODE_GUI) {
    // Wait logic
}
// Skipped in CLI mode!
```

**Result:** CLI/Auto mode proceeds immediately, no waiting.

---

## Testing

### Test 1: GUI Mode with Seat Selection

```bash
# Terminal 1: Start backend
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui

# Expected logs:
# Player init state: WALLET_READY
# [► TO GUI] Player state: WALLET_READY
# Player init state: TABLE_FOUND
# [► TO GUI] Player state: TABLE_FOUND
# Player init state: WAIT_JOIN - waiting for GUI approval...
# [► TO GUI] Player state: WAIT_JOIN
# [BLOCKS HERE]

# Terminal 2: Start GUI
cd /root/pangea-poker
npm start

# Browser:
# - GUI connects to ws://localhost:9001
# - Sees "Click on an available seat to join the table"
# - User clicks seat 0
# - GUI sends: {"method": "join_table", "seat": 0}

# Terminal 1 continues:
# [◄ FROM GUI] {"method":"join_table","seat":0}
# GUI join approved, proceeding to join table
# Player init state: JOINING
# [► TO GUI] Player state: JOINING
# ...payin_tx executes...
# Player init state: JOINED
# [► TO GUI] Player state: JOINED
```

### Test 2: CLI Mode (No Wait)

```bash
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini

# Expected logs (no wait):
# Player init state: WALLET_READY
# Player init state: TABLE_FOUND
# Player init state: JOINING  ← Proceeds immediately
# ...continues...
```

---

## Files Modified

### Backend
1. `poker/include/common.h` - Synchronization variable declarations
2. `poker/src/bet.c` - Variable definitions
3. `poker/src/player.c` - Wait logic at P_INIT_WAIT_JOIN
4. `poker/src/client.c` - Signal logic on `join_table` message

### GUI
1. `src/store/initialState.ts` - Added `playerInitState` to state
2. `src/components/Game/onMessage.ts` - Handler for `player_init_state`, updates notices

---

## Summary

### What Problem Was Solved?
Backend now **respects GUI mode** and waits for user approval before executing payin transaction.

### Key Mechanism
- **Condition Variable:** `pthread_cond_wait()` blocks backend thread
- **Signal:** GUI sends `join_table` → backend thread wakes up
- **Mode-Aware:** Only waits if `g_betting_mode == BET_MODE_GUI`

### User Experience
1. Backend starts, loads wallet/ID
2. Backend finds table, sends table info to GUI
3. **Backend waits** (P_INIT_WAIT_JOIN)
4. User sees table, clicks seat
5. GUI sends join_table
6. Backend wakes up, proceeds with payin_tx
7. User sees "Joining table... 10-30 seconds"

✅ User is in control!

