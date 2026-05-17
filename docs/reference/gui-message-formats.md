# GUI Message Formats

Reference for the JSON messages exchanged over WebSocket between a `bet`
backend node (dealer, player, cashier) and a graphical front-end. The
canonical client today is
[`pangea-poker`](https://github.com/sg777-VRSC/pangea-poker), and any
other client is free to speak this wire format.

This document reflects what the C source actually sends and accepts in
the current code base (post-Lightning, post-nanomsg). Methods that
existed only in the legacy build are not listed.

---

## 1. Connection

Each node type opens its own WebSocket listener:

| Node      | Default port (configurable in INI) | Source            |
|-----------|------------------------------------|-------------------|
| Dealer    | `9000`                             | `host.c`          |
| Player    | `9001` (P1), `9002` (P2), ...      | `client.c`        |
| Cashier   | `9000` (cashier node)              | `cashier.c`       |

The GUI connects with a plain `ws://<host>:<port>` upgrade. There is
no authentication on the socket itself — the assumption is that the
GUI and the backend run on the same machine (or a trusted LAN) and
that the underlying node holds the operator's Verus keys.

All messages are JSON objects with at least a `"method"` field
identifying the message type.

---

## 2. Wallet messages

The wallet helpers (`bet_wallet_get_bal_info` and friends in
`payment.c`) are shared across node types, but each node frontend
decides which subset to expose. The matrix below lists what the
respective WebSocket handlers actually dispatch:

| `method`            | Dealer | Player | Cashier |
|---------------------|:------:|:------:|:-------:|
| `get_bal_info`      |   y    |   y    |    y    |
| `get_addr_info`     |   y    |        |    y    |
| `withdrawRequest`   |        |   y    |    y    |
| `withdraw`          |        |   y    |    y    |

### 2.1 `get_bal_info`

Request:
```json
{ "method": "get_bal_info" }
```

Response:
```json
{
  "method":    "bal_info",
  "chips_bal": 123.45678900
}
```

`chips_bal` is the node's on-chain wallet balance, in CHIPS (i.e.
the chain's native unit — `VRSCTEST` on the dev regtest), as a double.

### 2.2 `get_addr_info`

Request:
```json
{ "method": "get_addr_info" }
```

Response:
```json
{
  "method":     "addr_info",
  "chips_addr": "R..."
}
```

`chips_addr` is a freshly generated R-address from the node's wallet.

### 2.3 `withdrawRequest`

Request:
```json
{ "method": "withdrawRequest" }
```

Response:
```json
{
  "method":  "withdrawResponse",
  "balance": 123.45678900,
  "tx_fee":  0.00010000,
  "addrs":   [ { "address": "R...", "label": "" }, ... ]
}
```

`addrs` is the result of the daemon's `listaddressgroupings` RPC,
i.e. only addresses the local wallet already controls. The
front-end uses this list to constrain user input on the withdraw
form.

### 2.4 `withdraw`

Request:
```json
{
  "method": "withdraw",
  "amount": 10.5,
  "addr":   "R..."
}
```

Response:
```json
{
  "method": "withdrawInfo",
  "tx":     { "...": "..." }
}
```

`tx` is the raw `z_sendmany`/`sendcurrency` result returned by the
daemon. The handler returns `null` if `amount <= 0` or `addr` is
missing.

---

## 3. Dealer frontend (`host.c`)

The dealer's WebSocket frontend (`bet_dcv_frontend`) is intentionally
small — it serves a legacy reset/inspection UI that is rarely used in
the Verus build. Methods accepted:

| `method`        | Effect                                                |
|-----------------|-------------------------------------------------------|
| `game`          | Returns table parameters (`gametype`, `seats`, `pot`). |
| `seats`         | Returns the current seat assignments.                  |
| `chat`          | Echoes `message` (no broadcast in the Verus build).    |
| `reset`         | Clears dealer in-memory state.                         |
| `get_bal_info`  | See §2.1.                                              |
| `get_addr_info` | See §2.2.                                              |

### 3.1 `game` response

```json
{
  "method": "game",
  "game": {
    "seats":    2,
    "pot":      [0],
    "gametype": "Texas Holdem Poker:1/2"
  }
}
```

`gametype` is formatted as `"Texas Holdem Poker:<small_blind>/<big_blind>"`
where the blinds are integer table-chip values (1 CHIP = 100 table chips).

### 3.2 `seats` response

```json
{
  "method": "seats",
  "seats":  [
    { "seat": 0, "name": "Player1", "chips": 1000, "empty": false, "playing": 0 },
    { "seat": 1, "name": "Player2", "chips": 1000, "empty": false, "playing": 0 }
  ]
}
```

---

## 4. Player frontend (`client.c`)

The player frontend (`bet_player_frontend`) is the busy one — it
drives the actual gameplay UI. Incoming methods are dispatched
through a `switchs` macro:

| `method`           | Effect                                                                          |
|--------------------|---------------------------------------------------------------------------------|
| `backend_status`   | Triggers an immediate `backend_status` push (§4.1).                             |
| `betting`          | Player's action for the current betting round (§4.4).                           |
| `get_bal_info`     | See §2.1.                                                                       |
| `player_join`      | Player chooses a seat (§4.3).                                                   |
| `reset`            | Resets the player's in-memory game vars.                                        |
| `sitout`           | `{ value: 1 }` to sit out, `0` to sit in.                                       |
| `table_info`       | Asks backend to publish current table info (§4.2) and unblocks the join thread. |
| `join_table`       | Final confirmation to actually commit the payin and join the seat.              |
| `withdraw`         | See §2.4.                                                                       |
| `withdrawRequest`  | See §2.3.                                                                       |

Unknown methods cause the backend to send back
`{ "method": "error", "msg": "Method::<name> is not handled" }`.

### 4.1 `backend_status` (backend → GUI)

Sent automatically on connect, and on demand when the GUI sends
`{ "method": "backend_status" }`:

```json
{
  "method":         "backend_status",
  "backend_status": 1,
  "message":        "Backend ready!"
}
```

`backend_status` is `0` (not ready — wallet/identity still being
validated) or `1` (ready — `bet_player` can join a table). The
codes come from `enum be_status` in `bet.h`.

### 4.2 `table_info` (request and response)

Request from GUI:
```json
{ "method": "table_info" }
```

Two side effects:

1. The backend signals the join thread that the GUI is alive
   (`gui_table_requested = 1`) so it stops waiting and starts
   resolving the configured `table_id`.
2. The backend publishes `table_info` back to the GUI:

```json
{
  "method":          "table_info",
  "addr":            "R...",
  "balance":         132.52420000,
  "backend_status":  1,
  "max_players":     2,
  "table_min_stake": 0.5,
  "small_blind":     0.01,
  "big_blind":       0.02,
  "table_id":        "t1",
  "dealer_id":       "d1",
  "occupied_seats":  [ { "seat": 0, "player_id": "p1@" } ]
}
```

The `*_blind` and `table_min_stake` values are in CHIPS for display;
table-chip-denominated equivalents are stored in `struct
privatebet_vars`.

### 4.3 `player_join`

Request:
```json
{
  "method":       "player_join",
  "gui_playerID": 1
}
```

`gui_playerID` is 1-based (1 == first seat). The backend validates
`1 <= gui_playerID <= max_players`. The seat-index conversion to
0-based happens inside the backend.

The handler:

* If the wallet/ID is still initializing (`backend_status == 0`),
  responds with a `warning` (see §4.7) and does not register the join.
* Otherwise, generates the player's curve25519 deck keys and emits an
  internal `join_req` message for the dealer's nanomsg-era path
  (largely dormant — the actual join completes through `join_table`
  + on-chain payin).

### 4.4 `betting` (GUI → backend)

When it is the player's turn the backend has already sent a
`betting` message of action `"round_betting"` (see §4.6) describing
what actions are legal. The GUI replies with its chosen action:

```json
{
  "method":        "betting",
  "playerid":      0,
  "toCall":        20,
  "bet_amount":    40,
  "possibilities": [ 4 ]
}
```

Fields:

* `playerid` — 0-based seat index of the acting player.
* `toCall`   — table-chip amount needed to call (echoed back from the
                round_betting message).
* `bet_amount` — only consulted for `raise`, `small_blind`,
                `big_blind`. Ignored for `check`, `fold`, `call`,
                `allin`.
* `possibilities` — single-element array carrying the chosen
                action code. The action codes are from
                `enum action_type` in `bet.h`:

  | code | action       |
  |-----:|--------------|
  | 1    | `small_blind`|
  | 2    | `big_blind`  |
  | 3    | `check`      |
  | 4    | `raise`      |
  | 5    | `call`       |
  | 6    | `allin`      |
  | 7    | `fold`       |

Notably, **`round` is not read from this message**. The backend
re-reads the round from the dealer's `T_BETTING_STATE_KEY` on-chain
state so that a stale round number from the GUI cannot cause the
dealer to auto-fold the action. The amount handling for `call` and
`allin` is computed server-side from `vars->player_funds` rather
than trusted from the GUI.

### 4.5 `join_table`

```json
{ "method": "join_table" }
```

No body. Sets `gui_join_approved = 1` and signals
`gui_join_cond`, unblocking the join-thread which then submits the
payin and runs the rest of the join flow.

### 4.6 Game-state pushes (backend → GUI)

These are emitted by helpers in `gui.c` and pushed via
`player_lws_write`. They cover the visible gameplay surface:

#### `walletInfo`

```json
{
  "method":          "walletInfo",
  "backend_status":  1,
  "balance":         132.5242,
  "addr":            "R...",
  "table_min_stake": 50,
  "small_blind":     1,
  "big_blind":       2,
  "max_players":     2
}
```

The blind and stake fields are in **table chips** here (integer);
the `balance` is in CHIPS (the wallet's on-chain unit). This is the
post-`join_table` push, in contrast to `table_info` (§4.2) where
blinds are quoted in CHIPS.

#### `seats`

```json
{
  "method": "seats",
  "seats":  [
    { "name": "Player1", "playing": 0, "seat": 0, "chips": 1000, "empty": false },
    { "name": "Player2", "playing": 0, "seat": 1, "chips": 1000, "empty": false }
  ]
}
```

#### `blindsInfo`

```json
{ "method": "blindsInfo", "small_blind": 1, "big_blind": 2 }
```

Table-chip integers.

#### `dealer`

```json
{ "method": "dealer", "playerid": 0 }
```

Identifies the seat that currently holds the dealer button.

#### `deal` (hole cards)

Sent to the *local* player only, never broadcast:

```json
{
  "method": "deal",
  "deal":   {
    "holecards": [ "Ah", "Kd" ],
    "balance":   980
  }
}
```

Card strings use the convention `<rank><suit>` where suit is one of
`c d h s` and rank is one of `2 3 4 5 6 7 8 9 T J Q K A`.

#### `deal` (board)

```json
{
  "method": "deal",
  "deal":   {
    "board": [ "Th", "9c", "2d" ]
  }
}
```

Sent on flop, turn, and river — `board` carries the cumulative
revealed community cards (3, 4, then 5 entries).

#### `betting` — `round_betting` (backend → GUI)

The dealer's "you are on the clock" message:

```json
{
  "method":        "betting",
  "action":        "round_betting",
  "playerid":      1,
  "pot":           50,
  "toCall":        20,
  "minRaiseTo":    40,
  "possibilities": [ 3, 4, 5, 7 ],
  "player_funds":  [ 980, 960 ]
}
```

`possibilities` here is a list of legal action codes the GUI must
restrict the player to. The GUI replies with §4.4.

#### `betting` — opponent action (backend → GUI)

Echoed to all players whenever the dealer commits a betting action
to the chain:

```json
{
  "method":     "betting",
  "action":     "raise",
  "playerid":   1,
  "bet_amount": 40
}
```

For the two blinds, the variant carries `amount` instead of
`bet_amount`:

```json
{ "method": "betting", "action": "small_blind", "playerid": 0, "amount": 1 }
```

This is the channel through which a player's GUI learns what other
seats have just done.

#### `finalInfo`

Sent at showdown:

```json
{
  "method":     "finalInfo",
  "winners":    [ 0 ],
  "win_amount": 80,
  "showInfo":   {
    "allHoleCardsInfo": [ [ "Ah", "Kd" ], [ "9c", "9d" ] ],
    "boardCardInfo":    [ "Th", "9h", "2c", "5d", "Js" ]
  }
}
```

`winners` is an array of seat indices (can have multiple entries for
a chopped pot). `win_amount` is the table-chip amount each winner
receives. The `showInfo` block carries every player's hole cards
indexed by seat, plus the full board, so the GUI can animate the
showdown.

### 4.7 `warning`

Backend-initiated, used for soft errors during join:

```json
{ "method": "warning", "warning_num": 0 }
```

`warning_num` values are from `enum bet_warnings` in `bet.h`:

| code | meaning              |
|-----:|----------------------|
| 0    | `seat_already_taken` |
| 1    | `insufficient_funds` |
| 2    | `table_is_full`      |

### 4.8 `error`

Backend-initiated, used for unhandled methods and unrecoverable
state:

```json
{ "method": "error", "msg": "Method::foo is not handled" }
```

---

## 5. Cashier frontend (`cashier.c`)

The cashier exposes only the four shared wallet methods — there is
no game-specific UI surface on the cashier node. Behaviour is
identical to §2, with the same wire formats.

---

## 6. Implementation pointers

* Dispatcher: `bet_dcv_frontend` (`host.c`), `bet_player_frontend`
  (`client.c`), `bet_cashier_frontend` (`cashier.c`).
* Outbound helpers for the player: `gui.c` (`gui_build_*`,
  `gui_send_message`).
* Wallet helpers: `payment.c`
  (`bet_wallet_get_bal_info`, `bet_wallet_get_addr_info`,
  `bet_wallet_withdraw_request`, `bet_wallet_withdraw`).
* Buffer for write-back is `dcv_gui_data` / `player_gui_data`; the
  WebSocket write happens out of the `LWS_CALLBACK_SERVER_WRITEABLE`
  branch, gated by a `data_exists` flag.

The dormant `nanomsg` / `nng` paths in `host.c` and `client.c` are
unrelated to GUI messaging — they are inter-node remnants that
remain compiled but are no longer wired to a socket.
