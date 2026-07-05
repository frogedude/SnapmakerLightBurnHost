// module_utils.cpp - Module type detection, compatibility, etc.
#include "host.h"

ModuleType GetModuleTypeFromToolHead(const std::string& toolHead) {
    try {
        if (toolHead.find("LASER") != std::string::npos) return MODULE_LASER;
        if (toolHead.find("3DP") != std::string::npos) return MODULE_3DP;
        if (toolHead.find("CNC") != std::string::npos) return MODULE_CNC;
        return MODULE_UNKNOWN;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] GetModuleTypeFromToolHead: " << e.what() << std::endl;
        return MODULE_UNKNOWN;
    }
}

bool IsFileCompatibleWithModule(const std::string& filename, ModuleType module) {
    try {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        switch (module) {
        case MODULE_LASER:
            return std::find(SUPPORTED_EXTENSIONS_LASER.begin(), SUPPORTED_EXTENSIONS_LASER.end(), ext) != SUPPORTED_EXTENSIONS_LASER.end();
        case MODULE_3DP:
            return std::find(SUPPORTED_EXTENSIONS_3DP.begin(), SUPPORTED_EXTENSIONS_3DP.end(), ext) != SUPPORTED_EXTENSIONS_3DP.end();
        case MODULE_CNC:
            return std::find(SUPPORTED_EXTENSIONS_CNC.begin(), SUPPORTED_EXTENSIONS_CNC.end(), ext) != SUPPORTED_EXTENSIONS_CNC.end();
        default:
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] IsFileCompatibleWithModule: " << e.what() << std::endl;
        return false;
    }
}

std::string GetExpectedExtensions(ModuleType module) {
    try {
        switch (module) {
        case MODULE_LASER:
            return SUPPORTED_EXTENSIONS_LASER[0];
        case MODULE_3DP:
            return SUPPORTED_EXTENSIONS_3DP[0];
        case MODULE_CNC:
            return SUPPORTED_EXTENSIONS_CNC[0];
        default:
            return "Unknown";
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] GetExpectedExtensions: " << e.what() << std::endl;
        return "Unknown";
    }
}