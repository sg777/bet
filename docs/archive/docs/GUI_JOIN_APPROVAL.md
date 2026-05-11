# GUI Join Approval Mechanism

## Overview

Implemented a proper GUI join approval mechanism where the backend **waits for explicit user confirmation** before spending funds and joining a table when started in `--gui` mode.

## Problem Solved

### Before (Bad Behavior)
```
./bin/bet player config/verus_player_p1.ini --gui
  ↓
WebSocket starts on port 9001
  ↓
Backend IMMEDIATELY joins table (spends 0.5 CHIPS!)
  ↓
GUI connects later... (too late, money already spent)
```

**Issues:**
- ❌ User has no control over joining
- ❌ Money spent before GUI even connects
- ❌ GUI misses join/payin events
- ❌ Not truly interactive

### After (Good Behavior)
```
./bin/bet player config/verus_player_p1.ini --gui
  ↓
WebSocket starts on port 9001
  ↓
Backend WAITS for GUI join command
  ↓
GUI connects → requests walletInfo
  ↓
GUI shows: Balance: 132.56 CHIPS, Table: 50 CHIPS
  ↓
User clicks "Join Table" button
  ↓
GUI sends { "method": "join_table" }
  ↓
Backend receives approval → proceeds with payin_tx
  ↓
User watches entire game flow from start
```

**Benefits:**
- ✅ User sees balance before joining
- ✅ Explicit confirmation required
- ✅ GUI receives all events from start
- ✅ Safe and interactive experience

## Implementation Details

### 1. Global State Variables

**Location:** `poker/src/bet.c`

```c
// GUI join approval mechanism
bool gui_join_approved = false;                    // Flag: has GUI approved?
pthread_mutex_t gui_join_mutex = PTHREAD_MUTEX_INITIALIZER;   // Thread safety
pthread_cond_t gui_join_cond = PTHREAD_COND_INITIALIZER;      // Signaling
```

**Declared in:** `poker/include/common.h`

### 2. Main Thread Waiting Logic

**Location:** `poker/src/bet.c` - Player startup

```c
if (g_betting_mode == BET_MODE_GUI) {
    // Start WebSocket thread
    pthread_t ws_thread;
    OS_thread_create(&ws_thread, NULL, (void *)bet_player_frontend_loop, NULL);
    sleep(1);  // Let WebSocket server start
    
    dlg_info("Waiting for GUI to connect and send join command...");
    dlg_info("GUI should send { \"method\": \"join_table\" } to proceed");
    
    // WAIT for GUI approval
    pthread_mutex_lock(&gui_join_mutex);
    while (!gui_join_approved) {
        pthread_cond_wait(&gui_join_cond, &gui_join_mutex);  // Block here
    }
    pthread_mutex_unlock(&gui_join_mutex);
    
    dlg_info("GUI join approved! Proceeding with table join...");
    
    // NOW proceed with join
    retval = handle_verus_player();
    
    // Keep WebSocket alive
    pthread_join(ws_thread, NULL);
}
```

### 3. WebSocket Message Handler

**Location:** `poker/src/client.c` - `bet_player_frontend()`

```c
cases("join_table")
    // GUI approved joining table
    dlg_info("Received join_table command from GUI");
    
    // Signal main thread
    pthread_mutex_lock(&gui_join_mutex);
    gui_join_approved = true;
    pthread_cond_signal(&gui_join_cond);  // Wake up main thread
    pthread_mutex_unlock(&gui_join_mutex);
    
    // Send acknowledgment to GUI
    cJSON *join_ack = cJSON_CreateObject();
    cJSON_AddStringToObject(join_ack, "method", "join_ack");
    cJSON_AddStringToObject(join_ack, "status", "approved");
    cJSON_AddStringToObject(join_ack, "message", "Joining table...");
    player_lws_write(join_ack);
    cJSON_Delete(join_ack);
    break;
```

## Message Flow

### GUI → Backend: Join Request
```json
{
  "method": "join_table"
}
```

**When to send:**
- After user clicks "Join Table" button
- After GUI has displayed wallet info and table details
- Only in GUI mode (`--gui`)

### Backend → GUI: Join Acknowledgment
```json
{
  "method": "join_ack",
  "status": "approved",
  "message": "Joining table..."
}
```

**What it means:**
- Backend has received join approval
- payin_tx will now be executed
- Game will proceed

## Usage

### For Real GUI (React/Electron)

