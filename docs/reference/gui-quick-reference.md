# GUI Message Quick Reference

One-page cheat sheet for the WebSocket protocol between a `bet` node and its
GUI. Authoritative document is
[`GUI_MESSAGE_FORMATS.md`](gui-message-formats.md); this page is just the
at-a-glance derivative.

## 1. Connect

Each node runs its own GUI WebSocket server. Default ports
(`poker/include/common.h:140-143`):

| Role    | Port |
|---------|------|
| Dealer  | 9000 |
| Player  | 9001 |
| Cashier | 9002 |

Override with `gui_ws_port` in the role's INI file (`[dealer]`, `[player]`,
`[cashier]`).

```javascript
const ws = new WebSocket('ws://127.0.0.1:9001/'); // player GUI
```

## 2. Wallet methods (supported per role)

| Method            | Dealer | Player | Cashier |
|-------------------|:------:|:------:|:-------:|
| `get_bal_info`    |   ✓    |   ✓    |    ✓    |
| `get_addr_info`   |   ✓    |   ✓    |    ✓    |
| `walletInfo`      |   ✓    |   ✓    |    ✓    |
| `withdrawRequest` |   ✗    |   ✓    |    ✓    |
| `withdraw`        |   ✗    |   ✓    |    ✓    |

`bet` queries Verus through the `verus -chain=VRSCTEST` CLI; balances and
addresses are sourced from `getidentity` / `getaddressbalance`. There is no
shielded-balance distinction.

### Examples

```json
// → {"method":"get_bal_info"}
// ← {"method":"bal_info","chips_bal":123.45}

// → {"method":"walletInfo"}
// ← {"method":"walletInfo","addr":"iAddr…","balance":123.45, ...}

// → {"method":"withdrawRequest"}
// ← {"method":"withdrawResponse","balance":123.45,"tx_fee":0.0001,"addrs":[…]}

// → {"method":"withdraw","amount":10.5,"addr":"iAddr…"}
// ← {"method":"withdrawInfo","tx":{…}}
```

## 3. Player → Backend (game)

### Join

```json
{"method":"player_join","table_id":"<game_id>","player_id":"player@VRSCTEST"}
```

### Betting (request)

```json
{"method":"betting","round":0,"possibilities":[5]}
```

The GUI sends a **single** chosen action as the first element of
`possibilities`. The backend infers `amount` from the current round and the
table blinds; the GUI must not invent amounts. Action codes match
`enum action_type` in `poker/include/bet.h:49`:

| Code | Action        |
|-----:|---------------|
|    1 | `small_blind` |
|    2 | `big_blind`   |
|    3 | `check`       |
|    4 | `raise`       |
|    5 | `call`        |
|    6 | `allin`       |
|    7 | `fold`        |

> Note: codes **4 = raise** and **5 = call**. (Old drafts of this cheat
> sheet had these reversed.)

### Sit out

```json
{"method":"sitout","value":1}
```

### Backend status

```json
// → {"method":"backend_status"}
// ← {"method":"backend_status","backend_status":1}
```

## 4. Backend → GUI (unsolicited)

These are pushed by `gui.c::gui_send_to_clients(...)` without a request:

| Method                   | Sent by  | Purpose                                                                     |
|--------------------------|----------|-----------------------------------------------------------------------------|
| `walletInfo`             | all      | Initial wallet snapshot on GUI connect.                                     |
| `seats`                  | dealer   | Seat list / chip stacks. Re-sent on join, leave, hand reset.                |
| `blindsInfo`             | dealer   | `{small_blind, big_blind}` for the table.                                   |
| `dealer`                 | dealer   | Current dealer-button seat index.                                           |
| `deal` (hole)            | player   | Two encrypted hole cards revealed to this player only.                      |
| `deal` (board)           | dealer   | Flop / turn / river (3 / 1 / 1 cards), batched per street.                  |
| `betting` (round_betting)| dealer   | Whose turn it is + allowed `possibilities` array.                           |
| `betting` (action)       | dealer   | Action taken by a player: `{seat, action, amount}`.                         |
| `betting` (small_blind / big_blind) | dealer | Blind postings.                                                  |
| `finalInfo`              | dealer   | End-of-hand pot distribution + `showInfo` (winning hand description).       |
| `warning`                | any      | Non-fatal notice. Includes `warning_num`.                                   |
| `error`                  | any      | Fatal protocol error. Includes `error_code` + message.                      |

There is **no** `init_d` / `init` / `turn` / `game_info` message — those names
appeared in pre-Verus drafts of this protocol and are not in current code.

## 5. Error handling

* Validate the `method` field on every inbound message — the backend may
  interleave unsolicited push messages with replies to a request.
* `warning_num` and `error_code` numeric values are listed in
  [`GUI_MESSAGE_FORMATS.md`](gui-message-formats.md#warnings-and-errors).
* The backend closes the socket on `error` for protocol-level faults; the
  GUI should reconnect and resync via `walletInfo` + `seats`.

---

## See also

* [`GUI_MESSAGE_FORMATS.md`](gui-message-formats.md) — full spec with field
  types, payload sizes, and the per-method handlers in `gui.c` / `host.c` /
  `client.c` / `cashier.c`.
* [`../GUI_BACKEND_MAPPING.md`](gui-backend-mapping.md) — module-level
  source map (which C file emits which GUI message).
* [`../GUI_SIMULATOR_GUIDE.md`](../guides/gui-simulator.md) — `tools/gui_simulator.js`
  walkthrough for headless testing.
