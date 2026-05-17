# Player rejoining

`bet` lets a player disconnect mid-hand and reconnect later without
losing their seat or their stake. The rejoin path is the only reason
the local SQLite database in `~/.bet/db/pangea.db` still has live
readers — everything else the player needs is on-chain. This document
describes what gets stored, where, and how reconnection actually
proceeds.

If a player fails to reconnect before the dealer's per-turn timeout
expires (currently 120 seconds **and** 12 blocks, whichever is higher),
the dealer treats their next action as an auto-fold and the hand
continues. The thresholds are compile-time constants
`BET_TURN_TIMEOUT_SECS` and `BET_TURN_TIMEOUT_BLOCKS` in
`poker/include/common.h`; the timeout check itself lives in
`poker/src/game.c:verus_check_turn_timeout`. Both conditions must be
satisfied — a single slow block or a single slow player won't trigger
a fold on its own.

---

## What needs to survive a player crash

Three categories of state need to be available after the player
process restarts:

**Identity and config.** The player's payin-recipient identity, the
table identity it joined, and the dealer identity all come from
`poker/config/verus_player.ini`. These are read fresh at startup;
nothing about the player's identity itself is volatile state.

**Deck secrets.** During deck initialization (`player_init_deck`) the
player generates a key per card and a per-player shuffling permutation.
These are written into the `player_deck_info` SQLite table
(`storage.c:35`) with schema
`(game_id, tx_id, pa, table_id, dealer_id, player_id, player_priv, player_deck_priv)`.
The `player_deck_priv` column stores the serialized deck-key array
(varchar 4000 is sized for the full 52-card set), and `player_priv`
stores the per-game player keypair. These secrets are local to the
node — they are not posted on-chain, encrypted or otherwise.

**Game progress.** As the game advances, the player writes a running
snapshot into the `player_local_state` table (`storage.c:39`) with
schema
`(game_id, table_id, payin_tx, player_id, decoded_cards, cards_decoded_count, last_card_id, last_game_state)`.
The `decoded_cards` field is a comma-separated string holding the
seven cards the player has so far decoded (2 hole + 5 board);
`last_game_state` records the dealer's authoritative game-state value
at the time of the last save. This is what makes "where were we?"
answerable after a restart.

---

## The reconnect path

When the player binary restarts and finds a `game_id` already
established on the table identity (`player.c:1430`-ish onwards), it
follows a fixed sequence rather than going through a fresh join:

1. Read the `game_id` from the table identity's
   `T_GAME_ID_KEY`.
2. Load `player_deck_info` from SQLite keyed by that `game_id`. If
   the row is missing, the player cannot rejoin this specific hand —
   the deck keys are gone — and bails out with
   `ERR_GAME_ALREADY_STARTED`, expecting to be reseated at the next
   hand.
3. Load `player_local_state` from SQLite. If present, the player now
   knows its `payin_tx`, the cards it had already decoded, and the
   `last_game_state` it had seen. If missing, the player initializes a
   fresh local-state row but continues with the loaded deck info.
4. Enter the normal game event loop. The loop reconstructs the
   round-by-round betting state by reading the dealer's
   `T_BETTING_STATE_KEY` history via `getidentitycontent` (see
   `docs/explanation/getidentitycontent.md`), so the player picks
   up exactly where the dealer's view of the hand is, not where the
   player's last crash was.

Decoded cards are persisted incrementally as the player processes each
reveal: `update_player_decoded_card` in `storage.c:893` writes one row
update per card. If the player crashes between two reveals, on rejoin
the loaded `decoded_cards` array shows what it had already done and
the player resumes from there.

---

## What is deliberately not yet wired up

A few rejoin-related ideas appear in older documents and design notes
but are not implemented in the current build. Calling them out so
readers don't expect them to work:

**Payin reversal on early disconnect.** Older notes describe the
dealer requesting a reversal of the payin transaction if a player
disconnects before deck shuffling, and a percentage-based penalty if
they disconnect mid-shuffle. The code does not contain that path
today. A player who disconnects pre-shuffle currently has their seat
freed by the timeout mechanism, but the payin transaction stays at
the cashier identity until the dispute flow (below) is wired up.

**Player-initiated dispute to reclaim payin.** The on-chain key layout
for player disputes (`P_DISPUTE_REQUEST_KEY` on the player identity,
`C_DISPUTE_RESULT_KEY` on the cashier) is defined, but the cashier's
main loop does not currently call `cashier_poll_disputes` or
`cashier_resolve_dispute`. This path is the open work tracked by
`docs/TODO.md` item 3.

**Encrypted deck secrets on the player identity.** Some earlier
documents floated the idea of storing the deck keys themselves on the
player's Verus identity, encrypted under a PIN held only by the
player, so a brand-new install on a different machine could rejoin
just by knowing the PIN. The current build keeps these secrets in
local SQLite only; nothing is uploaded anywhere. Migrating to a
remembered-PIN model would mean reworking `save_player_local_state`
and `load_player_local_state` to round-trip through the chain, and is
not on the near-term roadmap.
