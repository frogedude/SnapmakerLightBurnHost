// connection_setup.cpp - Connection setup UI flow
#include "host.h"

bool ConnectionSetup(const std::string& preIp, bool isRenewal)
{
    try {
        auto promptForKasaIp = [&]() -> std::string {
            UserConfig cfg = HostContext::instance().snapshotConfig();
            std::string existing = cfg.kasa.ipAddress;
            bool valid = !existing.empty() && IsValidIPAddress(existing);

            if (existing.empty()) {
                std::cout << "Enter Kasa smart plug IP address (e.g., 192.168.1.200): ";
            }
            else if (valid) {
                std::cout << "Enter Kasa smart plug IP address (Press ENTER to use existing IP, " << existing << "): ";
            }
            else {
                std::cout << "Stored Kasa IP '" << existing << "' is INVALID." << std::endl;
                std::cout << "Enter a valid IP address (e.g., 192.168.1.200): ";
            }

            std::string ip;
            std::getline(std::cin, ip);
            ip.erase(std::remove_if(ip.begin(), ip.end(),
                [](unsigned char c) { return std::isspace(c); }),
                ip.end());

            if (ip.empty() && !existing.empty() && valid) {
                return existing;
            }
            return ip;
            };

        auto acquireToken = [&](const std::string& machineIp, bool isRenewalOnly) -> bool {
            std::cout << std::endl;
            if (isRenewalOnly) {
                std::cout << GetTimeStamp() << "Waiting for any existing connection to clear. If a prompt appears, do not click 'Yes' at this time." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(11));
            }
            std::cout << GetTimeStamp() << "Machine is online. Requesting connection..." << std::endl;
            std::cout << GetTimeStamp() << "[ACTION REQUIRED] Please tap 'Yes' on the Snapmaker touchscreen." << std::endl;
            std::cout << GetTimeStamp() << "Waiting for confirmation (timeout: " << CONFIRMATION_TIMEOUT_SECONDS << " seconds)..." << std::endl;

            std::string connectUrl = "http://" + machineIp + ":8080/api/v1/connect";
            std::string response;
            auto [res, code] = PerformHttpPostWithCode(connectUrl, "", response, 5);

            if (res != CURLE_OK || code != 200 || response.empty()) {
                std::cout << GetTimeStamp() << "[ERR] Failed to initiate connection." << std::endl;
                return false;
            }

            std::string tempToken;
            json connectData;
            try {
                connectData = json::parse(response);
                if (connectData.contains("token") && connectData["token"].is_string() && !connectData["token"].empty()) {
                    tempToken = connectData["token"];
                    std::cout << GetTimeStamp() << "Connection initiated. Token received." << std::endl;
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] No token in response." << std::endl;
                    return false;
                }
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[ERR] Failed to parse response: " << e.what() << std::endl;
                return false;
            }

            std::cout << GetTimeStamp() << "Polling for confirmation (timeout: " << CONFIRMATION_TIMEOUT_SECONDS << " seconds)..." << std::endl;

            bool confirmed = false;
            auto startTime = std::chrono::steady_clock::now();
            int spinnerCount = 0;
            const char spinner[] = { '|', '/', '-', '\\' };

            while (!confirmed && IsRunning()) {
                auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
                if (elapsedSeconds >= CONFIRMATION_TIMEOUT_SECONDS) {
                    std::cout << "\n" << GetTimeStamp() << "[TIMEOUT] No confirmation received within " << CONFIRMATION_TIMEOUT_SECONDS << " seconds." << std::endl;
                    std::cout << GetTimeStamp() << "[INFO] Make sure you tap 'Yes' on the Snapmaker touchscreen." << std::endl;
                    break;
                }

                std::cout << "\r" << GetTimeStamp() << "Waiting for confirmation... Click 'Yes' on the touch controller. "
                    << spinner[spinnerCount % 4] << " (" << elapsedSeconds << "/" << CONFIRMATION_TIMEOUT_SECONDS << "s)   (ESC to cancel)   " << std::flush;
                spinnerCount++;

                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == ESC_ASCII) {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Cancelled by user." << std::endl;
                        return false;
                    }
                }

                std::string statusUrl = BuildStatusUrl(machineIp, tempToken);
                std::string statusResponse;
                auto [statusRes, statusCode] = PerformHttpGetWithCode(statusUrl, statusResponse, 3);

                if (statusRes == CURLE_OK && statusCode == 200) {
                    confirmed = true;
                    std::cout << "\n" << GetTimeStamp() << "[OK] Connection confirmed!" << std::endl;
                    break;
                }

                for (int i = 0; i < DEFAULT_POLL_INTERVAL_SECONDS * 10; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (_kbhit() && _getch() == ESC_ASCII) {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Cancelled by user." << std::endl;
                        return false;
                    }
                    if (!IsRunning()) {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Shutdown requested." << std::endl;
                        return false;
                    }
                }
            }

            if (!confirmed) return false;

            std::cout << GetTimeStamp() << "[OK] Successfully connected with verified token!" << std::endl;

            UserConfig newConfig;
            if (!isRenewalOnly) {
                UserConfig current = HostContext::instance().snapshotConfig();
                newConfig = current;

                newConfig.ipAddress = machineIp;
                newConfig.token = tempToken;

                bool modelDetected = false;
                if (connectData.contains("series") && connectData["series"].is_string()) {
                    std::string series = connectData["series"];
                    if (series.find("A350") != std::string::npos) {
                        newConfig.model = MODEL_A350;
                        modelDetected = true;
                        std::cout << GetTimeStamp() << "[OK] Auto-detected model: Snapmaker 2.0 A350" << std::endl;
                    }
                    else if (series.find("A250") != std::string::npos) {
                        newConfig.model = MODEL_A250;
                        modelDetected = true;
                        std::cout << GetTimeStamp() << "[OK] Auto-detected model: Snapmaker 2.0 A250" << std::endl;
                    }
                    else {
                        std::cout << GetTimeStamp() << "[WARN] Unknown model series: " << series << std::endl;
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "[WARN] Could not auto-detect model from connect response." << std::endl;
                }

                if (!modelDetected) {
                    std::cout << "\nSelect Snapmaker 2.0 Model:" << std::endl;
                    std::cout << "  1. A350 (X: 232.0 Y: 178.0 Z: 290.0)" << std::endl;
                    std::cout << "  2. A250 (X: 160.0 Y: 120.0 Z: 175.0)" << std::endl;
                    std::cout << "  [ENTER] Other/Unknown (no base positions set). Note: Base positions can be updated later." << std::endl;
                    std::cout << "Enter choice (1, 2, or ENTER): " << std::flush;

                    std::string modelChoice;
                    bool validModelChoice = false;
                    while (!validModelChoice && IsRunning()) {
                        int ch = _getch();
                        if (ch == ENTER_ASCII) {
                            modelChoice = "";
                            std::cout << std::endl;
                            validModelChoice = true;
                            break;
                        }
                        else if (ch == '1') {
                            modelChoice = "1";
                            std::cout << "1" << std::endl;
                            validModelChoice = true;
                            break;
                        }
                        else if (ch == '2') {
                            modelChoice = "2";
                            std::cout << "2" << std::endl;
                            validModelChoice = true;
                            break;
                        }
                        else {
                            std::cout << "\nInvalid choice. Enter 1, 2, or ENTER for unknown: " << std::flush;
                            FlushInputBuffer();
                        }
                    }

                    if (modelChoice == "1") {
                        newConfig.model = MODEL_A350;
                        std::cout << GetTimeStamp() << "[INFO] Model set to A350." << std::endl;
                    }
                    else if (modelChoice == "2") {
                        newConfig.model = MODEL_A250;
                        std::cout << GetTimeStamp() << "[INFO] Model set to A250." << std::endl;
                    }
                    else {
                        newConfig.model = MODEL_UNKNOWN;
                        std::cout << GetTimeStamp() << "[INFO] Model set to Other/Unknown. Please set base positions manually." << std::endl;
                    }
                }

                switch (newConfig.model) {
                case MODEL_A350:
                    newConfig.basePositionX = A350_BASE_POS_X;
                    newConfig.basePositionY = A350_BASE_POS_Y;
                    newConfig.basePositionZ = A350_BASE_POS_Z;
                    break;
                case MODEL_A250:
                    newConfig.basePositionX = A250_BASE_POS_X;
                    newConfig.basePositionY = A250_BASE_POS_Y;
                    newConfig.basePositionZ = A250_BASE_POS_Z;
                    break;
                default:
                    break;
                }

                FlushInputBuffer();
                std::cout << "\nWould you like to set up a Kasa smart plug (HS100/HS110/HS300/KP115) for auto power-on? (y/N): ";
                std::string setupKasa;
                std::getline(std::cin, setupKasa);
                if (setupKasa == "y" || setupKasa == "Y") {
                    UserConfig currentCfg = HostContext::instance().snapshotConfig();
                    std::string existingKasa = currentCfg.kasa.ipAddress;
                    bool validKasa = !existingKasa.empty() && IsValidIPAddress(existingKasa);
                    if (validKasa) {
                        std::cout << "Enter Kasa plug IP address (Press ENTER to keep " << existingKasa << "): ";
                    }
                    else if (!existingKasa.empty()) {
                        std::cout << "Stored Kasa IP '" << existingKasa << "' is invalid." << std::endl;
                        std::cout << "Enter a valid IP address: ";
                    }
                    else {
                        std::cout << "Enter Kasa plug IP address: ";
                    }
                    std::string kasaIp;
                    std::getline(std::cin, kasaIp);
                    kasaIp.erase(remove_if(kasaIp.begin(), kasaIp.end(), ::isspace), kasaIp.end());
                    if (kasaIp.empty() && validKasa) kasaIp = existingKasa;
                    if (!kasaIp.empty() && IsValidIPAddress(kasaIp)) {
                        newConfig.kasa.ipAddress = kasaIp;
                        std::cout << "Enable auto power-on when machine is offline? (y/N): ";
                        std::string autoPower;
                        std::getline(std::cin, autoPower);
                        newConfig.kasa.autoPowerOn = (autoPower == "y" || autoPower == "Y");
                        std::cout << "Enable auto power-off when print finishes? (y/N): ";
                        std::string autoPowerOff;
                        std::getline(std::cin, autoPowerOff);
                        newConfig.kasa.autoPowerOff = (autoPowerOff == "y" || autoPowerOff == "Y");
                        std::cout << GetTimeStamp() << "[OK] Kasa smart plug configured." << std::endl;
                    }
                    else if (kasaIp.empty()) {
                        std::cout << GetTimeStamp() << "[INFO] No Kasa IP provided – skipping Kasa setup." << std::endl;
                    }
                    else {
                        std::cout << GetTimeStamp() << "[WARN] Invalid IP address. Kasa smart plug setup skipped." << std::endl;
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "[INFO] Kasa smart plug setup skipped." << std::endl;
                }

                std::cout << "\nDo you want to automatically start printing after file upload? (y/N): ";
                std::string autoStartChoice;
                std::getline(std::cin, autoStartChoice);
                if (autoStartChoice == "y" || autoStartChoice == "Y") {
                    newConfig.autoStartPrint = true;
                    std::cout << GetTimeStamp() << "[OK] Auto-start print ENABLED. Files dragged onto the executable will start printing automatically." << std::endl;
                }
                else {
                    newConfig.autoStartPrint = false;
                    std::cout << GetTimeStamp() << "[INFO] Auto-start print DISABLED (upload-only). Files will be uploaded but not started automatically." << std::endl;
                }

                PrintHeader("", 60, '-');
                std::cout << "\nDo you want to set up an RTSP camera for remote monitoring? (y/N): ";
                std::string setupCamera;
                std::getline(std::cin, setupCamera);
                if (setupCamera == "y" || setupCamera == "Y") {
                    ConfigureRtspCamera();
                    newConfig = HostContext::instance().snapshotConfig();
                }

                PrintHeader("", 60, '-');
                std::cout << "\nWould you like to configure email alerts for print completion, filament runout, and other events? (y/N): ";
                std::string setupEmail;
                std::getline(std::cin, setupEmail);
                if (setupEmail == "y" || setupEmail == "Y") {
                    ConfigureEmailSettings();
                    newConfig = HostContext::instance().snapshotConfig();
                }
                PrintHeader("", 60, '-');
            }
            else {
                UserConfig current = HostContext::instance().snapshotConfig();
                newConfig = current;
                newConfig.ipAddress = machineIp;
                newConfig.token = tempToken;
                std::cout << GetTimeStamp() << "[INFO] Token renewed - existing settings preserved." << std::endl;
            }

            HostContext::instance().updateConfig([newConfig](UserConfig& cfg) {
                cfg = newConfig;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Configuration saved to " << CONFIG_FILE << "." << std::endl;

            return true;
            };

        std::string ip;
        bool success = false;
        bool firstAttempt = true;

        if (!preIp.empty() && IsValidIPAddress(preIp)) {
            ip = preIp;
            std::cout << GetTimeStamp() << "Using existing IP address: " << ip << std::endl;
            firstAttempt = false;
        }
        else {
            std::cout << "No valid token found in machine.json or " << CONFIG_FILE << "." << std::endl;
            std::cout << "Please enter your Snapmaker's IP address to request a new token." << std::endl;
            std::cout << "The IP address should be in the format: 192.168.1.xxx" << std::endl;
            std::cout << "Press ENTER to skip and use a Kasa smart plug to power on the Snapmaker." << std::endl;
            std::cout << std::endl;
        }

        while (!success && IsRunning()) {
            if (firstAttempt) {
                FlushInputBuffer();
                std::cout << "Enter Snapmaker IP address: ";
                std::getline(std::cin, ip);
                ip.erase(std::remove_if(ip.begin(), ip.end(),
                    [](unsigned char c) { return std::isspace(c); }),
                    ip.end());

                if (ip.empty()) {
                    std::cout << GetTimeStamp() << "[INFO] No Snapmaker IP entered. Checking for Kasa smart plug option..." << std::endl;
                    std::cout << "Do you want to power on the Snapmaker using a Kasa smart plug? (y/N): ";
                    char kasaChoice = _getch();
                    std::cout << std::endl;
                    if (tolower(kasaChoice) == 'y') {
                        UserConfig currentCfg = HostContext::instance().snapshotConfig();
                        std::string currentKasaIp = currentCfg.kasa.ipAddress;
                        bool currentValid = !currentKasaIp.empty() && IsValidIPAddress(currentKasaIp);

                        std::string kasaIp = promptForKasaIp();
                        if (kasaIp.empty()) {
                            std::cout << GetTimeStamp() << "[ERR] Kasa IP cannot be empty." << std::endl;
                            continue;
                        }

                        if (IsValidIPAddress(kasaIp)) {
                            bool ipChanged = (kasaIp != currentKasaIp);
                            bool needSave = ipChanged || !currentValid;

                            if (needSave) {
                                HostContext::instance().updateConfig([kasaIp](UserConfig& cfg) {
                                    cfg.kasa.ipAddress = kasaIp;
                                    cfg.kasa.autoPowerOn = true;
                                    return true;
                                    });
                                SaveConfig(HostContext::instance().getConfigPath().string());
                                std::cout << GetTimeStamp() << "[OK] Kasa IP saved." << std::endl;
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Using existing Kasa IP (no config save needed)." << std::endl;
                            }

                            std::cout << GetTimeStamp() << "Attempting to power on Snapmaker using Kasa plug at " << kasaIp << "..." << std::endl;
                            if (AutoPowerOnKasaIfNeeded()) {
                                std::cout << GetTimeStamp() << "[OK] Snapmaker is now online with a valid token." << std::endl;
                                return true;
                            }
                            else {
                                std::cout << GetTimeStamp() << "[ERR] Failed to power on or confirm machine online." << std::endl;
                            }
                            firstAttempt = true;
                            continue;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address. Returning to IP entry." << std::endl;
                            firstAttempt = true;
                            continue;
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[INFO] Kasa power-on declined. Please enter a Snapmaker IP address." << std::endl;
                        firstAttempt = true;
                        continue;
                    }
                }

                if (ip.find('.') == std::string::npos) {
                    std::cout << "[ERR] Invalid IP address format. Must contain dots (e.g., 192.168.1.100)" << std::endl;
                    continue;
                }
                firstAttempt = false;
            }

            if (!IsValidIPAddress(ip)) {
                std::cout << "[ERR] Invalid IP address format. Please use format: 192.168.1.xxx" << std::endl;
                firstAttempt = true;
                continue;
            }

            std::cout << GetTimeStamp() << "Checking if machine is online at " << ip << "... " << std::flush;

            std::string testUrl = BuildConnectUrl(ip);
            std::string testResponse;
            auto [testRes, testCode] = PerformHttpGetWithCode(testUrl, testResponse, 3);

            if (testRes != CURLE_OK) {
                std::cout << "[OFFLINE]" << std::endl;
                if (testRes == CURLE_OPERATION_TIMEDOUT) {
                    std::cout << GetTimeStamp() << "[ERR] Connection timeout. Machine did not respond." << std::endl;
                }
                else if (testRes == CURLE_COULDNT_CONNECT) {
                    std::cout << GetTimeStamp() << "[ERR] Could not connect to " << ip << ":8080" << std::endl;
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] Connection failed: " << curl_easy_strerror(testRes) << std::endl;
                }
                FlushInputBuffer();

                UserConfig cfg = HostContext::instance().snapshotConfig();
                if (cfg.kasa.autoPowerOn && !cfg.kasa.ipAddress.empty() && IsValidIPAddress(cfg.kasa.ipAddress)) {
                    std::cout << GetTimeStamp() << "[INFO] Auto power-on enabled. Attempting to power on Snapmaker via Kasa..." << std::endl;
                    if (AutoPowerOnKasaIfNeeded()) {
                        std::cout << GetTimeStamp() << "[OK] Snapmaker is now online with a valid token." << std::endl;
                        return true;
                    }
                    else {
                        std::cout << GetTimeStamp() << "[WARN] Auto power-on failed. Falling back to manual options." << std::endl;
                    }
                }

                std::string choice;
                bool validChoice = false;

                while (!validChoice && IsRunning()) {
                    std::cout << "\nOptions:" << std::endl;
                    std::cout << "  [ENTER] - Retry with same IP" << std::endl;
                    std::cout << "  [E]     - Enter a different IP address" << std::endl;
                    std::cout << "  [K]     - Use Kasa smart plug to power on Snapmaker (use existing saved IP or enter new) and retry" << std::endl;
                    std::cout << "  [Q]     - Quit setup" << std::endl;
                    std::cout << "Enter choice: " << std::flush;

                    int ch = _getch();

                    if (ch == ENTER_ASCII) {
                        choice = "R";
                        std::cout << std::endl;
                        validChoice = true;
                        break;
                    }
                    if (ch == ESC_ASCII) {
                        choice = "Q";
                        std::cout << std::endl;
                        validChoice = true;
                        break;
                    }

                    char upperCh = toupper(static_cast<char>(ch));
                    if (upperCh == 'E') {
                        choice = "E";
                        std::cout << std::endl;
                        validChoice = true;
                    }
                    else if (upperCh == 'K') {
                        choice = "K";
                        std::cout << std::endl;
                        validChoice = true;
                    }
                    else if (upperCh == 'Q') {
                        choice = "Q";
                        std::cout << std::endl;
                        validChoice = true;
                    }
                    else {
                        std::cout << "\nInvalid choice. Press ENTER to retry, E for new IP, K for Kasa power-on, or Q to quit." << std::endl;
                        FlushInputBuffer();
                    }
                }

                if (choice == "R") {
                    std::cout << GetTimeStamp() << "Retrying connection to " << ip << "..." << std::endl;
                    continue;
                }
                else if (choice == "E") {
                    firstAttempt = true;
                    continue;
                }
                else if (choice == "K") {
                    UserConfig currentCfg = HostContext::instance().snapshotConfig();
                    std::string currentKasaIp = currentCfg.kasa.ipAddress;
                    bool currentValid = !currentKasaIp.empty() && IsValidIPAddress(currentKasaIp);

                    std::string kasaIp = promptForKasaIp();
                    if (kasaIp.empty()) {
                        std::cout << GetTimeStamp() << "[ERR] Kasa IP cannot be empty." << std::endl;
                        continue;
                    }

                    if (IsValidIPAddress(kasaIp)) {
                        bool ipChanged = (kasaIp != currentKasaIp);
                        bool needSave = ipChanged || !currentValid;

                        if (needSave) {
                            HostContext::instance().updateConfig([kasaIp](UserConfig& cfg) {
                                cfg.kasa.ipAddress = kasaIp;
                                cfg.kasa.autoPowerOn = true;
                                return true;
                                });
                            SaveConfig(HostContext::instance().getConfigPath().string());
                            std::cout << GetTimeStamp() << "[OK] Kasa IP saved." << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Using existing Kasa IP (no config save needed)." << std::endl;
                        }

                        std::cout << GetTimeStamp() << "Attempting to power on Snapmaker using Kasa plug at " << kasaIp << "..." << std::endl;
                        if (AutoPowerOnKasaIfNeeded()) {
                            std::cout << GetTimeStamp() << "[OK] Snapmaker is now online with a valid token." << std::endl;
                            return true;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[ERR] Failed to power on or confirm machine online." << std::endl;
                            continue;
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[ERR] Invalid Kasa smart plug IP address." << std::endl;
                        continue;
                    }
                }
                else if (choice == "Q") {
                    std::cout << "[INFO] Setup cancelled." << std::endl;
                    return false;
                }
                continue;
            }

            if (acquireToken(ip, isRenewal)) {
                success = true;
            }
            else {
                FlushInputBuffer();
                std::string choice;
                bool validChoice = false;

                std::cout << "\nOptions:" << std::endl;
                std::cout << "  [ENTER] - Retry connection attempt" << std::endl;
                std::cout << "  [E]     - Enter a different IP address" << std::endl;
                std::cout << "  [Q]     - Quit setup" << std::endl;

                while (!validChoice && IsRunning()) {
                    std::cout << "Enter choice: " << std::flush;
                    int ch = _getch();

                    if (ch == ENTER_ASCII) {
                        choice = "R";
                        std::cout << std::endl;
                        validChoice = true;
                        break;
                    }
                    if (ch == ESC_ASCII) {
                        choice = "Q";
                        std::cout << std::endl;
                        validChoice = true;
                        break;
                    }
                    char upperCh = toupper(static_cast<char>(ch));
                    if (upperCh == 'E') {
                        choice = "E";
                        std::cout << std::endl;
                        validChoice = true;
                    }
                    else if (upperCh == 'Q') {
                        choice = "Q";
                        std::cout << std::endl;
                        validChoice = true;
                    }
                    else {
                        std::cout << "\nInvalid choice. Press ENTER to retry, E for new IP, or Q to quit." << std::endl;
                        FlushInputBuffer();
                    }
                }
                if (choice == "R") {
                    continue;
                }
                else if (choice == "E") {
                    firstAttempt = true;
                    continue;
                }
                else if (choice == "Q") {
                    return false;
                }
            }
        }
        return success;
    }
    catch (const std::bad_alloc& e) {
        std::cout << GetTimeStamp() << "[ERR] Out of memory in connection setup: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cout << GetTimeStamp() << "[ERR] Connection setup error: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cout << GetTimeStamp() << "[ERR] Unknown error in connection setup" << std::endl;
        return false;
    }
}