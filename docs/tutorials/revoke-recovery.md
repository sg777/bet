# Setting up revoke and recovery authorities

Every Verus identity carries two authority fields independent of its
primary signing set: `revocationauthority` and `recoveryauthority`.

The revocation authority is the identity (or i-address) allowed to mark
this identity as revoked, removing its ability to sign or be updated
through the normal primary-address path. The recovery authority is the
identity allowed to restore control over a revoked identity — typically
by replacing its primary addresses and lifting the revocation. Both are
referenced as i-addresses on the identity record and are independent
identities themselves, so the keys that hold them never need to be in
the day-to-day signing wallet.

`bet` does not create these identities programmatically — they're set
up once by the operator using the Verus CLI, and then referenced by
i-address when registering all the operational identities under them
(`sg777z.VRSCTEST@`, `dealer.sg777z.VRSCTEST@`, `cashier.sg777z.VRSCTEST@`,
`p1.sg777z.VRSCTEST@`, etc.). This document walks through that one-time
setup.

The examples below use VRSCTEST and assume a parent identity already
registered at `<PARENT-I-ADDRESS>`. R-addresses come from
`verus -chain=VRSCTEST getnewaddress`; identity i-addresses are the
`nameid` field returned by `registernamecommitment`.

---

## Two-step identity registration in Verus

Every Verus identity is created in two transactions:

1. `registernamecommitment <name> <controlling-R-address> "" "<parent>"`
   reserves the name and returns a `namereservation` blob plus an
   `i-address` (the `nameid` field). The reservation has to be
   confirmed in a block before step 2 will accept it.
2. `registeridentity '{...}'` consumes the reservation and writes the
   actual identity, attaching primary addresses, signature threshold,
   and the optional revoke/recovery authority fields.

Most of the verbose JSON the two commands return is the
`namereservation` blob, which is passed verbatim from step 1 into
step 2. The fields the operator actually chooses are the controlling
R-address (must hold spendable funds to pay the registration cost) and
the `identity` block in step 2.

---

## Register the revocation authority identity

```
verus -chain=VRSCTEST registernamecommitment revoke_authority \
    <R-ADDR-FROM-getnewaddress> "" "<PARENT-NAME>"
```

The output looks like:

```json
{
  "txid": "<commitment-txid>",
  "namereservation": {
    "version": 1,
    "name": "revoke_authority",
    "parent": "<PARENT-I-ADDRESS>",
    "salt": "<salt>",
    "referral": "",
    "nameid": "<REVOKE-I-ADDRESS>"
  }
}
```

Wait one block for the commitment transaction to confirm, then submit
the registration:

```
verus -chain=VRSCTEST registeridentity '{
  "txid": "<commitment-txid>",
  "namereservation": { ...verbatim from step 1... },
  "identity": {
    "name": "revoke_authority.<PARENT-NAME>",
    "primaryaddresses": ["<R-ADDR-FROM-getnewaddress>"],
    "minimumsignatures": 1,
    "privateaddress": ""
  }
}'
```

The R-address in `primaryaddresses` is the key that, in future, will be
allowed to sign a revocation against any identity that names
`<REVOKE-I-ADDRESS>` as its `revocationauthority`. Keep that key cold;
it does not need to be in the wallet that runs the dealer or cashier
daemons.

---

## Register the recovery authority identity

Identical flow with a different name and (recommended) a separately
generated R-address:

```
verus -chain=VRSCTEST registernamecommitment recovery_authority \
    <R-ADDR-FROM-getnewaddress> "" "<PARENT-NAME>"
```

Then the corresponding `registeridentity` call with
`"name": "recovery_authority.<PARENT-NAME>"`. The `nameid` returned by
the commitment becomes `<RECOVERY-I-ADDRESS>` — the value to pass as
`recoveryauthority` on subsequent identity registrations.

Using separate R-addresses for the two authorities is a deliberate
choice: it means an attacker who compromises the revocation key alone
cannot also recover, and vice versa. Combining them into one address
is allowed by the protocol but defeats most of the purpose of having
two separate fields.

---

## Use them when registering an operational identity

Once both authority identities exist, every operational identity in
the deployment can be registered referencing them:

```
verus -chain=VRSCTEST registeridentity '{
  "txid": "<commitment-txid>",
  "namereservation": { ... },
  "identity": {
    "name": "p1.<PARENT-NAME>",
    "primaryaddresses": ["<P1-R-ADDR>"],
    "revocationauthority": "<REVOKE-I-ADDRESS>",
    "recoveryauthority": "<RECOVERY-I-ADDRESS>",
    "minimumsignatures": 1,
    "privateaddress": ""
  }
}'
```

The `primaryaddresses` key is what `bet`'s wallet needs to sign
day-to-day updates. The revoke/recovery authorities sit on the side
and never participate in signing unless something goes wrong.

---

## Design choice: one authority pair per deployment, or per identity

The lazy answer — one pair of revoke/recovery authority identities
shared across every operational identity in the deployment — is what
this document walks through and what most existing `bet` development
work has used. It minimizes setup and keeps the cold-key surface
small (two private keys for the entire deployment).

The strict answer — a unique pair per operational identity — limits
blast radius if one authority key leaks but at the cost of N pairs of
cold keys to manage. For production deployments that hold real
balances on the cashier identity, the cost is worth it for the
cashier identity at minimum.

`bet` doesn't care which choice you make. It only ever reads
`primaryaddresses` and signs updates; the authority fields are
invisible to it.
