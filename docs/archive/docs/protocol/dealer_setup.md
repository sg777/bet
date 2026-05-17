# Dealer Setup (legacy)

> **Deprecated.** This document combined LN-era compilation, GUI
> URL discovery, and a `dispute_resolution` flow that depended on a
> CHIPS multisig design no longer in use. The instructions reference
> `bet/poker/config/dealer_setup.ini` (a file that does not exist),
> `./bin/bin/bet dcv` (an entrypoint that no longer accepts an IPv4
> argument), and a "Set Nodes" GUI flow that has been replaced.
>
> Current equivalents:
>
> * **Build:** [`compile.md`](../../../guides/build-from-source.md).
> * **Chain setup:** [`docs/VRSCTEST_LOCAL_SETUP.md`](../../../tutorials/cli-auto-vrsctest.md).
> * **Config reference:** [`dealer_configuration.md`](../../../reference/dealer-config.md).
> * **Running a dealer:**
>   ```bash
>   cd poker
>   ./bin/bet start dealer --config config/dealer.ini --cli --reset
>   ```
>   See [`docs/COMMUNITY_TESTING_GUIDE.md § 5`](../../../tutorials/community-quickstart.md)
>   for the full bring-up.
>
> Kept to preserve any inbound links; will be archived under
> `docs/archive/` in a follow-up cleanup.
