# Deferred work

Items that are designed/discussed but intentionally **not** part of the current iteration. The current code paths these would replace are working and we don't want to disturb them while we stabilise the payin / join flow on a fresh chain.

---

## 1. Dealer reads player IDs from `dealer.ini`

**Status:** deferred. Hardcoded `known_players[] = {"p1"..."p9", NULL}` is duplicated in three places and stays as the discovery list for now:
- `poker/src/poker_vdxf.c:poker_poll_players_for_joins` (dealer)
- `poker/src/blinder.c:known_players` (cashier — added with the cashier-side join discovery in `PLAYER_JOIN_FLOW.md` §1.4)
- `poker/src/vdxf.c:cashier_poll_disputes` (cashier dispute polling, currently dormant — see item 2 below)

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

## 2. Graceful dispute resolution

**Status:** deferred. The functions exist in `poker/src/vdxf.c` (`cashier_poll_disputes`, `cashier_resolve_dispute`) and the on-chain key layout is defined (`P_DISPUTE_REQUEST_KEY` on the player ID, `C_DISPUTE_RESULT_KEY` on the cashier ID). The cashier's main loop (`handle_game_state_cashier` in `blinder.c`) **does not call `cashier_poll_disputes`** today — the path is dormant.

**What needs to be answered before we wire it up:**
1. **When is it safe for the cashier to resolve a dispute?** A dispute can only be resolved when no game referencing the disputed `payin_tx` is active. Today there is no `is_game_active(game_id)` check — `cashier_resolve_dispute` would happily process a dispute against a payin in a game still in progress.
2. **Single-threaded vs. dedicated worker.** The cashier today is single-threaded — `handle_game_state_cashier` runs the game loop on the main thread. Dispute polling on the same thread blocks the game loop; on a separate thread it has to coordinate with the game-state machine for the "is the game active?" guard. Pick one explicitly.
3. **Refund correctness.** `cashier_resolve_dispute` writes `C_DISPUTE_RESULT_KEY` and (presumably) issues a refund `sendcurrency`. Need to confirm idempotency: re-running on the same dispute must not double-refund.
4. **Discovery list.** Same `known_players[]` problem as item 1 — once we move to config-driven players, dispute polling iterates the same list.

**Touched when picked up:**
- `poker/src/blinder.c` (`handle_game_state_cashier` — invoke `cashier_poll_disputes` at a defined cadence/state)
- `poker/src/vdxf.c` (`cashier_resolve_dispute` — add active-game guard, audit refund idempotency)
- Possibly a new `poker/src/game.c` helper `is_game_active(game_id)` reading `T_GAME_STATE_KEY` / `T_GAME_RESULT_KEY` from the table id.

---

## Out of scope here

- Replacing or deleting `process_block` (cashier-side blocknotify path). It's currently dead code for joins (see `PLAYER_JOIN_FLOW.md` §1.3) but the decision of whether to delete vs. repurpose stays open until joins are stable.
