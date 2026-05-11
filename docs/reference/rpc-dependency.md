# Daemon Compatibility Surface

`bet` does not embed the Verus daemon; it shells out to the `verus`
CLI for every on-chain operation. The compatibility contract between
`bet` and a given Verus build is therefore exactly the set of RPC
commands and their response shapes that `bet`'s wrappers consume.

When upgrading the paired Verus daemon (or porting `bet` to a
different Verus fork), the commands listed below are the surface to
re-validate.

---

## Identity / VDXF

| RPC                  | Used by               | Purpose                                                                  |
|----------------------|-----------------------|--------------------------------------------------------------------------|
| `getidentity`        | dealer, player, cashier | Read identity object: i-address, `primaryaddresses`, content hash, etc. |
| `getidentitycontent` | dealer, player, cashier | Replay CMM updates from a given block height. Supports the `<fromHeight> <toHeight>` form `bet` uses for windowed reads. |
| `updateidentity`     | every writer node     | Rewrites the entire CMM on an identity. Size capped at `MAX_SCRIPT_ELEMENT_SIZE_PBAAS` (5872 bytes). |
| `getvdxfid`          | every node            | Resolves human-readable key names to 32-byte vdxfids. Verified at startup. |

These four are the load-bearing identity APIs — a regression in any
of them blocks every gameplay path.

## Wallet

| RPC                    | Used by                  | Purpose                                                |
|------------------------|--------------------------|--------------------------------------------------------|
| `getnewaddress`        | all                      | Generate a fresh R-address for incoming funds.         |
| `validateaddress`      | all                      | Address-format check before importing or sending.      |
| `importaddress`        | cashier, dealer          | Watch-only registration for legacy multisig paths.     |
| `listaddressgroupings` | all                      | Powers the `withdrawRequest` GUI response.             |
| `listunspent`          | all                      | UTXO selection for raw-tx assembly.                    |

## Transactions

| RPC                    | Used by                                  | Purpose                                                              |
|------------------------|------------------------------------------|----------------------------------------------------------------------|
| `sendcurrency`         | players (payin), cashier (payout)        | Native currency send. The main money-moving call.                    |
| `createrawtransaction` | legacy multisig path                     | Used by the dormant `legacy_m_of_n_msig_addr` code; not exercised at runtime in the Verus build but still compiled. |
| `signrawtransaction`   | legacy multisig path                     | Same.                                                                |
| `sendrawtransaction`   | legacy multisig path, dispute-resolution | Same.                                                                |
| `addmultisigaddress`   | legacy multi-cashier setup               | Pre-Verus multisig vault construction. Not used today.               |
| `getrawtransaction`    | dealer, cashier                          | Inspecting a payin tx during reconciliation.                         |
| `decoderawtransaction` | dealer, cashier                          | Same.                                                                |
| `getrawmempool`        | dealer                                   | Detecting stuck payouts.                                             |

## Chain state

| RPC                | Used by | Purpose                                                                    |
|--------------------|---------|----------------------------------------------------------------------------|
| `getblockcount`    | all     | Heartbeat / "wait for confirmation" loops.                                 |
| `getblock`         | dealer  | Resolving a confirmation height to a block timestamp.                      |

---

## What is **not** required

* No shielded-tx RPCs (`z_*`). `bet` operates entirely on transparent
  outputs.
* No `signmessage` / `verifymessage` flows — message authentication
  comes from identity-CMM signatures via `updateidentity`.
* No Lightning Network commands. The LN integration that the
  original CHIPS-era `bet` used (`pay`, `invoice`, `fundchannel`,
  etc.) was removed in early 2025. Any `bet` code referencing those
  paths is dormant.

---

## Compatibility checklist when upgrading Verus

1. `verus getvdxfid <name>` returns the same 32-byte id for every
   key name in `keys.ini` and `vdxf.h` (the chain has no concept of
   "version" here, but the hash function being stable is the
   contract).
2. `verus updateidentity` accepts the JSON identity object format
   `bet` constructs in `vdxf.c::format_update_payload` (look for
   any new required fields).
3. `verus getidentitycontent <name@> <from> <to>` still returns a
   `contentmultimap` array merged over the requested height range.
4. `verus sendcurrency <from> '[ { address, currency, amount } ]'`
   still returns the txid on the first line of stdout (the wrappers
   in `commands.c` parse stdout, not JSON-RPC).

A regression at the wire-format level on any of the above is the
likely failure mode of a Verus upstream port.
