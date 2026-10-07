#pragma once
// HashUtils — CRC32 (IEEE) + SHA256 used for write verification guardrails.
// Dependency-free so both aegisrom-cli and the Qt GUI can share them.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aegis::hash {

[[nodiscard]] std::uint32_t crc32(const std::uint8_t* data, std::size_t len);
[[nodiscard]] inline std::uint32_t crc32(const std::vector<std::uint8_t>& v) {
  return crc32(v.data(), v.size());
}

[[nodiscard]] std::vector<std::uint8_t> sha256(const std::uint8_t* data, std::size_t len);
[[nodiscard]] inline std::vector<std::uint8_t> sha256(const std::vector<std::uint8_t>& v) {
  return sha256(v.data(), v.size());
}

[[nodiscard]] std::string toHex(const std::vector<std::uint8_t>& digest);

}  // namespace aegis::hash
