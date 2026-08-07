param(
    [string]$WinampDirectory = 'C:\Program Files (x86)\Winamp'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $projectRoot 'build\x86\release\in_svloopback.dll'
$pluginsDirectory = Join-Path $WinampDirectory 'Plugins'
$target = Join-Path $pluginsDirectory 'in_svloopback.dll'

if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Built plug-in was not found: $source"
}
if (-not (Test-Path -LiteralPath (Join-Path $WinampDirectory 'winamp.exe') -PathType Leaf)) {
    throw "Winamp executable was not found: $WinampDirectory"
}
if (-not (Test-Path -LiteralPath $pluginsDirectory -PathType Container)) {
    throw "Winamp plug-in directory was not found: $pluginsDirectory"
}

$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
if (Test-Path -LiteralPath $target -PathType Leaf) {
    $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($targetHash -eq $sourceHash) {
        Write-Output "Already installed: $target"
        exit 0
    }
    throw "A different in_svloopback.dll already exists; refusing to overwrite: $target"
}

Copy-Item -LiteralPath $source -Destination $target
$installedHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
if ($installedHash -ne $sourceHash) {
    throw "Installed plug-in hash mismatch: $target"
}

Write-Output "Installed: $target"
Write-Output "SHA-256: $installedHash"

