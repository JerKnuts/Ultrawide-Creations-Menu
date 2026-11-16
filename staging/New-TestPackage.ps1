[CmdletBinding()]
param(
    [ValidatePattern('^[a-zA-Z0-9][a-zA-Z0-9-]{0,23}$')]
    [string]$Label = 'local'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousXmakeGlobalDir = $env:XMAKE_GLOBALDIR

Push-Location -LiteralPath $projectRoot
try {
    $version = (Get-Content -LiteralPath VERSION -Raw).Trim()
    if ($version -notmatch '^\d+\.\d+\.\d+$') {
        throw "VERSION must contain an A.B.C release version."
    }
    if (Test-Path -LiteralPath 'build/xmake-local' -PathType Container) {
        $env:XMAKE_GLOBALDIR = (Resolve-Path -LiteralPath 'build/xmake-local').Path
    }

    xmake build UltrawideCreationsMenu
    if ($LASTEXITCODE -ne 0) {
        throw "XMake failed with exit code $LASTEXITCODE."
    }
    $dll = Get-Item -LiteralPath 'build/windows/x64/release/UltrawideCreationsMenu.dll'
    if ($dll.VersionInfo.ProductVersion -ne "$version.0") {
        throw "Built DLL reports version $($dll.VersionInfo.ProductVersion), expected $version.0."
    }

    $nonce = [guid]::NewGuid().ToString('N').Substring(0, 8)
    $candidateName = "Ultrawide-Creations-Menu-v$version-$Label-$nonce"
    $stage = New-Item -ItemType Directory -Path "build/staging/$candidateName"
    $plugins = New-Item -ItemType Directory -Path (Join-Path $stage.FullName 'SFSE/Plugins')
    Copy-Item -LiteralPath $dll.FullName -Destination $plugins.FullName
    # MO2 uses the leading words before the first hyphen for local installs.
    $archiveName = "Ultrawide Creations Menu-v$version-MO2.zip"
    $archive = Join-Path $stage.FullName $archiveName
    $pluginPath = "SFSE/Plugins/$($dll.Name)"
    tar -a -cf $archive -C $stage.FullName $pluginPath
    if ($LASTEXITCODE -ne 0) {
        throw "Packaging failed with exit code $LASTEXITCODE."
    }
    $entries = @(tar -tf $archive)
    if ($LASTEXITCODE -ne 0 -or $entries.Count -ne 1 -or $entries[0] -ne $pluginPath) {
        throw "Unexpected archive contents in $archive."
    }
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $archiveName" | Set-Content -LiteralPath (Join-Path $stage.FullName 'SHA256SUMS.txt') -Encoding ASCII
    Write-Output $archive
}
finally {
    $env:XMAKE_GLOBALDIR = $previousXmakeGlobalDir
    Pop-Location
}
