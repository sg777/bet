# Ad-hoc multisignature in Verus identities

An ad-hoc multisignature is a signing scheme where the set of authorized
signers and the threshold can change over time without changing the
address that holds the funds. Traditional Bitcoin-style m-of-n
multisignature has no equivalent: the address is itself a function of
the pubkey set, so adding or removing a signer produces a new address
and forces every depositor to switch over. `bet` historically lived
inside that constraint — `bet_compute_m_of_n_msig_addr` in
`poker/src/cashier.c:114` walked the configured cashier pubkeys and
derived a P2SH multisig address per game.

Verus identities sidestep the problem at the protocol level. Every Verus
identity carries three fields that together define its signing policy:

- `primaryaddresses` — the list of R-addresses (or other identities)
  whose signatures are accepted as authority over this identity. This is
  the equivalent of the "n" set.
- `minimumsignatures` — the threshold. Updates to the identity, and
  spends of any UTXO owned by the identity, require at least this many
  signatures from `primaryaddresses`.
- `revocationauthority` and `recoveryauthority` — separate identities
  that hold the right to revoke or recover the identity itself, kept
  outside the day-to-day signing set.

All three fields are part of the identity record, and updating any of
them is just another `updateidentity` transaction. The identity's name
and its i-address — the thing players send funds to — stay constant
across signer rotations.

---

## How `bet` uses it

Player payins flow into the cashier identity's i-address. `vdxf.c:join_table`
issues `verus_sendcurrency_data(cashier_fqn, payin_amount, NULL)`, which
the daemon resolves to the cashier identity's current i-address. The
funds become UTXOs owned by that identity, spendable only by holders of
its `primaryaddresses` keys up to the `minimumsignatures` threshold. The
cashier identity is therefore both the visible "deposit address" for the
table and the multi-sig vault holding the funds during a hand.

At settlement time the cashier sends payouts out of those UTXOs via a
plain `sendcurrency` from the cashier identity. In a single-cashier dev
setup (`minimumsignatures = 1`, one primary address held by the operator
running the cashier node) this is a one-step signature and the bet
binary signs and submits the transaction directly. In a multi-cashier
production setup, the cashier identity would hold multiple primary
addresses and `minimumsignatures > 1`; assembling the required
signatures becomes a Verus daemon concern (partial signatures collected
via `signrawtransactionwithwallet` on each cosigner and combined before
broadcast). `bet` itself does not implement signature coordination
logic — it submits a `sendcurrency` and relies on the daemon's wallet
to handle whatever signing policy the identity's record defines.

The operator-facing benefit follows directly. Rotating the cashier set
— adding a new cashier operator, removing a compromised one — is a
single `updateidentity` against the cashier identity. Players keep
sending payins to the same `cashier.sg777z.VRSCTEST@` they always did;
no client-side reconfiguration is required.

---

## What `bet` is not doing today

The legacy P2SH m-of-n multisig path is still compiled into the binary
but is dead at runtime. Specifically:

- `bet_compute_m_of_n_msig_addr` (`cashier.c:114`) is still invoked at
  startup from `bet.c:313`. It returns a `legacy_m_of_n_msig_addr`
  string derived from configured cashier pubkeys via the daemon's
  `addmultisigaddress` RPC.
- `legacy_m_of_n_msig_addr` is read by `bet_player_log_bet_info`
  (`payment.c:74`), which is in turn called from
  `bet_player_small_blind` and `bet_player_big_blind` in `states.c`. An
  explicit comment at `states.c:657` flags this as "a dead
  nanomsg/multisig-era code path"; in practice the call either no-ops
  (when the legacy address is `NULL`) or writes a side-tx that nothing
  reads.
- The actual betting actions reach the chain via
  `P_BETTING_ACTION_KEY` on the player identity (see
  `docs/reference/player-join-flow.md`), which is the Verus-identity path
  described above. The legacy global is read for backward
  compatibility but doesn't gate the current flow.

Removing the legacy startup computation and the related globals is a
housekeeping cleanup tracked alongside the broader nanomsg removal in
`docs/TODO.md`. The doc you're reading is here to describe the
Verus-identity behaviour, not the legacy carcass.
