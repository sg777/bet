# Backend → GUI Implementation Map

This is the dev-facing companion to
[`docs/reference/gui-message-formats.md`](gui-message-formats.md).
That document specifies *what* the wire format looks like; this one
points at *where in the C code* each message gets built and what
backend variable feeds each field.

For protocol-only questions ("what JSON does the GUI receive?"), go
straight to `GUI_MESSAGE_FORMATS.md`. For "how do I add a new
GUI message?" or "which struct holds X", read on.

---

## Module layout

| File                       | Responsibility                                                                    |
|----------------------------|-----------------------------------------------------------------------------------|
| `poker/src/gui.c`          | Pure outbound message builders (`gui_build_*`) and the `gui_send_message` writer. |
| `poker/include/gui.h`      | Declarations and the `BET_MODE_*` enum.                                           |
| `poker/src/client.c`       | Player WebSocket frontend (`bet_player_frontend`, inbound dispatcher).            |
| `poker/src/host.c`         | Dealer WebSocket frontend (`bet_dcv_frontend`).                                   |
| `poker/src/cashier.c`      | Cashier WebSocket frontend (`bet_cashier_frontend`).                              |
| `poker/src/payment.c`      | Shared wallet helpers consumed by all three frontends.                            |
| `poker/src/states.c`       | Game-state machine; builds `betting` round-betting messages.                      |
| `poker/src/player.c`       | Player-side game loop; calls `gui_build_*` at state transitions.                  |

`gui_send_message` only writes on the wire when
`g_betting_mode == BET_MODE_GUI`; otherwise it just logs the JSON
under `[GUI]` so CLI/AUTO runs are still observable.

---

## Backend variable → GUI field

Quick lookup for the most-frequently-confused fields.

| GUI field                        | Backend source                                                | Notes                                                                 |
|----------------------------------|---------------------------------------------------------------|-----------------------------------------------------------------------|
| `playerid`                       | `p_deck_info.player_id` / loop index                          | 0-based seat number.                                                  |
| `holecards`                      | `p_local_state.decoded_cards[0..1]`                           | Backend stores 0..51, `card_value_to_string` converts to `<rank><suit>`. |
| `board`                          | `dcv_vars.cards[burn + community]` (dealer) or table CMM read | Cumulative across flop/turn/river.                                    |
| `pot`                            | `vars->pot`                                                   | Table-chip integer (1 CHIP = 100 table chips).                        |
| `toCall`                         | `to_call` in `bet_player_round_betting`                       | Same units.                                                           |
| `minRaiseTo`                     | `min_raise_to` in `gui_build_betting_round`                   | Same units.                                                           |
| `player_funds[]`                 | `vars->funds[i] - sum(vars->betamount[i][])`                  | Per-seat remaining stack.                                             |
| `possibilities[]` (outbound)     | `enum action_type` codes the player is allowed to send back   | See §Action codes below.                                              |
| `winners[]` (in `finalInfo`)     | `T_GAME_RESULT_KEY` payload on the table identity             | Seat indices, not Verus IDs.                                          |
| `showInfo.allHoleCardsInfo`      | All players' `P_HOLE_REVEAL_KEY` reveals merged on cmm        | Indexed by seat.                                                      |

For balance / chain-side numbers the player frontend uses the
shared wallet helpers in `payment.c`. These are documented in
[`GUI_MESSAGE_FORMATS.md § 2`](gui-message-formats.md).

---

## Action codes

There are **two** action enumerations in play and they do not match:

