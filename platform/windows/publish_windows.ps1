# Run from the repo root: .\platform\windows\publish_windows.ps1
# Builds (unless -SkipBuild) and publishes a clean, portable dist folder -
# double-click Bili.exe in the output, no installer, no PATH setup needed.
param(
    [string]$BuildDir = "build\windows-portable",
    [string]$OutDir = "dist\windows-portable",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

if (-not $SkipBuild) {
    . .\platform\windows\dev-env.ps1
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

if (Test-Path $OutDir) {
    # Best-effort clean of stale content (e.g. a previous debug run's log
    # files) - tolerate items still open in Explorer or a running Bili.exe;
    # Copy-Item -Force below will still overwrite what actually matters.
    Get-ChildItem $OutDir -Recurse -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Copy-Item "$BuildDir\app\Bili.exe" -Destination $OutDir -Force
# Qt's qt_add_qml_module deploys the "Bili" QML module as loose files
# (qmldir + Main.qml + .qmltypes) next to the exe rather than fully
# resource-embedding it (QTP0001 policy default on this Qt/CMake version) -
# without this folder the app builds and launches but exits immediately
# (QQmlApplicationEngine fails to load "Bili"/"Main", exit code -1).
Copy-Item "$BuildDir\app\Bili" -Destination $OutDir -Recurse -Force

$windeployqt = "D:\Qt\6.8.3\mingw_64\bin\windeployqt.exe"
& $windeployqt --qmldir ui "$OutDir\Bili.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

Write-Host "Portable build published to $OutDir - double-click Bili.exe directly, no installer needed."
