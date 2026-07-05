// lightburn_utils.cpp - LaunchLightBurn, CheckLightBurnCameraSetting, CheckSoftcamRegistry
#include "host.h"

bool LaunchLightBurn() {
    std::cout << GetTimeStamp() << "Checking registry for LightBurn..." << std::endl;

    RegKey key(HKEY_CLASSES_ROOT, LightBurn_REGISTRY_KEY);
    if (!key.valid()) {
        std::cout << GetTimeStamp() << "[ERR] Failed to launch LightBurn.\n";
        std::cout << GetTimeStamp() << "[INFO] Make sure LightBurn is installed.\n";
        return false;
    }

    std::string command = key.getStringValue();
    if (command.empty()) {
        std::cout << GetTimeStamp() << "[ERR] Failed to launch LightBurn.\n";
        std::cout << GetTimeStamp() << "[INFO] Make sure LightBurn is installed.\n";
        return false;
    }

    size_t exePos = command.find(".exe");
    if (exePos == std::string::npos) {
        std::cout << GetTimeStamp() << "[ERR] Failed to launch LightBurn.\n";
        return false;
    }
    exePos += 4;

    size_t argPos = command.find(' ', exePos);
    std::string exePath = (argPos != std::string::npos) ? command.substr(0, argPos) : command;

    if (!exePath.empty() && exePath.front() == '"') {
        exePath = exePath.substr(1);
    }
    if (!exePath.empty() && exePath.back() == '"') {
        exePath.pop_back();
    }

    if (!fs::exists(exePath)) {
        std::cout << GetTimeStamp() << "[ERR] LightBurn executable not found: " << exePath << std::endl;
        return false;
    }

    HINSTANCE res = ShellExecuteA(NULL, "open", exePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res > 32) {
        std::cout << GetTimeStamp() << "[OK] LightBurn launched." << std::endl;
        return true;
    }

    std::cout << GetTimeStamp() << "[ERR] Failed to launch LightBurn.\n";
    std::cout << GetTimeStamp() << "[INFO] Make sure LightBurn is installed.\n";
    return false;
}

bool CheckLightBurnCameraSetting() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
        fs::path prefs = fs::path(appDataPath) / LightBurn_PREFS_PATH;

        try {
            if (!fs::exists(prefs)) {
                std::cout << GetTimeStamp() << "[WARN] LightBurn prefs.ini not found." << std::endl;
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cout << GetTimeStamp() << "[WARN] Cannot access LightBurn prefs: " << e.what() << std::endl;
            return false;
        }

        std::ifstream f(prefs.string());
        if (!f.is_open()) {
            std::cout << GetTimeStamp() << "[WARN] Could not open LightBurn prefs.ini" << std::endl;
            return false;
        }

        std::string line;
        int lineNumber = 0;
        bool foundSetting = false;
        bool isEnabled = false;

        while (std::getline(f, line)) {
            lineNumber++;
            if (line.find("UseDefaultCameraCapture") != std::string::npos) {
                foundSetting = true;
                if (line.find("true") != std::string::npos) {
                    isEnabled = true;
                }
                else if (line.find("false") != std::string::npos) {
                    isEnabled = false;
                }
                break;
            }
        }

        if (foundSetting && isEnabled) {
            std::cout << GetTimeStamp() << "[OK] LightBurn camera setting correct." << std::endl;
            return true;
        }
        else if (foundSetting && !isEnabled) {
            std::cout << GetTimeStamp() << "[WARN] LightBurn camera setting is incorrect." << std::endl;
            std::cout << GetTimeStamp() << "[WARN] UseDefaultCameraCapture is set to false." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Please enable: Edit -> Settings -> Camera -> Use Default Camera Capture System" << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[WARN] LightBurn camera setting not found in prefs.ini." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Please enable in LightBurn: Edit -> Settings -> Camera -> Use Default Camera Capture System" << std::endl;
        }
        return false;
    }
    std::cout << GetTimeStamp() << "[WARN] Could not access LocalAppData folder." << std::endl;
    return false;
}

bool CheckSoftcamRegistry() {
    RegKey key1(HKEY_CLASSES_ROOT, SOFTCAM_REGISTRY_KEY1);
    if (key1.valid()) {
        return true;
    }
    RegKey key2(HKEY_LOCAL_MACHINE, SOFTCAM_REGISTRY_KEY2, KEY_READ | KEY_WOW64_64KEY);
    if (key2.valid()) {
        return true;
    }
    return false;
}