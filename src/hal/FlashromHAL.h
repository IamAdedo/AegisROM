#pragma once
// FlashromHAL — C++20 RAII wrapper over the flashrom C API (libflashrom.h).
//
// Reviewed against ../flashrom/include/libflashrom.h:
//   flashrom_init / flashrom_shutdown, flashrom_programmer_init / _shutdown,
//   flashrom_create_context, flashrom_flash_probe_v2, flashrom_flash_getsize,
//   flashrom_flash_erase / flashrom_flash_release, flashrom_image_read /
//   _write / _verify, flashrom_set_log_callback_v2,
//   flashrom_set_progress_callback_v2, flashrom_supported_programmers,
//   flashrom_data_free, flashrom_version_info.
//
// Direct binding only: this wrapper never spawns `flashrom` subprocesses.
// Define AEGISROM_MOCK_HAL=1 (done automatically by CMake when no libflashrom
// binary is found) to build a hardware-free simulated 4 MiB chip so the CLI,
// agent, and CI stay functional on machines without drivers/toolchains.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aegis {

enum class ProgressStage { Read, Write, Erase };

using LogCallback = std::function<void(int level, const std::string& message)>;
using ProgressCallback =
    std::function<void(ProgressStage stage, std::size_t current, std::size_t total)>;

struct ProbeResult {
  std::string chipName;
  std::vector<std::string> allMatched;
  std::size_t chipSize = 0;
};

class FlashromHAL {
 public:
  FlashromHAL();
  ~FlashromHAL();

  FlashromHAL(const FlashromHAL&) = delete;
  FlashromHAL& operator=(const FlashromHAL&) = delete;
  FlashromHAL(FlashromHAL&&) noexcept;
  FlashromHAL& operator=(FlashromHAL&&) noexcept;

  bool init(bool selfCheck = false);
  void shutdown();
  [[nodiscard]] bool initialized() const { return initialized_; }
  [[nodiscard]] bool isMock() const;

  static const char* version();
  std::vector<std::string> listProgrammers();

  void setLogCallback(LogCallback cb);
  void setProgressCallback(ProgressCallback cb);

  // Probe exactly one chip through `programmer` (e.g. "ch341a_spi").
  // `chipName` empty => probe all known chips (must resolve to exactly one).
  bool open(const std::string& programmer, const std::string& params,
            const std::string& chipName = "");
  void close();
  [[nodiscard]] bool isOpen() const { return isOpen_; }
  [[nodiscard]] std::size_t chipSize() const { return chipSize_; }
  [[nodiscard]] const ProbeResult& probeResult() const { return probe_; }

  bool read(std::vector<std::uint8_t>& out);
  bool erase();
  // `image` is taken by value: libflashrom may alter the buffer during full
  // verification, and the caller keeps its pristine copy for hash checks.
  bool write(std::vector<std::uint8_t> image);
  bool verify(const std::vector<std::uint8_t>& image);

  [[nodiscard]] const std::string& lastError() const { return lastError_; }

 private:
  void* ctx_ = nullptr;   // struct flashrom_flashctx* (opaque here)
  void* prog_ = nullptr;  // struct flashrom_programmer* (opaque here)
  bool initialized_ = false;
  bool isOpen_ = false;
  std::size_t chipSize_ = 0;
  ProbeResult probe_;
  std::string lastError_;
  LogCallback logCb_;
  ProgressCallback progressCb_;
  std::vector<std::uint8_t> mockImage_;  // mock-HAL simulated flash only
};

}  // namespace aegis
