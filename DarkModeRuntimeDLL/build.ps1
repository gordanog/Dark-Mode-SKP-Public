# build.ps1 - Configure and build DarkModeRuntimeDLL with CMake.

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path $PSCommandPath -Parent
$buildDirectory = Join-Path $root 'out\build'
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    throw 'CMake was not found. Install CMake and the Visual Studio C++ Build Tools.'
}

$configureArgs = @(
    '-S', $root,
    '-B', $buildDirectory,
    '-A', 'x64'
)

& $cmake.Source @configureArgs
& $cmake.Source --build $buildDirectory --config $Configuration --target DarkModeRuntimeDLL