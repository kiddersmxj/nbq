#include "../inc/config.hpp"

int InitConfig() {
    if (!k::config::Config::getInstance().load(ConfigFilePath)) {
        std::cerr << "Failed to load config file: " << ConfigFilePath << std::endl;
        return 1;
    }

    KCONFIG_VAR_REQUIRED(DataDir, "data.dir")

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
