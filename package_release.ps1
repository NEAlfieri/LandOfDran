<#
.SYNOPSIS
Packages a release Windows binary along with the files it needs at runtime
(Assets, Shaders, Add-ons, serverstart.lua, and Saves if present) into a single zip archive.
Assets are taken from the working tree, so the gitignored ones (music, .webm prints) go in too.

.DESCRIPTION
Unlike the Linux release script, there's no glibc-style ABI baseline to
worry about here, and vcpkg's VCPKG_APPLOCAL_DEPS behavior already copies
every DLL the exe needs (SDL2, assimp, etc.) into the build output folder
next to LandOfDran.exe. So packaging just means zipping that folder up
alongside the runtime assets.

.PARAMETER Output
Path of the zip file to create. Defaults to LandOfDran-release-<hash>-windows.zip.

.PARAMETER SkipBuild
Skip building and package whatever is already in cmake-build-windows/Release.
Faster for local iteration, but you're responsible for it being up to date.

.EXAMPLE
.\package_release.ps1

.EXAMPLE
.\package_release.ps1 -SkipBuild -Output test.zip
#>
param(
    [string]$Output,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$Version = $null
try { $Version = (git rev-parse --short HEAD 2>$null).Trim() } catch {}
if (-not $Version) { $Version = Get-Date -Format "yyyyMMdd" }

if (-not $Output) { $Output = "LandOfDran-release-$Version-windows.zip" }

$BuildDir = "cmake-build-windows"
$OutDir = Join-Path $BuildDir "Release"

if (-not $SkipBuild) {
    cmake --build --preset windows-release
}

$Binary = Join-Path $OutDir "LandOfDran.exe"
if (-not (Test-Path $Binary)) {
    Write-Error "No binary found at '$Binary' - build the project first (or drop -SkipBuild)."
}

$Staging = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
$PkgDir = Join-Path $Staging "LandOfDran"
New-Item -ItemType Directory -Path $PkgDir -Force | Out-Null

try {
    Copy-Item (Join-Path $OutDir "*.exe") $PkgDir
    Copy-Item (Join-Path $OutDir "*.dll") $PkgDir -ErrorAction SilentlyContinue
    # From the local copy rather than git, so gitignored assets are packaged too: Assets/music and the
    # .webm prints under Assets/brick/prints are both kept out of git for their size, see .gitignore
    Copy-Item "Assets" $PkgDir -Recurse
    Copy-Item "Shaders" $PkgDir -Recurse
    # The add-ons, which is where the game's systems live too, loaded by path relative to the working directory
    Copy-Item "Add-ons" $PkgDir -Recurse
    Copy-Item "serverstart.lua" $PkgDir
    Copy-Item "FallingTiles.lua" $PkgDir
    # The game that plays behind the main menu, which runs serverstart.lua itself, see LoopClient::startMenuDemo
    Copy-Item "menudemo.lua" $PkgDir

    # Saves is gitignored, so this packages whatever builds are in the local copy, if there is one
    if (Test-Path "Saves") {
        Copy-Item "Saves" $PkgDir -Recurse
    }

    if (Test-Path $Output) { Remove-Item $Output }
    Compress-Archive -Path $PkgDir -DestinationPath $Output

    $Size = "{0:N1} MB" -f ((Get-Item $Output).Length / 1MB)
    Write-Host "Created $Output ($Size)"
}
finally {
    Remove-Item $Staging -Recurse -Force
}
