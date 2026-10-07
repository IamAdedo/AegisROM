// aegisrom-cli — Qt-free CLI over FlashromHAL (+ AegisAgent for --auto-flash).
//
// Manual mode:  aegisrom-cli --programmer ch341a_spi --write BIOS.bin
// Agentic mode: aegisrom-cli --auto-flash --target-type laptop_bios [--write BIOS.bin]
//               (without --write, the agent performs a backup-only run)
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "AegisAgent.h"
#include "FlashromHAL.h"
#include "HashUtils.h"

namespace {

struct Options {
  std::string programmer = "ch341a_spi";
  std::string programmerParams;
  std::string chip;
  std::string readPath;
  std::string writePath;
  std::string verifyPath;
  bool erase = false;
  bool listProgrammers = false;
  bool autoFlash = false;
  std::string targetType = "generic_spi_nor";
  bool help = false;
  bool version = false;
};

void printHelp(const char* argv0) {
  std::cout
      << "Usage: " << argv0 << " [options]\n"
      << "\nManual mode:\n"
      << "  -p, --programmer NAME    programmer (default ch341a_spi)\n"
      << "      --params S           programmer parameters\n"
      << "  -c, --chip NAME          chip name (default: probe, must match exactly one)\n"
      << "  -r, --read FILE         read flash to FILE\n"
      << "  -w, --write FILE         write FILE to flash (backup + verify enforced)\n"
      << "  -v, --verify FILE        verify flash against FILE\n"
      << "  -E, --erase              erase chip (backup enforced first)\n"
      << "  -L, --list-programmers   list supported programmers\n"
      << "\nAgentic mode:\n"
      << "      --auto-flash         autonomous probe -> backup -> erase -> write -> verify\n"
      << "      --target-type T      e.g. laptop_bios (default generic_spi_nor)\n"
      << "      --write FILE         image for the agent to flash (omit for backup-only)\n"
      << "\nOther:\n"
      << "  -h, --help               this help\n"
      << "      --version            version info\n";
}

bool readFile(const std::string& path, std::vector<std::uint8_t>& out, std::string& err) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    err = "cannot open input file: " + path;
    return false;
  }
  const auto size = static_cast<std::size_t>(in.tellg());
  in.seekg(0);
  out.resize(size);
  if (size > 0 && !in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size))) {
    err = "failed reading file: " + path;
    return false;
  }
  return true;
}

