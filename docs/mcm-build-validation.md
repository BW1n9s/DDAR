# MCM build validation — 2026-10-21

Version 0.0.3 adds the actual test interface: a start-game-enabled ESL-flagged
`DDAR.esp`, `SEQ/DDAR.seq`, `DDAR_MCM.pex`, `DDAR_Native.pex`,
`DDAR_PlayerAlias.pex`, and the native DLL. No existing MO2/game files were changed.

Validation performed:

- MSVC x64 Release build and eight native slot-policy cases.
- Caprica 0.3.0 Skyrim compilation of all three own scripts against SkyUI SDK,
  installed game/SKSE sources, then missing base declarations as fallback.
- Mutagen 0.54.4 ESP write/readback of quest flags, VMAD attachment and the forced
  player alias with its load-game callback.
- Independent binary checks in `tools/verify_mcm.py`: x64 DLL, all nine matching
  Papyrus/native bindings, Skyrim PEX 3.2 object names, parent types and callbacks,
  own-only format-44 quest, VMAD and exact on-disk FormID match in SEQ.
- Existing sixteen Python parser/binary-inspection checks; package member list,
  SHA-256 manifest and ZIP CRC verification.

The SEQ uses the serialized quest's FixedFormID `01000800`, including its
file-local master index. This follows [xEdit's SEQ generator](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/xEdit/xeMainForm.pas#L3901),
which writes FixedFormID rather than the runtime load-order ID or bare local ID.

The DD NG API header is vendored with its Apache-2.0 license. Database access is
read-only and gated by the inspected local DLL SHA-256, export/vtable addresses,
API version 2 and MSVC layout assertions. It does not invoke the insertion-prone
unknown-inventory forward lookup. Inventory aliases sharing a rendered form are
shown explicitly; a rendered mapping affects those aliases on the player.

The MCM queues apply/restore until paused menus close, performs changes on the
game task queue, keeps operation errors separate from catalog-refresh status,
and clears pending operations/caches and disables native mappings on load.

Build commands, from the F-drive repository:

```powershell
./tools/Build-Probe.ps1
./tools/Build-Mcm.ps1
C:/Python312/python.exe -m unittest discover -s tests -p 'test_*.py'
C:/Python312/python.exe tools/package_probe.py
```

**No Skyrim runtime acceptance has been performed.** MCM registration, visible
replacement, restoration, DD locks/keys, physics and NPC isolation still require
the [game test](mcm-test.md). This package exposes those tests through MCM;
it does not claim the test results in advance. External JSON/multiple mappings,
AE and a general multi-ARMA backend remain outside 0.0.3.
