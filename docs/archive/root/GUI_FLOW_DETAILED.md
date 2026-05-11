# EXACT GUI FLOW - What Happens When You Click "Set Nodes"

## 1. USER CLICKS "SET NODES" BUTTON

**File:** `/root/pangea-poker/src/components/Modal/CustomIP.tsx`

```
handleSubmit() called:
├── Updates state.nodes to { player: "159.69.23.31:9001" }
├── Updates state.nodeType to "player"
├── Calls game() → sends { method: "backend_status" } to backend
└── Sets state.nodesSet = true
```

## 2. WebSocket COMPONENT RENDERS

**File:** `/root/pangea-poker/src/components/Game/Game.tsx` + `WebSocket.ts`

```
state.nodesSet === true && nodeType === "player"
├── Renders <WebSocket nodeName="player" server="ws://159.69.23.31:9001" />
└── WebSocket connects to backend
```

## 3. WebSocket CONNECTION ESTABLISHED

**File:** `/root/pangea-poker/src/components/Game/WebSocket.ts`

```
readyState changes from 0 → 1 ("Connected")
├── calls sendInitMessage() → just updates connection status
└── calls game() → sends { method: "backend_status" } to backend
```

### Messages Sent to Backend on Connection:
```json
{
  "method": "backend_status"
}
```

**WAIT... WHERE THE FUCK IS walletInfo SENT?**

There's NO AUTOMATIC walletInfo REQUEST in the GUI code!

## 4. Backend Responds

**Backend sends:**
```json
{
  "method": "backend_status",
  "backend_status": 0,
  "message": "Connected! Request wallet info and click 'Join Table' to start."
}
```

## 5. GUI Receives backend_status Response

**File:** `/root/pangea-poker/src/components/Game/onMessage.ts`

```typescript
case "backend_status":
  updateStateValue("backendStatus", message.backend_status, dispatch);
  break;  // <-- THAT'S IT! NOTHING ELSE HAPPENS!
```

### GUI State After This:
- `state.backendStatus = 0`
- `state.balance = 0` (NOT SET YET!)
- `state.joinApprovalSent = false`

## 6. GUI Tries to Render

**File:** `/root/pangea-poker/src/components/Table/Table.tsx`

```tsx
// Condition for showing wallet info:
{!state.isStartupModal && 
 nodeType === "player" && 
 state.backendStatus === 0 && 
 !state.joinApprovalSent && 
 state.balance > 0 &&  // <-- FALSE! balance is 0!
 (
   <div>Balance: {state.balance} CHIPS...</div>
 )}

// This condition is TRUE instead:
{!state.isStartupModal && 
 nodeType === "player" && 
 state.backendStatus === 0 && 
 state.joinApprovalSent &&  // <-- FALSE, but doesn't matter
 (
   <div>{notifications.MINING_TX}</div>  // <-- Shows "tx being mined"
 )}
```

## THE PROBLEM:

**GUI NEVER SENDS `walletInfo` REQUEST TO BACKEND!**

The GUI:
1. Connects
2. Sends `backend_status` request
3. Gets `backend_status: 0` response
4. **STOPS THERE**
5. Never requests wallet info
6. Never gets balance
7. Shows "tx being mined" because `backendStatus === 0` and `balance === 0`

## THE FIX:

**Option 1: Auto-send walletInfo after backend_status response**

In `/root/pangea-poker/src/components/Game/onMessage.ts`:
```typescript
case "backend_status":
  updateStateValue("backendStatus", message.backend_status, dispatch);
  
  // If backend is waiting for join, request wallet info
  if (message.backend_status === 0) {
    sendMessage({ method: "walletInfo" }, nodeName, state, dispatch);
    sendMessage({ method: "get_bal_info" }, nodeName, state, dispatch);
  }
  break;
```

**Option 2: Have backend send walletInfo automatically in initial response**

Backend already does this in `bet_gui_init_message()`, but we only send `backend_status`. Need to also send wallet info in that initial message.

## CORRECT FLOW SHOULD BE:

```
1. GUI connects
2. Backend sends backend_status (already does this)
3. GUI receives backend_status AND auto-requests walletInfo
4. Backend sends walletInfo with balance
5. GUI shows balance for 1 second
6. GUI auto-sends join_table
7. GUI shows "tx being mined"
```

