# AegisROM

Autonomous, hybrid AI hardware flashing and EEPROM programming suite.

AegisROM merges three codebases into one unified C++20 / Qt6 / CMake tree:

| Source          | Role in AegisROM                                              |
|-----------------|---------------------------------------------------------------|
| `../flashrom`   | Low-level backend motor. Linked via the C API (`libflashrom.h`), never via `system("flashrom ...")` subprocess calls. |
| `../FlashromGUI`| Driver-management and wrapper-logic reference (programmer lists, detect/read/write/verify flows). Reimplemented natively against `libflashrom` in `src/hal`. |
| `../IMSProg`    | Modern Qt GUI reference: `qhexedit2` hex editor, chip database (`IMSProg.Dat`), visual pinout UI. Ported into `src/gui` in Phase 2. |

> Sibling layout: this repo lives at `<workspace>/AegisROM` next to `<workspace>/flashrom`,
> `<workspace>/FlashromGUI`, and `<workspace>/IMSProg`. The CMake tree references those
> paths but never commits their code.

## Operating modes

- **CLI Manual** — explicit flag-driven flashing:
  `aegisrom-cli --programmer ch341a_spi --write BIOS.bin`
- **CLI Agentic** — autonomous engine over heuristics (LLM hooks optional):
  `aegisrom-cli --auto-flash --target-type laptop_bios`
- **GUI Manual** — Qt6 window with hex editor, pinout diagrams, read/write/erase controls.
- **GUI Agentic** — "One-Click AI Auto-Flash" toggle with live agent decision log.

## Layout

```text
AegisROM/
├── CMakeLists.txt                 # unified tree: aegisrom-cli + aegisrom
├── src/
│   ├── hal/FlashromHAL.*          # Phase 1: direct libflashrom binding (mock-capable)
│   ├── cli/main.cpp               # Phase 1: Qt-free CLI, manual + --auto-flash
│   ├── agent/AegisAgent.*         # Phase 3: tool-calling core, backup-enforced
│   ├── common/                    # ChipDatabaseManager, HashUtils (CRC32/SHA256)
│   └── gui/                       # Phase 2/3: MainWindow, hexeditor bridge,
│       ├── components/hexeditor/  #   pinout viewer, autonomous widget
│       └── widgets/
├── cmake/                         # CMake helpers
├── docs/BUILD_WINDOWS.md          # Windows toolchain setup (Chocolatey + vcpkg)
└── tests/                         # Phase 4: ctest smoke tests
```

## Safety guardrails (enforced in `AegisAgent`)

1. A verified backup is **forced before ANY erase or write**. The sequence aborts if the backup fails.
2. Every write is followed by a whole-chip verify plus CRC32/SHA256 comparison against the source image.
3. `verify_voltage()` warns when a 1.8 V part is driven at 3.3 V (CH341A and similar).

## Build (Windows / macOS / Linux)

AegisROM is universal — one tree, three platforms:

| Platform | Toolchain | Deps | Guide |
|----------|-----------|------|-------|
| Windows  | MSVC 2019+ or Clang (Chocolatey `llvm`) | vcpkg: `qtbase libusb libftdi` | `docs/BUILD_WINDOWS.md` |
| macOS (arm64 + x64) | Apple Clang | Homebrew: `qt@6 libusb libftdi` | `docs/BUILD_MACOS.md` |
| Linux | GCC / Clang | distro: `qt6-base-dev libusb-1.0-0-dev libftdi1-dev libpci-dev` | `docs/BUILD_LINUX.md` |

`libpci` is Linux-only and is never required on Windows/macOS (only the
`internal` chipset programmer needs it; USB programmers use
`libusb`/`libftdi` everywhere).

```powershell
# Windows (details in docs/BUILD_WINDOWS.md)
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

```bash
# macOS (details in docs/BUILD_MACOS.md)
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

```bash
# Linux (details in docs/BUILD_LINUX.md)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

No `libflashrom` binary around? The tree still configures: the HAL compiles in
**mock mode** (`AEGISROM_MOCK_HAL=1`, 4 MiB simulated chip) so CLI logic and CI
stay green. Re-configure with `-DFLASHROM_LIBRARY=... -DFLASHROM_INCLUDE_DIR=...`
once the library exists.

## Roadmap

- **Phase 1** — Core engine & HAL (this scaffold): `FlashromHAL`, `aegisrom-cli`.
- **Phase 2** — Port IMSProg `qhexedit2` + chip database parser + `PinoutViewer`.
- **Phase 3** — `AegisAgent` mechanics, CLI `--agent` flag, GUI autonomous widget.
- **Phase 4** — Unified build hardening, hardware-in-the-loop tests.

## License

GPL-2.0-or-later for anything derived from flashrom/IMSProg sources, matching
their upstream licenses. New AegisROM glue follows the same terms unless noted.
