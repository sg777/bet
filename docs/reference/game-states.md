# The game state machine

A `bet` table moves through a fixed sequence of states as a hand
unfolds. The state lives on the table identity's contentmultimap as
an integer field named `game_state` inside the value of
`T_GAME_INFO_KEY.<game_id>` (see `game.c:get_game_state` for the
authoritative read path), and the dealer updates it after each
transition. Players, the cashier, and any external
observer can read the same value to know where the game is. The
state machine is what makes it possible for an actor to disconnect
mid-hand and rejoin cleanly: the current state is on-chain, and the
next action of every actor is a function of that state.

This document lists every state in the current implementation, who
writes each transition, and the two cases where the cashier writes a
parallel "cashier-side" state on its own identity that the dealer
later canonicalizes onto the table.

---

## States in declaration order

The full list is defined in `poker/include/game.h:6-29`. Sequence
matches the typical hand progression:

| Value | Name | What it means |
|-------|------|---------------|
| 0  | `G_ZEROIZED_STATE`                | Table identity exists but has no active game. |
| 1  | `G_TABLE_ACTIVE`                  | Table is configured and waiting for the dealer to start a hand. |
| 2  | `G_TABLE_STARTED`                 | A new hand has been started; players may join via `P_JOIN_REQUEST_KEY`. |
| 3  | `G_PLAYERS_JOINED`                | Enough players have committed payins; deck initialization can begin. |
| 4  | `G_DECK_SHUFFLING_P`              | Players have published their public decks under `PLAYER_DECK_KEY`. |
| 5  | `G_DECK_SHUFFLING_D`              | Dealer has shuffled and blinded each player's deck onto `T_D_P<n>_DECK_KEY`. |
| 6  | `G_DECK_SHUFFLING_B`              | Cashier has reshuffled/reblinded and the dealer has canonicalized onto `T_B_P<n>_DECK_KEY`. |
| 7  | `G_REVEAL_CARD`                   | A card-reveal cycle is in progress: dealer has prompted, cashier publishes blinding values, players decode. |
| 8  | `G_REVEAL_CARD_P_DONE`            | All non-folded players have published a `P_DECODED_CARD_KEY` snapshot for the prompted card(s). |
| 9  | `G_ROUND_BETTING`                 | A betting round is open; the dealer is collecting `P_BETTING_ACTION_KEY` from each player in turn. |
| 10 | `G_SHOWDOWN`                      | All four betting rounds are over; non-folded players publish hole cards via `P_HOLECARDS_REVEAL_KEY`. |
| 11 | `G_SETTLEMENT_PENDING`            | Dealer has computed winner(s) and written `T_SETTLEMENT_INFO_KEY`; cashier may begin payouts. |
| 12 | `G_SETTLEMENT_COMPLETE`           | Cashier payouts confirmed; canonical end-of-hand. Dealer writes this on the table identity. |
| 13 | `G_SETTLEMENT_COMPLETE_BY_CASHIER` | Cashier-side mirror of `G_SETTLEMENT_COMPLETE`, written on the cashier identity (see below). |

The string names returned by `game_state_str` (`game.c:32-62`) match
this table. `./bet print_id table` decodes the integer back into the
human-readable name when inspecting the table identity from the CLI.

---

## Who writes which state

The dealer writes every state on the table identity. That is what
the single-writer-per-identity rule for the table id requires: the
dealer is the sole writer, and game-state transitions are part of
its sole-writer responsibility.

Two states are different because they are *cashier work products*. In
both cases, "cashier-side state" means the cashier writes a similar
`game_state` field into `T_GAME_INFO_KEY.<game_id>` *on its own
identity* — the dealer is still the sole writer of the table
identity's `T_GAME_INFO_KEY`.

