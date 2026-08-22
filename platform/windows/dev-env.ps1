# platform/windows/dev-env.ps1
# Dot-source this once per new PowerShell session before running any
# cmake/ctest/ninja command, or the built .exe, in this repo:
#   . .\platform\windows\dev-env.ps1
# Includes Qt's own bin dir (Qt6Core.dll etc.) and SDL2's bin dir (SDL2.dll)
# in addition to the build tools, since the built .exe needs those DLLs on
# PATH to launch locally (windeployqt packaging in Task 13 handles the Qt
# side of this for the distributed build; SDL2.dll will need to be copied
# alongside the exe too at that point).
#
# Does NOT include this repo's own platform/windows/tools dir (vendored
# 7za.exe) on PATH: it used to be here on the assumption it would help
# EmulatorProvider find 7za.exe, but EmulatorProvider::sevenZipExecutablePath()
# calls QStandardPaths::findExecutable("7za", {applicationDirPath()}) with a
# non-empty paths list, which makes Qt search *only* that list and never
# also fall back to PATH - confirmed during Task 8's manual verification
# that this PATH entry never actually helped that lookup at all. A plain
# `cmake --build` instead finds 7za.exe via app/CMakeLists.txt's own
# POST_BUILD step, which copies it next to Bili.exe directly.
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.8.3\mingw_64\bin;D:\SDL2\x86_64-w64-mingw32\bin;$env:PATH"
Write-Host "Dev environment ready: cmake $(cmake --version | Select-Object -First 1), ninja $(ninja --version), mingw g++ $(g++ --version | Select-Object -First 1)"
