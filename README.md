# Pangea-Bet

[![bet CD](https://github.com/sg777/bet/actions/workflows/bet-cd.yml/badge.svg?branch=master)](https://github.com/sg777/bet/actions/workflows/bet-cd.yml)

Decentralized poker, with every hand of state recorded on a Verus
chain. Players, the dealer, and the cashier each control a Verus
identity; the gameplay protocol is a sequence of
`updateidentity` writes against those identities' content
multimaps (CMMs). There is no central server, no off-chain
matchmaking, and no separate audit log — the chain is the audit log.

> **Status:** Active development. The poker product is functional
> end-to-end on a local Verus regtest (`VRSCTEST`). There is no
> mainnet release at this time.

---

## Node types

| Role     | Identity (dev regtest)        | Responsibilities                                                 |
|----------|-------------------------------|------------------------------------------------------------------|
| Dealer   | `d1.<parent>@`                | Hosts tables, drives the per-hand state machine, deals cards.    |
| Player   | `p1.<parent>@`, `p2.<parent>@`, … | Buys in with `sendcurrency`, signs deck-shuffle steps, places bets. |
| Cashier  | `cashier.<parent>@`           | Custodies payin funds, settles payouts, blinds the deck.         |

A single `bet` binary plays all three roles — selected by the first
argument (`./bin/bet start dealer`, `./bin/bet start player`, etc.).

---

## Quick start (local regtest)

A fully-local development cycle needs a patched `verusd` running the
`VRSCTEST` chain. The end-to-end bring-up (chain config, identity
registration, helper scripts) is documented in
[`docs/tutorials/cli-auto-vrsctest.md`](./docs/tutorials/cli-auto-vrsctest.md).
Once that's in place:

```bash
# Build
git clone --recurse-submodules <fork-url> bet
cd bet
make -j$(nproc)

# Run a 2-player hand (each command in its own terminal / tmux pane)
cd poker
./bin/bet start cashier
./bin/bet start dealer
./bin/bet start player -c config/p1.ini
./bin/bet start player -c config/p2.ini
```

The legacy positional form (`./bin/bet dealer`, `./bin/bet player`,
`./bin/bet cashier`) is preserved for compatibility.

GUI mode (per-player) starts a local WebSocket on the port from
`config/pN.ini`:

```bash
./bin/bet start player -c config/p1.ini --gui
# Then open http://localhost:9001/ from a pangea-poker build.
```

Detailed build instructions:
[`docs/guides/build-from-source.md`](docs/guides/build-from-source.md).

---

## Architecture in one paragraph

`bet` is structured around the *single-writer-per-identity* rule:
every identity in play has exactly one owning node, and that node
is the only one allowed to update its CMM. The dealer writes the
table identity's hand state; each player writes their own identity's
deck-shuffle output; the cashier writes payment acknowledgements
and settlement on its own identity. Reading is open — every
participant can `getidentitycontent` any identity to reconstruct the
full game. The state machine in
[`docs/reference/game-states.md`](docs/reference/game-states.md)
enumerates the 14 hand states; the full key-by-key reference is in
[`docs/reference/vdxf-keys.md`](docs/reference/vdxf-keys.md).

---

## Ports

`bet` itself opens only WebSocket ports for GUI integration. There
is no inter-node TCP — coordination flows entirely through the chain.

| Port  | Default for | Configurable in        |
|-------|-------------|------------------------|
| 9000  | Dealer GUI  | `dealer:gui_ws_port`   |
| 9001  | Player 1 GUI| `verus:ws_port` (`p1.ini`) |
| 9002  | Player 2 / cashier GUI | `verus:ws_port` / `cashier:gui_ws_port` |

The chain RPC is whichever port your `verusd` is configured for
(18843 on the dev regtest); the `verus` CLI handles that connection
on `bet`'s behalf.

---

## Documentation

The full doc tree is under [`docs/`](./docs/):

* **[Documentation Index](docs/README.md)** — top-level navigation.
* **[Building from source](docs/guides/build-from-source.md)** — system
  packages, recursive submodule clone, `make -j`.
* **[VRSCTEST Local Setup](docs/tutorials/cli-auto-vrsctest.md)** —
  bring up a local regtest with the patched `verusd`.
* **[System Architecture](docs/explanation/architecture.md)** —
  layered design, RPC abstraction, where each module lives.
* **[Verus Overview](docs/explanation/verus-overview.md)** —
  how identities, CMMs, and VDXF keys are used.
* **[Game State Machine](docs/reference/game-states.md)** —
  the 14 `G_*` states and the writer for each.
* **[Keys and Data Structure](docs/reference/vdxf-keys.md)** —
  every CMM key `bet` reads or writes, with payload shapes.
* **[Player Join Flow](docs/reference/player-join-flow.md)** — the full
  player join + payin sequence.
* **[GUI Message Formats](docs/reference/gui-message-formats.md)** —
  WebSocket wire format for GUI integrators.
* **[Glossary](docs/reference/glossary.md)** — terminology
  (CMM, vdxfid, primaryaddresses, single-writer rule, etc.).

---

## Configuration

Configuration lives under `poker/config/`. Each node reads a
subset of:

* `dealer.ini` / `player.ini` / `pN.ini` / `cashier.ini` —
  per-node knobs.
* `blockchain.ini` — chain CLI and currency name.
* `keys.ini` — identity short names and VDXF key prefix.
* `.rpccredentials` — Verus daemon RPC user/password.

Reference: [`dealer_configuration.md`](docs/reference/dealer-config.md) |
[`player_configuration.md`](docs/reference/player-config.md) |
[`cashier_configuration.md`](docs/reference/cashier-config.md).

---

## Wallet operations

All wallet operations route through the paired Verus daemon's
wallet — `bet` does not implement key management of its own.
Day-to-day tasks:

```bash
# Get a fresh receiving address from your wallet
verus -chain=VRSCTEST getnewaddress

# Check balance
./bin/bet balance

# Withdraw to an arbitrary R-address
./bin/bet withdraw <amount|all> <destination_address>
```

The GUI `withdrawRequest` / `withdraw` methods (player and cashier
nodes) provide the same functionality through the WebSocket
front-end.

---

## License

See [LICENSE](./LICENSE).
