# Deferred work

Items that are designed/discussed but intentionally **not** part of the current iteration. The current code paths these would replace are working and we don't want to disturb them while we stabilise the payin / join flow on a fresh chain.

---

## High priority

### 1. Enforce single-writer-per-identity for game-state CMM keys

**Status:** open. Audit complete; migration template established; not yet implemented.

#### Why this is the rule

The system has three on-chain identities per game:

| Identity | Owner / sole writer | What lives here |
|---|---|---|
| `table_id` | **Dealer** | canonical game state, all per-game CMM keys (`T_*_KEY.<game_id>`), state transitions |
| `cashier_id` | **Cashier** | cashier-private state + cashier-produced data + cashier-self-reported state transitions |
| `player_id` | **Player** | player-private state + player-produced data + player-self-reported state transitions |

Each identity has exactly **one** writer. Readers cross-cut: any party can read any identity. State machine progresses by:

- Each actor writes its own state transition to its own ID (signal).
- The dealer polls the other IDs, observes the signals, and writes the **canonical** transition on the table ID.

This already exists for one half of the deck-shuffle handshake — `blinder.c:499` writes `G_DECK_SHUFFLING_B` on `cashier_id`, `dealer.c:282 is_cashier_shuffled_deck()` polls it, `dealer.c:493` writes the canonical `G_DECK_SHUFFLING_B` on `table_id`. That's the pattern; the rest needs to match.

#### Audit results

Cross-checked every `poker_append_key_*`, `poker_update_key_*`, `append_game_state(...)`, and `merge_cmm_from_id_key_data_*` call site:

**Player writes (clean — never touches `table_id`):**
- `T_GAME_ID_KEY` on `verus_pid` — `player.c:94`
- `PLAYER_DECK_KEY.<gid>` on `verus_pid` — `player.c:98`
- `G_DECK_SHUFFLING_P` on `verus_pid` — `player.c:104`
- `P_DECODED_CARD_KEY.<gid>` on `verus_pid` — `player.c:154`
- `G_REVEAL_CARD_P_DONE` on `verus_pid` — `player.c:388`
- `P_BETTING_ACTION_KEY.<gid>` on `verus_pid` — `player.c:454`
- `P_JOIN_REQUEST_KEY` on `verus_pid` — `vdxf.c:join_table`
- `P_GAME_HISTORY_KEY.<gid>` on `verus_pid` — `vdxf.c:490`
- `P_DISPUTE_REQUEST_KEY` on `verus_pid` — `vdxf.c:1988`
- `sendcurrency` to cashier address — `vdxf.c:387` (wallet op, not a CMM write)

**Dealer writes (clean — table_id canonical, plus self-publish on `dealer_id`):**
- All `G_*` state transitions on `t->table_id` — `dealer.c:423, 476, 482, 487, 493, 515` and `game.c:291, 493, 519, 635, 1026`
- `T_PLAYER_INFO_KEY.<gid>`, `T_GAME_INFO_KEY.<gid>`, `T_BOARD_CARDS_KEY.<gid>`, `T_TABLE_INFO_KEY.<gid>`, `T_GAME_ID_KEY` on `t->table_id`
- `T_D_DECK_KEY.<gid>`, `T_D_P*_DECK_KEY.<gid>` (per-player blinded deck) on `t->table_id` — `dealer.c:90, 313`
- `T_SETTLEMENT_INFO_KEY.<gid>` (initial order: who gets paid, how much) on `t->table_id` — `dealer.c:411`
- `T_TABLE_INFO_KEY` advertisement on `t.dealer_id` (self-publish) — `dealer.c:587`
- Dealer-aggregator registration write on `dealers_short` — `dealer.c:55` (cluster registry, not per-game; out of scope for this rule)

**Cashier writes — four ownership violations on `table_id`:**

