# Building `bet` from source

`bet` is a C poker app that links against the user's system Verus
toolchain via the `verus` CLI. There is **no compile-time
dependency on Verus**; you can build `bet` first and pair it with
any `verus` binary at runtime.

The build has been tested on Ubuntu 20.04 / 22.04 and recent
Debian-based systems. macOS builds (Intel + Apple Silicon) work
with the Homebrew packages noted at the end.

For getting a local VRSCTEST chain running, see
[`docs/tutorials/cli-auto-vrsctest.md`](../tutorials/cli-auto-vrsctest.md). The
present document covers only the `bet` binary itself.

---

## 1. System packages

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y \
  build-essential autoconf automake libtool pkg-config git make \
  libgmp-dev libsqlite3-dev libssl-dev \
  libcurl4-gnutls-dev libevent-dev libsodium-dev \
  libwebsockets-dev
```

If `libwebsockets-dev` is not packaged (older Ubuntu releases), the
repository ships a vendored copy under `external/libwebsockets/`
that `make` will pick up; in that case you'll additionally need
`cmake` (the repository also vendors a CMake submodule but a system
CMake is faster).

macOS (Homebrew):

```bash
brew install autoconf automake libtool pkg-config \
  gmp openssl@3 sqlite libevent libsodium libwebsockets
```

The macOS rules in `poker/Makefile` automatically point `CPPFLAGS`
and `LDFLAGS` at `/opt/homebrew/opt/openssl/` on Apple Silicon.

---

## 2. Cloning

The build pulls vendored libraries (`iniparser`, `jsmn`, `dlg`,
`nng`, `libwebsockets`, `libwally-core`) as Git submodules. Clone
recursively, or initialise them after the fact:

```bash
git clone --recurse-submodules https://github.com/<your-fork>/bet.git
cd bet
# or, post-clone:
# git submodule update --init --recursive
```

---

## 3. Compiling

The simplest invocation builds vendored crypto and the `bet`
binary in one go from the repository root:

```bash
make -j$(nproc)
```

Internally this runs `make -C libs/crypto` followed by
`make -C poker`, producing:

* `poker/bin/bet` — the poker daemon (used as `dealer`, `player`,
  `cashier`, and CLI front-end depending on the first argument).
* `poker/bin/cashierd` — symlink copy of the same binary, kept
  for legacy launch scripts.

If you only want to rebuild after a code change inside `poker/src/`,
just run `make` inside `poker/`:

```bash
cd poker && make -j$(nproc)
```

A clean reset:

```bash
make clean      # at the repo root, cleans poker + vendored libs
```

---

## 4. Sanity check

```bash
cd poker
./bin/bet --version    # prints the git-derived VERSION string
./bin/bet help         # lists the available subcommands
```

If the wrapper script you'll run for tests is in `poker/`, you can
launch the dealer/player/cashier from there with:

```bash
./bin/bet dealer
./bin/bet player -c config/p1.ini
./bin/bet cashier
```

See [`dealer_configuration.md`](../reference/dealer-config.md) and
[`player_configuration.md`](../reference/player-config.md) for the
config files each command consults.

---

## 5. Connecting to a chain

`bet` invokes the `verus` CLI as a subprocess (`popen()`-style) for
every on-chain operation. Two things have to be true at runtime:

1. The chain's daemon (`verusd -chain=VRSCTEST` for the dev
   regtest) is running and the wallet holds keys for the identities
   referenced in `keys.ini` / `dealer.ini` / `player.ini`.
2. The `verus` binary is on the `PATH` of the user running `bet`.
   The convention used in development is to symlink it into
   `/usr/local/bin/`:

   ```bash
   sudo ln -sf /path/to/VerusCoin/src/verus /usr/local/bin/verus
   ```

   Without this, `bet` exits early with `sh: 1: verus: not found`
   because `popen()` runs each subshell with a minimal `PATH`.

The full bring-up of the local regtest, including the one-line
`verusd` patch (`-offline` flag), Verus identity registration, and
the helper tmux sessions, is documented in
[`docs/tutorials/cli-auto-vrsctest.md`](../tutorials/cli-auto-vrsctest.md).