- **`G_DECK_SHUFFLING_B` (cashier's reshuffle done).** The cashier
  writes its per-player blinded deck output to its own identity
  (`C_B_P<n>_DECK_KEY`). The dealer polls the cashier identity,
  sees those keys populated, canonicalizes them onto the table
  identity (`T_B_P<n>_DECK_KEY`), and at that point writes
  `G_DECK_SHUFFLING_B` to advance the game state.
- **`G_SETTLEMENT_COMPLETE` / `G_SETTLEMENT_COMPLETE_BY_CASHIER`.**
  The cashier writes `G_SETTLEMENT_COMPLETE_BY_CASHIER` into the
  `game_state` field of `T_GAME_INFO_KEY.<game_id>` on its own
  identity once `cashier_process_settlement` has issued all payouts. The dealer polls
  (`is_cashier_settlement_complete`) and, on observing it, writes
  the canonical `G_SETTLEMENT_COMPLETE` on the table identity.

In both cases the dealer is still the only writer of the table
identity. The cashier-side state is on the cashier identity, where
the cashier is the only writer. This is exactly the mirror pattern
described in `vdxf.h:19-29` for the cashier's deck output and in
`vdxf.h:280-295` for settlement.

---

## Typical transition sequence for a single hand

```
G_ZEROIZED_STATE  (table id has never seen a hand, or was reset)
    │ dealer creates a game record
    ▼
G_TABLE_ACTIVE     (waiting)
    │ dealer issues a "start hand" call
    ▼
G_TABLE_STARTED    (players may now join with payin + P_JOIN_REQUEST_KEY)
    │ dealer's poll loop verifies payins and admits players
    ▼
G_PLAYERS_JOINED   (admitted players locked in)
    │ players publish PLAYER_DECK_KEY
    ▼
G_DECK_SHUFFLING_P
    │ dealer shuffles + blinds, writes T_D_P<n>_DECK_KEY
    ▼
G_DECK_SHUFFLING_D
    │ cashier reshuffles + reblinds onto its own C_B_P<n>_DECK_KEY
    │ dealer canonicalizes onto T_B_P<n>_DECK_KEY
    ▼
G_DECK_SHUFFLING_B
    │ dealer enters the deal/bet loop
    ▼

  ┌──────────────────────────────────────────────────────┐
  │ Repeating block for each card-reveal + betting round │
  │                                                      │
  │   G_REVEAL_CARD   (dealer prompts hole/board card)   │
  │      │ cashier publishes blinding values,            │
  │      │ players decode and snapshot                   │
  │      ▼                                               │
  │   G_REVEAL_CARD_P_DONE                               │
  │      │ dealer opens the round                        │
  │      ▼                                               │
  │   G_ROUND_BETTING (players act in turn)              │
  │      │ round closes when all matching bets in        │
  │      ▼                                               │
  │   (back to G_REVEAL_CARD for next street)            │
  └──────────────────────────────────────────────────────┘

After the river round closes:
    │
    ▼
G_SHOWDOWN         (non-folded players publish hole cards)
    │ dealer evaluates hands, writes T_SETTLEMENT_INFO_KEY
    ▼
G_SETTLEMENT_PENDING
    │ cashier processes payouts to winning players' i-addresses
    │ cashier writes G_SETTLEMENT_COMPLETE_BY_CASHIER on its own id
    │ dealer observes that, writes G_SETTLEMENT_COMPLETE on table id
    ▼
G_SETTLEMENT_COMPLETE  (hand done, table can be reset to G_TABLE_ACTIVE)
```

The repeating reveal/bet block is the same shape on the preflop,
flop, turn, and river streets. Burn cards in the standard
hold'em sense are handled inside the reveal sub-protocol — see
`deck_shuffling.md` for how a specific card index is picked and
revealed.

---

## How non-dealer actors observe transitions

The cashier and every player poll the table identity's
`T_GAME_INFO_KEY.<game_id>` on a fixed cadence (using
`get_game_state` / `get_game_state_info`). When the value advances they
take their next action — issuing a payin, publishing a deck,
publishing a betting action, decoding a card. Because the state
itself is on-chain, the actors don't need any direct messaging
channel: as soon as the dealer's `updateidentity` confirms, every
poller sees the new state on its next read.

For a player rejoining mid-hand, the value of `game_state` loaded
from the table identity's `T_GAME_INFO_KEY.<game_id>` is the single
most important piece of recovery information. Combined with the local
`player_local_state.last_game_state` row (see
[`player-rejoin.md`](../explanation/player-rejoin.md)), the player knows where the
dealer is and where it was the last time the player itself saved
state — and can skip forward over any phases it already completed.

---

## Related documentation

- [`verus-overview.md`](../explanation/verus-overview.md) — single-writer rule
  that determines who writes each state, and the size constraints
  that shape the deck-shuffle states.
- [`deck_shuffling.md`](../explanation/deck-shuffling.md) — what actually happens
  during the four `G_DECK_SHUFFLING_*` states.
- [`player-rejoin.md`](../explanation/player-rejoin.md) — how the state machine
  is used during disconnect/reconnect.
- [`ids_keys_data.md`](vdxf-keys.md) — full per-key reference,
  including `T_GAME_INFO_KEY` (where `game_state` lives) and
  `T_SETTLEMENT_INFO_KEY`.