| # | Site | Key | Today (violator) | Should be |
|---|---|---|---|---|
| 1.1 | `blinder.c:69` (`cashier_sb_deck` via `cashier_shuffle_deck(table_id)`) | `T_B_P*_DECK_KEY.<gid>` × 10 keys (P1..P9 + B_DECK) — the blinded decks themselves | cashier writes its primary work product onto `table_id` | Cashier writes new `C_B_P*_DECK_KEY.<gid>` (10 keys, mirrored) on `cashier_id`. Dealer polls cashier id, sees the 10 keys for `<gid>` populated, copies their values onto `T_B_P*_DECK_KEY.<gid>` on `table_id`. Player reader (`player.c:255`) unchanged. |
| 1.2 | `blinder.c:168` (`reveal_bv`) | `T_CARD_BV_KEY.<gid>` | cashier writes onto `table_id` | Cashier writes a new `C_CARD_BV_KEY.<gid>` on `cashier_id`. Dealer polls cashier id, copies the value onto `T_CARD_BV_KEY.<gid>` on `table_id`. Players continue to read from `table_id` — no reader change. |
| 1.3 | `blinder.c:298` (`cashier_process_settlement`) | `T_SETTLEMENT_INFO_KEY.<gid>` (status mutation `pending → completed`, plus `payout_txs` array) | cashier mutates the dealer's key on `table_id` | Cashier writes a new `C_SETTLEMENT_RESULT_KEY.<gid>` on `cashier_id` carrying `{status, payout_txs}`. Dealer polls cashier id, sees `status: completed`, marks the canonical `status: completed` on the original `T_SETTLEMENT_INFO_KEY.<gid>` on `table_id`. The dealer-written settlement *order* (who, how much) is never mutated by the cashier. |
| 1.4 | `blinder.c:316` | `G_SETTLEMENT_COMPLETE` (terminal game state) | cashier writes terminal state directly on `table_id` (with the explicit "I'm doing this so the dealer doesn't poll" workaround comment in-source) | Cashier writes its own terminal state (e.g. `G_SETTLEMENT_COMPLETE_BY_CASHIER`) on `cashier_id`. Dealer adds a new poll case mirroring `is_cashier_shuffled_deck`, then writes the canonical `G_SETTLEMENT_COMPLETE` on `table_id`. Mirrors the existing shuffle handshake exactly. |

Cashier writes that are already clean (own id, no migration needed):
- `T_GAME_ID_KEY` on cashier own id — `blinder.c:462`
- `G_DECK_SHUFFLING_B` (the shuffle-complete *signal* — note: only the signal is on cashier_id today; the deck *data* is the violation in 1.1) — `blinder.c:499`
- `C_DISPUTE_RESULT_KEY` on cashier own id — `vdxf.c:2226` (dormant path)

Latent / dead-code violation worth noting (not a runtime issue today, structurally a violation if the path is ever revived):
- `vdxf.c:1670` `process_payin_tx_data` writes to `table_id`. When called by the dealer's join poll (`poker_poll_players_for_joins → process_player_join`) this is correct (dealer write). When called by the cashier's `process_block` (the dead blocknotify path), it would be a cashier-on-table_id violation. Leaving alone until the `process_block` cleanup decision is made.

#### Migration template (one site at a time)

For each violation site:

1. **Define a new `C_*` key macro** in `poker/include/vdxf.h` (or wherever `T_*_KEY` macros live) — opaque VDXF id, no reuse of existing `T_*` ids.
2. **Switch the cashier write** in `blinder.c` from the table-id key to the new cashier-id key. Cashier ID is `bet_get_cashiers_id_fqn()` (or short name where the helpers expect it).
3. **Add a dealer-side poller** in `dealer.c` mirroring `is_cashier_shuffled_deck`: read the new key from `cashier_id`, return the value/state.
4. **Add a dealer-side canonicalizer** at the right place in `handle_game_state` (probably alongside the existing shuffle case at `dealer.c:489-495`): when the cashier signal is observed, write the canonical entry on `table_id`.
5. **Audit readers** for the *original* key on `table_id`. They should still work — the dealer is still the writer of the canonical entry, just gated on cashier signal now. Do not change reader call sites unless the audit reveals a need.
6. **CLI auto retest** (the same end-to-end run we used to validate cashier self-discovery) — must reach `G_SETTLEMENT_COMPLETE` and lifecycle reset.

#### Recommended order (smallest blast radius first)

