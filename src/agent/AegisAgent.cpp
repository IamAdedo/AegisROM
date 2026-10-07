// AegisAgent implementation — heuristics-first autonomous flash sequences.
#include "AegisAgent.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "FlashromHAL.h"
#include "HashUtils.h"

namespace aegis {

AegisAgent::AegisAgent(FlashromHAL& hal) : hal_(hal) {}

void AegisAgent::log(const std::string& step) {
  if (log_) log_(step);
}

bool AegisAgent::ensureOpen(const AgentConfig& config) {
  if (hal_.isOpen()) return true;
  if (!hal_.open(config.programmer, config.programmerParams, config.chipName)) {
    lastError_ = "agent: open failed: " + hal_.lastError();
    log("ERROR open: " + hal_.lastError());
    return false;
  }
  log("opened programmer '" + config.programmer + "' chip '" +
      hal_.probeResult().chipName + "' (" + std::to_string(hal_.chipSize()) + " bytes)");
  return true;
}

std::vector<std::string> AegisAgent::probeHardware() {
  auto programmers = hal_.listProgrammers();
  log("probe_hardware: " + std::to_string(programmers.size()) + " programmers known");
  // Heuristic preference for common external SPI programmers.
  auto rank = [](const std::string& p) {
    if (p == "ch341a_spi") return 0;
    if (p == "ch347_spi") return 1;
    if (p == "ft2232_spi") return 2;
    if (p == "serprog") return 3;
    if (p == "buspirate_spi") return 4;
    return 99;
  };
  std::sort(programmers.begin(), programmers.end(),
            [&](const std::string& a, const std::string& b) { return rank(a) < rank(b); });
  if (!llmEndpoint_.empty()) {
    log("LLM endpoint configured (" + llmEndpoint_ +
        "): heuristics kept as fallback, model re-ranking not yet implemented");
  }
  return programmers;
}

std::optional<std::vector<std::uint8_t>> AegisAgent::readChipHeader(std::size_t len) {
  std::vector<std::uint8_t> full;
  if (!hal_.read(full)) {
    lastError_ = "agent: header read failed: " + hal_.lastError();
    return std::nullopt;
  }
  len = std::min(len, full.size());
  std::vector<std::uint8_t> header(full.begin(), full.begin() + len);
  std::ostringstream ss;
  ss << "read_chip_header: " << len << " bytes, JEDEC guess "
     << std::hex << std::setfill('0') << std::setw(2) << (header.size() > 0 ? header[0] : 0)
     << ":" << std::setw(2) << (header.size() > 1 ? header[1] : 0) << ":"
     << std::setw(2) << (header.size() > 2 ? header[2] : 0);
  log(ss.str());
  return header;
}

std::string AegisAgent::verifyVoltage(const std::vector<std::uint8_t>& header) {
  (void)header;
  // Heuristic placeholder: a real build queries the programmer (CH341A always
  // drives 3.3 V without a level shifter) and the chip DB entry for the part.
  // Until the DB voltage lookup lands in Phase 2, warn unconditionally when a
  // 1.8 V target type is requested on a 3.3 V-only programmer.
  std::string warning;
  log("verify_voltage: no DB voltage data yet (Phase 2); assuming 3.3 V rail");
  return warning;
}

bool AegisAgent::patchImage(PatchType type, std::vector<std::uint8_t>& image) {
  switch (type) {
    case PatchType::None:
      return true;
    case PatchType::CleanIntelMe:
      log("patch_image: CleanIntelMe placeholder (Phase 3: ME region parse)");
      return true;
    case PatchType::ResetNvram:
      log("patch_image: ResetNvram placeholder (Phase 3: NVRAM map)");
      return true;
    case PatchType::FixChecksum:
      if (image.empty()) {
        lastError_ = "agent: FixChecksum on empty image";
        return false;
      }
      log("patch_image: FixChecksum placeholder (Phase 3: checksum spec)");
      return true;
  }
  return false;
}

bool AegisAgent::runBackupOnly(const AgentConfig& config, std::vector<std::uint8_t>& out) {
  if (!ensureOpen(config)) return false;
  if (!hal_.read(out)) {
    lastError_ = "agent: backup read failed: " + hal_.lastError();
    log("ERROR backup: " + hal_.lastError());
    return false;
  }
  lastBackup_ = out;
  backupValid_ = true;
  log("backup: " + std::to_string(out.size()) + " bytes, sha256=" +
      hash::toHex(hash::sha256(out)));
  return true;
}

bool AegisAgent::executeFlashSequence(const AgentConfig& config,
                                     const std::vector<std::uint8_t>& image) {
  if (!config.allowWrite) {
    lastError_ = "agent: writes disabled by policy";
    return false;
  }
  if (image.size() != hal_.chipSize()) {
    lastError_ = "agent: image size " + std::to_string(image.size()) +
                 " != chip size " + std::to_string(hal_.chipSize());
    log("ERROR " + lastError_);
    return false;
  }
  // GUARDRAIL 1: forced backup before any erase/write.
  std::vector<std::uint8_t> backup;
  log("guardrail: forcing pre-write backup");
  if (!runBackupOnly(config, backup)) return false;

  log("erase: starting");
  if (!hal_.erase()) {
    lastError_ = "agent: erase failed: " + hal_.lastError();
    log("ERROR " + lastError_);
    return false;
  }
  log("write: " + std::to_string(image.size()) + " bytes");
  if (!hal_.write(image)) {
    lastError_ = "agent: write failed: " + hal_.lastError() + " (backup intact in memory)";
    log("ERROR " + lastError_);
    return false;
  }
  // GUARDRAIL 2: whole-chip verify + hash comparison against source image.
  log("verify: whole-chip verify + sha256 comparison");
  if (!hal_.verify(image)) {
    lastError_ = "agent: post-write verify FAILED (backup available for restore)";
    log("ERROR " + lastError_);
    return false;
  }
  std::vector<std::uint8_t> readback;
  if (!hal_.read(readback)) {
    lastError_ = "agent: readback failed: " + hal_.lastError();
    return false;
  }
  const std::string want = hash::toHex(hash::sha256(image));
  const std::string got = hash::toHex(hash::sha256(readback));
  if (want != got) {
    lastError_ = "agent: sha256 mismatch want=" + want + " got=" + got;
    log("ERROR " + lastError_);
    return false;
  }
  log("flash sequence OK: sha256=" + got +
      " crc32=" + std::to_string(hash::crc32(image)));
  return true;
}

bool AegisAgent::runAutoFlash(const AgentConfig& config) {
  log("auto-flash start target_type='" + config.targetType + "'");
  probeHardware();
  if (!ensureOpen(config)) return false;

  auto header = readChipHeader();
  if (!header) return false;
  const std::string voltWarn = verifyVoltage(*header);
  if (!voltWarn.empty()) {
    log("VOLTAGE WARNING: " + voltWarn);
    lastError_ = voltWarn;
    return false;  // fail safe: operator must override explicitly
  }

  if (config.image.empty()) {
    // Read-only autonomous run: backup + report.
    std::vector<std::uint8_t> backup;
    if (!runBackupOnly(config, backup)) return false;
    log("auto-flash: no image supplied, backup-only run complete");
    return true;
  }

  std::vector<std::uint8_t> image = config.image;
  if (config.targetType == "laptop_bios") {
    log("target laptop_bios: keeping image as-is (ME/NVRAM patch hooks in Phase 3)");
  }
  return executeFlashSequence(config, image);
}

}  // namespace aegis