bool writeFile(const std::string& path, const std::vector<std::uint8_t>& data,
               std::string& err) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    err = "cannot open output file: " + path;
    return false;
  }
  if (!data.empty() &&
      !out.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()))) {
    err = "failed writing file: " + path;
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](std::string& dst) {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        return false;
      }
      dst = argv[++i];
      return true;
    };
    if (a == "-h" || a == "--help") opt.help = true;
    else if (a == "--version") opt.version = true;
    else if (a == "-p" || a == "--programmer") { if (!need(opt.programmer)) return 2; }
    else if (a == "--params") { if (!need(opt.programmerParams)) return 2; }
    else if (a == "-c" || a == "--chip") { if (!need(opt.chip)) return 2; }
    else if (a == "-r" || a == "--read") { if (!need(opt.readPath)) return 2; }
    else if (a == "-w" || a == "--write") { if (!need(opt.writePath)) return 2; }
    else if (a == "-v" || a == "--verify") { if (!need(opt.verifyPath)) return 2; }
    else if (a == "-E" || a == "--erase") opt.erase = true;
    else if (a == "-L" || a == "--list-programmers") opt.listProgrammers = true;
    else if (a == "--auto-flash" || a == "--agent") opt.autoFlash = true;
    else if (a == "--target-type") { if (!need(opt.targetType)) return 2; }
    else {
      std::cerr << "unknown option: " << a << "\n";
      return 2;
    }
  }

  if (opt.help) {
    printHelp(argv[0]);
    return 0;
  }
  if (opt.version) {
    std::cout << "aegisrom-cli 0.1.0 (HAL: " << aegis::FlashromHAL::version() << ")\n";
    return 0;
  }

  aegis::FlashromHAL hal;
  hal.setLogCallback([](int level, const std::string& msg) {
    std::cerr << "[flashrom:" << level << "] " << msg << "\n";
  });
  hal.setProgressCallback([](aegis::ProgressStage stage, std::size_t cur, std::size_t total) {
    const char* name =
        stage == aegis::ProgressStage::Read ? "read" : stage == aegis::ProgressStage::Write ? "write" : "erase";
    std::cerr << "\r" << name << ": " << cur << "/" << total << "  " << std::flush;
    if (cur >= total) std::cerr << "\n";
  });
  if (!hal.init(false)) {
    std::cerr << "init failed: " << hal.lastError() << "\n";
    return 1;
  }

  if (opt.listProgrammers) {
    for (const auto& p : hal.listProgrammers()) std::cout << p << "\n";
    return 0;
  }

  // ------------------------- agentic mode -------------------------
  if (opt.autoFlash) {
    aegis::AgentConfig cfg;
    cfg.programmer = opt.programmer;
    cfg.programmerParams = opt.programmerParams;
    cfg.chipName = opt.chip;
    cfg.targetType = opt.targetType;
    if (!opt.writePath.empty()) {
      std::string err;
      if (!readFile(opt.writePath, cfg.image, err)) {
        std::cerr << err << "\n";
        return 1;
      }
    }
    aegis::AegisAgent agent(hal);
    agent.setDecisionLog([](const std::string& s) { std::cout << "[agent] " << s << "\n"; });
    if (!agent.runAutoFlash(cfg)) {
      std::cerr << "auto-flash failed: " << agent.lastError() << "\n";
      return 1;
    }
    std::cout << "auto-flash complete\n";
    return 0;
  }

  // ------------------------- manual mode -------------------------
  const bool wantRead = !opt.readPath.empty();
  const bool wantWrite = !opt.writePath.empty();
  const bool wantVerify = !opt.verifyPath.empty();
  if (!wantRead && !wantWrite && !wantVerify && !opt.erase) {
    printHelp(argv[0]);
    return 2;
  }
  if (!hal.open(opt.programmer, opt.programmerParams, opt.chip)) {
    std::cerr << "open failed: " << hal.lastError() << "\n";
    return 1;
  }
  std::cout << "chip: " << hal.probeResult().chipName << " (" << hal.chipSize() << " bytes)\n";

  if (wantRead) {
    std::vector<std::uint8_t> data;
    if (!hal.read(data)) {
      std::cerr << "read failed: " << hal.lastError() << "\n";
      return 1;
    }
    std::string err;
    if (!writeFile(opt.readPath, data, err)) {
      std::cerr << err << "\n";
      return 1;
    }
    std::cout << "read " << data.size() << " bytes -> " << opt.readPath
              << " sha256=" << aegis::hash::toHex(aegis::hash::sha256(data)) << "\n";
  }

  if (opt.erase || wantWrite) {
    // Manual-mode guardrail mirrors the agent: back up first.
    std::vector<std::uint8_t> backup;
    if (!hal.read(backup)) {
      std::cerr << "pre-write backup failed: " << hal.lastError() << "\n";
      return 1;
    }
    std::cout << "pre-write backup held in memory (" << backup.size() << " bytes)\n";
    if (opt.erase && !wantWrite) {
      if (!hal.erase()) {
        std::cerr << "erase failed: " << hal.lastError() << "\n";
        return 1;
      }
      std::cout << "erase complete\n";
    }
  }

  if (wantWrite) {
    std::vector<std::uint8_t> image;
    std::string err;
    if (!readFile(opt.writePath, image, err)) {
      std::cerr << err << "\n";
      return 1;
    }
    if (!hal.erase()) {
      std::cerr << "erase failed: " << hal.lastError() << "\n";
      return 1;
    }
    if (!hal.write(image)) {
      std::cerr << "write failed: " << hal.lastError() << "\n";
      return 1;
    }
    if (!hal.verify(image)) {
      std::cerr << "post-write verify FAILED\n";
      return 1;
    }
    std::cout << "write+verify OK sha256="
              << aegis::hash::toHex(aegis::hash::sha256(image)) << "\n";
  }

  if (wantVerify) {
    std::vector<std::uint8_t> image;
    std::string err;
    if (!readFile(opt.verifyPath, image, err)) {
      std::cerr << err << "\n";
      return 1;
    }
    if (hal.verify(image)) {
      std::cout << "verify OK\n";
    } else {
      std::cerr << "verify FAILED\n";
      return 1;
    }
  }
  return 0;
}
