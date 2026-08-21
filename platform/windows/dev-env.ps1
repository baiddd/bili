# platform/windows/dev-env.ps1
# Dot-source this once per new PowerShell session before running any
# cmake/ctest/ninja command, or the built .exe, in this repo:
#   . .\platform\windows\dev-env.ps1
# Includes Qt's own bin dir (Qt6Core.dll etc.) and SDL2's bin dir (SDL2.dll)
# in addition to the build tools, since the built .exe needs those DLLs on
# PATH to launch locally (windeployqt packaging in Task 13 handles the Qt
# side of this for the distributed build; SDL2.dll will need to be copied
# alongside the exe too at that point). Also includes this repo's own
# platform/windows/tools dir (vendored 7za.exe, used by EmulatorProvider to
# extract RetroArch's .7z distribution) so a dev build finds it the same way
# the shipped app finds it next to Bili.exe.
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.8.3\mingw_64\bin;D:\SDL2\x86_64-w64-mingw32\bin;$PSScriptRoot\tools;$env:PATH"
Write-Host "Dev environment ready: cmake $(cmake --version | Select-Object -First 1), ninja $(ninja --version), mingw g++ $(g++ --version | Select-Object -First 1)"
