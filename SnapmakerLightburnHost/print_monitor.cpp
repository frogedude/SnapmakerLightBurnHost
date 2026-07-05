// print_monitor.cpp - Print monitoring functionality
#include "host.h"

void MonitorPrintWithProgress(bool skipInitialCheck) {
// =================================================================
// SINGLE INSTANCE CHECK – MUST BE FIRST (before any console output)
// =================================================================
// Set console title immediately so FindWindow can locate the window
    SetConsoleTitleW(L"Snapmaker Light Burn Host - Monitor");
    
    // mutex
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"SnapmakerLightBurnHost_Monitor_Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cout << GetTimeStamp() << "Monitor is already running.\n" << std::endl;
        CloseHandle(hMutex);
        std::cout << "\n" << "Press any key to exit monitor..." << std::endl;
        (void)_getch();
        return;
    }
    
    // First, check if there's actually a print running
    if (!skipInitialCheck) {
        std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

        MachineStatusData initialStatus = GetMachineStatus(false);
        MachineStatus initialState = initialStatus.getMachineStatus();

        if (initialState != STATUS_RUNNING && initialState != STATUS_PAUSED) {
            std::cout << GetTimeStamp() << "[INFO] No active print job found on the machine." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Please start a print first." << std::endl;
            return;
        }
    }

    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::string printType = "Unknown";
    std::string toolheadInfo = "";

    // Clear screen - this removes any camera start messages
    ClearConsole();
    SetConsoleTitleW(L"Snapmaker LightBurn Host - Monitor");

    bool printActive = true;
    bool isPaused = false;
    bool printCompletedSuccessfully = false;
    bool wasStoppedByUser = false;
    int lastPercent = -1;
    int noProgressCount = 0;
    long long lastKnownElapsedTime = 0;
    std::string lastFileName;
    std::string lastMessage;
    std::chrono::steady_clock::time_point messageClearTime;

    int currentPollInterval = POLL_INTERVAL_PRINTING_SECONDS;
    auto lastPollTime = std::chrono::steady_clock::now();
    auto monitorStartTime = std::chrono::steady_clock::now();

    static bool pollingPaused = false;
    static bool enclosurePresent = false;
    static std::string currentFanState = "?";
    static std::string currentLedState = "?";
    static int fanLevel = 0;
    static int ledLevel = 0;
    static bool levelsInitialized = false;

    // Get initial enclosure status - ONLY ONCE
    if (!levelsInitialized) {
        MachineStatusData initStatus = GetMachineStatus(false);
        enclosurePresent = initStatus.enclosure;

        if (enclosurePresent) {
            std::string enclosureUrl = BuildApiUrl(cfg.ipAddress, "/api/v1/enclosure?token=" + cfg.token);
            std::string enclosureResponse;
            auto [res, code] = PerformHttpGetWithCode(enclosureUrl, enclosureResponse, 5);

            if (res == CURLE_OK && code == 200) {
                try {
                    auto jsonResponse = json::parse(enclosureResponse);
                    if (jsonResponse.contains("fan")) {
                        int fanSpeed = jsonResponse["fan"].get<int>();
                        if (fanSpeed == 0) {
                            currentFanState = "OFF";
                            fanLevel = 0;
                        }
                        else if (fanSpeed == 50) {
                            currentFanState = "HALF";
                            fanLevel = 1;
                        }
                        else if (fanSpeed == 100) {
                            currentFanState = "FULL";
                            fanLevel = 2;
                        }
                        else {
                            currentFanState = std::to_string(fanSpeed) + "%";
                        }
                    }
                    if (jsonResponse.contains("led")) {
                        int ledBrightness = jsonResponse["led"].get<int>();
                        if (ledBrightness == 0) {
                            currentLedState = "OFF";
                            ledLevel = 0;
                        }
                        else if (ledBrightness == 50) {
                            currentLedState = "HALF";
                            ledLevel = 1;
                        }
                        else if (ledBrightness == 100) {
                            currentLedState = "FULL";
                            ledLevel = 2;
                        }
                        else {
                            currentLedState = std::to_string(ledBrightness) + "%";
                        }
                    }
                }
                catch (...) {}
            }
        }
        levelsInitialized = true;
    }

    // Helper to show a temporary message at the bottom
    auto showMessage = [&](const std::string& msg, bool isError = false) {
        lastMessage = msg;
        messageClearTime = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        std::cout << "\x1b[11;1H" << "\x1b[2K" << std::flush;
        if (!msg.empty()) {
            if (isError) {
                std::cout << GetTimeStamp() << "[ERR] " << msg;
            }
            else {
                std::cout << GetTimeStamp() << "[INFO] " << msg;
            }
            std::cout << std::flush;
        }
        };

    // Draw initial static display
    auto drawHeader = [&]() {
        std::cout << "\x1b[1;1H";
        PrintHeader("SNAPMAKER PRINT MONITOR");

        // Row 2: Main controls
        std::cout << "\x1b[2;1H" << "\x1b[2K";
        std::string controls = "Main Controls: [P]ause | [R]esume | [S]top | P[O]lling | E[X]it";
        std::cout << controls << "\n";

        // Row 3: Option controls
        std::cout << "\x1b[3;1H" << "\x1b[2K";
        std::string options = "Option Controls: ";
        if (IsRtspCameraConfigured(cfg.rtspCamera)) {
            options += std::string(HostContext::instance().isCameraRunning() ? "Stop" : "Start") + " [C]amera | ";
        }
        options += "[B]eep [" + std::string(cfg.enableSoundAlerts ? "ON" : "OFF") + "] | ";
        if (cfg.enableEmailAlerts && !cfg.emailRecipient.empty() &&
            !cfg.emailSender.empty() && !cfg.emailAppPassword.empty()) {
            options += "[E]mail [" + std::string(cfg.enableEmailAlerts ? "ON" : "OFF") + "] | ";
        }
        if (!cfg.kasa.ipAddress.empty() && IsValidIPAddress(cfg.kasa.ipAddress)) {
            options += "[K]asa Auto Power-Off [" + std::string(cfg.kasa.autoPowerOff ? "ON" : "OFF") + "]";
        }
        std::cout << options << "\n";

        // Row 4: Enclosure controls (blank line if not present)
        std::cout << "\x1b[4;1H" << "\x1b[2K";
        if (enclosurePresent) {
            std::cout << "Enclosure Controls: [F]an [" << currentFanState << "] | [L]ED [" << currentLedState << "]\n";
        }
        else {
            std::cout << "\n";  // Keep blank line for consistent layout
        }

        // Row 5: Separator
        std::cout << "\x1b[5;1H";
        PrintHeader("");
        std::cout.flush();
        };

    drawHeader();

    // Initialize display area rows 6-10
    for (int row = 6; row <= 10; row++) {
        std::cout << "\x1b[" << row << ";1H" << "\x1b[2K" << std::flush;
    }

    while (printActive && IsRunning() && !g_shutdownRequested) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastPollTime).count();

        if (!lastMessage.empty() && now >= messageClearTime) {
            lastMessage.clear();
            std::cout << "\x1b[11;1H" << "\x1b[2K" << std::flush;
        }

        // Check for keyboard input
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'P' || ch == 'p') {
                if (!isPaused) {
                    auto [success, msg] = PausePrint();
                    showMessage(msg, !success);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    MachineStatusData checkStatus = GetMachineStatus(false);
                    isPaused = (checkStatus.getMachineStatus() == STATUS_PAUSED);
                    lastPollTime = std::chrono::steady_clock::now() - std::chrono::seconds(currentPollInterval + 1);
                }
                else {
                    showMessage("Already paused. Press 'R' to resume.");
                }
            }
            else if (ch == 'R' || ch == 'r') {
                if (isPaused) {
                    auto [success, msg] = ResumePrint();
                    showMessage(msg, !success);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    MachineStatusData checkStatus = GetMachineStatus(false);
                    isPaused = (checkStatus.getMachineStatus() == STATUS_PAUSED);
                    for (int row = 6; row <= 10; row++) {
                        std::cout << "\x1b[" << row << ";1H" << "\x1b[2K" << std::flush;
                    }
                    lastPollTime = std::chrono::steady_clock::now() - std::chrono::seconds(currentPollInterval + 1);
                    noProgressCount = 0;
                }
                else {
                    showMessage("Print is not paused.");
                }
            }
            else if (ch == 'S' || ch == 's') {
                /*
                showMessage("Stopping print...");
                auto [success, msg] = StopPrint();
                showMessage(msg, !success);
                printActive = false;
                break;
                */
                showMessage("Press S again within 3 seconds to confirm stop.", false);
                auto confirmStart = std::chrono::steady_clock::now();
                bool confirmed = false;
                while (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - confirmStart).count() < 3) {
                    if (_kbhit()) {
                        int confirmKey = _getch();
                        if (confirmKey == 'S' || confirmKey == 's') {
                            confirmed = true;
                            break;
                        }
                        // Any other key aborts confirmation
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                if (!confirmed) {
                    showMessage("Stop cancelled.", false);
                    continue;
                }

                wasStoppedByUser = true;
                showMessage("Stopping print...");
                auto [success, msg] = StopPrint();
                showMessage(msg, !success);
                if (success) {
                    // Stop polling
                    printActive = false;

                    // Change the status line (row 8) to STOPPED (do NOT clear rows 6-10)
                    std::cout << "\x1b[8;1H" << "\x1b[2K" << "Status: STOPPED" << std::flush;

                    // Use the message area (row 11) for the exit prompt
                    std::cout << "\x1b[11;1H" << "\x1b[2K"
                        << "Print stopped. Press any key to exit monitor..." << std::flush;

                    (void)_getch();
                    break;
                }
                // If stop fails, continue monitoring (printActive remains true)
            }
            else if (ch == 'O' || ch == 'o') {
                pollingPaused = !pollingPaused;
                showMessage(std::string("Polling ") + (pollingPaused ? "PAUSED" : "RESUMED"));
                if (!pollingPaused) {
                    lastPollTime = std::chrono::steady_clock::now() - std::chrono::seconds(currentPollInterval + 1);
                    for (int row = 6; row <= 10; row++) {
                        std::cout << "\x1b[" << row << ";1H" << "\x1b[2K" << std::flush;
                    }
                }
                continue;
            }
            else if (ch == 'C' || ch == 'c') {
                cfg = HostContext::instance().snapshotConfig();
                if (!IsRtspCameraConfigured(cfg.rtspCamera)) {
                    showMessage("Camera not configured", true);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                if (!CheckVlcInstalled()) {
                    showMessage("VLC not found", true);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                if (HostContext::instance().isCameraRunning()) {
                    HostContext::instance().stopCamera();
                    showMessage("Camera stopped");
                }
                else {
                    if (HostContext::instance().startCamera(cfg.rtspCamera.rtspUrl)) {
                        showMessage("Camera started");
                    }
                    else {
                        showMessage("Failed to start camera", true);
                    }
                }
                drawHeader();
                lastPollTime = std::chrono::steady_clock::now() - std::chrono::seconds(currentPollInterval + 1);
            }
            else if (ch == 'B' || ch == 'b') {
                UserConfig currentCfg = HostContext::instance().snapshotConfig();
                bool newSoundState = !currentCfg.enableSoundAlerts;
                HostContext::instance().updateConfig([newSoundState](UserConfig& c) {
                    c.enableSoundAlerts = newSoundState;
                    return true;
                    });
                SaveConfig(HostContext::instance().getConfigPath().string(), false);
                showMessage(std::string("Beep ") + (newSoundState ? "ENABLED" : "DISABLED"));
                cfg = HostContext::instance().snapshotConfig();
                drawHeader();
                continue;
                }
            else if (ch == 'E' || ch == 'e') {
                    UserConfig currentCfg = HostContext::instance().snapshotConfig();
                    bool newEmailState = !currentCfg.enableEmailAlerts;
                    HostContext::instance().updateConfig([newEmailState](UserConfig& c) {
                        c.enableEmailAlerts = newEmailState;
                        return true;
                        });
                    SaveConfig(HostContext::instance().getConfigPath().string(), false);
                    showMessage(std::string("Email ") + (newEmailState ? "ENABLED" : "DISABLED"));
                    cfg = HostContext::instance().snapshotConfig();
                    drawHeader();
                    continue;
                    }
            else if (ch == 'F' || ch == 'f') {
                if (!enclosurePresent) {
                    showMessage("Enclosure module not detected", true);
                    continue;
                }

                fanLevel = (fanLevel + 1) % 3;

                std::string fanSpeed;
                std::string fanDisplay;
                switch (fanLevel) {
                case 0:
                    fanSpeed = "off";
                    fanDisplay = "OFF";
                    break;
                case 1:
                    fanSpeed = "half";
                    fanDisplay = "HALF";
                    break;
                case 2:
                    fanSpeed = "full";
                    fanDisplay = "FULL";
                    break;
                }

                auto [success, msg] = SetEnclosureFanSpeed(fanSpeed);
                if (success) {
                    currentFanState = fanDisplay;
                    showMessage("Fan: " + fanDisplay);
                    drawHeader();
                }
                else {
                    showMessage(msg, true);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            else if (ch == 'L' || ch == 'l') {
                if (!enclosurePresent) {
                    showMessage("Enclosure module not detected", true);
                    continue;
                }

                ledLevel = (ledLevel + 1) % 3;

                std::string ledBrightness;
                std::string ledDisplay;
                switch (ledLevel) {
                case 0:
                    ledBrightness = "off";
                    ledDisplay = "OFF";
                    break;
                case 1:
                    ledBrightness = "half";
                    ledDisplay = "HALF";
                    break;
                case 2:
                    ledBrightness = "full";
                    ledDisplay = "FULL";
                    break;
                }

                auto [success, msg] = SetEnclosureLed(ledBrightness);
                if (success) {
                    currentLedState = ledDisplay;
                    showMessage("LED: " + ledDisplay);
                    drawHeader();
                }
                else {
                    showMessage(msg, true);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            else if (ch == 'K' || ch == 'k') {
                cfg = HostContext::instance().snapshotConfig();
                if (cfg.kasa.ipAddress.empty() || !IsValidIPAddress(cfg.kasa.ipAddress)) {
                    showMessage("Kasa plug not configured", true);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                bool newValue = !cfg.kasa.autoPowerOff;
                HostContext::instance().updateConfig([newValue](UserConfig& c) {
                    c.kasa.autoPowerOff = newValue;
                    return true;
                    });
                SaveConfig(HostContext::instance().getConfigPath().string(), false);
                showMessage(std::string("Auto power-off ") + (newValue ? "ENABLED" : "DISABLED"));
                cfg = HostContext::instance().snapshotConfig();
                drawHeader();
                continue;
            }
            else if (ch == 'X' || ch == 'x' || ch == ESC_ASCII) {
                printActive = false;
                break;
            }
        }

        // Poll only if not paused
        if (!pollingPaused && elapsed >= currentPollInterval) {
            lastPollTime = now;

            MachineStatusData status = GetMachineStatus(false);

            // ====== FILAMENT RUNOUT NOTIFICATION ======
            static bool lastFilamentOut = false;
            if (!lastFilamentOut && status.isFilamentOut) {
                UserConfig cfg = HostContext::instance().snapshotConfig();

                // Send email notification if enabled and configured
                if (cfg.enableEmailAlerts &&
                    !cfg.emailRecipient.empty() &&
                    !cfg.emailSender.empty() &&
                    !cfg.emailAppPassword.empty()) {
                    SendEmailNotification("Filament Runout",
                        "Filament has run out on your Snapmaker.\n\n"
                        "Please load new filament and resume the print.");
                }

                // Play sound alert if enabled (independent of email)
                if (cfg.enableSoundAlerts) {
                    PlayAlertChime();
                }
            }
            lastFilamentOut = status.isFilamentOut;
            // =========================================

            MachineStatus machineState = status.getMachineStatus();

            enclosurePresent = status.enclosure;

            // Update print type and toolhead info based on tool head
            if (status.isLaser()) {
                printType = "Laser";
                std::stringstream ss;
                ss << "Power: " << status.laserPower << "%";
                if (status.workSpeed > 0) {
                    ss << " | Speed: " << status.workSpeed << " mm/min";
                }
                toolheadInfo = ss.str();
            }
            else if (status.is3DPrinter()) {
                printType = "3DP";
                std::stringstream ss;
                ss << "Nozzle 1: " << status.nozzleTemperature1 << "C/" << status.nozzleTargetTemperature1 << "C";
                if (status.nozzleTemperature2 > 0 || status.nozzleTargetTemperature2 > 0) {
                    ss << " | Nozzle 2: " << status.nozzleTemperature2 << "C/" << status.nozzleTargetTemperature2 << "C";
                }
                if (status.heatedBedTemperature > 0 || status.heatedBedTargetTemperature > 0) {
                    ss << " | Bed: " << status.heatedBedTemperature << "C/" << status.heatedBedTargetTemperature << "C";
                }
                toolheadInfo = ss.str();
            }
            else if (status.isCNC()) {
                printType = "CNC";
                std::stringstream ss;
                ss << "RPM: " << status.spindleSpeed;
                if (status.workSpeed > 0) {
                    ss << " | Feed: " << status.workSpeed << " mm/min";
                }
                toolheadInfo = ss.str();
            }
            else {
                printType = "Unknown";
                toolheadInfo = "";
            }

            bool wasPaused = isPaused;
            isPaused = (machineState == STATUS_PAUSED);

            if (!status.fileName.empty() && status.fileName != lastFileName) {
                lastFileName = status.fileName;
            }

            int percent = -1;
            if (status.progress > 0) {
                percent = static_cast<int>(status.progress * 100);
                noProgressCount = 0;
            }
            else if (status.totalLines > 0 && status.currentLine > 0) {
                percent = (status.currentLine * 100) / status.totalLines;
                noProgressCount = 0;
            }
            else {
                noProgressCount++;
                if (lastPercent >= 0 && !isPaused) percent = lastPercent;
            }

            if (percent > 100) percent = 100;
            if (percent < 0) percent = 0;

            if (status.elapsedTime > 0) {
                lastKnownElapsedTime = static_cast<long long>(status.elapsedTime);
            }

            long long displayElapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - monitorStartTime).count();
            if (lastKnownElapsedTime > 0 && !isPaused) {
                displayElapsedSeconds = lastKnownElapsedTime;
            }

            long long elapsedHours = displayElapsedSeconds / 3600;
            long long elapsedMins = (displayElapsedSeconds % 3600) / 60;
            long long elapsedSecs = displayElapsedSeconds % 60;

            std::string elapsedStr;
            if (elapsedHours > 0) elapsedStr = std::to_string(elapsedHours) + "h ";
            if (elapsedMins > 0 || elapsedHours > 0) elapsedStr += std::to_string(elapsedMins) + "m ";
            elapsedStr += std::to_string(elapsedSecs) + "s";

            std::string eta = "";
            if (status.remainingTime > 0 && !isPaused) {
                auto now_time = std::chrono::system_clock::now();
                auto completion = now_time + std::chrono::seconds(static_cast<long long>(status.remainingTime));
                auto completion_time = std::chrono::system_clock::to_time_t(completion);
                tm tmBuf{};
                localtime_s(&tmBuf, &completion_time);
                time_t now_t = std::chrono::system_clock::to_time_t(now_time);
                tm nowTm{};
                localtime_s(&nowTm, &now_t);
                std::stringstream ss;
                char buffer[32];
                bool isDifferentDay = (tmBuf.tm_year != nowTm.tm_year ||
                    tmBuf.tm_mon != nowTm.tm_mon ||
                    tmBuf.tm_mday != nowTm.tm_mday);
                if (isDifferentDay) {
                    strftime(buffer, sizeof(buffer), "%b %d, %I:%M %p", &tmBuf);
                    eta = " | ETA: " + std::string(buffer);
                }
                else {
                    strftime(buffer, sizeof(buffer), "%I:%M %p", &tmBuf);
                    eta = " | ETA: " + std::string(buffer);
                }
            }

            std::string remainingWithETA = "";
            if (status.remainingTime > 0 && !isPaused) {
                long long remSeconds = static_cast<long long>(status.remainingTime);
                long long remHours = remSeconds / 3600;
                long long remMinutes = (remSeconds % 3600) / 60;
                long long remSecs = remSeconds % 60;
                std::string remainingStr;
                if (remHours > 0) remainingStr = std::to_string(remHours) + "h ";
                if (remMinutes > 0 || remHours > 0) remainingStr += std::to_string(remMinutes) + "m ";
                remainingStr += std::to_string(remSecs) + "s";
                remainingWithETA = remainingStr + eta;
            }
            else if (percent >= 0 && !isPaused) {
                remainingWithETA = "Calculating... (" + std::to_string(percent) + "% complete)";
            }

            // Check for print completion
            if (machineState != STATUS_RUNNING && machineState != STATUS_PAUSED &&
                (wasPaused || lastPercent > 0 || lastKnownElapsedTime > 0)) {

                // Update display one final time to show completion
                std::cout << "\x1b[6;1H" << "\x1b[2K" << "Type: " << printType << std::flush;
                std::cout << "\x1b[7;1H" << "\x1b[2K" << "File: " << lastFileName << std::flush;

                // Determine if print completed naturally (progress near 100%)
                bool naturalCompletion = false;
                if (lastPercent >= 99 || (status.totalLines > 0 && status.currentLine >= status.totalLines - 1)) {
                    naturalCompletion = true;
                    printCompletedSuccessfully = true;
                }

                // Check stopped FIRST - it overrides everything else
                if (wasStoppedByUser) {
                    std::cout << "\x1b[8;1H" << "\x1b[2K" << "Status: STOPPED" << std::flush;
                }
                else if (naturalCompletion) {
                    std::cout << "\x1b[8;1H" << "\x1b[2K" << "Status: COMPLETE [========================================] 100%" << std::flush;
                }
                else {
                    std::cout << "\x1b[8;1H" << "\x1b[2K" << "Status: ENDED" << std::flush;
                }

                std::cout << "\x1b[9;1H" << "\x1b[2K" << "Remaining: 0s | Elapsed: " << elapsedStr << std::flush;

                // Brief pause to show final status
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                printActive = false;

                // Play beep notification (only for natural completion)
                if (naturalCompletion && !wasStoppedByUser) {
                    PlayCompletionBeep();
                }

                // Show completion message
                std::cout << "\x1b[11;1H" << "\x1b[2K" << std::flush;
                std::cout << "\n\n";

                if (wasStoppedByUser) {  // Check this FIRST
                    PrintHeader("PRINT STOPPED", 50);
                    std::cout << "Print was stopped by user.\n";
                }
                else if (naturalCompletion) {
                    PrintHeader("PRINT COMPLETE", 50);
                    std::cout << "Print finished successfully!\n";
                    std::cout << "Please check your machine.\n";
                }
                else {
                    PrintHeader("PRINT ENDED", 50);
                    std::cout << "Print has ended.\n";
                }
                PrintHeader("", 50);
                std::cout << std::endl;

                // Send email notification ONLY for successful completion (not stopped)
                if (naturalCompletion && !wasStoppedByUser) {
                    cfg = HostContext::instance().snapshotConfig();
                    if (cfg.enableEmailAlerts && !cfg.emailRecipient.empty() &&
                        !cfg.emailSender.empty() && !cfg.emailAppPassword.empty()) {
                        std::string subject = "Snapmaker Print Complete";
                        std::stringstream body;
                        body << "Print job completed successfully!\n\n";
                        body << "File: " << lastFileName << "\n";
                        body << "Total time: " << elapsedStr << "\n\n";
                        body << "Snapmaker LightBurn Host";
                        SendEmailNotification(subject, body.str());
                        std::cout << std::endl;
                    }
                }

                // ========== AUTO POWER-OFF (BEFORE user prompt) ==========
                cfg = HostContext::instance().snapshotConfig();
                bool autoPowerOffEnabled = cfg.kasa.autoPowerOff;
                if (autoPowerOffEnabled && !wasStoppedByUser && !cfg.kasa.ipAddress.empty() && IsValidIPAddress(cfg.kasa.ipAddress)) {
                    //Allow enough time for machine to finish any processes before turning off
                    std::cout << GetTimeStamp() << "Auto power-off in 30 seconds (press SHIFT to cancel)... ";
                    std::cout.flush();

                    if (!CheckForShiftKey(30)) {
                        std::cout << "\n" << GetTimeStamp() << "Waiting for machine to become idle...\n";

                        auto startWait = std::chrono::steady_clock::now();
                        const int MAX_WAIT_SECONDS = 60;
                        bool cancelled = false;

                        while (!cancelled) {
                            if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
                                cancelled = true;
                                std::cout << GetTimeStamp() << "[INFO] Auto power-off cancelled by Shift key.\n";
                                break;
                            }

                            MachineStatusData idleStatus = GetMachineStatus(false);
                            if (!idleStatus.isBusy()) {
                                std::cout << GetTimeStamp() << "Machine is idle. Powering off Kasa plug...\n";
                                if (SetKasaPlugState(cfg.kasa.ipAddress, false)) {
                                    std::cout << GetTimeStamp() << "[OK] Kasa plug turned OFF.\n";
                                }
                                else {
                                    std::cout << GetTimeStamp() << "[ERR] Failed to turn off Kasa plug.\n";
                                }
                                break;
                            }

                            auto elapsedWait = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - startWait).count();
                            if (elapsedWait >= MAX_WAIT_SECONDS) {
                                std::cout << GetTimeStamp() << "[WARN] Timeout waiting for idle. Skipping auto power-off.\n";
                                break;
                            }

                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            std::cout << "." << std::flush;
                        }
                    }
                    else {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Auto power-off cancelled by Shift key.\n";
                    }
                }

                // Wait for user input AFTER auto power-off
                std::cout << "\n" << "Press any key to exit monitor..." << std::endl;
                (void)_getch();

                if (HostContext::instance().isCameraRunning()) {
                    HostContext::instance().stopCamera();
                }
                HostContext::instance().requestShutdown();
                break;
            }

            // Update display area (Rows 6-10)
            // Row 6: Type with toolhead info
            std::cout << "\x1b[6;1H" << "\x1b[2K";
            if (!toolheadInfo.empty()) {
                std::cout << "Type: " << printType << " | " << toolheadInfo;
            }
            else {
                std::cout << "Type: " << printType;
            }

            // Row 7: File name
            std::cout << "\x1b[7;1H" << "\x1b[2K";
            std::string shortName = lastFileName;
            if (shortName.empty() && !status.fileName.empty()) {
                shortName = status.fileName;
                lastFileName = shortName;
            }
            if (!shortName.empty()) {
                if (shortName.length() > 60) {
                    shortName = shortName.substr(0, 57) + "...";
                }
                std::cout << "File: " << shortName;
            }

            // Row 8: Status and Progress bar on same line
            std::cout << "\x1b[8;1H" << "\x1b[2K";
            if (isPaused) {
                std::cout << "Status: PAUSED";
                if (status.isEnclosureDoorOpen) std::cout << " | DOOR OPEN";
                if (status.isFilamentOut) std::cout << " | FILAMENT OUT";
                if (pollingPaused) std::cout << " | POLL PAUSED";
            }
            else {
                std::cout << "Status: RUNNING";
                if (pollingPaused) std::cout << " | POLL PAUSED";
            }

            // Add progress bar on same line
            if (percent >= 0) {
                int barWidth = 30;
                int pos = (barWidth * percent) / 100;
                std::cout << " [";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos) std::cout << "=";
                    else if (i == pos && percent < 100) std::cout << ">";
                    else std::cout << " ";
                }
                std::cout << "] " << percent << "%";
                lastPercent = percent;
            }

            // Row 9: Remaining and Elapsed on same line
            std::cout << "\x1b[9;1H" << "\x1b[2K";
            if (!remainingWithETA.empty()) {
                std::cout << "Remaining: " << remainingWithETA;
            }
            else if (!isPaused) {
                std::cout << "Remaining: Calculating...";
            }
            else {
                std::cout << "Remaining: --";
            }
            std::cout << " | Elapsed: " << elapsedStr;

            // Row 10: Separator
            std::cout << "\x1b[10;1H";
            PrintHeader("");

            std::cout.flush();

            if (machineState == STATUS_RUNNING) {
                currentPollInterval = POLL_INTERVAL_PRINTING_SECONDS;
            }
            else if (machineState == STATUS_PAUSED) {
                currentPollInterval = POLL_INTERVAL_PAUSED_SECONDS;
            }
        }

        if (pollingPaused) {
            std::cout << "\x1b[8;1H" << "\x1b[2K";
            if (isPaused) {
                std::cout << "Status: PAUSED | POLL PAUSED";
            }
            else {
                std::cout << "Status: RUNNING | POLL PAUSED";
            }
            std::cout.flush();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}