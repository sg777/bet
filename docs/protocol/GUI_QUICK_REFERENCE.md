# GUI Message Quick Reference

Quick reference for GUI developers implementing WebSocket communication with Pangea-Bet backend.

## Connection

```javascript
const ws = new WebSocket('ws://<backend_ip>:9000');
```

## Common Messages (All Node Types)

### Get Balance
```json
// Request
{"method": "get_bal_info"}

// Response
{"method": "bal_info", "chips_bal": 123.45}
```

### Get Address
```json
// Request
{"method": "get_addr_info"}

// Response
{"method": "addr_info", "chips_addr": "Rxxx..."}
```

### Withdraw Request
```json
// Request
{"method": "withdrawRequest"}

// Response
{"method": "withdrawResponse", "balance": 123.45, "tx_fee": 0.0001, "addrs": [...]}
```

### Withdraw
```json
// Request
{"method": "withdraw", "amount": 10.5, "addr": "Rxxx..."}

// Response
{"method": "withdrawInfo", "tx": {...}}
```

## Dealer Messages

### Game Info
```json
// Request
{"method": "game"}

// Response
{"method": "game", "game": {"seats": 2, "pot": [0], "gametype": "NL Hold'em<br>Blinds: 3/6"}}
```

### Seats
```json
// Request
{"method": "seats"}

// Response
{"method": "seats", "seats": [{"playerid": 0, "name": "Player1", "chips": 1000}]}
```

### Chat
```json
{"method": "chat", "message": "Hello!"}
```

### Reset
```json
{"method": "reset"}
```

## Player Messages

### Backend Status
```json
// Request
{"method": "backend_status"}

// Response
{"method": "backend_status", "backend_status": 1}
```

### Betting
```json
{"method": "betting", "round": 0, "action": 4, "amount": 10.5}
```
Actions: 1=small_blind, 2=big_blind, 3=check, 4=call, 5=raise, 6=allin, 7=fold

### Player Join
```json
{"method": "player_join", "table_id": "abc...", "player_id": "player@id"}
```

### Wallet Info
```json
// Request
{"method": "walletInfo"}

// Response
{"method": "walletInfo", "addr": "Rxxx...", "balance": 123.45, ...}
```

### Sit Out
```json
{"method": "sitout", "value": 1}
```

## Cashier Messages

Cashier supports **only** the common wallet messages listed above.

## Backend → GUI Messages (Unsolicited)

These messages are sent from backend without a request:

- `{"method": "warning", "warning_num": 0}` - Warning notifications
- `{"method": "init_d", ...}` - Deck initialization
- `{"method": "turn", ...}` - Turn information
- `{"method": "game_info", ...}` - Game state updates
- `{"method": "finalInfo", ...}` - Game end information
- `{"method": "seats", ...}` - Seat updates
- `{"method": "reset", ...}` - Reset notifications

## Error Handling

- Monitor WebSocket connection status
- Handle timeouts for requests
- Validate response `method` field matches request
- Check for `null` responses (indicates error)

## Full Documentation

See [GUI_MESSAGE_FORMATS.md](./GUI_MESSAGE_FORMATS.md) for complete documentation with all fields, types, and examples.

