# Seat Selection Implementation

## Overview
Implemented full seat selection system using the existing pangea-poker GUI's table layout. Players can now see occupied seats and click on empty seats to join.

## Backend Changes

### `/root/bet/poker/src/client.c`

Modified `bet_player_table_info()` to send seat occupancy data:

```c
// Reads t_player_info from blockchain
// Extracts seat numbers and player IDs from player_info array
// Sends occupied_seats array to GUI
```

**Message Format:**
```json
{
  "method": "table_info",
  "table_id": "t1",
  "dealer_id": "d1",
  "max_players": 2,
  "balance": 132.54,
  "occupied_seats": [
    {"seat": 0, "player_id": "p1"},
    {"seat": 3, "player_id": "p2"}
  ]
}
```

### `/root/bet/poker/src/vdxf.c`

Made `get_t_player_info()` non-static so it can be called from `client.c` to fetch player info from blockchain.

## GUI Changes

### `/root/pangea-poker/src/components/Game/onMessage.ts`

**When receiving `table_info`:**
1. Stores `occupied_seats` array
2. Creates `seatsData` array for all positions (0 to max_players-1)
3. Marks seats as empty or occupied based on `occupied_seats`
4. Calls `seats()` action to update player widgets

```typescript
// Convert backend occupied_seats to GUI seats format
const seatsData = [];
for (let i = 0; i < maxPlayers; i++) {
  const occupiedSeat = occupiedSeatsArray.find((s: any) => s.seat === i);
  seatsData.push({
    name: occupiedSeat ? occupiedSeat.player_id : "",
    seat: i,
    playing: 0,
    empty: !occupiedSeat,
    chips: 0
  });
}
seats(seatsData, dispatch);
```

### `/root/pangea-poker/src/store/actions.ts`

**Modified `playerJoin()` to send correct message:**
```typescript
export const playerJoin = (seat: string, state, dispatch) => {
  const seatNum = Number(seat.slice(-1)) - 1; // player1 -> 0
  sendMessage({ method: "join_table", seat: seatNum }, "player", state, dispatch);
  updateStateValue("joinApprovalSent", true, dispatch);
};
```

### `/root/pangea-poker/src/components/Table/Table.tsx`

**Added small info banner at top:**
- Shows table ID, dealer ID, balance, and buy-in
- Displays "Click on an empty seat to join" instruction
- Appears only when backend is waiting for join approval

## Visual Flow

```
┌──────────────────────────────────────────────────────────────┐
│  Table: t1 | Dealer: d1 | Balance: 132.54 | Buy-in: 50      │
│  ☝️ Click on an empty seat to join                           │
└──────────────────────────────────────────────────────────────┘

              [Player 1 - p1]
              ┌─────────────┐
              │  🎴🎴       │
              │  p1         │
              │  50 CHIPS   │
              └─────────────┘
                    
 [SIT HERE]                      [SIT HERE]
 ┌────────┐                      ┌────────┐
 │ Click  │   ╔════════════╗    │ Click  │
 │ to join│   ║   TABLE    ║    │ to join│
 └────────┘   ║            ║    └────────┘
              ╚════════════╝
              
 [SIT HERE]                      [SIT HERE]
```

## User Experience

1. **Connect to backend** → GUI shows table with all seats
2. **See occupied seats** → Shows player ID and chips
3. **See empty seats** → Shows "SIT HERE" text
4. **Click empty seat** → Sends `join_table` with seat number
5. **Backend processes** → Payin transaction and deck initialization
6. **Game starts** → GUI shows cards and betting controls

## Technical Details

### Seat Numbering
- **Backend:** 0-indexed (0, 1, 2, ..., max_players-1)
- **GUI:** player1, player2, player3, ..., player9
- **Conversion:** `seatNum = Number(seat.slice(-1)) - 1`

### Data Flow
```
Backend Blockchain (t_player_info)
    ↓
get_t_player_info() reads player_info array
    ↓
bet_player_table_info() parses and sends occupied_seats
    ↓
GUI onMessage.ts receives table_info
    ↓
Creates seatsData array with empty/occupied flags
    ↓
seats() action updates player widgets
    ↓
PlayerGrid9Max displays seats around table
    ↓
User clicks empty seat → playerJoin() → join_table message
```

### Backend Message Handler
The backend already has a `join_table` handler in `bet_player_frontend()` (client.c) that:
1. Sets `gui_join_approved = true`
2. Signals `gui_join_cond` to wake up main thread
3. Main thread proceeds with payin transaction

**Note:** The backend currently doesn't enforce seat selection - all players are assigned seats sequentially. Future enhancement: modify `handle_verus_player()` to use the seat number from the GUI's `join_table` message.

## Testing

1. Start dealer with `--reset`
2. Start player with `--gui`
3. Open browser to GUI
4. Click "Set Nodes"
5. See table layout with seats
6. Click on "SIT HERE" seat
7. Backend processes payin and starts game

## Files Modified

### Backend
- `/root/bet/poker/src/client.c` - Added seat info to table_info
- `/root/bet/poker/src/vdxf.c` - Made get_t_player_info() public

### GUI
- `/root/pangea-poker/src/components/Game/onMessage.ts` - Parse occupied_seats
- `/root/pangea-poker/src/store/actions.ts` - Update playerJoin()
- `/root/pangea-poker/src/components/Table/Table.tsx` - Add info banner
- `/root/pangea-poker/src/store/initialState.ts` - Add occupiedSeats state

### Documentation
- `/root/bet/docs/GUI_BACKEND_MAPPING.md` - Updated table_info message format

## Status
✅ Backend sends seat occupancy data
✅ GUI visualizes occupied/empty seats using existing table layout
✅ User can click empty seats to join
✅ Join message includes seat number
✅ Documentation updated

## Next Steps (Future Enhancement)
- Backend should honor the seat number from GUI's `join_table` message
- Currently seats are assigned sequentially regardless of user's choice
- Would require modifying `handle_verus_player()` to accept and use the requested seat

