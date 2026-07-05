// kasa_controller.cpp - TP-Link Kasa plug commands, auto power on/off
#include "host.h"

bool ExecuteHs100Command(const std::string& args, std::string& output, int timeoutMs) {
    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(NULL, exePathW, MAX_PATH);
    fs::path exePath(exePathW);
    fs::path hs100Path = exePath.parent_path() / HS100_EXE_PATH;
    std::string cmd = hs100Path.string() + " " + args;

    ProcessHandle proc(cmd, false, true);

    if (!proc.valid()) {
        return false;
    }

    auto startTime = std::chrono::steady_clock::now();
    output.clear();

    while (proc.isRunning()) {
        char buffer[1024];
        DWORD bytesRead = 0;

        if (proc.readPipeOutput(buffer, sizeof(buffer) - 1, bytesRead, 100) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= timeoutMs) {
            proc.terminate();
            return false;
        }
        Sleep(10);
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    while (proc.readPipeOutput(buffer, sizeof(buffer) - 1, bytesRead, 100) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    DWORD exitCode = 0;
    proc.getExitCode(exitCode);
    return exitCode == 0;
}

bool CheckKasaPlugStatus(const std::string& ip, bool& isOn) {
    if (ip.empty() || !IsValidIPAddress(ip)) {
        if (!ip.empty()) std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address: " << ip << std::endl;
        return false;
    }

    std::string args = ip + " info";
    std::string output;
    if (!ExecuteHs100Command(args, output)) return false;

    // Line-by-line search for first ON or OFF
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        std::string upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::toupper);

        // Search for ON
        if (upperLine.find("ON") != std::string::npos) {
            isOn = true;
            return true;
        }

        // Search for OFF
        if (upperLine.find("OFF") != std::string::npos) {
            isOn = false;
            return true;
        }
    }

    std::cout << GetTimeStamp() << "[ERR] Could not determine Kasa plug status" << std::endl;
    return false;
}

bool SetKasaPlugState(const std::string& ip, bool on) {
    if (ip.empty() || !IsValidIPAddress(ip)) {
        if (!ip.empty()) std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address: " << ip << std::endl;
        return false;
    }

    std::string args = ip + " outlet 1 " + (on ? "on" : "off");
    std::string output;
    return ExecuteHs100Command(args, output);
}

bool AutoPowerOnKasaIfNeeded() {
    if (IsKasaModeActive()) {
        return false;
    }

    UserConfig cfg = HostContext::instance().snapshotConfig();
    if (!cfg.kasa.autoPowerOn || cfg.kasa.ipAddress.empty()) return false;
    if (!IsValidIPAddress(cfg.kasa.ipAddress)) {
        std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address: " << cfg.kasa.ipAddress << std::endl;
        return false;
    }
    bool isOn = false;
    if (!CheckKasaPlugStatus(cfg.kasa.ipAddress, isOn)) {
        std::cout << GetTimeStamp() << "[WARN] Could not check plug status." << std::endl;
        return false;
    }
    auto checkMachineVerbose = [&]() -> bool {
        std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
        std::string dummy;
        auto [res, code] = PerformHttpGetWithCode(connectUrl, dummy, 5);
        if (res != CURLE_OK) {
            std::cout << GetTimeStamp() << "[ERR] Machine not reachable." << std::endl;
            return false;
        }
        std::cout << GetTimeStamp() << "[OK] Machine is reachable." << std::endl;
        MachineStatusData status = GetMachineStatus(false);
        if (!IsMachineOffline(status)) {
            std::cout << GetTimeStamp() << "[OK] Machine online with existing token." << std::endl;
            return true;
        }
        return false;
        };
    auto checkMachineSilent = [&]() -> bool {
        std::string connectUrl = BuildConnectUrl(cfg.ipAddress);
        std::string dummy;
        auto [res, code] = PerformHttpGetWithCode(connectUrl, dummy, 5);
        if (res != CURLE_OK) return false;
        MachineStatusData status = GetMachineStatus(false);
        return !IsMachineOffline(status);
        };
    if (!isOn) {
        std::cout << GetTimeStamp() << "[INFO] Auto power-on: Plug off. Turning on." << std::endl;
        if (!SetKasaPlugState(cfg.kasa.ipAddress, true)) {
            std::cout << GetTimeStamp() << "[ERR] Failed to turn on plug." << std::endl;
            return false;
        }
        std::cout << GetTimeStamp() << "[OK] Plug turned on. Waiting up to " << MAX_WAIT_SECONDS << " seconds for boot..." << std::endl;
        int spinIdx = 0;
        const char spinner[] = { '|', '/', '-', '\\' };
        for (int waited = 0; waited < MAX_WAIT_SECONDS; waited += POLL_INTERVAL_SECONDS) {
            for (int i = 0; i < POLL_INTERVAL_SECONDS; ++i) {
                if (!IsRunning()) {
                    std::cout << "\r" << std::string(40, ' ') << "\r";
                    std::cout << GetTimeStamp() << "[INFO] Shutdown requested." << std::endl;
                    return false;
                }
                if (_kbhit() && _getch() == ESC_ASCII) {
                    std::cout << "\r" << std::string(40, ' ') << "\r";
                    std::cout << GetTimeStamp() << "[INFO] Cancelled by user." << std::endl;
                    return false;
                }
                std::cout << "\r" << GetTimeStamp() << "[WAIT] " << spinner[spinIdx] << " Booting... (ESC to cancel)";
                std::cout.flush();
                spinIdx = (spinIdx + 1) % 4;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (checkMachineSilent()) {
                std::cout << "\r" << std::string(80, ' ') << "\r";
                if (checkMachineVerbose()) return true;
                std::cout << GetTimeStamp() << "[ERR] Machine responded but token check failed." << std::endl;
                return false;
            }
        }
        std::cout << "\r" << std::string(80, ' ') << "\r";
        std::cout << GetTimeStamp() << "[WARN] Machine still offline after power-on." << std::endl;
        std::cout << GetTimeStamp() << "[NOTE] The Snapmaker's IP address may have changed if using DHCP." << std::endl;
        std::cout << GetTimeStamp() << "Please check your router or the machine's screen for the current IP." << std::endl;
        return false;
    }
    else {
        std::cout << GetTimeStamp() << "[INFO] Plug already turned on." << std::endl;
        if (checkMachineVerbose()) return true;
        std::cout << GetTimeStamp() << "[WARN] Machine offline despite plug being on." << std::endl;
        std::cout << GetTimeStamp() << "[NOTE] The Snapmaker's IP address may have changed if using DHCP." << std::endl;
        std::cout << GetTimeStamp() << "Please check your router or the machine's screen for the current IP." << std::endl;
        return false;
    }
}