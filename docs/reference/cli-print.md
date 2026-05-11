# Inspecting on-chain state with the `print` commands

Verus identities store the bet game state in their contentmultimap (CMM) as
hex-encoded values under VDXF keys. `getidentity` and `getidentitycontent`
will return those bytes, but reading them back as poker state means knowing
which keys to ask for, in what order, suffixed with which `game_id`, and how
each one decodes. The `bet` CLI bundles all of that into a handful of
`print*` subcommands, each tuned for a different debugging shape.

Five commands exist today:

- `print_id <id> <type>` — type-aware structured dump of one identity.
- `print_table_key <table_id> <key>` — one game-suffixed key on a table id.
- `print <id> <key>` — single-key lookup; supports a fixed subset of keys.
- `print_keys <id> [block_height]` — bulk dump of all known table keys.
- `show <id> [--height <block>]` — same bulk dump, with a different flag style.

They're listed in the rough order you'd reach for them: from "tell me what
this dealer/player/table looks like right now" all the way down to "give me
the raw value of one specific key".

---

## `print_id <id> <type>`

Implemented in `print.c:172` as `print_id_info`. It first verifies the
identity exists on-chain with `is_id_exists(id, 0)` and then dispatches to a
type-specific printer. Five types are supported, each with a single-letter
alias:

- `table` or `t` — `print_table_id` (`print.c:75`)
- `dealer` or `d` — `print_dealer_id` (`print.c:64`)
- `player` or `p` — `print_player_id` (`print.c:124`)
- `dealers` — `print_dealers_id` (`print.c:57`)
- `cashiers` or `c` — `print_cashiers_id` (`print.c:20`)

Anything else prints `Print is not supported for this ID::<id> of type::<type>`.

### `table` (alias `t`)

A table id is the game's main state board. `print_table_id` starts by reading
`T_GAME_ID_KEY` from the table — the current `game_id` is what every
game-scoped key on this id is suffixed with. If no `game_id` resolves, the
table is dormant and nothing else is printed.

When a `game_id` is present, the printer walks the table's game-scoped keys
in order: `T_TABLE_INFO_KEY` (stake config and seat count),
`T_PLAYER_INFO_KEY` (seated players and their payin amounts),
`T_SETTLEMENT_INFO_KEY` (post-showdown payouts), `T_BOARD_CARDS_KEY` (flop /
turn / river), `T_BETTING_STATE_KEY` (current turn, pot, last action), and
the ten dealer-side deck keys held in `all_t_d_p_keys[]` (`t_d_deck` plus
`t_d_p1_deck` … `t_d_p9_deck`). It closes with `T_GAME_INFO_KEY` decoded into
a human-readable game state name via `game_state_str()`.

Two keys that used to live on the table id are deliberately *not* printed
here: `T_B_P*_DECK_KEY` (per-player blinded decks) and `T_CARD_BV_KEY`
(blinding values). Both moved to the cashier id under
`C_B_P*_DECK_KEY` / `C_CARD_BV_KEY` (see `docs/TODO.md` items 1.1 and 1.2),
and `print.c:109` and `print.c:118` carry the migration comments. To inspect
them, run `print_id <cashier_id> cashiers`.

### `dealer` (alias `d`)

A dealer id holds the table template (stakes, seat count, `dealer_id`,
`table_id`) under an unsuffixed `T_TABLE_INFO_KEY`, plus an unsuffixed
`registration_info` key recording when this dealer was added to the
aggregator. `print_dealer_id` reads both. Note that this is the *template* —
the per-game state lives on the table id (see above), not the dealer id.

### `player` (alias `p`)

`print_player_id` is the most useful command during a flaky payin or a
half-broken hand. It first reads `T_GAME_ID_KEY` from the player id — which
is set after the dealer writes `T_PLAYER_INFO_KEY` and the player has joined
a hand. Independent of any `game_id`, it always reads `P_JOIN_REQUEST_KEY`
(bare, no suffix), so you can see the player's last intent-to-join even
before they've been seated.

Once a `game_id` resolves, five game-scoped keys are printed:

- `P_GAME_HISTORY_KEY` — the player's per-hand log entries.
- `PLAYER_DECK_KEY` — the player's blinded deck contribution.
- `P_BETTING_ACTION_KEY` — the player's most recent action this hand.
- `P_DECODED_CARD_KEY` — cumulative snapshot of cards this player has
  successfully decoded (community + hole-card acknowledgments).
- `P_DISPUTE_REQUEST_KEY` — populated only if the player has raised a
  dispute against their payin.

### `dealers`

There is one well-known aggregator identity that lists every registered
dealer in the deployment under the unsuffixed `DEALERS_KEY`.
`print_dealers_id` reads exactly that one key. It's a one-line check for
"which dealers does this deployment know about?"; the equivalent for
operators who don't want to remember the key name is `./bet list dealers`,
which calls `poker_list_dealers()` internally.

### `cashiers` (alias `c`)

