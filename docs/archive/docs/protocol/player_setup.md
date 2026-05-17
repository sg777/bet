# Player Setup (legacy)

> **Deprecated.** This document described setting up a player node on
> the CHIPS + Lightning Network era stack: downloading `chipsd` and
> `lightningd` binaries, a `player_setup.ini` file that no longer
> exists (the real file is `player.ini` / `pN.ini`), a
> `[gui]` section listing cashier-hosted GUI URLs (these are no
> longer the deployment model — each player runs its own GUI), and
> the legacy `./bin/bin/bet player` invocation pattern.
>
> Current equivalents:
>
> * **Build:** [`compile.md`](../../../guides/build-from-source.md).
> * **Chain setup:** [`docs/VRSCTEST_LOCAL_SETUP.md`](../../../tutorials/cli-auto-vrsctest.md).
> * **Config reference:** [`player_configuration.md`](../../../reference/player-config.md).
> * **Running a player:**
>   ```bash
>   cd poker
>   ./bin/bet start player --config config/p1.ini --auto
>   ```
>   See [`docs/COMMUNITY_TESTING_GUIDE.md § 5`](../../../tutorials/community-quickstart.md)
>   for the full bring-up.
>
> Kept to preserve any inbound links; will be archived under
> `docs/archive/` in a follow-up cleanup.
