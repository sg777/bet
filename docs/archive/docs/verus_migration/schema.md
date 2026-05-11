# Local SQLite schema — superseded

The original content of this file documented three SQLite tables
(`player_deck_info`, `dealer_deck_info`, `cashier_deck_info`) as the
local-storage schema for `bet`. Two things made that document
obsolete:

- **The schemas drifted.** The actual `player_deck_info` schema in
  `poker/src/storage.c:35` has eight columns (`game_id`, `tx_id`,
  `pa`, `table_id`, `dealer_id`, `player_id`, `player_priv`,
  `player_deck_priv`), not the five the old doc showed. The
  documented schema omitted `tx_id`, `pa`, `table_id`, and
  `dealer_id` — fields the current code reads on rejoin.
- **The doc was self-deprecating.** It ended with *"Note: Since we
  are moving to use VDXF IDs for bet, we are not storing any
  information in the local database"*, which was inconsistent with
  the rest of its own content and with what the code actually does.

The truthful picture of the local SQLite database is now covered
between two documents:

- **[Verus Overview](../../../explanation/verus-overview.md)** — the "What's still on
  the local SQLite DB" section explains why 14 tables get created
  on startup, which of them are pre-Verus leftovers, and which one
  (`player_local_state`) is the only table the current build
  actively reads.
- **[Player Rejoining](../../../explanation/player-rejoin.md)** — inlines both the
  `player_deck_info` and `player_local_state` schemas with their
  per-column semantics, because that's the only flow that depends
  on them.

This file is kept as a stub during Phase 1 of the documentation
refresh so existing inbound links still resolve. Phase 2 will decide
whether to fold this stub into an `archive/` tree or delete it
outright.
