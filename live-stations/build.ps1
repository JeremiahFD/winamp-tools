param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

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
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$common = @(
    '/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/WX',
    '/DUNICODE', '/D_UNICODE', '/DNOMINMAX',
    "/I`"$(Join-Path $projectRoot 'include')`"",
    "/I`"$(Join-Path $projectRoot 'src')`""
)
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
    'comctl32.lib', 'dnsapi.lib', 'gdi32.lib', 'ole32.lib', 'user32.lib',
    'winhttp.lib', 'ws2_32.lib', 'xmllite.lib'
)

Push-Location $projectRoot
try {
    Invoke-Compiler ($common + @(
        '/LD', 'src\plugin.cpp', 'src\radio_browser.cpp',
        "/Fe:$outputDirectory\ml_livestations.dll",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/DEF:$(Join-Path $projectRoot 'live_stations.def')",
        "/PDB:$outputDirectory\ml_livestations.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\probe.cpp', 'src\radio_browser.cpp',
        "/Fe:$outputDirectory\radio_browser_probe.exe",
        "/Fo:$outputDirectory\",
        '/link'
    ) + $libraries + @(
        "/PDB:$outputDirectory\radio_browser_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\plugin_abi_probe.cpp',
        "/Fe:$outputDirectory\plugin_abi_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'user32.lib',
        "/PDB:$outputDirectory\plugin_abi_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\plugin_host_probe.cpp',
        "/Fe:$outputDirectory\plugin_host_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'comctl32.lib', 'user32.lib',
        "/PDB:$outputDirectory\plugin_host_probe.pdb"
    ))
} finally {
    Pop-Location
}

Get-Item -LiteralPath `
    (Join-Path $outputDirectory 'ml_livestations.dll'), `
    (Join-Path $outputDirectory 'radio_browser_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_abi_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_host_probe.exe') |
    Select-Object FullName, Length, LastWriteTime
