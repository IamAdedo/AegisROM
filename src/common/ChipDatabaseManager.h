#pragma once
// ChipDatabaseManager — unified chip database (Phase 2 target).
//
// Ports IMSProg's chip database role: IMSProg ships `database/IMSProg.Dat`
// plus the `IMSProg_editor` (ezp_chip_editor) XML-style records. This manager
// exposes vendor/name/size/voltage/package/protocol lookups backed by a plain
// `vendor|name|sizeKb|voltage|package|protocol` text table, so Phase 2 can
// import IMSProg.Dat / chips.xml content without changing callers.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aegis {

struct ChipInfo {
  std::string vendor;
  std::string name;
  std::uint32_t sizeKb = 0;
  std::string voltage;   // e.g. "3.3V", "1.8V"
  std::string package;   // e.g. "SOIC8", "DIP8", "SOIC16"
  std::string protocol;  // e.g. "SPI", "I2C", "Microwire"
};

class ChipDatabaseManager {
 public:
  bool load(const std::string& path, std::string* error = nullptr);
  [[nodiscard]] std::size_t size() const { return chips_.size(); }

  [[nodiscard]] std::optional<ChipInfo> lookup(const std::string& name) const;
  [[nodiscard]] std::vector<ChipInfo> findByVendor(const std::string& vendor) const;
  [[nodiscard]] std::vector<ChipInfo> findBySizeKb(std::uint32_t sizeKb) const;

 private:
  std::vector<ChipInfo> chips_;
};

}  // namespace aegis