**1. Startup Sequence:**
```javascript
// Connect to WebSocket
const ws = new WebSocket('ws://localhost:9001');

ws.onopen = () => {
  // Request wallet info first
  ws.send(JSON.stringify({ method: 'backend_status' }));
  ws.send(JSON.stringify({ method: 'walletInfo' }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  
  if (data.method === 'walletInfo') {
    // Display to user:
    // "Balance: 132.56 CHIPS"
    // "Table Cost: 50 CHIPS"
    // [Join Table Button]
  }
  
  if (data.method === 'join_ack') {
    // Show: "Joining table, waiting for blockchain confirmation..."
  }
};
```

**2. Join Button Handler:**
```javascript
function onJoinTableClick() {
  // User clicked "Join Table" button
  ws.send(JSON.stringify({ method: 'join_table' }));
  
  // Update UI: "Sending join request..."
}
```

### For GUI Simulator (Automated Testing)

**Location:** `tools/gui_simulator.js`

```javascript
// Auto-join after 2 seconds (for testing)
setTimeout(() => {
  ws.send(JSON.stringify({ method: 'join_table' }));
  console.log('   → join_table (auto-join for testing)');
}, 2000);
```

**Why auto-join in simulator?**
- Allows automated testing without manual intervention
- Real GUI waits for user button click
- Simulator mimics user clicking join after seeing wallet info

## Testing

### Test 1: Manual GUI Mode
```bash
# Terminal 1: Start player in GUI mode
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui

# Expected output:
# Starting GUI WebSocket server on port 9001...
# Player node started. WebSocket server listening on port 9001
# Waiting for GUI to connect and send join command...
# GUI should send { "method": "join_table" } to proceed

# Terminal 2: Connect simulator
cd /root/bet/tools
node gui_simulator.js 9001 random

# Expected output in Terminal 1:
# Received join_table command from GUI
# GUI join approved! Proceeding with table join...
# (payin_tx now happens)
```

### Test 2: Non-GUI Modes (Should work as before)
```bash
# Auto mode - joins immediately
./bin/bet player config/verus_player_p1.ini --auto

# CLI mode - joins immediately, prompts for actions
./bin/bet player config/verus_player_p1.ini --cli
```

## Thread Safety

### Why Mutex + Condition Variable?

**Problem:** Two threads need to coordinate:
- **Main thread:** Waits for approval
- **WebSocket thread:** Receives approval from GUI

**Solution:**
- **Mutex:** Protects `gui_join_approved` flag from race conditions
- **Condition Variable:** Efficient waiting (no busy-loop polling)

### Execution Flow

```
Main Thread                    WebSocket Thread
     |                              |
     | pthread_cond_wait()          |
     | (blocks, releases mutex)     |
     |                              | Receives "join_table"
     |                              | pthread_mutex_lock()
     |                              | gui_join_approved = true
     |                              | pthread_cond_signal()
     | (wakes up)                   | pthread_mutex_unlock()
     | pthread_mutex_lock()         |
     | checks gui_join_approved     |
     | (true, exits loop)           |
     | pthread_mutex_unlock()       |
     | Proceeds with join           |
```

## Benefits

### 1. User Safety
- No funds spent without confirmation
- User sees balance before committing
- Transparent process

### 2. Better UX
- GUI receives all events from start
- User watches entire game flow
- No missed notifications

### 3. Flexibility
- Auto mode: unchanged (immediate join)
- CLI mode: unchanged (immediate join)
- GUI mode: wait for user confirmation

### 4. Testing
- Simulator can auto-join for automated tests
- Real GUI waits for user interaction
- Both use same mechanism

## Future Enhancements

### Possible Additions

1. **Join with Custom Amount:**
   ```json
   {
     "method": "join_table",
     "amount": 0.75
   }
   ```

2. **Cancel Join:**
   ```json
   {
     "method": "cancel_join"
   }
   ```
   - User closes GUI before joining
   - Backend exits gracefully

3. **Table Selection:**
   ```json
   {
     "method": "join_table",
     "table_id": "t2"
   }
   ```
   - Multiple tables available
   - User selects which to join

4. **Join Timeout:**
   - If GUI doesn't send join within 5 minutes
   - Auto-exit to free resources

## Summary

**Implementation Strategy:** Global flag + condition variable for thread-safe signaling

**Core Components:**
1. Global state variables (flag, mutex, cond var)
2. Main thread waits on condition variable
3. WebSocket thread receives message and signals
4. Clean, safe, POSIX-compliant threading

**Result:**
- ✅ Safe fund management
- ✅ Interactive user experience
- ✅ All events visible to GUI
- ✅ Backwards compatible with auto/CLI modes
- ✅ Easy to test with simulator
- ✅ Thread-safe implementation

This is the **proper way** to implement GUI interaction for a financial application where user confirmation is critical!

