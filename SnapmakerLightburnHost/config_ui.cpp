// config_ui.cpp - Kasa mode, email config, RTSP config, thickness UI
#include "host.h"

void UpdateBasePosition() {
    ClearAndDrawHeader(true);
    UserConfig cfg = HostContext::instance().snapshotConfig();
    if (!IsRunning()) {
        return;
    }
    std::cout << "\nCurrent base position:" << std::endl;
    std::cout << "  X: " << cfg.basePositionX << std::endl;
    std::cout << "  Y: " << cfg.basePositionY << std::endl;
    std::cout << "  Z: " << cfg.basePositionZ << std::endl;
    std::cout << "\nEnter new coordinates (press Enter to keep current):" << std::endl;
    std::string input;
    double newX = cfg.basePositionX, newY = cfg.basePositionY, newZ = cfg.basePositionZ;
    std::cout << "New X [" << cfg.basePositionX << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) try { newX = std::stod(input); }
    catch (...) { std::cout << "[ERR] Invalid input." << std::endl; }
    if (!IsRunning()) {
        return;
    }
    std::cout << "New Y [" << cfg.basePositionY << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) try { newY = std::stod(input); }
    catch (...) { std::cout << "[ERR] Invalid input." << std::endl; }

    if (!IsRunning()) {
        return;
    }
    std::cout << "New Z [" << cfg.basePositionZ << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) try { newZ = std::stod(input); }
    catch (...) { std::cout << "[ERR] Invalid input." << std::endl; }

    HostContext::instance().updateConfig([newX, newY, newZ](UserConfig& c) {
        c.basePositionX = newX; c.basePositionY = newY; c.basePositionZ = newZ; return true;
        });
    SaveConfig(HostContext::instance().getConfigPath().string());
    ClearAndDrawHeader(true);
}

void ResetBasePositionToDefault() {
    double oldX = 0.0, oldY = 0.0, oldZ = 0.0;
    HostContext::instance().updateConfig([&oldX, &oldY, &oldZ](UserConfig& cfg) {
        oldX = cfg.basePositionX; oldY = cfg.basePositionY; oldZ = cfg.basePositionZ;
        switch (cfg.model) {
        case MODEL_A350:
            cfg.basePositionX = A350_BASE_POS_X; cfg.basePositionY = A350_BASE_POS_Y; cfg.basePositionZ = A350_BASE_POS_Z; break;
        case MODEL_A250:
            cfg.basePositionX = A250_BASE_POS_X; cfg.basePositionY = A250_BASE_POS_Y; cfg.basePositionZ = A250_BASE_POS_Z; break;
        default: break;
        }
        return true;
        });
    SaveConfig(HostContext::instance().getConfigPath().string());
    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::cout << GetTimeStamp() << "[OK] Base position reset to default for " << GetModelName(cfg.model) << std::endl;
    std::cout << GetTimeStamp() << "     Previous: X=" << oldX << " Y=" << oldY << " Z=" << oldZ << std::endl;
    std::cout << GetTimeStamp() << "     New: X=" << cfg.basePositionX << " Y=" << cfg.basePositionY << " Z=" << cfg.basePositionZ << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(STATUS_REFRESH_COOLDOWN_SECONDS));
    ClearAndDrawHeader(true);
}