1. **Site 1.4 (`G_SETTLEMENT_COMPLETE`)** — single state-machine signal, exact mirror of the existing shuffle handshake. ~10 LOC change. Validates the template; once this works the rest are mechanical.
2. **Site 1.3 (`T_SETTLEMENT_INFO_KEY` status)** — adds one new `C_SETTLEMENT_RESULT_KEY`, splits write authority cleanly between dealer (order) and cashier (receipts). ~30 LOC.
3. **Site 1.2 (`T_CARD_BV_KEY`)** — single key, but written multiple times per game (per card per street). ~30 LOC.
4. **Site 1.1 (`T_B_P*_DECK_KEY`)** — biggest: 10 keys mirrored, written once per game in a tight loop. Dealer canonicalizer needs to wait for all 10 to be present before copying. Save for last so the template is solid.

Each site is a separate commit + CLI auto retest. The retest must reach `Cashier returning to idle — polling player IDs for next join` (the lifecycle we just shipped) — i.e. a full game played end-to-end with the new key on its new home.

#### Touched when picked up

- `poker/src/blinder.c` (move write target on each violation site)
- `poker/src/dealer.c` (new pollers + canonicalizer cases in `handle_game_state`)
- `poker/include/vdxf.h` + `poker/src/vdxf.c` (new `C_*_KEY` macros + their VDXF id registrations)
- `poker/src/game.c` if any of the canonicalizer logic lives in shared helpers
- `docs/PLAYER_JOIN_FLOW.md` (or a new `OWNERSHIP_MODEL.md`) — document the rule + the handshake patterns for posterity

---

## Backlog

### 2. Dealer reads player IDs from `dealer.ini`

**Status:** deferred. Hardcoded `known_players[] = {"p1"..."p9", NULL}` is duplicated in three places and stays as the discovery list for now:
- `poker/src/poker_vdxf.c:poker_poll_players_for_joins` (dealer)
- `poker/src/blinder.c:known_players` (cashier — added with the cashier-side join discovery in `PLAYER_JOIN_FLOW.md` §1.4)
- `poker/src/vdxf.c:cashier_poll_disputes` (cashier dispute polling, currently dormant — see item 3 below)

**What's already documented:** full design is in [`PLAYER_JOIN_FLOW.md`](./PLAYER_JOIN_FLOW.md) §3 — `[players] ids = p1,p2` block in `dealer.ini`, `is_id_exists` validation at startup, fail-fast on missing IDs, replacement of both `known_players[]` call sites.

**Why deferred:** the hardcoded list is functionally correct (just inefficient — up to 9 `getidentity` RPCs per 2-second poll tick instead of 2). We want to land payin / join correctness first on a clean chain before touching the discovery layer.

**Touched when picked up:**
- `poker/config/dealer.ini` (new `[players]` section)
- `poker/include/bet.h`, `poker/src/bet.c` (globals: `dealer_player_ids[][]`, `dealer_player_count`)
- `poker/include/err.h`, `poker/src/err.c` (`ERR_NO_PLAYERS_CONFIGURED`, `ERR_PLAYER_ID_NOT_FOUND`)
- `poker/src/config.c` (`bet_parse_dealer_config_ini_file` — parse `players:ids`)
- `poker/src/dealer.c` (`dealer_init`, `dealer_init_with_reset` — startup `is_id_exists` loop)
- `poker/src/poker_vdxf.c` (`poker_poll_players_for_joins` — drop `known_players[]`)
- `poker/src/blinder.c` (`cashier_poll_players_for_joins` — drop `known_players[]`)
- `poker/src/vdxf.c` (`cashier_poll_disputes` — drop `known_players[]`, takes config-driven list once dispute polling is wired up)

---

### 3. Graceful dispute resolution

**Status:** deferred. The functions exist in `poker/src/vdxf.c` (`cashier_poll_disputes`, `cashier_resolve_dispute`) and the on-chain key layout is defined (`P_DISPUTE_REQUEST_KEY` on the player ID, `C_DISPUTE_RESULT_KEY` on the cashier ID). The cashier's main loop (`handle_game_state_cashier` in `blinder.c`) **does not call `cashier_poll_disputes`** today — the path is dormant.

