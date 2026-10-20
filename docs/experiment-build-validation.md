# Experimental build validation — 2026-10-21

DDARProbe **0.0.2**, Windows x64 Release, built with the pinned toolchain and
dependencies from the previous build, plus MinHook 1.3.3#4 from the same vcpkg baseline.
The final compilation emitted no compiler warning. Sixteen Python parser checks
and eight native slot-policy cases passed.

The staged DLL SHA256 is:

```text
b5582808a8fca37c4f1671194b5d17447e4a52aa96558dd5581b5ff70b160a42
```

The inspected local EXE resolves Address Library ID 17361 to RVA 0x227070;
its entry instructions and first 24 bytes were checked offline. The experiment
checks the same location and bytes at runtime before requesting a detour.
The equipment refresh ID 19316 resolves to RVA 0x2943B0.

The inspected DD NG binary exports GetAPI at RVA 0x5F40. Its API vtable is at
0x1FD410, with first four functions at 0xD8950, 0xD8960, 0xD8980, 0xD89A0.
The version function returns 2. The forward/reverse methods were checked against
the downloaded reference, but the actual local binary fingerprint and entries
are authoritative. The forward function can insert an unknown database key;
the experiment therefore requires reverse confirmation before calling forward.
It never consumes the cross-DLL STL database container.

All of the above is **offline evidence and implemented runtime guards**.
No hook has yet been exercised in Skyrim; neither visible replacement nor
restoration, DD locking, physics, NPC isolation or uninstall behavior has passed
gameplay acceptance. The test package is disabled and strict by default.
See [the test guide](phase0-experiment.md) for the explicit slot-52 experiment.

The user's revised selection flow is reflected in the experiment: only A must be
in the player's inventory. B is resolved as a registered device and can be absent.
The future MCM catalog and multi-slot requirements are recorded in
[the catalog design](mcm-device-catalog.md); that MCM is not implemented yet.
