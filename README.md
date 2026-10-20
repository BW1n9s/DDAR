# DD Appearance Replacer (DDAR)

Skyrim SE 1.5.97 Phase 0: diagnostics plus an opt-in, player-only ARMA selection experiment. **Not gameplay-verified and not DDAR v0.1.**

The experiment retains B as the Armor owner while selecting A armatures downstream of DD hiding. It is disabled by default, gated to the inspected local binaries, and does not edit shared armor records or equip/unequip devices.

- [Chinese test guide](docs/phase0-experiment.md)
- [Revised MCM catalog design: inventory A, catalog B](docs/mcm-device-catalog.md)
- [Development and build instructions](docs/development.md)
- [0.0.2 build validation](docs/experiment-build-validation.md)

16 offline parser tests and 8 native slot-policy cases passed. B is not required in the player inventory. The MCM catalog and general multi-slot replacement are future work. The local padded belt ARMA has an extra slot 52: strict defaults reject it; an explicit experimental exception is documented.

No game assets, third-party mods, saves, credentials or machine-specific audit reports are committed.
