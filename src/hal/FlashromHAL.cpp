// FlashromHAL implementation. Real path binds directly to libflashrom;
// mock path simulates a 4 MiB SPI flash (all 0xFF + fake header).
#include "FlashromHAL.h"

#include <algorithm>
#include <cstring>

#ifndef AEGISROM_MOCK_HAL
#include "libflashrom.h"
#endif

namespace aegis {

constexpr std::size_t kMockSize = 4u * 1024u * 1024u;

FlashromHAL::FlashromHAL() = default;

FlashromHAL::~FlashromHAL() {
  close();
  shutdown();
}

FlashromHAL::FlashromHAL(FlashromHAL&& other) noexcept
    : ctx_(other.ctx_),
      prog_(other.prog_),
      initialized_(other.initialized_),
      isOpen_(other.isOpen_),
      chipSize_(other.chipSize_),
      probe_(std::move(other.probe_)),
      lastError_(std::move(other.lastError_)),
      logCb_(std::move(other.logCb_)),
      progressCb_(std::move(other.progressCb_)),
      mockImage_(std::move(other.mockImage_)) {
  other.ctx_ = nullptr;
  other.prog_ = nullptr;
  other.initialized_ = false;
  other.isOpen_ = false;
  other.chipSize_ = 0;
}

FlashromHAL& FlashromHAL::operator=(FlashromHAL&& other) noexcept {
  if (this != &other) {
    close();
    shutdown();
    ctx_ = other.ctx_;
    prog_ = other.prog_;
    initialized_ = other.initialized_;
    isOpen_ = other.isOpen_;
    chipSize_ = other.chipSize_;
    probe_ = std::move(other.probe_);
    lastError_ = std::move(other.lastError_);
    logCb_ = std::move(other.logCb_);
    progressCb_ = std::move(other.progressCb_);
    mockImage_ = std::move(other.mockImage_);
    other.ctx_ = nullptr;
    other.prog_ = nullptr;
    other.initialized_ = false;
    other.isOpen_ = false;
    other.chipSize_ = 0;
  }
  return *this;
}

bool FlashromHAL::isMock() const {
#ifdef AEGISROM_MOCK_HAL
  return true;
#else
  return false;
#endif
}

const char* FlashromHAL::version() {
#ifdef AEGISROM_MOCK_HAL
  return "AegisROM-HAL mock (no libflashrom linked)";
#else
  return flashrom_version_info();
#endif
}

void FlashromHAL::setLogCallback(LogCallback cb) {
  logCb_ = std::move(cb);
#ifdef AEGISROM_MOCK_HAL
  (void)0;
#else
  if (initialized_) {
    if (logCb_) {
      flashrom_set_log_callback_v2(
          [](flashrom_log_level level, const char* message, void* userData) {
            auto* cb = static_cast<LogCallback*>(userData);
            if (cb && *cb) (*cb)(static_cast<int>(level), message ? message : "");
          },
          &logCb_);
    } else {
      flashrom_set_log_callback_v2(nullptr, nullptr);
    }
  }
#endif
}

void FlashromHAL::setProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }

bool FlashromHAL::init(bool selfCheck) {
#ifdef AEGISROM_MOCK_HAL
  (void)selfCheck;
  initialized_ = true;
  return true;
#else
  if (initialized_) return true;
  if (flashrom_init(selfCheck ? 1 : 0) != 0) {
    lastError_ = "flashrom_init failed (selfcheck requested)";
    return false;
  }
  initialized_ = true;
  if (logCb_) {
    LogCallback cb = logCb_;  // re-arm through the setter path
    logCb_ = nullptr;
    setLogCallback(std::move(cb));
  }
  return true;
#endif
}

void FlashromHAL::shutdown() {
  if (!initialized_) return;
  close();
#ifdef AEGISROM_MOCK_HAL
  initialized_ = false;
#else
  flashrom_shutdown();
  initialized_ = false;
#endif
}

