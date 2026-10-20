# DDAR development status

DD Appearance Replacer aims to show an ordinary Armor A while retaining the
identity, locking, scripts, keywords and effects of an existing DD device B.
The first milestone is a player-only, reversible runtime proof. This repository
now contains an opt-in ARMA-selection experiment, but it has not been exercised
in Skyrim and is not an accepted replacement implementation. There is no MCM yet.
See [the Phase 0 test guide](phase0-experiment.md).

## Current code

- `src/Probe.cpp`: SKSE diagnostics, gated to Skyrim SE 1.5.97.
  Logs player armor inventory, equipment events and first/third-person biped
  entries. `src/Experiment.cpp` adds a default-disabled, fingerprint-gated engine
  hook. It never edits shared armor records or calls equip/unequip functions.
- `tools/audit_installation.py`: read-only offline MO2 plugin audit. Uses
  originating plugin plus local ID, follows VMAD device properties, and resolves
  selected armor/armature records through plugin overrides. It does not validate
  runtime DD registration, archives, NIFs, physics or visible geometry. A partial
  scan returns an error status instead of claiming a clean installation.
- `tests/test_audit.py` and `tests/test_native_inspection.py`: synthetic binary parser tests.
- `tests/SlotPolicyTests.cpp`: native slot policy checks including strict rejection
  of the local target's extra slot 52 and the explicit, limited experimental exception.

## Build

Windows x64, Visual Studio 2022 C++ Build Tools, Windows SDK and CMake >=3.25
are required. Check out CommonLibSSE-NG and vcpkg at the commits recorded in
`dependencies.lock.json`; bootstrap vcpkg. The root vcpkg manifest pins its
dependency baseline. The build uses static dependencies with the dynamic MSVC
runtime (`x64-windows-static-md`, `/MD`).

The supplied PowerShell build script expects this layout (the root is configurable):

```text
F:/DDAR-Dev/
  BuildTools2022/
  Dependencies/CommonLibSSE-NG/
  Dependencies/vcpkg/
  Workspace/DDAR/                 # this repository
```

From the repository:

```powershell
python -m unittest discover -s tests -v
./tools/Build-Probe.ps1 -DevRoot F:/DDAR-Dev
```

The script directs downloads, temporary files, vcpkg caches, installed libraries,
builds and staging to the development root. It sets process environment variables
only. The Windows SDK and Microsoft installer may use existing system locations.
It does not install the DLL into MO2 or launch the game.

The plugin writes `DDARProbe.log` in SKSE's standard log directory. Successful
compilation or a non-null biped clone does not prove a visible model replacement.

## Runtime research gate

Read the installed DD NG scripts and verify the actual device database.
`zadNativeFunctions` exposes Papyrus functions for inventory/rendered resolution;
these declarations are not a documented C++ ABI. Do not identify devices by an
effects keyword alone.

DD NG's public [Hider implementation](https://github.com/IHateMyKite/DeviousDevicesNG/blob/main/src/Hider.cpp)
already intervenes in armor rendering. Any future hook must preserve its hiding
decisions and detect conflicts, using verified installed binaries. A possible
experiment is to select A's ARMA downstream while retaining B as the Armor owner
and restricting the operation to the player's biped. This is implemented
experimentally in `Experiment.cpp`, with exact local binary guards, and remains
unproven in-game. No shared Armor/ARMA mutation is acceptable as a player-only result.

Before implementing MCM, demonstrate A display, B identity/locking, restoration,
NPC isolation, first/third-person behavior, load/cell rebuilding, automatic DD
equipment and safe uninstall. Armor slot equality alone is insufficient: ARMA
masks and NIF partitions also matter. Source alternate textures, race/gender,
BodySlide outputs, skinning and physics require explicit verification.

## Publication scope

Only project code and general development documentation are published. Installed
mods, game assets, saves, private attachments, credentials and machine-specific
audit reports are excluded.
