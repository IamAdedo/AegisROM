// ChipDatabaseManager implementation.
#include "ChipDatabaseManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace aegis {
namespace {

std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  std::size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool ChipDatabaseManager::load(const std::string& path, std::string* error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    if (error) *error = "cannot open chip database: " + path;
    return false;
  }
  std::vector<ChipInfo> chips;
  std::string line;
  std::size_t lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    ChipInfo c;
    std::string sizeTok;
    if (!std::getline(ss, c.vendor, '|') || !std::getline(ss, c.name, '|') ||
        !std::getline(ss, sizeTok, '|') || !std::getline(ss, c.voltage, '|') ||
        !std::getline(ss, c.package, '|') || !std::getline(ss, c.protocol, '|')) {
      if (error) *error = "parse error at " + path + ":" + std::to_string(lineno);
      return false;
    }
    c.vendor = trim(c.vendor);
    c.name = trim(c.name);
    c.voltage = trim(c.voltage);
    c.package = trim(c.package);
    c.protocol = trim(c.protocol);
    try {
      c.sizeKb = static_cast<std::uint32_t>(std::stoul(trim(sizeTok)));
    } catch (...) {
      if (error) *error = "bad size at " + path + ":" + std::to_string(lineno);
      return false;
    }
    chips.push_back(std::move(c));
  }
  chips_ = std::move(chips);
  return true;
}

std::optional<ChipInfo> ChipDatabaseManager::lookup(const std::string& name) const {
  for (const auto& c : chips_) {
    if (iequals(c.name, name)) return c;
  }
  return std::nullopt;
}

std::vector<ChipInfo> ChipDatabaseManager::findByVendor(const std::string& vendor) const {
  std::vector<ChipInfo> out;
  for (const auto& c : chips_) {
    if (iequals(c.vendor, vendor)) out.push_back(c);
  }
  return out;
}

std::vector<ChipInfo> ChipDatabaseManager::findBySizeKb(std::uint32_t sizeKb) const {
  std::vector<ChipInfo> out;
  for (const auto& c : chips_) {
    if (c.sizeKb == sizeKb) out.push_back(c);
  }
  return out;
}

}  // namespace aegis