* **Backend** (`enum action_type` in `bet.h`): `small_blind = 1, big_blind, check, raise, call, allin, fold` — i.e. `4 == raise`, `5 == call`.
* **GUI** (pangea-poker's `Possibilities` in `lib/constants.ts`): an
  independent JS enum using its own numbering (`fold=0, call=1,
  raise=2, …`).

The wire format uses the **backend** codes — `gui_build_betting_round`
puts backend codes into `possibilities[]`, and `bet_player_round_betting`
reads a backend code out of the GUI's reply. The GUI is responsible
for mapping between its internal `Possibilities` and the backend
codes when it builds the action message.

Full reference of valid codes in
[`GUI_MESSAGE_FORMATS.md § 4.4`](gui-message-formats.md).

---

## Where each outbound message originates

| Message               | Built by (function)                | Called from                                            |
|-----------------------|------------------------------------|--------------------------------------------------------|
| `backend_status`      | `gui_build_backend_status`         | `bet_gui_init_message` on connect; on demand from GUI. |
| `walletInfo`          | `gui_build_wallet_info`            | After `join_table` succeeds.                           |
| `table_info`          | `bet_player_table_info` (client.c) | In response to GUI's `table_info` request.             |
| `seats`               | `gui_build_seats_info`             | On seat-state changes during join phase.               |
| `blindsInfo`          | `gui_build_blinds_info`            | Hand start.                                            |
| `dealer`              | `gui_build_dealer_info`            | Hand start (button position).                          |
| `deal` (holecards)    | `gui_build_deal_holecards`         | `player.c` after the hole-card reveal batch lands.     |
| `deal` (board)        | `gui_build_deal_board`             | `player.c` at flop / turn / river reveal.              |
| `betting` (round)     | `gui_build_betting_round`          | `states.c::bet_player_round_betting_request`.          |
| `betting` (action)    | `gui_build_betting_action`         | `player.c` on each broadcast of an opponent's action.  |
| `betting` (blind)     | `gui_build_blind_post`             | `player.c` on small_blind / big_blind broadcasts.      |
| `finalInfo`           | `gui_build_final_info`             | `player.c` once `T_GAME_RESULT_KEY` is readable.       |
| `warning`             | inline `cJSON_*` (client.c)        | On invalid join attempts.                              |
| `error`               | inline `cJSON_*` (client.c)        | On unhandled methods.                                  |

Inbound dispatch is in `bet_player_frontend`'s `switchs(method)` —
that's where to add a handler if you're introducing a new
GUI → backend method.

---

## Adding a new GUI message

1. Add a builder in `gui.c` (`gui_build_<thing>`) and its
   declaration in `gui.h`.
2. Call the builder from the appropriate state transition in
   `player.c` / `states.c` / `host.c` and pass the result to
   `gui_send_message` (or `player_lws_write` / `bet_dcv_lws_write`
   for non-player frontends).
3. Update [`GUI_MESSAGE_FORMATS.md`](gui-message-formats.md)
   so the wire format is documented in exactly one place.
4. For inbound messages, add a `cases("<method>")` arm to
   `bet_player_frontend` (or the dealer/cashier equivalent).

The `g_betting_mode` gate in `gui_send_message` ensures CLI/AUTO
runs continue to work — outbound messages are logged but not sent.

---

## Ports

| Node      | Default port | Constant in code            |
|-----------|--------------|-----------------------------|
| Dealer    | 9000         | `DEFAULT_DEALER_WS_PORT`    |
| Player    | 9001         | `DEFAULT_PLAYER_WS_PORT`    |
| Cashier   | 9002         | `DEFAULT_CASHIER_WS_PORT`   |

All three are configurable per node (see
[`dealer_configuration.md`](dealer-config.md) and
[`player_configuration.md`](player-config.md)).

---

## Status of the GUI integration

The "Phase 1 / Phase 2 / Phase 3" rollout described in earlier
versions of this document is complete. Both outbound (backend →
GUI) and inbound (GUI → backend) paths are wired in
`bet_player_frontend`; GUI-mode players block on the GUI's reply
during betting instead of auto-acting. The pangea-poker front-end
connects to `ws://localhost:<player_ws_port>` and is the canonical
client.

Known remaining work in this area is tracked in
[`TODO.md`](../TODO.md).