std::vector<std::string> FlashromHAL::listProgrammers() {
#ifdef AEGISROM_MOCK_HAL
  return {"ch341a_spi", "ft2232_spi", "buspirate_spi", "serprog",
          "ch347_spi", "dirtyjtag_spi", "stlinkv3_spi", "dummy"};
#else
  std::vector<std::string> out;
  const char** names = flashrom_supported_programmers();
  if (!names) {
    lastError_ = "flashrom_supported_programmers returned null";
    return out;
  }
  for (const char** p = names; *p != nullptr; ++p) out.emplace_back(*p);
  flashrom_data_free(names);
  return out;
#endif
}

bool FlashromHAL::open(const std::string& programmer, const std::string& params,
                       const std::string& chipName) {
  close();
  if (!initialized_ && !init(false)) return false;

#ifdef AEGISROM_MOCK_HAL
  if (programmer.empty()) {
    lastError_ = "programmer name required";
    return false;
  }
  mockImage_.assign(kMockSize, 0xFF);
  // Fake SPI header: JEDEC-like ID + magic so read_chip_header has content.
  const char magic[] = "AegisROM-MOCK-NOR";
  std::memcpy(mockImage_.data(), magic, sizeof(magic) - 1);
  mockImage_[0x10] = 0xEF;  // Winbond-style manufacturer ID (simulated)
  mockImage_[0x11] = 0x40;
  mockImage_[0x12] = 0x18;
  probe_.chipName = chipName.empty() ? "MOCK W25Q32" : chipName;
  probe_.allMatched = {probe_.chipName};
  probe_.chipSize = kMockSize;
  chipSize_ = kMockSize;
  isOpen_ = true;
  if (logCb_) logCb_(2, "mock HAL: opened programmer '" + programmer + "'");
  return true;
#else
  struct flashrom_programmer* prog = nullptr;
  if (flashrom_programmer_init(&prog, programmer.c_str(),
                               params.empty() ? nullptr : params.c_str()) != 0) {
    lastError_ = "flashrom_programmer_init failed for '" + programmer + "'";
    return false;
  }
  struct flashrom_flashctx* ctx = nullptr;
  if (flashrom_create_context(&ctx) != 0) {
    lastError_ = "flashrom_create_context failed";
    flashrom_programmer_shutdown(prog);
    return false;
  }
  flashrom_set_progress_callback_v2(
      ctx,
      [](flashrom_progress_stage stage, std::size_t current, std::size_t total,
         void* userData) {
        auto* cb = static_cast<ProgressCallback*>(userData);
        if (!cb || !*cb) return;
        ProgressStage s = ProgressStage::Read;
        if (stage == FLASHROM_PROGRESS_WRITE) s = ProgressStage::Write;
        if (stage == FLASHROM_PROGRESS_ERASE) s = ProgressStage::Erase;
        (*cb)(s, current, total);
      },
      &progressCb_);

  const char** matched = nullptr;
  int count = flashrom_flash_probe_v2(
      ctx, &matched, prog, chipName.empty() ? nullptr : chipName.c_str());
  ProbeResult probe;
  if (matched) {
    for (const char** p = matched; *p != nullptr; ++p) probe.allMatched.emplace_back(*p);
    flashrom_data_free(matched);
  }
  if (count < 0) {
    lastError_ = "probe error (libflashrom failure)";
    flashrom_flash_release(ctx);
    flashrom_programmer_shutdown(prog);
    return false;
  }
  if (count == 0) {
    lastError_ = "no flash chip found";
    flashrom_flash_release(ctx);
    flashrom_programmer_shutdown(prog);
    return false;
  }
  if (count > 1) {
    lastError_ = "multiple chips matched (" + std::to_string(count) +
                 "); specify --chip explicitly";
    flashrom_flash_release(ctx);
    flashrom_programmer_shutdown(prog);
    return false;
  }
  probe.chipName = probe.allMatched.empty() ? chipName : probe.allMatched.front();
  probe.chipSize = flashrom_flash_getsize(ctx);
  ctx_ = ctx;
  prog_ = prog;
  probe_ = std::move(probe);
  chipSize_ = probe_.chipSize;
  isOpen_ = true;
  return true;
#endif
}

