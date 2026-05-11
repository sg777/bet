# GUI Testing Guide - Join Approval Flow

## What Was Fixed

The GUI now properly implements the join approval flow:

1. **GUI connects** → Backend sends wallet info
2. **GUI receives wallet info** → Auto-sends `join_table` after 1 second
3. **Backend receives approval** → Proceeds with payin transaction
4. **GUI shows notification** → "Joining table..."

## Fresh Start (Important!)

If you're seeing "Your tx is being mined" immediately, you might have an existing game state. **Start fresh:**

### 1. Reset Backend State

```bash
# Stop all running nodes
pkill -f "bin/bet"

# Clear local database (optional, for completely fresh start)
rm -f ~/.bet/db/pangea.db

# Start dealer with --reset flag
cd /root/bet/poker
./bin/bet dealer config/verus_dealer.ini --reset

# Start cashier (in another terminal)
./bin/bet cashier config/verus_cashier.ini

# Start player in GUI mode (in another terminal)
./bin/bet player config/verus_player_p1.ini --gui
```

### 2. Expected Backend Logs

When you start the player with `--gui`, you should see:

```
[src/bet.c] Starting GUI WebSocket server on port 9001...
[src/client.c] [GUI] Player WebSocket server started successfully on port 9001
[src/bet.c] Player node started. WebSocket server listening on port 9001
[src/bet.c] Waiting for GUI to connect and send join command...
[src/bet.c] GUI should send { "method": "join_table" } to proceed
```

**Backend is now WAITING** - no payin yet!

### 3. Connect GUI

```bash
# Start GUI (in another terminal)
cd /root/pangea-poker
npm start
# Opens at http://localhost:1234
```

### 4. In Browser

1. Click "Player" tab
2. Enter IP: `localhost` (or `159.69.23.31`)
3. Enter Port: `9001`
4. Click "Set Nodes"

### 5. Expected Flow

**In Browser (Console - Press F12):**
```
Backend waiting for join approval. Auto-sending join_table...
Join approved by backend: Joining table...
```

**In Backend Terminal:**
```
[src/client.c] LWS_CALLBACK_ESTABLISHED
[src/client.c] GUI connected. Waiting for user to request wallet info and approve join.
[src/client.c] Recv from GUI :: {"method":"backend_status"}
[src/client.c] Recv from GUI :: {"method":"walletInfo"}
[src/client.c] Sending wallet info...
[src/client.c] Recv from GUI :: {"method":"join_table"}
[src/client.c] Received join_table command from GUI
[src/bet.c] GUI join approved! Proceeding with table join...
[src/vdxf.c] Creating payin transaction...
```

**NOW** the payin happens - you'll see "Your tx is being mined"

## Troubleshooting

### Issue: "Your tx is being mined" appears immediately

**Cause 1: Existing game state (rejoin scenario)**
```bash
# Solution: Reset database
rm -f ~/.bet/db/pangea.db

# Restart player
./bin/bet player config/verus_player_p1.ini --gui
```

**Cause 2: Old GUI cache**
```bash
# Solution: Hard refresh browser
# Press Ctrl+Shift+R (or Cmd+Shift+R on Mac)
# Or clear browser cache
```

**Cause 3: Backend not in GUI mode**
```bash
# Make sure you're using --gui flag
./bin/bet player config/verus_player_p1.ini --gui
# NOT --auto or --cli
```

### Issue: Backend shows "Backend is not ready"

**This is normal!** The backend is waiting for join approval. Check if:
1. GUI is sending `walletInfo` request (check browser console)
2. Backend is responding with wallet info (check backend logs)
3. GUI is auto-sending `join_table` after 1 second

### Issue: GUI not connecting

**Check WebSocket port:**
```bash
# Verify backend is listening
netstat -tlnp | grep 9001

# Should show:
# tcp  0  0.0.0.0:9001  0.0.0.0:*  LISTEN  <pid>/./bin/bet
```

**Check backend logs:**
```bash
tail -f /root/bet/poker/logs/player1.log | grep -i "websocket\|gui"
```

## Complete Test Flow (Step by Step)

### Terminal 1: Dealer
```bash
cd /root/bet/poker
./bin/bet dealer config/verus_dealer.ini --reset
```
Wait for: "Dealer node started successfully"

### Terminal 2: Cashier
```bash
cd /root/bet/poker
./bin/bet cashier config/verus_cashier.ini
```
Wait for: "Cashier node started successfully"

### Terminal 3: Player 1 (GUI mode)
```bash
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui
```
Wait for: "Waiting for GUI to connect and send join command..."

### Terminal 4: GUI
```bash
cd /root/pangea-poker
npm start
```
Browser opens automatically

### In Browser:
1. **F12** to open console (see logs)
2. Click **"Player"** tab
3. Enter IP: **localhost**
4. Enter Port: **9001**
5. Click **"Set Nodes"**

### Watch the Flow:

**Browser Console:**
```
Connected!
Sending backend_status...
Sending walletInfo...
Received wallet info: Balance 132.56 CHIPS
Backend waiting for join approval. Auto-sending join_table...
Join approved by backend: Joining table...
```

**Backend Terminal 3:**
```
GUI connected. Waiting for user to request wallet info...
Recv from GUI :: {"method":"backend_status"}
Recv from GUI :: {"method":"walletInfo"}
Recv from GUI :: {"method":"join_table"}
Received join_table command from GUI
GUI join approved! Proceeding with table join...
```

**Now the payin happens** - "Your tx is being mined" appears!

Wait 10-30 seconds for blockchain confirmation, then game starts!

## Timing Breakdown

```
T=0s:     GUI connects
T=0.1s:   Backend sends init message
T=0.2s:   GUI requests walletInfo
T=0.3s:   Backend sends wallet data (Balance: 132.56 CHIPS)
T=1.3s:   GUI auto-sends join_table (1 second delay)
T=1.4s:   Backend wakes up, proceeds with payin
T=1.5s:   "Your tx is being mined" message appears
T=10-30s: Blockchain confirms transaction
T=30s+:   Player joins table, game can start
```

## Two Players (Advanced)

### Terminal 5: Player 2
```bash
cd /root/bet/poker
./bin/bet player config/verus_player_p2.ini --gui
```

### Browser Tab 2:
1. Open new tab: http://localhost:1234
2. Click "Player" tab
3. Enter IP: **localhost**
4. Enter Port: **9002** (Player 2's port!)
5. Click "Set Nodes"

Both players will go through the same approval flow independently!

## Summary

✅ **Before:** Backend auto-joined immediately (no control)
✅ **Now:** Backend waits for GUI approval
✅ **User sees:** Wallet balance before any funds are spent
✅ **Auto-join:** GUI sends approval after 1 second (can be changed to button later)
✅ **Safe:** No funds spent until approval received

The 1-second delay gives you time to see your balance in the console/UI before the join happens automatically. In the future, this can be changed to a manual "Join Table" button for more control.

