param([string]$DevRoot='F:\DDAR-Dev', [string]$GameData='Z:\000skyrim\SKSE\lunhuiSE\SkyrimSE\Data')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $DevRoot 'Workspace\stage\DDARProbe'
$compiler=Join-Path $DevRoot 'Dependencies\Caprica\Caprica.exe'
$skyui=Join-Path $DevRoot 'Dependencies\SkyUI-SDK\dist\Data\Scripts\Source'
$scripts=Join-Path $stage 'Scripts'
New-Item -ItemType Directory -Path $scripts -Force | Out-Null
$env:TEMP=Join-Path $DevRoot 'Temp'
$env:TMP=$env:TEMP
$env:DOTNET_CLI_HOME=Join-Path $DevRoot 'Caches\dotnet'
$env:NUGET_PACKAGES=Join-Path $DevRoot 'Dependencies\nuget'
$env:NUGET_HTTP_CACHE_PATH=Join-Path $DevRoot 'Caches\nuget-http'
$env:DOTNET_GENERATE_ASPNET_CERTIFICATE='false'
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$base=Join-Path $DevRoot 'Dependencies\Papyrus-base-reference'
$imports="$root\papyrus;$skyui;$GameData\Scripts\Source;$base"
foreach($name in @('DDAR_Native','DDAR_MCM','DDAR_PlayerAlias')) {
    & $compiler --game skyrim --ignorecwd --import $imports --flags "$root\papyrus\TESV_Papyrus_Flags.flg" --output $scripts --release "$root\papyrus\$name.psc"
    if($LASTEXITCODE -ne 0) { throw "Papyrus failed: $name" }
}
& dotnet restore "$PSScriptRoot\McmPluginWriter\McmPluginWriter.csproj" --source https://api.nuget.org/v3/index.json
if($LASTEXITCODE -ne 0) { throw 'ESP writer restore failed' }
& dotnet run --no-restore --project "$PSScriptRoot\McmPluginWriter\McmPluginWriter.csproj" -c Release -- $stage
if($LASTEXITCODE -ne 0) { throw 'ESP generation/readback failed' }
foreach($name in @('DDAR_Native','DDAR_MCM','DDAR_PlayerAlias')) {
    $file=Join-Path $scripts "$name.pex"
    if(!(Test-Path -LiteralPath $file)) { throw "Missing $file" }
    Get-FileHash -LiteralPath $file -Algorithm SHA256
}
