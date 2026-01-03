# Find Table Button - User-Controlled Table Discovery

## Overview
Replaced automatic table_info request with a user-triggered "Find Table" button. This gives users explicit control over when they want to search for and join a table.

---

## Problem with Previous Flow

### Before (Auto-Request):
```
1. Backend starts → sends backend_status=1
2. GUI receives backend_status=1
3. GUI AUTO-SENDS table_info request (no user action)
4. Backend responds with table_info
5. Seats appear automatically
6. User clicks seat
```

**Issue:** No user control. Table search happens automatically without user awareness.

---

## New Flow (User-Controlled)

### After (Manual Button):
```
1. Backend starts → sends backend_status=1
2. GUI receives backend_status=1
3. GUI SHOWS "Find Table" button ← USER SEES THIS
4. User clicks "Find Table" ← USER ACTION
5. GUI sends table_info request
6. Backend responds with table_info
7. Seats appear
8. User clicks seat to join
```

**Benefit:** User explicitly initiates table search. Clear UX feedback at each step.

---

## Implementation Details

### 1. Removed Auto-Request (`src/components/Game/onMessage.ts`)

**Before:**
```typescript
case "backend_status":
  updateStateValue("backendStatus", message.backend_status, dispatch);
  
  // Auto-request table info
  if (message.backend_status === 0 && !state.balance) {
    sendMessage({ method: "table_info" }, player, state, dispatch);
  }
  break;
```

**After:**
```typescript
case "backend_status":
  updateStateValue("backendStatus", message.backend_status, dispatch);
  console.log(`[GUI STATE] backendStatus = ${message.backend_status}`);
  // User will manually click "Find Table" button to request table_info
  break;
```

### 2. Created FindTableButton Component (`src/components/FindTableButton/`)

**File Structure:**
```
src/components/FindTableButton/
├── FindTableButton.tsx
└── index.ts
```

**FindTableButton.tsx:**
```typescript
import React from "react";
import { sendMessage } from "../../store/actions";
import { IState } from "../../store/initialState";

interface FindTableButtonProps {
  state: IState;
  dispatch: (arg: object) => void;
}

const FindTableButton: React.FC<FindTableButtonProps> = ({ state, dispatch }) => {
  const handleFindTable = () => {
    console.log("[USER ACTION] Finding table...");
    sendMessage({ method: "table_info" }, "player", state, dispatch);
  };

  return (
    <div style={{ /* centered on table */ }}>
      <div style={{ /* gradient card */ }}>
        <div>🎰 Ready to Play!</div>
        <div>Your wallet is ready. Find an available table to join.</div>
        <button onClick={handleFindTable}>
          🔍 Find Table
        </button>
        <div>Balance: {state.balance?.toFixed(4) || "0.0000"} CHIPS</div>
      </div>
    </div>
  );
};
```

**Styling Features:**
- Centered on table (position: absolute, top/left 50%)
- Gradient background (purple/pink theme)
- Hover effects (scale, shadow)
- Shows current balance
- Clear call-to-action

### 3. Integrated into Table.tsx (`src/components/Table/Table.tsx`)

**Import:**
```typescript
import FindTableButton from "../FindTableButton";
```

**Destructure playerInitState:**
```typescript
const {
  activePlayer,
  balance,
  backendStatus,
  playerInitState,  // NEW
  // ... rest
} = state;
```

**Conditional Rendering:**
```typescript
{/* Find Table Button - Show when backend ready but table not found yet */}
{nodeType === "player" && 
 backendStatus === 1 && 
 playerInitState === 1 && 
 !state.isStartupModal && (
  <FindTableButton state={state} dispatch={dispatch} />
)}
```

