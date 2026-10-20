# Build validation — 2026-10-20

The observation-only DDARProbe built successfully as Windows x64 Release.
The final build script also passed an incremental build and component-only staging.

| Item | Verified version / result |
|---|---|
| MSVC compiler | 19.44.35229.0 |
| MSVC toolset directory | 14.44.35207 |
| MSBuild | 17.14.60 |
| CMake | 3.31.6-msvc6 |
| Windows SDK | 10.0.26100.0 |
| CommonLibSSE-NG | `b93280e832f263dbef44e44cbe2936622a02f91a` |
| vcpkg | `469a0504e4a6ebf6173d1c0f51a7fa34c382fd43` |
| fmt / spdlog / rapidcsv | 10.2.1 / 1.12.0 / 8.64 |
| Dependency triplet | x64-windows-static-md |
| Parser tests | 10 passed |
| PE image | AMD64 / PE32+ DLL |
| SKSE exports | SKSEPlugin_Query, SKSEPlugin_Load |
| Dynamic dependencies | Windows system libraries and MSVC/UCRT; no separate spdlog/fmt DLL |
| Binary size | 574,464 bytes |

The inspected binary SHA-256 is:

```text
03e198d3bc8b8563a30771ec6b31d34b72c300101cca6fb1f862f04403b2f56f
```

The binary was not loaded into Skyrim. SKSE acceptance, event delivery, inventory
snapshots and runtime DD behavior remain untested. Appearance replacement is
still unimplemented. A successful build is not Phase 0 gameplay acceptance.

No existing MO2 mods, DD plugins/scripts or game saves were changed. The DLL was
staged in the separate development workspace only. Third-party source checkouts,
local installation reports and binary build outputs are not committed here.