void KasaInteractiveMode(bool fromShortcut) {
    // RAII guard automatically manages the flag
    KasaModeGuard guard;

    ClearConsole();
    PrintHeader("KASA SMART PLUG CONTROL");
    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::cout << "Controlling TP-Link HS100/HS110/HS300/KP115 plug." << std::endl;
    PrintHeader("");
    std::cout << std::endl;

    auto promptForIp = [&]() -> std::string {
        std::string ip;
        while (true) {
            FlushInputBuffer();
            std::cout << "Enter the IP address of your TP-Link smart plug (e.g., 192.168.1.100): ";
            std::getline(std::cin, ip);
            ip.erase(std::remove_if(ip.begin(), ip.end(),
                [](unsigned char c) { return std::isspace(c); }),
                ip.end());
            if (ip.empty()) {
                std::cout << GetTimeStamp() << "[INFO] No IP entered. Returning to main menu." << std::endl;
                return "";
            }
            if (IsValidIPAddress(ip)) {
                return ip;
            }
            std::cout << GetTimeStamp() << "[ERR] Invalid IP address format. Please try again." << std::endl;
        }
        };

    std::string ip = cfg.kasa.ipAddress;
    if (!ip.empty() && !IsValidIPAddress(ip)) {
        std::cout << GetTimeStamp() << "[WARN] Stored Kasa IP is invalid: " << ip << std::endl;
        ip.clear();
    }
    if (ip.empty()) {
        ip = promptForIp();
        if (ip.empty()) {
            if (!fromShortcut) {
                ClearAndDrawHeader(true);
            }
            return;
        }
        // Preserve both autoPowerOn and autoPowerOff when saving a new IP
        HostContext::instance().updateConfig([ip, &cfg](UserConfig& c) {
            c.kasa.ipAddress = ip;
            c.kasa.autoPowerOn = cfg.kasa.autoPowerOn;
            c.kasa.autoPowerOff = cfg.kasa.autoPowerOff;
            return true;
            });
        SaveConfig(HostContext::instance().getConfigPath().string());
        cfg = HostContext::instance().snapshotConfig();
        std::cout << GetTimeStamp() << "[OK] Kasa IP saved." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    bool autoPowerOn = cfg.kasa.autoPowerOn;
    bool autoPowerOff = cfg.kasa.autoPowerOff;
    bool keep_going = true;

    while (keep_going && IsRunning() && !g_shutdownRequested.load(std::memory_order_acquire)) {
        ClearConsole();
        PrintHeader("KASA SMART PLUG CONTROL");
        if (!IsValidIPAddress(ip)) {
            std::cout << "Plug IP: [INVALID - please use 'C' to change]" << std::endl;
        }
        else {
            std::cout << "Plug IP: " << ip << std::endl;
        }
        std::cout << "Auto power-on when machine offline: " << (autoPowerOn ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "Auto power-off when print finishes: " << (autoPowerOff ? "ENABLED" : "DISABLED") << std::endl;
        PrintHeader("");
        std::cout << std::endl;

        if (IsValidIPAddress(ip)) {
            bool isOn = false;
            if (CheckKasaPlugStatus(ip, isOn)) {
                std::cout << "Current state: " << (isOn ? "ON" : "OFF") << std::endl;
            }
            else {
                std::cout << "[WARN] Could not get plug status. Check IP and " << HS100_EXE_PATH << "." << std::endl;
            }
        }
        else {
            std::cout << "[WARN] Invalid IP address. Use 'C' to change." << std::endl;
        }

        std::cout << "\nCommands:" << std::endl;
        std::cout << "  [O] Turn ON" << std::endl;
        std::cout << "  [F] Turn OFF" << std::endl;
        std::cout << "  [A] Toggle auto power-on" << std::endl;
        std::cout << "  [P] Toggle auto power-off" << std::endl;
        std::cout << "  [C] Change plug IP address" << std::endl;
        std::cout << "  [Enter] Refresh status" << std::endl;
        std::cout << "  [ESC] Return to main menu" << std::endl;
        std::cout << "Enter choice: " << std::flush;

        while (_kbhit()) (void)_getch();
        int ch = _getch();
        std::cout << std::endl;

        switch (ch) {
        case 'O': case 'o':
            if (!IsValidIPAddress(ip)) {
                std::cout << GetTimeStamp() << "[ERR] Invalid IP. Use 'C' to set a valid IP." << std::endl;
            }
            else if (SetKasaPlugState(ip, true)) {
                std::cout << GetTimeStamp() << "[OK] Plug turned ON." << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Failed to turn ON plug." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(KASA_WAIT_FOR_POWER_SWITCH_MILLISECONDS));
            break;
        case 'F': case 'f':
            if (!IsValidIPAddress(ip)) {
                std::cout << GetTimeStamp() << "[ERR] Invalid IP. Use 'C' to set a valid IP." << std::endl;
            }
            else if (SetKasaPlugState(ip, false)) {
                std::cout << GetTimeStamp() << "[OK] Plug turned OFF." << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Failed to turn OFF plug." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(KASA_WAIT_FOR_POWER_SWITCH_MILLISECONDS));
            break;
        case 'A': case 'a':
            autoPowerOn = !autoPowerOn;
            HostContext::instance().updateConfig([autoPowerOn, autoPowerOff, ip](UserConfig& c) {
                c.kasa.autoPowerOn = autoPowerOn;
                c.kasa.autoPowerOff = autoPowerOff;   // preserve autoPowerOff
                c.kasa.ipAddress = ip;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Auto power-on " << (autoPowerOn ? "enabled" : "disabled") << std::endl;
            break;
        case 'P': case 'p':
            autoPowerOff = !autoPowerOff;
            HostContext::instance().updateConfig([autoPowerOn, autoPowerOff, ip](UserConfig& c) {
                c.kasa.autoPowerOn = autoPowerOn;     // preserve autoPowerOn
                c.kasa.autoPowerOff = autoPowerOff;
                c.kasa.ipAddress = ip;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Auto power-off " << (autoPowerOff ? "enabled" : "disabled") << std::endl;
            break;
        case 'C': case 'c': {
            std::string newIp = promptForIp();
            if (!newIp.empty()) {
                ip = newIp;
                HostContext::instance().updateConfig([ip, autoPowerOn, autoPowerOff](UserConfig& c) {
                    c.kasa.ipAddress = ip;
                    c.kasa.autoPowerOn = autoPowerOn;
                    c.kasa.autoPowerOff = autoPowerOff;
                    return true;
                    });
                SaveConfig(HostContext::instance().getConfigPath().string());
                std::cout << GetTimeStamp() << "[OK] Plug IP updated to " << ip << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[INFO] IP unchanged." << std::endl;
            }
            break;
        }
        case '\r':
            break;
        case ESC_ASCII:
            keep_going = false;
            break;
        default:
            std::cout << GetTimeStamp() << "[INFO] Unrecognized command." << std::endl;
            break;
        }

        if (keep_going && ch != ESC_ASCII) {
            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
            (void)_getch();
        }
    }
    if (!fromShortcut) {
        ClearConsole();
        ClearAndDrawHeader(true);
        std::cout << std::flush;
    }
}

void ConfigureRtspCamera() {
    try {
        ClearConsole();
        PrintHeader("RTSP CAMERA SETUP");
        std::cout << "Configure your RTSP camera for remote monitoring.\n\n";

        if (!CheckVlcInstalled()) {
            std::cout << GetTimeStamp() << "[WARN] VLC not found. Please install VLC from https://www.videolan.org/vlc/\n";
            std::cout << GetTimeStamp() << "[INFO] The camera feature will not work until VLC is installed.\n\n";
        }

        UserConfig cfg = HostContext::instance().snapshotConfig();

        std::cout << "Current settings:\n";
        std::cout << "  Auto-launch on print start: " << (cfg.rtspCamera.autoLaunch ? "YES" : "NO") << "\n";
        std::cout << "  RTSP URL: " << (cfg.rtspCamera.rtspUrl.empty() ? "(not set)" : cfg.rtspCamera.rtspUrl) << "\n\n";

        std::cout << "Do you want to modify these settings? (y/N): ";
        int resp = _getch();
        std::cout << std::endl;
        if (resp != 'y' && resp != 'Y') {
            std::cout << "Camera setup unchanged.\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return;
        }

        std::cout << "Auto-launch camera when a print starts? (y/N): ";
        resp = _getch();
        std::cout << std::endl;
        cfg.rtspCamera.autoLaunch = (resp == 'y' || resp == 'Y');

        std::cout << "\nHow would you like to provide the RTSP URL?\n";
        std::cout << "  1. Enter full RTSP URL directly\n";
        std::cout << "  2. Enter individual components (IP, port, path, etc.)\n";
        std::cout << "Enter choice (1 or 2): ";
        int method = _getch();
        std::cout << std::endl;

        std::string fullUrl;

        if (method == '2') {
            std::string ip, path, username, password;
            int port = 554;

            while (true) {
                std::cout << "Enter camera IP address (e.g., 192.168.1.50): ";
                std::getline(std::cin, ip);
                ip.erase(std::remove_if(ip.begin(), ip.end(), ::isspace), ip.end());
                if (ip.empty()) {
                    std::cout << "[ERR] IP address cannot be empty.\n";
                    continue;
                }
                if (!IsValidIPAddress(ip)) {
                    std::cout << "[ERR] Invalid IP address format. Please use format: 192.168.1.xxx\n";
                    continue;
                }
                break;
            }

            std::cout << "Enter port [default 554]: ";
            std::string portStr;
            std::getline(std::cin, portStr);
            if (!portStr.empty()) {
                try { port = std::stoi(portStr); }
                catch (...) {}
            }

            std::cout << "Enter path (e.g., stream1, /stream1, live, /live): ";
            std::getline(std::cin, path);
            if (path.empty()) {
                path = "/stream1";
            }
            else {
                if (path.front() != '/') {
                    path = "/" + path;
                }
            }

            std::cout << "Enter username (optional, press Enter to skip): ";
            std::getline(std::cin, username);

            std::cout << "Enter password (optional, press Enter to skip): ";
            std::getline(std::cin, password);

            fullUrl = "rtsp://";
            if (!username.empty()) {
                fullUrl += username;
                if (!password.empty())
                    fullUrl += ":" + password;
                fullUrl += "@";
            }
            fullUrl += ip + ":" + std::to_string(port) + path;

            std::cout << "Built RTSP URL: " << fullUrl << "\n";
        }
        else {
            std::cout << "Enter full RTSP URL (e.g., rtsp://192.168.1.50:554/stream1):\n";
            std::cout << "If your camera requires a username/password, include it in the URL:\n";
            std::cout << "  rtsp://username:password@ip:port/path\n";
            std::cout << "RTSP URL: ";
            std::getline(std::cin, fullUrl);
            fullUrl.erase(std::remove_if(fullUrl.begin(), fullUrl.end(), ::isspace), fullUrl.end());
        }

        if (!fullUrl.empty()) {
            cfg.rtspCamera.rtspUrl = fullUrl;
        }

        if (cfg.rtspCamera.rtspUrl.empty()) {
            std::cout << "[WARN] RTSP URL not set. Camera feature will not be available.\n";
        }
        else if (!CheckVlcInstalled()) {
            std::cout << "[WARN] VLC not found. Install VLC to use the camera feature.\n";
        }
        else {
            std::cout << "[OK] Camera configuration complete.\n";
        }

        HostContext::instance().updateConfig([&cfg](UserConfig& c) {
            c.rtspCamera = cfg.rtspCamera;
            return true;
            });
        SaveConfig(HostContext::instance().getConfigPath().string());

        std::cout << "\nCamera configuration saved.\n";
        std::cout << "Press any key to return...\n";
        (void)_getch();
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] ConfigureRtspCamera: " << e.what() << std::endl;
        std::cout << "Press any key...\n";
        (void)_getch();
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] ConfigureRtspCamera: unknown exception" << std::endl;
        std::cout << "Press any key...\n";
        (void)_getch();
    }
}