**Conditions Explained:**
- `nodeType === "player"` - Only for player nodes (not dealer/cashier)
- `backendStatus === 1` - Backend is ready (wallet + ID accessible)
- `playerInitState === 1` - State is WALLET_READY (haven't found table yet)
- `!state.isStartupModal` - Startup modal is closed

---

## State Flow with Button

### Complete User Journey:

```
┌─────────────────────────────────────────┐
│ 1. User starts backend with --gui      │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 2. Backend: bet_parse_verus_player()   │
│    Sends: player_init_state=1          │
│           (P_INIT_WALLET_READY)         │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 3. GUI: Receives player_init_state=1   │
│    Shows: "Find Table" button          │
│                                         │
│    ┌───────────────────────────┐       │
│    │  🎰 Ready to Play!        │       │
│    │  Your wallet is ready...  │       │
│    │  [🔍 Find Table]          │       │
│    │  Balance: 132.5399 CHIPS  │       │
│    └───────────────────────────┘       │
└────────────┬────────────────────────────┘
             ↓
        ⏳ USER WAITS ⏳
             ↓
┌─────────────────────────────────────────┐
│ 4. User clicks "Find Table"            │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 5. GUI sends: {"method": "table_info"} │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 6. Backend: poker_find_table()         │
│    Sends: player_init_state=2          │
│           (P_INIT_TABLE_FOUND)          │
│           + table_info response         │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 7. GUI: Receives table_info            │
│    playerInitState = 2                  │
│    Button disappears (condition fails)  │
│    Shows: Seat layout with occupied     │
│           seats marked                  │
│    Notice: "Click on available seat..." │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│ 8. Backend: Enters P_INIT_WAIT_JOIN    │
│    Sends: player_init_state=3          │
│    Blocks: pthread_cond_wait()          │
└────────────┬────────────────────────────┘
             ↓
        ⏳ USER WAITS ⏳
             ↓
┌─────────────────────────────────────────┐
│ 9. User clicks seat (e.g., seat 0)     │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│10. GUI sends: {"method": "join_table",  │
│                "seat": 0}               │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│11. Backend: pthread_cond_signal()       │
│     Wakes up from wait                  │
│     Sends: player_init_state=4          │
│            (P_INIT_JOINING)             │
│     Executes: poker_join_table()        │
│               (payin_tx)                │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│12. GUI: Shows "Joining table..."       │
│     Notice: "Please wait 10-30 sec"     │
└────────────┬────────────────────────────┘
             ↓
        [Transaction mines...]
             ↓
┌─────────────────────────────────────────┐
│13. Backend: Transaction confirmed      │
│     Sends: player_init_state=5          │
│            (P_INIT_JOINED)              │
└────────────┬────────────────────────────┘
             ↓
┌─────────────────────────────────────────┐
│14. GUI: Clears notice                  │
│     User is seated, game begins         │
└─────────────────────────────────────────┘
```

---

## UI/UX Improvements

### Visual Design

**Button Card:**
- **Background:** Purple gradient (`#667eea → #764ba2`)
- **Button:** Pink gradient (`#f093fb → #f5576c`)
- **Position:** Center of table (absolute positioning)
- **Z-index:** 9999 (above table elements)

**Interactive Effects:**
- **Hover:** Scale up (1.05x), increase shadow
- **Click:** Scale down (0.95x) for tactile feedback
- **Smooth transitions:** 0.3s ease

**Information Display:**
- **Emoji:** 🎰 for poker theme
- **Title:** "Ready to Play!"
- **Description:** Clear instruction
- **Balance:** Shows current CHIPS balance
- **Icon:** 🔍 on button for "search" action

### Responsive States

| Condition | Display |
|-----------|---------|
| `playerInitState === 0` | Nothing (backend not ready) |
| `playerInitState === 1` | **Find Table Button** (wallet ready) |
| `playerInitState === 2` | Seat layout (table found) |
| `playerInitState === 3` | Seat selection UI (waiting for join) |
| `playerInitState === 4` | "Joining table..." notice |
| `playerInitState === 5+` | Game UI |

---

## Files Modified

### GUI
1. **`src/components/Game/onMessage.ts`**
   - Removed auto-request in `backend_status` handler
   - Now just updates state

2. **`src/components/FindTableButton/FindTableButton.tsx`** (NEW)
   - Button component with styling
   - Click handler sends `table_info` request

3. **`src/components/FindTableButton/index.ts`** (NEW)
   - Export file for clean imports

4. **`src/components/Table/Table.tsx`**
   - Import `FindTableButton`
   - Destructure `playerInitState`
   - Conditionally render button

---

## Testing

### Manual Test Flow

**Terminal 1: Backend**
```bash
cd /root/bet/poker
./bin/bet player config/verus_player_p1.ini --gui
```

**Expected Backend Logs:**
```
[src/bet.c] Player node started. WebSocket server listening on port 9001
Player init state: WALLET_READY
[► TO GUI] Player state: WALLET_READY
[Backend waits, does NOT find table yet]
```

**Terminal 2: GUI**
```bash
cd /root/pangea-poker
npm start
```

**Expected GUI Behavior:**
1. Connect to `ws://localhost:9001`
2. Receive `player_init_state: 1` (WALLET_READY)
3. Show "Find Table" button in center of table
4. User sees: Balance, "Ready to Play!" message

**User Action: Click "Find Table"**

**Expected Backend Logs:**
```
[◄ FROM GUI] {"method":"table_info"}
Player init state: TABLE_FOUND
[► TO GUI] Player state: TABLE_FOUND
[► TO GUI] table_info: {...}
Player init state: WAIT_JOIN - waiting for GUI approval...
[► TO GUI] Player state: WAIT_JOIN
```

**Expected GUI Behavior:**
1. Button disappears (playerInitState now 2)
2. Seat layout appears
3. Occupied seats are marked
4. Notice: "Click on an available seat to join the table"

**User Action: Click Seat 0**

**Expected Backend Logs:**
```
[◄ FROM GUI] {"method":"join_table","seat":0}
GUI join approved, proceeding to join table
Player init state: JOINING
[► TO GUI] Player state: JOINING
[...payin_tx execution...]
Player init state: JOINED
[► TO GUI] Player state: JOINED
```

**Expected GUI Behavior:**
1. Notice: "Joining table... Please wait 10-30 seconds"
2. After tx confirms: Notice clears
3. User is seated at table

---

## Benefits of This Approach

### 1. **User Control**
- User explicitly initiates table search
- Clear feedback at each step
- No "magic" auto-actions

### 2. **Better UX**
- Visual button is more discoverable than automatic action
- Balance displayed prominently
- Hover effects provide tactile feedback

### 3. **Debugging**
- Clear console log: `[USER ACTION] Finding table...`
- Easy to see when user clicks vs. automatic actions
- State transitions are explicit

### 4. **Flexibility**
- Easy to add "Refresh" button later (re-request table_info)
- Can add table filters/preferences in the future
- Can show table list instead of auto-joining first table

### 5. **Error Handling**
- If table_info fails, user can click button again
- No infinite auto-retry loops
- User understands when to retry

---

## Future Enhancements

### Option 1: Table List
Instead of auto-joining first table, show list of available tables:
```typescript
<FindTableButton /> 
  ↓ (on click)
<TableListModal>
  - Table "t1" - 3/9 players - 0.5 CHIPS min
  - Table "t2" - 7/9 players - 1.0 CHIPS min
  [Join button for each]
</TableListModal>
```

### Option 2: Refresh Button
After table found, add refresh option:
```typescript
{playerInitState === 2 && (
  <RefreshButton onClick={() => sendMessage({method: "table_info"})} />
)}
```

### Option 3: Settings
Add table preferences:
```typescript
<FindTableButton preferences={{
  minPlayers: 4,
  maxBuyIn: 2.0,
  gameType: "texas_holdem"
}} />
```

---

## Summary

✅ **Removed:** Auto-request of table_info on backend_status  
✅ **Added:** Beautiful "Find Table" button  
✅ **Condition:** Shows only when `playerInitState === 1` (WALLET_READY)  
✅ **Action:** Sends `table_info` request on click  
✅ **Result:** User-controlled table discovery with clear visual feedback  

**User Journey:**
Backend Ready → See Button → Click Button → Table Found → See Seats → Click Seat → Join Table

**Next Test:**
Run backend + GUI, verify button appears and clicking it triggers table search!

