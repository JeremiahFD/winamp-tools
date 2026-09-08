param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$UiOnly
)

$ErrorActionPreference = 'Stop'

if (-not $UiOnly) {
    throw 'This release branch contains the radio UI only. Run .\build.ps1 -Configuration Release -UiOnly.'
}

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$visualStudio = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) {
    throw 'Visual Studio C++ x86 build tools were not found.'
}

$developerCommand = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'
$outputDirectory = Join-Path $projectRoot "build\x86\$($Configuration.ToLowerInvariant())"
if ($UiOnly) { $outputDirectory = Join-Path $outputDirectory 'radio-ui' }
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$common = @(
    '/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/WX',
    '/DUNICODE', '/D_UNICODE', '/DNOMINMAX',
    "/I`"$(Join-Path $projectRoot 'include')`"",
    "/I`"$(Join-Path $projectRoot 'src')`""
)
if ($UiOnly) { $common += '/DLIVE_STATIONS_UI_ONLY' }
$resolverSources = if ($UiOnly) { @() } else { @('src\resolver_contract.cpp') }
if ($Configuration -eq 'Release') {
    $common += @('/O2', '/DNDEBUG')
} else {
    $common += @('/Od', '/Zi')
}

function Invoke-Compiler {
    param([string[]]$Arguments)

    $quotedArguments = $Arguments | ForEach-Object {
        if ($_ -match '[\s&]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }
    $commandLine = 'call "' + $developerCommand + '" -arch=x86 -host_arch=x64 >nul && cl.exe ' +
        ($quotedArguments -join ' ')
    & $env:ComSpec /d /s /c $commandLine
    if ($LASTEXITCODE -ne 0) {
        throw "C++ compiler failed with exit code $LASTEXITCODE."
    }
}

$libraries = @(
    'comctl32.lib', 'dnsapi.lib', 'gdi32.lib', 'ole32.lib', 'shell32.lib', 'user32.lib',
    'winhttp.lib', 'ws2_32.lib', 'xmllite.lib'
)

Push-Location $projectRoot
try {
    Invoke-Compiler ($common + @(
        '/LD', 'src\plugin.cpp', 'src\favorites.cpp', 'src\radio_browser.cpp'
    ) + $resolverSources + @(
        "/Fe:$outputDirectory\ml_livestations.dll",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/DEF:$(Join-Path $projectRoot 'live_stations.def')",
        "/PDB:$outputDirectory\ml_livestations.pdb"
    ))

    # This probe-only DLL is intentionally separately named and never an install
    # candidate. Its provider has no WinHTTP, DNS, or socket calls.
    Invoke-Compiler ($common + @(
        '/DLIVE_STATIONS_OFFLINE_FIXTURE', '/LD', 'src\plugin.cpp', 'src\favorites.cpp', 'src\offline_fixture_provider.cpp'
    ) + $resolverSources + @(
        "/Fe:$outputDirectory\ml_livestations_fixture.dll",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/DEF:$(Join-Path $projectRoot 'live_stations.def')",
        "/PDB:$outputDirectory\ml_livestations_fixture.pdb"
    ))

    if (-not $UiOnly) {
      Invoke-Compiler ($common + @(
        'src\probe.cpp', 'src\radio_browser.cpp',
        "/Fe:$outputDirectory\radio_browser_probe.exe",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/PDB:$outputDirectory\radio_browser_probe.pdb"
    ))
    }

    Invoke-Compiler ($common + @(
        'src\plugin_abi_probe.cpp',
        "/Fe:$outputDirectory\plugin_abi_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'user32.lib',
        "/PDB:$outputDirectory\plugin_abi_probe.pdb"
    ))

    if (-not $UiOnly) {
      Invoke-Compiler ($common + @(
        '/LD', 'src\resolver_plugin.cpp', 'src\resolver_server.cpp', 'src\resolver_contract.cpp', 'src\radio_browser.cpp',
        "/Fe:$outputDirectory\gen_livestations_resolver.dll",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/DEF:$(Join-Path $projectRoot 'gen_livestations_resolver.def')",
        "/PDB:$outputDirectory\gen_livestations_resolver.pdb"
    ))

    Invoke-Compiler ($common + @(
        '/DLIVE_STATIONS_OFFLINE_FIXTURE', '/LD', 'src\resolver_plugin.cpp', 'src\resolver_server.cpp', 'src\resolver_contract.cpp', 'src\offline_fixture_provider.cpp',
        "/Fe:$outputDirectory\gen_livestations_resolver_fixture.dll",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/DEF:$(Join-Path $projectRoot 'gen_livestations_resolver.def')",
        "/PDB:$outputDirectory\gen_livestations_resolver_fixture.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\resolver_abi_probe.cpp',
        "/Fe:$outputDirectory\resolver_abi_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'user32.lib',
        "/PDB:$outputDirectory\resolver_abi_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\resolver_server_probe.cpp', 'src\resolver_server.cpp', 'src\resolver_contract.cpp',
        "/Fe:$outputDirectory\resolver_server_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'ws2_32.lib',
        "/PDB:$outputDirectory\resolver_server_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\video_contract_probe.cpp', 'src\video_stream_contract.cpp',
        "/Fe:$outputDirectory\video_contract_probe.exe",
        "/Fo:$outputDirectory\",
        '/link',
        "/PDB:$outputDirectory\video_contract_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\video_provider_probe.cpp', 'src\video_provider_contract.cpp', 'src\video_stream_contract.cpp',
        "/Fe:$outputDirectory\video_provider_probe.exe",
        "/Fo:$outputDirectory\",
        '/link',
        "/PDB:$outputDirectory\video_provider_probe.pdb"
    ))
    }

    Invoke-Compiler ($common + @(
        'src\plugin_host_probe.cpp',
        "/Fe:$outputDirectory\plugin_host_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'comctl32.lib', 'gdi32.lib', 'user32.lib',
        "/PDB:$outputDirectory\plugin_host_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\contracts_probe.cpp', 'src\favorites.cpp', 'src\radio_browser.cpp',
        "/Fe:$outputDirectory\live_stations_contracts_probe.exe",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/PDB:$outputDirectory\live_stations_contracts_probe.pdb"
    ))
} finally {
    Pop-Location
}

$packageDirectory = Join-Path $outputDirectory 'package'
$packagePluginDirectory = Join-Path $packageDirectory 'Plugins'
New-Item -ItemType Directory -Path $packagePluginDirectory -Force | Out-Null
if ($UiOnly) {
    # Separate output prevents stale companion/fixture binaries entering a radio release.
    if (Get-ChildItem -LiteralPath $packagePluginDirectory -File |
        Where-Object Name -ne 'ml_livestations.dll') {
        throw 'Unexpected DLL/package content in UI-only staging; preserve and inspect it.'
    }
    Copy-Item -LiteralPath (Join-Path $outputDirectory 'ml_livestations.dll') `
        -Destination $packagePluginDirectory -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'UI_RELEASE.md') `
        -Destination (Join-Path $packageDirectory 'README.md') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot '..\LICENSE') `
        -Destination (Join-Path $packageDirectory 'LICENSE') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.txt') `
        -Destination $packageDirectory -Force
    Get-ChildItem -LiteralPath $outputDirectory -File | Select-Object FullName, Length, LastWriteTime
    return
}
Copy-Item -LiteralPath `
    (Join-Path $outputDirectory 'ml_livestations.dll'), `
    (Join-Path $outputDirectory 'gen_livestations_resolver.dll') `
    -Destination $packagePluginDirectory -Force
Copy-Item -LiteralPath `
    (Join-Path $projectRoot 'installer\Install-LiveStations.ps1'), `
    (Join-Path $projectRoot 'installer\README.md') `
    -Destination $packageDirectory -Force

Get-Item -LiteralPath `
    (Join-Path $outputDirectory 'ml_livestations.dll'), `
    (Join-Path $outputDirectory 'ml_livestations_fixture.dll'), `
    (Join-Path $outputDirectory 'gen_livestations_resolver.dll'), `
    (Join-Path $outputDirectory 'gen_livestations_resolver_fixture.dll'), `
    (Join-Path $outputDirectory 'radio_browser_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_abi_probe.exe'), `
    (Join-Path $outputDirectory 'resolver_abi_probe.exe'), `
    (Join-Path $outputDirectory 'resolver_server_probe.exe'), `
    (Join-Path $outputDirectory 'video_contract_probe.exe'), `
    (Join-Path $outputDirectory 'video_provider_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_host_probe.exe'), `
    (Join-Path $outputDirectory 'live_stations_contracts_probe.exe') |
    Select-Object FullName, Length, LastWriteTime
