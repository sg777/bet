# GUI Join Approval - Wallet Info Access Fix

## Problem

After implementing GUI join approval, the backend was getting stuck when the GUI connected:

```
[src/client.c:lws_callback_http_player:1059] LWS_CALLBACK_ESTABLISHED
[src/client.c:bet_gui_init_message:946] Backend is not yet ready, it takes a while please wait...
[src/client.c:player_lws_write:131] Backend is not ready to write data to the GUI
```

**Root Cause:**
1. Backend waits for `join_table` command before calling `handle_verus_player()`
2. `backend_status` remains `backend_not_ready` until `handle_verus_player()` runs
3. `player_lws_write()` refuses to send ANY messages when `backend_status == backend_not_ready`
4. GUI can't get `walletInfo` to show user their balance
5. User can't make informed decision to join
6. **Deadlock!** GUI waiting for info, backend waiting for join command

## Solution

Allow "safe" pre-join messages to be sent even when backend is not fully ready:

### 1. Modified `player_lws_write()` 

**Before:**
```c
void player_lws_write(cJSON *data)
{
    if (backend_status == backend_ready) {
        // Send message
    } else {
        dlg_warn("Backend is not ready to write data to the GUI");
    }
}
```

**After:**
```c
void player_lws_write(cJSON *data)
{
    // Check if this is a "safe" message that can be sent before backend is fully ready
    bool is_safe_message = false;
    const char *method = jstr(data, "method");
    if (method != NULL) {
        if (strcmp(method, "walletInfo") == 0 ||
            strcmp(method, "backend_status") == 0 ||
            strcmp(method, "join_ack") == 0 ||
            strcmp(method, "bal_info") == 0) {
            is_safe_message = true;
        }
    }
    
    if (backend_status == backend_ready || is_safe_message) {
        // Send message (backend ready OR safe message)
    } else {
        dlg_warn("Backend is not ready to write data to the GUI");
    }
}
```

### 2. Updated `bet_gui_init_message()`

**Before:**
```c
if (backend_status == backend_not_ready) {
    warning_info = cJSON_CreateObject();
    cJSON_AddStringToObject(warning_info, "method", "warning");
    cJSON_AddNumberToObject(warning_info, "warning_num", backend_not_ready);
    dlg_warn("Backend is not yet ready, it takes a while please wait...");
    player_lws_write(warning_info);
}
```

**After:**
```c
if (backend_status == backend_not_ready) {
    // Send friendly status message
    init_info = cJSON_CreateObject();
    cJSON_AddStringToObject(init_info, "method", "backend_status");
    cJSON_AddNumberToObject(init_info, "backend_status", backend_not_ready);
    cJSON_AddStringToObject(init_info, "message", "Connected! Request wallet info and click 'Join Table' to start.");
    dlg_info("GUI connected. Waiting for user to request wallet info and approve join.");
    player_lws_write(init_info);
}
```

## Safe Messages

These messages can be sent **before** join approval:

1. **`walletInfo`** - User needs to see balance before joining
2. **`backend_status`** - GUI needs to know connection status
3. **`join_ack`** - Confirm join approval was received
4. **`bal_info`** - Balance queries for wallet

## Flow Now Works Correctly

```
Player starts with --gui
  ↓
WebSocket: port 9001 ✅
  ↓
Main thread: WAITING for join approval ⏳
  ↓
GUI connects
  ↓
Backend sends: "Connected! Request wallet info..."
  ↓
GUI requests: { "method": "walletInfo" }
  ↓
Backend responds: { "balance": 132.56, "table_stack": 50 } ✅
  ↓
User sees balance and decides to join 👀
  ↓
User clicks "Join Table" button 👆
  ↓
GUI sends: { "method": "join_table" }
  ↓
Backend receives approval ✅
  ↓
Main thread wakes up
  ↓
handle_verus_player() runs
  ↓
payin_tx happens (0.5 CHIPS spent)
  ↓
Game proceeds normally ✅
```

## Testing

### Test the Fix

```bash
# Terminal 1: Start player in GUI mode
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui

# Expected:
# "Waiting for GUI to connect and send join command..."

# Terminal 2: Run simulator
cd /root/bet/tools
node gui_simulator.js 9001 check

# Expected sequence:
# ✅ Connected to backend!
# → backend_status (receives status)
# → walletInfo (receives wallet info!) 
# → join_table (auto-join after 2 seconds)
# ✅ Join Approved: Joining table...
# (game proceeds)
```

### Expected Simulator Output

```
╔════════════════════════════════════════════════════════════════════╗
║                    GUI SIMULATOR                                   ║
╚════════════════════════════════════════════════════════════════════╝

Backend URL:    ws://localhost:9001
Action Mode:    check
Node Type:      Player

✅ Connected to backend!
   Connection time: 5:23:15 AM

📤 Sending initial requests...
   → backend_status

🔄 Monitoring for game messages...
   (Will auto-respond to betting prompts)

──────────────────────────────────────────────────────────────────────

[5:23:15 AM] 📊 Backend Status: 0
[5:23:15 AM] 👛 Wallet: 132.56 CHIPS, Table: 50       ← WALLET INFO RECEIVED!
   → walletInfo
   → get_bal_info
   → join_table (auto-join for testing)
[5:23:17 AM] ✅ Join Approved: Joining table...       ← JOIN ACKNOWLEDGED!
```

### Backend Logs Should Show

```
[src/client.c:lws_callback_http_player:1072] LWS_CALLBACK_ESTABLISHED
[src/client.c:bet_gui_init_message:960] GUI connected. Waiting for user to request wallet info and approve join.
[src/client.c:bet_player_frontend:878] Recv from GUI :: {"method":"backend_status"}
[src/client.c:bet_player_frontend:904] Recv from GUI :: {"method":"walletInfo"}
[src/client.c:bet_player_wallet_info:853] {"method":"walletInfo","addr":"RA7fucUzBCZyCGTd5wppUQ9SGzBgbmDqk7","balance":132.5598,"backend_status":0,"max_players":2,"table_stack_in_chips":50,"table_id":"","tx_fee":0.0001}
[src/client.c:bet_player_frontend:885] Recv from GUI :: {"method":"get_bal_info"}
[src/client.c:bet_player_frontend:909] Recv from GUI :: {"method":"join_table"}
[src/client.c:bet_player_frontend:911] Received join_table command from GUI
[src/bet.c:473] GUI join approved! Proceeding with table join...
```

## Key Changes Summary

| Aspect | Before | After |
|--------|--------|-------|
| Wallet queries | ❌ Blocked until join | ✅ Allowed before join |
| User sees balance | ❌ No | ✅ Yes, before joining |
| Join decision | ❌ Blind | ✅ Informed |
| Message blocking | ❌ All blocked | ✅ Safe messages allowed |
| Init message | ⚠️ "Not ready" warning | ✅ Friendly status |

## Benefits

1. ✅ **User can see wallet info** before committing funds
2. ✅ **No deadlock** - communication works both ways
3. ✅ **Informed decision** - user sees balance and cost
4. ✅ **Better UX** - clear status messages
5. ✅ **Safe** - only wallet queries allowed, no game actions

## What's Still Protected

Even with this fix, the following still require full backend ready status:
- Game actions (betting, folding, raising)
- Card dealing
- Player joining (actual blockchain payin)
- Game state changes

Only **read-only wallet queries** are allowed before join approval.

## Conclusion

The fix allows the GUI to query wallet information before the user approves joining, enabling an informed decision while maintaining security and game state integrity. This resolves the deadlock and creates a proper interactive flow for GUI mode.

