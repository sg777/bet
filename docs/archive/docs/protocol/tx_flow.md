# Transaction Flow (legacy)

> **Deprecated.** This document walked through an example two-player
> hand on the CHIPS-era `bet`, with `extract_tx_data` dumps showing
> each betting move encoded as an OP_RETURN-style data field on its
> own dust transaction, and a final payout transaction carrying the
> full game state JSON.
>
> The current Verus build does not work this way. Game state is
> stored in identity contentmultimaps, not in transaction data. The
> only on-chain transactions are payin/payout (`sendcurrency`) and
> identity updates (`updateidentity`). See
> [`tx_types.md`](../../../reference/tx-types.md) for the modern wire-level taxonomy
> and
> [`ids_keys_data.md`](../../../reference/vdxf-keys.md) for the
> per-key reference.
>
> A complete end-to-end walkthrough of a hand in the current build
> is in [`game_state.md`](../../../reference/game-states.md) (state
> machine) and
> [`PLAYER_JOIN_FLOW.md`](../../../reference/player-join-flow.md) (join + payin).
>
> This file is kept only to preserve inbound links during the
> documentation reorganisation and will be archived under
> `docs/archive/` in a follow-up cleanup.
