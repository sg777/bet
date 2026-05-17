# Player Joining — superseded

The original content of this file described an older player-join flow
in which the cashier embedded a 4-byte hash of each payin transaction
into the table identity's `t_player_info` key to deduplicate cashier
updates, and the payin transaction itself carried `dealer_id`,
`player_id`, and `primaryaddress` in its data field.

That design was simplified in commit `8e5eefa9` (January 2026,
*"Simplify sendcurrency to not embed data (identity addresses don't
support data field)"*). The payin transaction is now a plain
`sendcurrency` to the cashier identity's i-address with no embedded
data, and the canonical join request lives on the player's own Verus
identity under `P_JOIN_REQUEST_KEY`.

The current flow is documented end-to-end, with code citations, in:

- **[Player Join Flow](../../../reference/player-join-flow.md)** — the live reference.

Related cross-references:

- [Player Rejoining](../../../explanation/player-rejoin.md) — disconnect/reconnect path.
- [Verus Overview](../../../explanation/verus-overview.md) — how `bet` uses Verus
  identities at the architectural level.

This file is kept as a stub during Phase 1 of the documentation
refresh so existing inbound links still resolve. Phase 2 of the
documentation reorganization will decide whether to fold this stub
into an `archive/` tree or delete it outright.
