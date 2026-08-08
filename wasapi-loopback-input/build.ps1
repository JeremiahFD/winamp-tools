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

Push-Location $projectRoot
try {
    Invoke-Compiler ($common + @(
        '/LD', 'src\plugin.cpp', 'src\wasapi_loopback.cpp',
        "/Fe:$outputDirectory\in_svloopback.dll",
        "/Fo:$outputDirectory\",
        '/link', 'ole32.lib', 'propsys.lib', 'user32.lib',
        "/DEF:$(Join-Path $projectRoot 'winamp_loopback.def')",
        "/PDB:$outputDirectory\in_svloopback.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\probe.cpp', 'src\wasapi_loopback.cpp',
        "/Fe:$outputDirectory\loopback_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'ole32.lib', 'propsys.lib',
        "/PDB:$outputDirectory\loopback_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\endpoint_signal_probe.cpp',
        "/Fe:$outputDirectory\endpoint_signal_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'ole32.lib', 'propsys.lib', 'user32.lib',
        "/PDB:$outputDirectory\endpoint_signal_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\plugin_abi_probe.cpp',
        "/Fe:$outputDirectory\plugin_abi_probe.exe",
        "/Fo:$outputDirectory\",
        '/link',
        "/PDB:$outputDirectory\plugin_abi_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\plugin_host_probe.cpp',
        "/Fe:$outputDirectory\plugin_host_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'user32.lib',
        "/PDB:$outputDirectory\plugin_host_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\minidump_stack_probe.cpp',
        "/Fe:$outputDirectory\minidump_stack_probe.exe",
        "/Fo:$outputDirectory\",
        '/link', 'dbghelp.lib',
        "/PDB:$outputDirectory\minidump_stack_probe.pdb"
    ))

    Invoke-Compiler ($common + @(
        'src\rate_adapter_probe.cpp',
        "/Fe:$outputDirectory\rate_adapter_probe.exe",
        "/Fo:$outputDirectory\",
        '/link',
        "/PDB:$outputDirectory\rate_adapter_probe.pdb"
    ))
} finally {
    Pop-Location
}

Get-Item -LiteralPath `
    (Join-Path $outputDirectory 'in_svloopback.dll'), `
    (Join-Path $outputDirectory 'loopback_probe.exe'), `
    (Join-Path $outputDirectory 'endpoint_signal_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_abi_probe.exe'), `
    (Join-Path $outputDirectory 'plugin_host_probe.exe'), `
    (Join-Path $outputDirectory 'minidump_stack_probe.exe'), `
    (Join-Path $outputDirectory 'rate_adapter_probe.exe') |
    Select-Object FullName, Length, LastWriteTime
