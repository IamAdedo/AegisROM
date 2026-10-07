# Linux build (Debian/Ubuntu shown; adapt package names per distro)

AegisROM is universal: same tree builds on Linux with GCC or Clang and distro
packages. Linux is the only platform where `libpci` exists, so it's the only
platform where the `internal` chipset programmer can work.

## 1. Dependencies (apt)

```bash
sudo apt update
sudo apt install -y \
  build-essential clang cmake ninja-build pkg-config \
  qt6-base-dev qt6-svg-dev \
  libusb-1.0-0-dev libftdi1-dev libpci-dev
```

## 2. USB permissions

External programmers (CH341A, FTDI, …) need udev access, otherwise flashing
fails with permission errors even though probing lists the device:

```bash
sudo usermod -aG dialout,plugdev "$USER"
# log out and back in, then verify:
lsusb | grep -Ei '1a86|0403|0483'
```

## 3. Configure

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build
```

Without a `libflashrom` binary the tree configures in mock-HAL mode. For real
hardware, build the sibling `../flashrom` with Meson, then re-configure with
`-DFLASHROM_LIBRARY=<libflashrom.so> -DFLASHROM_INCLUDE_DIR=../flashrom/include`.
