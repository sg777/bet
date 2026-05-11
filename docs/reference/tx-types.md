# Transaction Types

`bet`'s on-chain footprint reduces to three primitive transaction
types on the Verus chain it is paired with. All of them are
standard Verus operations — `bet` does not invent any custom
transaction format on top.

| # | Type                                  | Verus RPC         | What it carries                                                            |
|---|---------------------------------------|-------------------|----------------------------------------------------------------------------|
| 1 | **Payin**                             | `sendcurrency`    | Player's stake going from the player's wallet to the cashier identity.     |
| 2 | **Payout**                            | `sendcurrency`    | Cashier returning settled funds to the winning players' identities.        |
| 3 | **Identity update (CMM write)**       | `updateidentity`  | Any game-state change — game ID, deck shuffle, bets, reveals, settlement.  |

The legacy "dust transaction carrying a JSON payload" pattern, where
betting actions and game state were encoded into OP_RETURN-style data
fields on a low-value transaction, is **no longer used**. All game
state writes are now identity contentmultimap (CMM) updates.

---

## 1. Payin

A player commits to a hand by sending the table stake (in the chain's
native currency) from their wallet address to the cashier identity:

```
verus -chain=VRSCTEST sendcurrency "<player_addr>" '[
  {
    "address":  "cashier.<parent>@",
    "currency": "VRSCTEST",
    "amount":   0.5
  }
]'
```

There is **no embedded data field** in the payin. The cashier
discovers the payin by watching its own incoming UTXOs against the
join intents written by each player to the table identity's CMM (the
`T_PLAYER_INFO_KEY` family); see
[`PLAYER_JOIN_FLOW.md`](player-join-flow.md) for the matching
logic.

Older versions encoded `{ table_id, player_id, dispute_addr, ... }`
in the transaction's data part and a legacy multi-cashier multisig
script in the output. That format is preserved in
`docs/archive/` for historical interest only — neither the cashier
nor the dealer in the current build reads it.

---

## 2. Payout

After settlement, the cashier issues one or more `sendcurrency`
transactions returning funds to the winning identities. The amounts
follow whatever payout decision the dealer recorded via the
`T_GAME_RESULT_KEY` and `C_*` settlement keys (see
[`ids_keys_data.md`](vdxf-keys.md)). No
embedded data; the audit trail is the on-chain CMM history of the
table and cashier identities, which any observer can reconstruct
with `getidentitycontent`.

---

## 3. Identity updates

The bulk of `bet`'s on-chain activity is `updateidentity` calls
that rewrite a single identity's contentmultimap. Each call:

* Replaces the **entire** contentmultimap on the target identity in
  one transaction.
* Is bounded by `MAX_SCRIPT_ELEMENT_SIZE_PBAAS` (currently 5872
  bytes) for the serialised identity object.
* Must be signed by enough of the target identity's
  `primaryaddresses` to satisfy `minimumsignatures` (typically 1
  for `bet`'s dev setup).

Examples:

* Dealer writing the round counter on `t1.<parent>@`:
  `t_betting_state` key under that identity gets a JSON payload
  `{ "round": 1 }`.
* Cashier publishing per-player payin acknowledgements on
  `cashier.<parent>@`: `c_payin_ack` key per `<player_id, game_id>`.
* Player writing their deck shuffle output on `p1.<parent>@`:
  `t_d_p1_deck` family of keys.

Single-writer-per-identity (see
[`verus-overview.md`](../explanation/verus-overview.md)) is the
rule that keeps these updates from contending on the identity UTXO.

A worked example of the full set of keys written across one hand is
in [`ids_keys_data.md`](vdxf-keys.md).

---

## 4. Inspecting transactions

* `getrawtransaction <txid> 1` — JSON decode of a tx, including any
  identity-update output.
* `getidentitycontent <name@> <fromHeight> -1` — replay all CMM
  writes from a given block height up to the chain tip. Use this
  to reconstruct a game from its table identity.
* `./bin/bet print_id <id>` (and related subcommands) — readable
  CLI view over the same CMM data; see
  [`cli-print.md`](cli-print.md).

The legacy `./bin/bet extract_tx_data <txid>` command parsed an
embedded data part out of a transaction — that command and the
underlying packing scheme are no longer wired (the `chips_extract_tx_data_in_JSON`
helper returns `NULL` for current `bet` transactions because they
no longer carry any embedded payload).
