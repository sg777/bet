# Cashier Configuration

The cashier is the node that custodies player payins and disburses
payouts. In the current build there is **one** operational cashier
identity per deployment (the dev regtest uses `cashier.<parent>@`),
in contrast to the legacy multi-cashier multisig election the
original CHIPS-era cashier doc described.

The cashier reads the same four configuration files as the other
nodes (see [Dealer Configuration §0](dealer-config.md)) but
the only cashier-specific knob lives in `cashier.ini`:

```ini
[cashier]
gui_ws_port = 9002
```

| Key            | Default | Meaning                                                                                |
|----------------|--------:|----------------------------------------------------------------------------------------|
| `gui_ws_port`  | `9002`  | Local WebSocket port the cashier exposes for the (wallet-only) GUI front-end (§GUI Message Formats §5). |

Everything else the cashier needs — its operational identity name,
the parent namespace, the VDXF key prefix, and the chain RPC — comes
from `keys.ini` and `blockchain.ini`, which are shared with the
dealer and player nodes.

The shipped `cashier.ini` also contains a commented-out `node-N`
block listing legacy CHIPS cashier IPs and pubkeys. Those entries
are parsed (`bet_parse_cashier_config_ini_file` in `config.c`) into
`notary_node_ips[]` / `notary_node_pubkeys[]` arrays that feed the
disused multi-cashier multisig path. They have no effect on the
current single-cashier Verus deployment and can be left commented
out.

---

## Becoming a cashier

There is no on-chain admission control for the cashier role today.
To stand up a new cashier:

1. Register an operational cashier identity (e.g. `cashier.<parent>@`)
   on the chain you are using. See
   [`id_creation_process.md`](../explanation/identity-tree.md)
   for the registration flow.
2. Make sure the node's local Verus wallet holds a key authorized
   in the identity's `primaryaddresses` so the cashier can sign
   `updateidentity` and `sendcurrency`.
3. Add the identity short name to the `cashiers.<parent>@`
   aggregator identity's CMM if you want the dealer to discover it
   via `poker_list_cashiers()`.
4. Start the node:

   ```bash
   cd poker
   ./bin/cashierd cashier
   # or, equivalently:
   ./bin/bet cashier
   ```

The two entrypoints (`cashierd` and `bet cashier`) are the same
binary; `cashierd` is a copy installed by the Makefile for legacy
launcher scripts.

---

## Funds and key management

The cashier's wallet holds all in-flight payin funds for the
lifetime of a hand. Operational discipline:

* The cashier identity's `primaryaddresses` determines who can sign
  payouts. For higher-trust deployments, the same identity can be
  reconfigured to require multiple signatures (see
  [`adhoc-multisig.md`](../explanation/adhoc-multisig.md)) —
  this is a runtime `updateidentity` change, not a code change.
* `revocationauthority` and `recoveryauthority` should be set to
  identities held by a separate operator. See
  [`revoke-recovery.md`](../tutorials/revoke-recovery.md).
* The `withdraw` / `withdrawRequest` WebSocket methods exposed by
  the cashier's GUI port let the operator drain the wallet to an
  arbitrary R-address; they are not rate-limited and assume a
  trusted operator at the keyboard.

---

## GUI surface

The cashier's WebSocket front-end exposes only the four shared
wallet methods (`get_bal_info`, `get_addr_info`, `withdrawRequest`,
`withdraw`). It has no game-specific UI. See
[`GUI_MESSAGE_FORMATS.md § 5`](gui-message-formats.md).
