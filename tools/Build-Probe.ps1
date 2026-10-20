param([string]$DevRoot = 'F:\DDAR-Dev')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$toolchainRoot = Join-Path $DevRoot 'BuildTools2022'
$commonlib = Join-Path $DevRoot 'Dependencies\CommonLibSSE-NG'
$vcpkg = Join-Path $DevRoot 'Dependencies\vcpkg'
$cmake = Join-Path $toolchainRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$build = Join-Path $DevRoot 'Workspace\build\DDARProbe'
$stage = Join-Path $DevRoot 'Workspace\stage\DDARProbe'
foreach ($path in @($cmake, (Join-Path $commonlib 'CMakeLists.txt'), (Join-Path $vcpkg 'vcpkg.exe'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing dependency: $path" }
}
foreach ($dir in @('Temp','Downloads\vcpkg','Caches\vcpkg','Caches\registries','Logs')) {
    New-Item -ItemType Directory -Path (Join-Path $DevRoot $dir) -Force | Out-Null
}
$env:TEMP = Join-Path $DevRoot 'Temp'
$env:TMP = $env:TEMP
$env:VCPKG_DOWNLOADS = Join-Path $DevRoot 'Downloads\vcpkg'
$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $DevRoot 'Caches\vcpkg'
$env:VCPKG_DISABLE_METRICS = '1'
$env:VCPKG_VISUAL_STUDIO_PATH = $toolchainRoot
$env:VCPKG_BINARY_SOURCES = 'clear;files,' + $env:VCPKG_DEFAULT_BINARY_CACHE + ',readwrite'
$env:X_VCPKG_REGISTRIES_CACHE = Join-Path $DevRoot 'Caches\registries'
$lock = Get-Content -LiteralPath (Join-Path $projectRoot 'dependencies.lock.json') -Raw | ConvertFrom-Json
foreach ($dependency in @(@{Root=$commonlib;Expected=$lock.'commonlibsse-ng'.commit}, @{Root=$vcpkg;Expected=$lock.vcpkg.commit})) {
    $actual = git -C $dependency.Root rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $dependency.Expected) { throw "Dependency commit mismatch: $($dependency.Root)" }
}
& $cmake -S $projectRoot -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_GENERATOR_INSTANCE=$toolchainRoot" `
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkg/scripts/buildsystems/vcpkg.cmake" `
    '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md' `
    "-DVCPKG_INSTALLED_DIR=$DevRoot/Dependencies/installed" `
    "-DDDAR_COMMONLIB_SOURCE_DIR=$commonlib" `
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' `
    '-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON'
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed: $LASTEXITCODE" }
& $cmake --build $build --config Release --target DDARProbe --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $LASTEXITCODE" }
& $cmake --install $build --config Release --prefix $stage --component DDARProbe
if ($LASTEXITCODE -ne 0) { throw "Staging failed: $LASTEXITCODE" }
$dll = Join-Path $stage 'SKSE\Plugins\DDARProbe.dll'
Get-FileHash -LiteralPath $dll -Algorithm SHA256
Write-Output "Observation-only plugin staged at $dll. Nothing was installed into MO2."
