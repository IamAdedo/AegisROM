# macOS build (Apple Silicon + Intel)

AegisROM is universal: same tree builds on macOS with Apple Clang, Homebrew
dependencies, and no vcpkg required.

## 1. Toolchain

```bash
xcode-select --install   # Command Line Tools (Clang, SDK, git)
```

## 2. Dependencies (Homebrew)

```bash
brew install cmake ninja pkg-config qt@6 libusb libftdi
```

`libpci` is Linux-only — never needed on macOS. The `internal` (chipset)
programmer is effectively unavailable on modern Macs; use external USB
programmers (CH341A, FTDI, Bus Pirate, …) over `libusb`.

## 3. Configure

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/aegisrom-cli --help
```

Without a `libflashrom` binary the tree configures in mock-HAL mode. For real
hardware, build the sibling `../flashrom` with Meson, then re-configure with
`-DFLASHROM_LIBRARY=<libflashrom.dylib> -DFLASHROM_INCLUDE_DIR=../flashrom/include`.

## 4. Notes

- CH341A on macOS needs no vendor driver (uses built-in USB stack via libusb);
  if the device doesn't enumerate, check System Information → USB first.
- GUI packaging as an `.app` bundle (icons, `Contents/Resources` lookup à la
  the `FlashromGUI` `resource_path` pattern) lands in Phase 4.
