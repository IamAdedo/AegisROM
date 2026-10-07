#pragma once
// AegisAgent — modular autonomous flashing core (Phase 3).
//
// Rule-based heuristics run offline by default. An LLM backend (local
// Ollama / llama.cpp HTTP endpoint, or a cloud API) can be attached via
// setLlmEndpoint() to re-rank decisions; hardware actions always go through
// the guarded tool methods below, never straight from model output.
//
// Safety: executeFlashSequence() refuses to erase or write until a backup
// has been read and stored, and every write ends with a whole-chip verify
// plus CRC32/SHA256 comparison against the source image.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace aegis {

class FlashromHAL;

enum class PatchType {
  None,
  CleanIntelMe,    // neutralize Intel ME region bytes in the image buffer
  ResetNvram,      // zero NVRAM/password region placeholder
  FixChecksum,     // recompute trailing checksum byte placeholder
};

struct AgentConfig {
  std::string programmer = "ch341a_spi";
  std::string programmerParams;
  std::string chipName;  // empty => probe all, must resolve to exactly one
  std::string targetType;             // e.g. "laptop_bios"
  std::vector<std::uint8_t> image;    // firmware image to flash (may be empty for read-only runs)
  bool allowWrite = true;
  std::string llmEndpoint;  // e.g. "http://localhost:11434"; empty => heuristics only
};

using DecisionLog = std::function<void(const std::string& step)>;

class AegisAgent {
 public:
  explicit AegisAgent(FlashromHAL& hal);

  void setDecisionLog(DecisionLog log) { log_ = std::move(log); }
  void setLlmEndpoint(const std::string& url) { llmEndpoint_ = url; }

  // --- Tool calls (safe hardware actions) ---
  [[nodiscard]] std::vector<std::string> probeHardware();
  // Reads the first `len` bytes for SPI/IFD/JEDEC header analysis.
  [[nodiscard]] std::optional<std::vector<std::uint8_t>> readChipHeader(
      std::size_t len = 4096);
  // Returns a warning string when voltage looks unsafe, else empty.
  [[nodiscard]] std::string verifyVoltage(const std::vector<std::uint8_t>& header);
  bool patchImage(PatchType type, std::vector<std::uint8_t>& image);
  // Forced backup -> erase -> write -> verify loop.
  bool executeFlashSequence(const AgentConfig& config,
                            const std::vector<std::uint8_t>& image);

  // High-level entry points used by `aegisrom-cli --auto-flash` and the GUI.
  bool runAutoFlash(const AgentConfig& config);
  bool runBackupOnly(const AgentConfig& config, std::vector<std::uint8_t>& out);

  [[nodiscard]] const std::string& lastError() const { return lastError_; }
  [[nodiscard]] const std::vector<std::uint8_t>& lastBackup() const { return lastBackup_; }

 private:
  void log(const std::string& step);
  bool ensureOpen(const AgentConfig& config);

  FlashromHAL& hal_;
  DecisionLog log_;
  std::string llmEndpoint_;
  std::string lastError_;
  std::vector<std::uint8_t> lastBackup_;
  bool backupValid_ = false;
};

}  // namespace aegis
