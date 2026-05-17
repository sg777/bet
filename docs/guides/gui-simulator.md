# GUI Simulator

`tools/gui_simulator.js` is a small Node script that connects to a
`bet` player node's GUI WebSocket and pretends to be the React
front-end. It is intended for fast iteration on backend changes —
you can run a hand to showdown without standing up the full
pangea-poker dev server.

For the full GUI wire format the simulator is acting against, see
[`docs/reference/gui-message-formats.md`](../reference/gui-message-formats.md).

---

## 1. Prerequisites

* A built `bet` (see [`compile.md`](build-from-source.md)).
* The local Verus regtest up and the four-node stack ready to run
  (see [`COMMUNITY_TESTING_GUIDE.md § 5`](../tutorials/community-quickstart.md)).
* Node.js ≥ 16 on `PATH`.
* The `ws` module installed under `tools/`:

  ```bash
  cd tools
  npm install ws
  ```

---

## 2. Usage

```bash
cd tools
node gui_simulator.js [player_port] [action_mode]
```

| Arg            | Default  | Meaning                                              |
|----------------|----------|------------------------------------------------------|
| `player_port`  | `9001`   | WebSocket port of the target player node.            |
| `action_mode`  | `check`  | Strategy used to respond on the simulator's turn.    |

Available `action_mode` values:

| Mode     | Behaviour                                                                       |
|----------|---------------------------------------------------------------------------------|
| `check`  | Check when legal, call when not. Never raises or folds. Default.                |
| `call`   | Always calls / never folds.                                                     |
| `fold`   | Always folds. Useful for testing showdown-with-one-player paths.                |
| `random` | Weighted random (10% fold, 40% check, 40% call, 10% raise) with legality checks.|

---

## 3. What the simulator does on connect

```
ws.send({ method: "backend_status" })
ws.send({ method: "walletInfo" })
ws.send({ method: "get_bal_info" })
ws.send({ method: "join_table" })  // auto-join, after a short delay
```

After that it listens for backend pushes and pretty-prints them.
Inbound message handling covers the messages you'd expect from a
real client:

* `backend_status`, `walletInfo`, `bal_info`, `join_ack`,
  `warning` — logged.
* `deal` (hole cards / board) — logged with card values.
* `betting` (`action: "round_betting"`) — triggers the
  auto-response described below.
* `betting` (opponent action) — logged.
* `finalInfo` — logged with winners and full showdown payload.

---

## 4. Auto-response on betting

When the backend sends a `betting` round-request, the simulator
waits 1.5 s (so the log is readable) and then sends back a JSON
object representing its chosen action.

> **Note:** the simulator's current betting reply uses the legacy
> `{ "action": "<string>", "bet_amount": N }` shape and echoes the
> full `possibilities` array. The backend in `states.c::bet_player_round_betting`
> expects `possibilities` to be a **single-element array** containing
> the chosen action code (from `enum action_type` in `bet.h`) and
> reads `bet_amount` only for raise / blind actions. This mismatch
> is tracked in [`docs/_code_suggestions.md`](../_code_suggestions.md);
> until it's fixed, the simulator's responses are rejected by the
> backend's validation path and the round will time out. The
> simulator is still useful for monitoring backend pushes (`deal`,
> `finalInfo`, etc.) — just not yet for closing the betting loop.

---

## 5. Running a smoke test

In one terminal, start a player node in GUI mode:

```bash
cd poker
./bin/bet start player --config config/p1.ini --gui
```

In another, point the simulator at it:

```bash
cd tools
node gui_simulator.js 9001 check
```

Expected output, abbreviated:

```
✅ Connected to backend!
📤 Sending initial requests...
   → backend_status
   → walletInfo
   → get_bal_info
   → join_table (auto-join for testing)
🔄 Monitoring for game messages...
[10:14:21] 📊 Backend Status: 1
[10:14:21] 👛 Wallet: 132.5242 CHIPS, Stake: 50, Blinds: 1/2
[10:14:33] 🃏 Hole Cards: [Ah, Kd]
```

The simulator exits automatically after 5 minutes, or on Ctrl-C.

---

## 6. Logs to cross-reference

* Backend log: `/root/.bet/logs/p1.log` (or `~/.bet/logs/...` in
  your home dir, depending on where the daemon was launched from).
  Look for `[GUI]` lines — those are exactly what `gui.c`
  serialised, prior to the WebSocket write.
* Backend WebSocket trace: `grep -E '\[GUI\]|LWS_CALLBACK_' p1.log`.
* Simulator stdout: redirect to a file if running headless
  (`node gui_simulator.js 9001 check 2>&1 | tee /tmp/sim.log`).

---

## 7. Extending the simulator

The script is small (~250 LOC) and is the right starting point if
you want a custom test client. Two patterns to be aware of:

* The backend's `possibilities[]` is a list of *action codes*, not
  action names. See
  [`GUI_MESSAGE_FORMATS.md § 4.4`](../reference/gui-message-formats.md)
  for the code values. A correct betting reply is
  `{ method: "betting", playerid: N, toCall: T, bet_amount: A, possibilities: [CODE] }`.
* `gui_send_message` on the backend only writes on the wire when
  `g_betting_mode == BET_MODE_GUI`. If you launched the player
  without `--gui`, the simulator will connect but the backend will
  never push game-state messages.

There is also a `tools/test_gui_flow.sh` orchestration script.
That script's harness paths (`/root/bet/...`) are pinned to a
specific dev machine and will need editing before running on
yours — see the inline comments at the top of the script.
