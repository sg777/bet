# Deck shuffling

The security of a hand of poker on `bet` rests on the deck shuffling
process and the controlled card revelation that follows it. The shuffle
is multi-party — no single actor can predict the order of the deck —
and every step that touches deck data is anchored to a different Verus
identity, so a compromised actor can be detected by what they did or
did not write.

This document covers the cryptographic representation of a card, the
sequence of shuffles between the player, dealer, and cashier, how the
results land on Verus identities, and how the on-chain layout
interacts with the 5872-byte identity-update size limit described in
[`verus-overview.md`](verus-overview.md).

---

## Representing a card

Each card is a 32-byte value generated as a Curve25519 secret-key
candidate. Section 3 of the [Curve25519 paper](https://cr.yp.to/ecdh/curve25519-20060209.pdf)
gives the bit pattern:

> Legitimate users generate independent uniform random secret keys.
> For example, generate 32 uniform random bytes, clear bits 0, 1, 2
> of the first byte, clear bit 7 of the last byte, and set bit 6 of
> the last byte.

`bet` follows that pattern via `gen_deck` in
`poker/src/poker.c`, with one twist: the 31st byte (0-indexed) of each
secret stores the card index, an integer in the range `[1, 52]`. So
the same 32 bytes that serve as a Curve25519 secret also carry a tag
that, once the card is decoded by a player, tells the player which
card it is. A 52-card deck is 52 of these 32-byte values, 1664 bytes
of raw key material per player.

Each player keeps its own 1664-byte secret table locally in the
`player_deck_info` SQLite row (see [`player-rejoin.md`](player-rejoin.md));
the matching public values, generated as Curve25519 base-point
products of those secrets, are what gets published on-chain.

---

## The shuffle chain

Three actors take turns transforming the deck. Each pass writes its
output to the actor's own Verus identity, so reads are always
well-defined: a downstream reader can always find the upstream
contribution at a predictable location.

**1. Player publishes its public deck.** `player_init_deck` in
`poker/src/player.c:63` generates the player's keypair and 52-card
secret table, saves both locally, and posts the public side to the
player's own identity under `PLAYER_DECK_KEY.<game_id>`:

```json
{
  "id":       <player_id>,
  "pubkey":   "<curve25519 product of player keypair>",
  "cardinfo": ["<P1>", "<P2>", ..., "<P52>"]
}
```

`Pi` is `curve25519(player_r[i].priv, base)` — the public point
corresponding to the player's secret for card `i`. Every other actor
can compute Diffie-Hellman against `pubkey` and the Pi values without
ever learning the underlying scalars.

**2. Dealer shuffles and blinds each player's deck.** The dealer
reads each player's `PLAYER_DECK_KEY` entry, applies a permutation
known only to the dealer, multiplies in a dealer blinding factor, and
writes the per-player output to the **table identity** under
`T_D_P<n>_DECK_KEY.<game_id>` (one key per seat, declared in
`poker/include/vdxf.h:113-121`). The dealer's permutation and blinding
factor are kept locally; only the transformed public values reach the
chain.

**3. Cashier re-shuffles and re-blinds.** The cashier reads
`T_D_P<n>_DECK_KEY` for each player, applies its own permutation and
blinding factor, and writes the result to **its own cashier identity**
under `C_B_P<n>_DECK_KEY.<game_id>`. This is the single-writer
migration in action: the cashier's blinded output used to live on the
table identity, which made the dealer and cashier joint writers of
that key; moving the cashier write to its own identity removed the
contention. The split is documented in `poker/include/vdxf.h:134-144`
and in `docs/TODO.md` item 1.1.

**4. Dealer canonicalizes onto the table.** With the cashier's
`C_B_P<n>_DECK_KEY` populated, the dealer reads it, optionally adds a
final dealer-side blind, and re-publishes the result under
`T_B_P<n>_DECK_KEY.<game_id>` on the table identity. This gives every
player a single canonical location to read the fully-blinded deck from
— they don't have to know the cashier identity's name to play.

After all four passes, every player has the same 52-position shuffled
deck of fully-blinded Curve25519 points. No actor (the player, the
dealer, the cashier) holds enough information by themselves to decode
any specific card. Each card needs a blinding-value reveal from the
cashier and the player's own secret to compute the underlying 32-byte
value and read out the card index from byte 30.

---

## Card revelation

When the dealer wants a specific player to see a specific card index,
the chain of events is:

1. Dealer writes a turn prompt into the table identity's
   `T_BETTING_STATE_KEY` naming the player and card index.
2. Player optionally writes a hole-card or community-card reveal
   request to its own identity under `P_HOLECARDS_REVEAL_KEY` (for
   hole cards) or relies on the dealer's prompt directly (for
   community cards).
3. Cashier reads the request, computes the blinding value for that
   (player_id, card_id) pair, and writes it to its own identity under
   `C_CARD_BV_KEY.<game_id>` (single-writer mirror of the older
   `T_CARD_BV_KEY`; see `vdxf.h:156-166`).
4. Player reads the blinding value directly from the cashier identity,
   combines it with its own secret scalar via
   `decode_card` (`player.c:112`), and recovers the card index from
   byte 30 of the resulting scalar.
5. For community cards, the player snapshots the decoded card into
   its own identity under `P_DECODED_CARD_KEY` so the dealer can
   confirm everyone saw the same value.

This is all the cryptography that's actually on the wire. Shamir
secret-sharing of the cashier's blinding values and an explicit
player-driven unshuffle step were considered earlier in the design
and dropped — they added complexity without adding security, since
the cashier is already the only actor that can produce a valid
blinding value and a single cashier write is already serialized by
the identity's UTXO.

---

## Size constraint and why the writes serialize

Each `updateidentity` transaction is capped at 5872 bytes for the
serialized identity object itself (the `MAX_SCRIPT_ELEMENT_SIZE_PBAAS
- 128` rule, described in `verus-overview.md`). A 52-card deck
encoded as the JSON shape above runs about 3 to 4 KB once vdxfid
keys, hex prefixes, and structural overhead are accounted for. One
full deck fits in one update; two do not.

Multiplying that out: for `n` players, the shuffle phase consists of
`n` player-side commits, `n` dealer-side per-player writes, `n`
cashier-side per-player writes, and `n` dealer canonicalization
writes back to the table. That's `4n` `updateidentity` calls in the
critical path, each costing roughly one blocktime under the current
`update_with_retry` policy. For nine players that's 36 sequential
on-chain writes, before the first card is dealt.

The doc this replaced gave an aggregate size figure of `(3n + 2) ×
1664 = 48,256 bytes ≈ 47 KB` for `n = 9`, which is a useful upper
bound on the *total* deck data the protocol ever pushes through the
chain in a single hand — but it's not a single-transaction figure. A
single transaction can carry at most one of those decks. The 47 KB
fans out across roughly the 36 writes above.

The natural questions — can the writes be batched, pipelined across
actors, or chained via mempool-aware UTXO selection? — are tracked in
`docs/TODO.md` as live performance work. Batching into one
transaction is *not* available because of the 5872-byte limit; the
options on the table are mempool chaining (item 4), cross-actor
pipelining (item 3), and binary packing of the deck representation
(item 2).

---

## Related documentation

- [Verus Overview](verus-overview.md) — single-writer-per-identity,
  the 5872-byte CMM size limit, the `update_with_retry` choke point.
- [Player Rejoining](player-rejoin.md) — where the player's secret
  deck table lives locally.
- [getidentitycontent](getidentitycontent.md) — how downstream
  actors read the multi-update history of these deck keys.
- [Architecture](architecture.md) — the layered call path that all
  of these writes descend through.
