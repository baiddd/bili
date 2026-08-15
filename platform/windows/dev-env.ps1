# platform/windows/dev-env.ps1
# Dot-source this once per new PowerShell session before running any
# cmake/ctest/ninja command, or the built .exe, in this repo:
#   . .\platform\windows\dev-env.ps1
# Includes Qt's own bin dir (Qt6Core.dll etc.) in addition to the build
# tools, since the built .exe needs those Qt DLLs on PATH to launch locally
# (windeployqt packaging in Task 13 handles this for the distributed build).
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.8.3\mingw_64\bin;$env:PATH"
Write-Host "Dev environment ready: cmake $(cmake --version | Select-Object -First 1), ninja $(ninja --version), mingw g++ $(g++ --version | Select-Object -First 1)"
