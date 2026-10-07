# Windows build environment (Chocolatey + vcpkg)

Target: native `x64-windows` build of `aegisrom-cli` and the Qt6 `aegisrom` GUI.

> `libpci` / `libpci-dev` are **Linux-only**. Never install or search for them on
> Windows. The libpci-backed flashrom programmers (internal chipset flashing)
> simply do not apply here; external USB programmers (CH341A, FTDI, Bus Pirate,
> …) work through `libusb`/`libftdi`, which *are* installed below.

## 0. Start an elevated shell

Chocolatey and `C:\vcpkg` need Administrator rights. Open **PowerShell as
Administrator** for steps 1–4. The agent loop runs non-elevated, so perform
these steps in a separate admin window and report back.

## 1. Chocolatey

```powershell
Get-Command choco -ErrorAction SilentlyContinue
# If missing, allow the official installer for this process only, then install:
Set-ExecutionPolicy Bypass -Scope Process -Force
[System.Net.ServicePointManager]::SecurityProtocol =
  [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
iex ((New-Object System.Net.WebClient).DownloadString(
  'https://community.chocolatey.org/install.ps1'))
choco --version
```

## 2. Build tools (CMake + LLVM/Clang)

```powershell
choco install cmake llvm -y
# Refresh PATH in the current session without breaking the agent loop:
$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') +
  ';' + [System.Environment]::GetEnvironmentVariable('Path','User')
cmake --version
clang --version
```

## 3. vcpkg

```powershell
if (-not (Test-Path -LiteralPath 'C:\vcpkg')) {
  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
}
C:\vcpkg\bootstrap-vcpkg.bat
```

## 4. Libraries

```powershell
C:\vcpkg\vcpkg install qtbase libusb libftdi --triplet=x64-windows
```

If a port fails to compile with errors about missing English language packs or
Visual Studio Build Tools components (e.g. ATL/MFC, Windows SDK, C++ CMake
tools), **stop, copy the exact error**, and notify before retrying — installing
the wrong VS workload wastes a full rebuild.

## 5. Configure AegisROM

```powershell
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\aegisrom-cli.exe --help
```

## Path summary (confirmed 2026-10-07)

| Tool         | Location                                                       | Version |
|--------------|----------------------------------------------------------------|---------|
| `cmake`      | `C:\Program Files\CMake\bin\cmake.exe` (Chocolatey)            | 4.4.4   |
| `clang`      | `C:\Program Files\LLVM\bin\clang.exe` (Chocolatey `llvm`)      | 22.1.8  |
| vcpkg        | `C:\vcpkg\vcpkg.exe`                                           | —       |
| vcpkg toolchain | `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`                 | —       |

Without `libflashrom` built yet, configure still succeeds in mock-HAL mode;
see the root `CMakeLists.txt` and `README.md`.
