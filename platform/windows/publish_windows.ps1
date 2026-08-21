# Run from the repo root: .\platform\windows\publish_windows.ps1
# Builds (unless -SkipBuild) and publishes a clean, portable dist folder.
# Layout: dist\windows-portable\Bili.lnk (double-click this) + app\ (the
# real exe + all its DLLs/subfolders). Windows resolves an exe's directly
# linked DLLs (Qt6Core.dll, SDL2.dll, MinGW runtime, etc.) only from the
# exe's own directory or PATH - there is no config-based redirect for
# this (qt.conf only affects Qt's own plugin/QML search, not the OS
# loader), so the exe and its DLLs must stay siblings in app\. The
# shortcut at the top level keeps dist\ itself uncluttered.
param(
    [string]$BuildDir = "build\windows-portable",
    [string]$OutDir = "dist\windows-portable",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$AppDir = Join-Path $OutDir "app"

if (-not $SkipBuild) {
    . .\platform\windows\dev-env.ps1
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

if (Test-Path $OutDir) {
    # Best-effort clean of stale content - tolerate items still open in
    # Explorer or a running Bili.exe; Copy-Item -Force below still
    # overwrites what actually matters.
    Get-ChildItem $OutDir -Recurse -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null

Copy-Item "$BuildDir\Bili.exe" -Destination $AppDir -Force
# Qt's qt_add_qml_module deploys the "Bili" QML module as loose files
# (qmldir + Main.qml + .qmltypes) next to the exe rather than fully
# resource-embedding it (QTP0001 policy default on this Qt/CMake version) -
# without this folder the app builds and launches but exits immediately
# (QQmlApplicationEngine fails to load "Bili"/"Main", exit code -1).
# qt_standard_project_setup() (added to fix Theme singleton registration -
# see CMakeLists.txt) moved the final Bili.exe to the build dir root, but
# the target's intermediate/loose QML module folder is still generated
# under its own target build subdir (app\Bili) - verified against the
# actual build tree, not just the CMake change's stated side effect.
Copy-Item "$BuildDir\app\Bili" -Destination $AppDir -Recurse -Force

$windeployqt = "D:\Qt\6.8.3\mingw_64\bin\windeployqt.exe"
& $windeployqt --qmldir ui "$AppDir\Bili.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# windeployqt only resolves Qt's own dependency graph, not SDL2 (added in
# Task 8 for gamepad support) - copy its runtime DLL manually or the
# published exe crashes with STATUS_DLL_NOT_FOUND.
Copy-Item "D:\SDL2\x86_64-w64-mingw32\bin\SDL2.dll" -Destination $AppDir -Force

# EmulatorProvider::sevenZipExecutablePath() resolves "7za" via PATH plus the
# app's own directory, so the vendored 7za.exe (used to extract RetroArch's
# .7z distribution - see docs/index.md) must sit next to Bili.exe in the
# published app folder.
Copy-Item "$PSScriptRoot\tools\7za.exe" -Destination $AppDir -Force

$shortcutPath = Join-Path (Resolve-Path $OutDir) "Bili.lnk"
$exePath = Join-Path (Resolve-Path $AppDir) "Bili.exe"
$ws = New-Object -ComObject WScript.Shell
$shortcut = $ws.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $exePath
$shortcut.WorkingDirectory = (Resolve-Path $AppDir).Path
$shortcut.Description = "Bili - emulator frontend"
$shortcut.Save()

Write-Host "Portable build published to $OutDir - double-click Bili.lnk directly, no installer needed."