This is the printer that pays for itself once cards start moving. It reads
four things on the cashier identity:

- `CASHIERS_KEY` (unsuffixed) — the aggregator entry for this cashier.
- `T_GAME_ID_KEY` on the cashier id. The cashier keeps its own copy of the
  active `game_id`; if this is missing, the cashier hasn't been wired into a
  game yet and the remaining keys won't resolve.
- `C_DISPUTE_RESULT_KEY` (suffixed) — the result of any dispute resolution
  the cashier has run for the active game.
- The ten `all_c_b_p_keys[]` entries (`c_b_p1_deck` … `c_b_p9_deck`,
  suffixed) — the cashier-owned blinded decks, one per seat.
- `C_CARD_BV_KEY` (suffixed) — the running blinding-value table the cashier
  publishes during reveals.

The last two are the keys that used to live on the table id. If you're
debugging deck shuffling or reveal failures, this is the printer that shows
you the cashier's current view.

---

## `print_table_key <table_id> <key>`

`print_table_key_info` (`print.c:157`) is the targeted complement to
`print_id table`: instead of dumping every game-scoped key on the table, it
prints exactly one. The command reads `T_GAME_ID_KEY` from the table to
discover the current `game_id`, suffixes the user-supplied key with that
`game_id`, and prints the resulting value.

```
./bet print_table_key t1.sg777z.VRSCTEST@ t_betting_state
```

Use this when you already know which key you want — typically
`t_betting_state` during a stuck hand, or `t_board_cards` to confirm a flop
landed. The key name is the **short** form (e.g. `t_betting_state`), not the
VDXF id; `get_key_data_vdxf_id` resolves it internally.

---

## `print <id> <key>`

The original CLI shortcut, kept for backward compatibility. Implemented in
`print.c:193` as `print_vdxf_info`, it supports a fixed subset of keys —
anything outside that subset prints
`Print operation is not supported for the given ID: <id> and key: <key>`.

The supported keys are:

- `T_TABLE_INFO_KEY` (unsuffixed JSON read)
- `T_PLAYER_INFO_KEY` (unsuffixed JSON read)
- `DEALERS_KEY` (unsuffixed JSON read, expects the aggregator id)
- `T_GAME_ID_KEY` (returns the current `game_id` as a string)
- `T_GAME_INFO_KEY` (decoded — prints `Game ID` then `Game State: <name>`)

Note that the suffix-handling is inconsistent: `T_GAME_INFO_KEY` is
auto-suffixed with `game_id` internally, the other JSON keys are not. For
anything more elaborate — game-scoped table keys, player keys, cashier keys
— reach for `print_id` or `print_table_key` instead. `print` is mostly
useful as a one-liner for a quick aggregator check or to fetch the raw
`game_id` string for scripting.

---

## `print_keys <id> [block_height]` and `show <id> [--height <block>]`

Both commands call `poker_print_table_keys(id, block_height)`
(`poker_vdxf.c:402`). They differ only in argument style: `print_keys` takes
an optional positional `block_height`; `show` takes an optional `--height`
flag. With no argument either way, the starting block defaults to
`chips_get_block_count() - 100`.

The output has two sections. The first is a fixed list of nine well-known
table keys, read from `block_height` onwards using the cumulative
`getidentitycontent`-style lookup:

```
T_GAME_ID_KEY            Game ID
T_TABLE_INFO_KEY         Table Info (max_players, BB, SB)
T_PLAYER_INFO_KEY        Player Info (players + payin amounts)
T_GAME_INFO_KEY          Game Info (state, round)
T_BETTING_STATE_KEY      Betting State
T_D_DECK_KEY             Dealer Deck
T_B_DECK_KEY             Blinder Deck
T_BOARD_CARDS_KEY        Board Cards
T_SETTLEMENT_INFO_KEY    Settlement Info
```

Each is printed as `(not set)` if absent, as JSON if it parses, as raw
string otherwise. If `T_GAME_ID_KEY` resolves, a second section follows
that re-reads `T_PLAYER_INFO_KEY` and `T_TABLE_INFO_KEY` suffixed with the
discovered `game_id` — useful when the unsuffixed reads are stale and you
want the live per-game view.

This is the right command to reach for when you're not sure what state a
table is in or you've just walked back into an old game. `block_height`
lets you cap how far back you read — set it to the height the game started
to suppress noise from earlier hands on the same table.

---

## When to reach for which

For a quick "what is this id?" peek with a known role, use `print_id`. For
a targeted single-key lookup against a table mid-hand, use
`print_table_key`. For the kitchen-sink view of a table id, including
stale history and decoded game state names, use `print_keys` or `show`.
`print` survives mostly for muscle memory and for the aggregator
shortcuts (`T_GAME_ID_KEY`, `DEALERS_KEY`); new code paths should prefer
`print_id` or `print_table_key`.

All five commands are read-only and safe to invoke against any identity
the daemon's wallet can resolve — none of them issue `updateidentity` or
spend a UTXO.