**What needs to be answered before we wire it up:**
1. **When is it safe for the cashier to resolve a dispute?** A dispute can only be resolved when no game referencing the disputed `payin_tx` is active. Today there is no `is_game_active(game_id)` check — `cashier_resolve_dispute` would happily process a dispute against a payin in a game still in progress.
2. **Single-threaded vs. dedicated worker.** The cashier today is single-threaded — `handle_game_state_cashier` runs the game loop on the main thread. Dispute polling on the same thread blocks the game loop; on a separate thread it has to coordinate with the game-state machine for the "is the game active?" guard. Pick one explicitly.
3. **Refund correctness.** `cashier_resolve_dispute` writes `C_DISPUTE_RESULT_KEY` and (presumably) issues a refund `sendcurrency`. Need to confirm idempotency: re-running on the same dispute must not double-refund.
4. **Discovery list.** Same `known_players[]` problem as item 2 — once we move to config-driven players, dispute polling iterates the same list.

**Touched when picked up:**
- `poker/src/blinder.c` (`handle_game_state_cashier` — invoke `cashier_poll_disputes` at a defined cadence/state)
- `poker/src/vdxf.c` (`cashier_resolve_dispute` — add active-game guard, audit refund idempotency)
- Possibly a new `poker/src/game.c` helper `is_game_active(game_id)` reading `T_GAME_STATE_KEY` / `T_GAME_RESULT_KEY` from the table id.

---

### 4. Persist `cashier_r` for crash recovery across the shuffle / BV-reveal window

**Status:** deferred. The current in-process idempotency gate (`blinder.c` `G_DECK_SHUFFLING_D`, added with item 1.1's direct-read migration) prevents the same cashier process from re-entering `cashier_init_deck` and regenerating `b_deck_info.cashier_r` while the table state is still propagating — this fixes the hot path. It does **not** survive a cashier process crash mid-game.

**Failure mode the gate doesn't cover:**

1. Cashier shuffles, writes `C_B_P*_DECK_KEY.<gid>` on its own id, signals `G_DECK_SHUFFLING_B`. `b_deck_info.cashier_r` lives only in process memory.
2. Cashier crashes (any reason — OOM, SIGSEGV, host reboot).
3. Cashier restarts. The on-chain deck is still there → idempotency gate skips the shuffle → `b_deck_info.cashier_r` is **never reinitialized**.
4. At reveal time `reveal_bv` reads `b_deck_info.cashier_r[player_id][card_id].priv` — zeroes / garbage — publishes garbage BV, decode_card returns -1 forever.

**What needs to happen:**

Persist `cashier_r` keyed by `<game_id>` so a restarted cashier can recover the same blinding values it used when it wrote the on-chain deck. Two storage candidates:

- **Cashier id (on-chain, encrypted to cashier's own key):** new `C_CASHIER_R_KEY.<game_id>` carrying the per-player `cashier_r[i][0..CARDS_MAXCARDS-1].priv` bytes, encrypted with the cashier's identity private key (or a derived symmetric key) so other parties can't read it. Pro: identity-rooted, naturally garbage-collected when the game completes via the same lifecycle reset. Con: encryption infra needed; on-chain bytes per game.
- **Local DB (`/root/.bet/db/pangea.db`):** new `cashier_blinding_values` table keyed by `(cashier_id, game_id)`. Pro: no on-chain bytes, simpler — plaintext is fine since the file is local to the cashier host. Con: lost if the disk is wiped / a new cashier host comes up; tied to a specific machine.

**Recommendation when picked up:** start with the local DB option (simpler, sufficient for crash recovery on the same host) and only escalate to encrypted-on-chain if multi-host cashier failover becomes a requirement.

**Recovery flow that needs writing:**

1. On cashier startup, load `cashier_r` from store keyed by `<game_id>` of the active table (if any).
2. In `cashier_init_deck`, if `cashier_r` for `<game_id>` is already loaded, skip the `gen_deck` calls (don't overwrite).
3. On `G_SETTLEMENT_COMPLETE` lifecycle reset, delete the entry.

**Touched when picked up:**
- `poker/src/blinder.c` (`cashier_init_deck` — load-or-generate; `G_SETTLEMENT_COMPLETE` lifecycle reset — delete)
- `poker/src/storage.c` + new schema migration (if local-DB path is chosen)
- `poker/include/vdxf.h` + `poker/src/vdxf.c` (if on-chain path is chosen — new `C_CASHIER_R_KEY` macro + encryption helpers)
- `docs/TODO.md` — close this item

---

## Out of scope here

- Replacing or deleting `process_block` (cashier-side blocknotify path). It's currently dead code for joins (see `PLAYER_JOIN_FLOW.md` §1.3) but the decision of whether to delete vs. repurpose stays open until joins are stable.
