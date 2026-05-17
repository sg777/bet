# Player GUI WebSocket Protocol (superseded)

> **Superseded.** The canonical, code-verified GUI protocol reference is now
> [`docs/protocol/GUI_MESSAGE_FORMATS.md`](../../../reference/gui-message-formats.md).
> That document covers dealer, player, and cashier frontends, including the
> per-node wallet-method support matrix, the actual betting reply format
> (single-element `possibilities` with integer action codes from
> `enum action_type` in `bet.h`), the warning/error envelopes, and the
> showdown payload (`finalInfo.showInfo.allHoleCardsInfo` /
> `boardCardInfo`).
>
> Diverges from this file in a few important places:
> - The `betting` reply in this doc uses `{ action: "raise", amount: N }`
>   strings; the backend (`states.c::bet_player_round_betting`) actually
>   reads an integer from `possibilities[0]` and ignores the string
>   `action`.
> - The `player_init_state` messages enumerated in the example session
>   below describe a state machine tied to the removed GUI-join-approval
>   flow. Most of those states are no longer emitted; only
>   `backend_status` and `walletInfo` survive on the modern path.
>
> Kept in place so existing inbound links don't 404. To be archived in
> a follow-up cleanup.