void FlashromHAL::close() {
  if (!isOpen_) {
#ifndef AEGISROM_MOCK_HAL
    // Defensive: free strays if open() failed midway (null-safe).
    if (ctx_ != nullptr) {
      flashrom_flash_release(static_cast<struct flashrom_flashctx*>(ctx_));
      ctx_ = nullptr;
    }
    if (prog_ != nullptr) {
      flashrom_programmer_shutdown(static_cast<struct flashrom_programmer*>(prog_));
      prog_ = nullptr;
    }
#endif
    chipSize_ = 0;
    return;
  }
#ifdef AEGISROM_MOCK_HAL
  mockImage_.clear();
#else
  if (ctx_ != nullptr) {
    flashrom_flash_release(static_cast<struct flashrom_flashctx*>(ctx_));
    ctx_ = nullptr;
  }
  if (prog_ != nullptr) {
    flashrom_programmer_shutdown(static_cast<struct flashrom_programmer*>(prog_));
    prog_ = nullptr;
  }
#endif
  isOpen_ = false;
  chipSize_ = 0;
}

bool FlashromHAL::read(std::vector<std::uint8_t>& out) {
  if (!isOpen_) {
    lastError_ = "read: no chip open";
    return false;
  }
#ifdef AEGISROM_MOCK_HAL
  out = mockImage_;
  if (progressCb_) progressCb_(ProgressStage::Read, chipSize_, chipSize_);
  return true;
#else
  out.assign(chipSize_, 0);
  auto* ctx = static_cast<struct flashrom_flashctx*>(ctx_);
  if (flashrom_image_read(ctx, out.data(), out.size()) != 0) {
    lastError_ = "flashrom_image_read failed";
    return false;
  }
  return true;
#endif
}

bool FlashromHAL::erase() {
  if (!isOpen_) {
    lastError_ = "erase: no chip open";
    return false;
  }
#ifdef AEGISROM_MOCK_HAL
  std::fill(mockImage_.begin(), mockImage_.end(), 0xFF);
  if (progressCb_) progressCb_(ProgressStage::Erase, chipSize_, chipSize_);
  return true;
#else
  auto* ctx = static_cast<struct flashrom_flashctx*>(ctx_);
  if (flashrom_flash_erase(ctx) != 0) {
    lastError_ = "flashrom_flash_erase failed";
    return false;
  }
  return true;
#endif
}

bool FlashromHAL::write(std::vector<std::uint8_t> image) {
  if (!isOpen_) {
    lastError_ = "write: no chip open";
    return false;
  }
  if (image.size() != chipSize_) {
    lastError_ = "write: image size " + std::to_string(image.size()) +
                 " != chip size " + std::to_string(chipSize_);
    return false;
  }
#ifdef AEGISROM_MOCK_HAL
  mockImage_ = std::move(image);
  if (progressCb_) progressCb_(ProgressStage::Write, chipSize_, chipSize_);
  return true;
#else
  auto* ctx = static_cast<struct flashrom_flashctx*>(ctx_);
  int rc = flashrom_image_write(ctx, image.data(), image.size(), nullptr);
  if (rc != 0) {
    lastError_ = "flashrom_image_write failed (rc=" + std::to_string(rc) + ")";
    return false;
  }
  return true;
#endif
}

bool FlashromHAL::verify(const std::vector<std::uint8_t>& image) {
  if (!isOpen_) {
    lastError_ = "verify: no chip open";
    return false;
  }
  if (image.size() != chipSize_) {
    lastError_ = "verify: image size mismatch";
    return false;
  }
#ifdef AEGISROM_MOCK_HAL
  return mockImage_ == image;
#else
  auto* ctx = static_cast<struct flashrom_flashctx*>(ctx_);
  return flashrom_image_verify(ctx, image.data(), image.size()) == 0;
#endif
}

}  // namespace aegis
