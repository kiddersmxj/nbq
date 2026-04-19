#include "../inc/config.hpp"

#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Project root discovery
// ---------------------------------------------------------------------------

// Walk upward from cwd until a directory named ".nbq" is found.
// Returns the absolute path to that .nbq directory, or empty string.
static std::string findProjectRoot() {
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    if (ec) return {};

    while (true) {
        fs::path candidate = dir / ".nbq";
        if (fs::is_directory(candidate, ec)) {
            return candidate.string();
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break;  // reached filesystem root
        dir = parent;
    }
    return {};
}

// ---------------------------------------------------------------------------
// InitConfig
// ---------------------------------------------------------------------------

int InitConfig() {
    NbqDir = findProjectRoot();
    if (NbqDir.empty()) {
        std::cerr << "error: no .nbq project directory found\n"
                  << "hint:  run 'nbq init' to create a project\n";
        return 1;
    }

    std::string configPath = NbqDir + "/config.ini";
    if (!k::config::Config::getInstance().load(configPath)) {
        std::cerr << "error: cannot load config: " << configPath << "\n";
        return 1;
    }

    KCONFIG_VAR_REQUIRED(PartlistPath, "files.partlist")
    KCONFIG_VAR_REQUIRED(NetlistPath,  "files.netlist")
    KCONFIG_VAR_REQUIRED(McuHalPath,   "files.mcu_hal")

    // Resolve configured paths relative to the .nbq directory.
    auto resolve = [&](std::string& p) {
        if (!p.empty() && fs::path(p).is_relative()) {
            p = (fs::path(NbqDir) / p).lexically_normal().string();
        }
    };
    resolve(PartlistPath);
    resolve(NetlistPath);
    resolve(McuHalPath);

    return 0;
}

void Usage() {
    std::cout << UsageNotes << std::endl;
}

void Usage(std::string Message) {
    std::cout << Message << std::endl;
    std::cout << UsageNotes << std::endl;
}

void PrintVersion() {
    std::cout << ProgramName << ": version " << Version << std::endl;
}
