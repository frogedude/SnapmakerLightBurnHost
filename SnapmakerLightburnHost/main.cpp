// main.cpp - Entry point, console handler, main event loop
#include "host.h"

/*
SnapmakerLightBurnHost/
|
+-- Header Files/
|   +-- host.h                             # Master header - all declarations, constants, structs, RAII classes
|   +-- resource.h                         # Resource identifiers (icon, etc.)
|   +-- upload_server.h                    # HTTP server header
|   +-- softcam.h                          # Softcam API (third-party)
|   +-- stb_image.h                        # STB image loader (third-party)
|   +-- stb_image_resize.h                 # STB image resizer (third-party)
|
+-- Resource Files/
|   +-- SnapmakerLightBurnHost.rc          # Resource script
|   +-- SnapmakerLightburnHost.ico         # Application icon
|
+-- main.cpp                               # Entry point, console handler, main event loop, keyboard routing
+-- host_context.cpp                       # Singleton - config storage, dropped files, status cache, shutdown
+-- stb_image.cpp                          # Third-party STB image library (JPEG/PNG load/resize)
|
+-- core/
|   +-- config_manager.cpp                 # Load/Save config.json, LoadLuban
|   +-- network_client.cpp                 # CURL HTTP requests, progress callbacks, StatusChecker, StatusCheckerGuard
|   +-- machine_api.cpp                    # Status polling, G-code, homing, setup, thickness, enclosure, print control, RAII classes
|
+-- features/
|   +-- kasa_controller.cpp                # TP-Link Kasa smart plug control, auto power on/off
|   +-- camera_capture.cpp                 # Softcam capture, image download, RTSP camera, OpenLatestImage
|   +-- image_processing.cpp               # Resize, pixel adjustment, test pattern generation
|   +-- gcode_handler.cpp                  # Macros folder, send macros, manual G-code with history recall
|   +-- notification_service.cpp           # Print completion beep, email SMTP
|
+-- ui/
|   +-- console_ui.cpp                     # Clear screen, print headers, main menu display
|   +-- status_page.cpp                    # Live position, homed status, Z-calibration, X/Y calibration, G-code file management, backup restore
|   +-- camera_ui.cpp                      # Auto-refresh mode, camera interactive menu
|   +-- config_ui.cpp                      # Base position update/reset, Kasa interactive mode, RTSP camera config, email config
|   +-- display_about.cpp                  # Display README.txt documentation
|
+-- utils/
    +-- lightburn_utils.cpp                # Launch LightBurn, camera setting check, Softcam registry
    +-- string_utils.cpp                   # Timestamp, wide string conversion, path cleaning, flush input, IP validation
    +-- file_utils.cpp                     # Supported extensions check, macro/firmware detection, command-line file detection
    +-- process_handlers.cpp               # File upload, print preparation, Kasa commands, LightBurn, email, beep, shortcuts
    +-- module_utils.cpp                   # Module type detection, file-module compatibility
    +-- input_utils.cpp                    # CheckForShiftKey, HasProblematicCharacters
    +-- print_monitor.cpp                  # Live print progress monitor with UI
    +-- connection_setup.cpp               # Setup wizard, IP entry, token request (60s), model selection
    +-- shortcuts.cpp                      # Command-line shortcuts parsing and execution
|
+-- server/
    +-- tray.cpp                           # System tray mode, folder monitoring, HTTP server management
    +-- upload_server.cpp                  # HTTP file upload server core (thread pool, request handling)
    +-- upload_server.hpp                  # HTTP server declarations (advanced)
    +-- upload_get.cpp                     # GET request handlers (status, capture, thickness, pages)
    +-- upload_post.cpp                    # POST request handlers (file upload, G-code, printer commands)
    +-- upload_pages.cpp                   # HTML page generators (upload, monitor, config, control, capture, thickness, about, G-code, XYZ calibration)
*/

// Global definition for Kasa mode
std::atomic<bool> g_kasaModeActive{ false };

// Global flag for safe shutdown from console handler
std::atomic<bool> g_shutdownRequested{ false };

BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        HostContext::instance().stopCamera();
        g_shutdownRequested.store(true, std::memory_order_release);
        HostContext::instance().requestShutdown();
        SetKasaModeActive(false);
        HostContext::instance().stopUploadServer();
        return TRUE;
    default:
        return FALSE;
    }
}

int main() {
    try {
        // RAII console guard
        ConsoleGuard consoleGuard;

        // Set up console appearance
        consoleGuard.setTitle("Snapmaker LightBurn Host");
        consoleGuard.clearScreen();
        consoleGuard.setWindowSize(120, 50);

        // Splash screen
        std::cout << "Starting Snapmaker LightBurn Host..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        SetConsoleCtrlHandler(ConsoleHandler, TRUE);

        CurlGlobalInit curlInit;

        // Parse command line for shortcuts BEFORE normal flow
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        ShortcutParams shortcutParams = ParseShortcutWithParams(argc, argv);

        if (shortcutParams.action == ShortcutAction::TRAY) {
            LocalFree(argv);
            return RunTray();
        }

        bool hasAnyArgs = (argc > 1);
        bool hasValidShortcut = (shortcutParams.action != ShortcutAction::NONE);

        // Get the first argument (excluding the exe path) to check if it was intended as a shortcut
        std::string firstArg;
        if (argc > 1) {
            std::wstring warg(argv[1]);
            firstArg = WStringToString(warg);
        }

        // Check if the first argument is a valid file (uses existing utility function)
        bool isAFile = !firstArg.empty() && IsSupportedFile(firstArg);

        // If first argument starts with / or -, it was intended as a shortcut command
        // BUT if it's a valid file, treat it as a file, not a shortcut
        bool intendedAsShortcut = (!firstArg.empty() &&
            (firstArg[0] == '/' || firstArg[0] == '-') &&
            !isAFile);

        if (hasAnyArgs && !hasValidShortcut && intendedAsShortcut) {
            // User tried to use a shortcut but typed it incorrectly
            ClearConsole();
            PrintHeader("INVALID SHORTCUT");
            std::cout << std::endl;
            std::cout << GetTimeStamp() << "[ERR] Invalid shortcut command: " << firstArg << std::endl;
            std::cout << std::endl;

            if (argc > 2) {
                std::cout << "All arguments provided:" << std::endl;
                for (int i = 1; i < argc; ++i) {
                    std::wstring warg(argv[i]);
                    std::string arg = WStringToString(warg);
                    std::cout << "  " << arg << std::endl;
                }
                std::cout << std::endl;
            }

            std::cout << "Use /help or /? to see all available shortcuts." << std::endl;
            std::cout << std::endl;
            std::cout << "Press any key to exit..." << std::endl;
            (void)_getch();
            LocalFree(argv);
            return 1;
        }

        if (shortcutParams.action == ShortcutAction::NONE) {
            CheckCommandLineFiles();
        }
        LocalFree(argv);

        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        fs::path configFile = fs::path(exePath).parent_path() / CONFIG_FILE;
        HostContext::instance().setConfigPath(configFile);

        // If this is a shortcut (not NONE)
        if (shortcutParams.action != ShortcutAction::NONE) {
            // Load config first
            UserConfig tempConfig;
            LoadLuban(tempConfig);

            UserConfig localConfig;
            LoadConfig(configFile.string(), localConfig);

            // Merge configs
            UserConfig mergedConfig = localConfig;
            if (mergedConfig.token.empty() && !tempConfig.token.empty()) {
                mergedConfig.token = tempConfig.token;
            }
            if (mergedConfig.ipAddress.empty() && !tempConfig.ipAddress.empty()) {
                mergedConfig.ipAddress = tempConfig.ipAddress;
            }
            if (mergedConfig.model == MODEL_UNKNOWN && tempConfig.model != MODEL_UNKNOWN) {
                mergedConfig.model = tempConfig.model;
                if (mergedConfig.basePositionX == 0.0) {
                    switch (mergedConfig.model) {
                    case MODEL_A350:
                        mergedConfig.basePositionX = A350_BASE_POS_X;
                        mergedConfig.basePositionY = A350_BASE_POS_Y;
                        mergedConfig.basePositionZ = A350_BASE_POS_Z;
                        break;
                    case MODEL_A250:
                        mergedConfig.basePositionX = A250_BASE_POS_X;
                        mergedConfig.basePositionY = A250_BASE_POS_Y;
                        mergedConfig.basePositionZ = A250_BASE_POS_Z;
                        break;
                    default: break;
                    }
                }
            }

            HostContext::instance().updateConfig([mergedConfig](UserConfig& cfg) {
                cfg = mergedConfig;
                return true;
                });

            // Execute the shortcut
            ExecuteShortcut(shortcutParams);
            return 0;
        }

        // Check for dropped files
        std::vector<std::string> droppedFiles = HostContext::instance().acquireDroppedFiles();
        if (!droppedFiles.empty()) {
            ClearConsole();
            PrintHeader("SNAPMAKER FILE UPLOAD");

            std::cout << GetTimeStamp() << "[INFO] File(s) detected for upload." << std::endl;

            // Load Luban config if available
            UserConfig lubanConfig;
            bool lubanOk = LoadLuban(lubanConfig);

            // Load local config.json (even if token missing, it contains Kasa, autoStartPrint, etc.)
            UserConfig localConfig;
            LoadConfig(configFile.string(), localConfig);

            // Start with local config as base
            UserConfig mergedConfig = localConfig;

            if (lubanOk) {
                if (mergedConfig.token.empty() && !lubanConfig.token.empty()) {
                    mergedConfig.token = lubanConfig.token;
                }
                if (mergedConfig.ipAddress.empty() && !lubanConfig.ipAddress.empty()) {
                    mergedConfig.ipAddress = lubanConfig.ipAddress;
                }
                if (mergedConfig.model == MODEL_UNKNOWN && lubanConfig.model != MODEL_UNKNOWN) {
                    mergedConfig.model = lubanConfig.model;
                    if (mergedConfig.basePositionX == 0.0) {
                        switch (mergedConfig.model) {
                        case MODEL_A350:
                            mergedConfig.basePositionX = A350_BASE_POS_X;
                            mergedConfig.basePositionY = A350_BASE_POS_Y;
                            mergedConfig.basePositionZ = A350_BASE_POS_Z;
                            break;
                        case MODEL_A250:
                            mergedConfig.basePositionX = A250_BASE_POS_X;
                            mergedConfig.basePositionY = A250_BASE_POS_Y;
                            mergedConfig.basePositionZ = A250_BASE_POS_Z;
                            break;
                        default: break;
                        }
                    }
                }
            }
            else if (mergedConfig.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No configuration found. Please run the program normally first to set up connection." << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Press any key to exit..." << std::endl;
                int ch = _getch();
                return 1;
            }

            HostContext::instance().updateConfig([mergedConfig](UserConfig& cfg) {
                cfg = mergedConfig;
                return true;
                });

            SaveConfig(configFile.string());

            std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

            if (droppedFiles.size() == 1) {
                ProcessFileWithMode(droppedFiles[0], FILEOP_SMART, true);
            }
            else {
                std::cout << GetTimeStamp() << "Multiple files detected. Please select one:" << std::endl;
                for (size_t i = 0; i < droppedFiles.size(); i++) {
                    std::cout << "  " << (i + 1) << ". " << fs::path(droppedFiles[i]).filename().string() << " (";
                    try {
                        double sizeKB = static_cast<double>(fs::file_size(droppedFiles[i])) / 1024.0;
                        std::cout << std::fixed << std::setprecision(1) << sizeKB << " KB)";
                    }
                    catch (const std::filesystem::filesystem_error& e) {
                        std::cout << "unknown size)";
                        std::cout << GetTimeStamp() << "[WARN] Cannot read file size: " << e.what() << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cout << "unknown size)";
                        std::cout << GetTimeStamp() << "[WARN] Unexpected error reading file size: " << e.what() << std::endl;
                    }
                    std::cout << std::endl;
                }
                std::cout << "Enter file number (or 0 to cancel): ";

                std::string input;
                std::getline(std::cin, input);
                int choice = 0;
                if (!input.empty()) {
                    try {
                        choice = std::stoi(input);
                    }
                    catch (const std::exception& e) {
                        std::cout << GetTimeStamp() << "Invalid input: " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(ERROR_DISPLAY_SECONDS));
                        choice = 0;
                    }
                }
                size_t index = static_cast<size_t>(choice - 1);
                if (choice > 0 && index < droppedFiles.size()) {
                    ProcessFileWithMode(droppedFiles[index], FILEOP_SMART, true);
                }
                else if (choice != 0) {
                    std::cout << GetTimeStamp() << "Invalid selection: " << choice << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(ERROR_DISPLAY_SECONDS));
                }
            }
            return 0;
        }

        // Normal startup
        UserConfig tempConfig;
        bool lubanLoaded = LoadLuban(tempConfig);

        if (!lubanLoaded) {
            PrintHeader("SNAPMAKER CONNECTION SETUP");
            std::cout << std::endl;
            std::cout << GetTimeStamp() << "Luban config not found or incomplete." << std::endl;

            if (!LoadConfig(configFile.string(), tempConfig)) {
                std::cout << GetTimeStamp() << "No valid configuration found." << std::endl;

                HostContext::instance().updateConfig([&tempConfig](UserConfig& cfg) { cfg = tempConfig; return true; });

                if (tempConfig.ipAddress.empty()) {
                    if (!ConnectionSetup()) {
                        std::cout << GetTimeStamp() << "[ERR] Setup failed. Exiting program." << std::endl;
                        return 1;
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "Found IP but no token. Attempting to get new token..." << std::endl;
                    if (ConnectionSetup(tempConfig.ipAddress, true)) {
                        tempConfig = HostContext::instance().snapshotConfig();
                        if (tempConfig.model == MODEL_UNKNOWN) {
                            SelectSnapmakerModel(tempConfig.model);
                            switch (tempConfig.model) {
                            case MODEL_A350:
                                tempConfig.basePositionX = A350_BASE_POS_X;
                                tempConfig.basePositionY = A350_BASE_POS_Y;
                                tempConfig.basePositionZ = A350_BASE_POS_Z;
                                break;
                            case MODEL_A250:
                                tempConfig.basePositionX = A250_BASE_POS_X;
                                tempConfig.basePositionY = A250_BASE_POS_Y;
                                tempConfig.basePositionZ = A250_BASE_POS_Z;
                                break;
                            }
                            HostContext::instance().updateConfig([tempConfig](UserConfig& cfg) {
                                cfg = tempConfig;
                                return true;
                                });
                            SaveConfig(configFile.string());
                            std::cout << GetTimeStamp() << "[OK] Model selection saved." << std::endl;
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[ERR] Failed to obtain token. Exiting." << std::endl;
                        return 1;
                    }
                }
            }
            else {
                HostContext::instance().updateConfig([tempConfig](UserConfig& cfg) {
                    cfg = tempConfig;
                    return true;
                    });
                std::cout << GetTimeStamp() << "[OK] Using local config." << std::endl;
            }
        }
        else {
            std::cout << GetTimeStamp() << "[OK] Loaded Luban config." << std::endl;

            UserConfig localConfig;
            LoadConfig(configFile.string(), localConfig);

            UserConfig mergedConfig = localConfig;

            if (mergedConfig.token.empty() && !tempConfig.token.empty()) {
                mergedConfig.token = tempConfig.token;
                std::cout << GetTimeStamp() << "[OK] Using token from Luban config." << std::endl;
            }
            if (mergedConfig.ipAddress.empty() && !tempConfig.ipAddress.empty()) {
                mergedConfig.ipAddress = tempConfig.ipAddress;
                std::cout << GetTimeStamp() << "[OK] Using IP from Luban config." << std::endl;
            }

            if (mergedConfig.model == MODEL_UNKNOWN && tempConfig.model != MODEL_UNKNOWN) {
                mergedConfig.model = tempConfig.model;
                std::cout << GetTimeStamp() << "[OK] Using model from Luban config." << std::endl;
                if (mergedConfig.basePositionX == 0.0) {
                    switch (mergedConfig.model) {
                    case MODEL_A350:
                        mergedConfig.basePositionX = A350_BASE_POS_X;
                        mergedConfig.basePositionY = A350_BASE_POS_Y;
                        mergedConfig.basePositionZ = A350_BASE_POS_Z;
                        break;
                    case MODEL_A250:
                        mergedConfig.basePositionX = A250_BASE_POS_X;
                        mergedConfig.basePositionY = A250_BASE_POS_Y;
                        mergedConfig.basePositionZ = A250_BASE_POS_Z;
                        break;
                    default: break;
                    }
                }
            }
            HostContext::instance().updateConfig([mergedConfig](UserConfig& cfg) {
                cfg = mergedConfig;
                return true;
                });
            SaveConfig(configFile.string());
        }

        // Final config sync
        UserConfig finalConfig;
        if (LoadConfig(configFile.string(), finalConfig)) {
            if (!finalConfig.token.empty()) {
                HostContext::instance().updateConfig([&finalConfig](UserConfig& cfg) {
                    cfg.token = finalConfig.token;
                    cfg.ipAddress = finalConfig.ipAddress;
                    cfg.kasa = finalConfig.kasa;
                    cfg.autoStartPrint = finalConfig.autoStartPrint;
                    return true;
                    });
            }
        }

        UserConfig configCopy = HostContext::instance().snapshotConfig();
        std::cout << GetTimeStamp() << "Ready - " << GetModelName(configCopy.model) << " at " << configCopy.ipAddress << std::endl;

        if (!fs::exists(HS100_EXE_PATH)) {
            std::cout << GetTimeStamp() << "[WARN] hs100 executable not found at " << HS100_EXE_PATH << ". Kasa integration disabled." << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[OK] hs100 found, Kasa integration available." << std::endl;
        }

        CreateMacrosFolder();
        ClearAndDrawHeader(false);

        std::cout << GetTimeStamp() << "Checking machine status at " << configCopy.ipAddress << "..." << std::endl;
        MachineStatusData startupStatus = GetMachineStatus(false);

        if (IsMachineOffline(startupStatus)) {
            std::string connectUrl = BuildConnectUrl(configCopy.ipAddress, configCopy.token);
            std::string dummy;
            auto [res, code] = PerformHttpGetWithCode(connectUrl, dummy, 5);
            if (res != CURLE_OK) {
                if (configCopy.kasa.autoPowerOn && !configCopy.kasa.ipAddress.empty()) {
                    std::cout << GetTimeStamp() << "[INFO] Machine not reachable. Attempting auto-power-on..." << std::endl;
                    if (AutoPowerOnKasaIfNeeded()) {
                        std::cout << GetTimeStamp() << "[INFO] Machine powered on." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        startupStatus = GetMachineStatus(false);
                    }
                }
            }
        }

        if (IsMachineOffline(startupStatus)) {
            std::cout << GetTimeStamp() << "[WARN] Machine is offline. Skipping machine setup." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Please check:" << std::endl;
            std::cout << GetTimeStamp() << "[INFO]   - Machine is powered on" << std::endl;
            std::cout << GetTimeStamp() << "[INFO]   - Network connection" << std::endl;
            std::cout << GetTimeStamp() << "[INFO]   - IP address " << configCopy.ipAddress << " is correct" << std::endl;
            std::cout << GetTimeStamp() << "[INFO]   - Token" << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[OK] Machine is online. Status: " << startupStatus.getStatusDisplay() << std::endl;
            if (startupStatus.isBusy()) {
                std::cout << GetTimeStamp() << "[INFO] Machine is busy. Skipping setup." << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[INFO] Machine is idle. Checking tool head and homing status..." << std::endl;
                std::string toolHead = startupStatus.getToolHeadDisplay();
                bool isHomed = startupStatus.homed;
                std::cout << GetTimeStamp() << "[INFO] Detected tool head: " << toolHead << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Homing status: " << (isHomed ? "HOMED" : "NOT HOMED") << std::endl;

                if (isHomed) {
                    std::cout << GetTimeStamp() << "[OK] Machine is already homed." << std::endl;
                    if (toolHead.find("Laser") != std::string::npos) {
                        std::cout << GetTimeStamp() << "[INFO] Laser detected - performing additional setup (G90 & G54)..." << std::endl;
                        std::cout << GetTimeStamp() << "Press SHIFT within " << SHIFT_DETECTION_TIMEOUT_SECONDS << " seconds to bypass laser setup: " << std::flush;
                        bool shiftPressed = CheckForShiftKey(SHIFT_DETECTION_TIMEOUT_SECONDS);
                        if (shiftPressed) {
                            std::cout << "[BYPASSED]" << std::endl;
                            std::cout << GetTimeStamp() << "[INFO] Laser setup bypassed." << std::endl;
                        }
                        else {
                            std::cout << "[SETUP]" << std::endl;
                            std::cout << GetTimeStamp() << "Setting absolute positioning (G90)..." << std::endl;
                            SendGCodeWithRetry("G90", MAX_RETRY_ATTEMPTS, false);
                            std::cout << GetTimeStamp() << "Switching to work coordinates (G54)..." << std::endl;
                            SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false);
                            std::cout << GetTimeStamp() << "[OK] Laser setup completed." << std::endl;
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[INFO] No additional setup needed for " << toolHead << "." << std::endl;
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "[INFO] Machine is not homed. Homing required for all toolheads." << std::endl;
                    if (toolHead.find("Laser") != std::string::npos) {
                        std::cout << GetTimeStamp() << "[INFO] Laser detected - performing full setup." << std::endl;
                        std::cout << GetTimeStamp() << "Press SHIFT within " << SHIFT_DETECTION_TIMEOUT_SECONDS << " seconds to bypass laser setup: " << std::flush;
                        bool shiftPressed = CheckForShiftKey(SHIFT_DETECTION_TIMEOUT_SECONDS);
                        if (shiftPressed) {
                            std::cout << "[BYPASSED]" << std::endl;
                            std::cout << GetTimeStamp() << "[INFO] Laser setup bypassed." << std::endl;
                            std::cout << GetTimeStamp() << "[WARNING] Laser functions may not work correctly if machine setup is not completed." << std::endl;
                        }
                        else {
                            std::cout << "[SETUP]" << std::endl;
                            PerformLaserSetup();
                        }
                    }
                    else if (toolHead.find("3D") != std::string::npos || toolHead.find("CNC") != std::string::npos) {
                        std::cout << GetTimeStamp() << "[INFO] " << toolHead << " detected - homing required." << std::endl;
                        std::cout << GetTimeStamp() << "Press SHIFT within " << SHIFT_DETECTION_TIMEOUT_SECONDS << " seconds to bypass homing: " << std::flush;
                        bool shiftPressed = CheckForShiftKey(SHIFT_DETECTION_TIMEOUT_SECONDS);
                        if (shiftPressed) {
                            std::cout << "[BYPASSED]" << std::endl;
                            std::cout << GetTimeStamp() << "[INFO] Homing bypassed." << std::endl;
                            std::cout << GetTimeStamp() << "[WARNING] Machine operations may not work correctly if not homed." << std::endl;
                        }
                        else {
                            std::cout << "[HOMING]" << std::endl;
                            PerformPrintSetup();
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[WARN] Unknown tool head. Using auto-detection..." << std::endl;
                        PerformMachineSetup();
                    }
                }
            }
        }

        std::cout << GetTimeStamp() << "Performing initial softcam check..." << std::endl;
        if (!CheckSoftcamRegistry()) {
            std::cout << GetTimeStamp() << "[ERR] Softcam is NOT installed. Camera capture will not function properly." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Please install softcam for camera capture to function as intended." << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[OK] Softcam is properly installed." << std::endl;
        }

        std::cout << GetTimeStamp() << "Checking LightBurn camera configuration..." << std::endl;
        CheckLightBurnCameraSetting();

        CameraSession cam(CAMERA_WIDTH, CAMERA_HEIGHT, DEFAULT_FPS);
        if (!cam) {
            std::cout << GetTimeStamp() << "[ERR] Camera creation failed." << std::endl;
            return 1;
        }

        std::string imgFile = "latest.jpg";
        int w, h, channels;
        std::cout << GetTimeStamp() << "[INFO] Ready. Camera feed active." << std::endl;
        std::cout << GetTimeStamp() << "[INFO] Creating test pattern with grid..." << std::endl;
        ImageBuffer img = CreateEnhancedTestPattern(CAMERA_WIDTH, CAMERA_HEIGHT);
        w = CAMERA_WIDTH;
        h = CAMERA_HEIGHT;
        channels = 3;
        if (img) cam.sendFrame(img.get());
        std::cout << GetTimeStamp() << "[OK] Initial frame sent to camera." << std::endl;
        std::cout << GetTimeStamp() << "[INFO] Returning to main screen in " << INITIAL_FRAME_DISPLAY_SECONDS << " seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(INITIAL_FRAME_DISPLAY_SECONDS));
        ClearAndDrawHeader(true);

        int lineCount = 0;
        auto lastFrameTime = std::chrono::steady_clock::now();

        while (IsRunning() && !g_shutdownRequested.load(std::memory_order_acquire)) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTime).count();
            if (elapsedMs >= (1000 / DEFAULT_FPS)) {
                UserConfig cfg = HostContext::instance().snapshotConfig();
                if (cfg.adjustment.useAdjustment && (cfg.adjustment.pixelsX != 0 || cfg.adjustment.pixelsY != 0)) {
                    ImageBuffer adjusted = ApplyImageAdjustment(img.get(), w, h, channels, cfg.adjustment.pixelsX, cfg.adjustment.pixelsY);
                    if (adjusted) cam.sendFrame(adjusted.get());
                }
                else {
                    cam.sendFrame(img.get());
                }
                lastFrameTime = currentTime;
            }

            if (_kbhit()) {
                int key = _getch();
                if (key == ARROW_PREFIX) {
                    key = _getch();
                    switch (key) {
                    case LEFT_ARROW_KEY: AdjustImageLeft(); lineCount += ADJUSTMENT_EXTRA_LINES; break;
                    case RIGHT_ARROW_KEY: AdjustImageRight(); lineCount += ADJUSTMENT_EXTRA_LINES; break;
                    case UP_ARROW_KEY: AdjustImageUp(); lineCount += ADJUSTMENT_EXTRA_LINES; break;
                    case DOWN_ARROW_KEY: AdjustImageDown(); lineCount += ADJUSTMENT_EXTRA_LINES; break;
                    }
                    continue;
                }

                char ch = static_cast<char>(key);
                switch (ch) {
                case ENTER_ASCII:
                    lineCount += MANUAL_CAPTURE_EXTRA_LINES;
                    if (CheckMachineHomedAndWarn() && CheckLaserInstalledAndWarn()) {
                        PerformFirstTimeWarning();
                        if (CaptureAndUpdateImage(imgFile, img, w, h)) {
                            channels = 3;
                            lineCount++;
                        }
                        else {
                            lineCount++;
                        }
                    }
                    else {
                        lineCount++;
                    }
                    break;

                case SPACE_ASCII:
                    ClearAndDrawHeader(true);
                    if (CheckMachineHomedAndWarn() && CheckLaserInstalledAndWarn()) {
                        PerformFirstTimeWarning();
                        double dummyThickness = 0.0;
                        GetMaterialThicknessFromSnapmaker(dummyThickness);
                    }
                    lineCount += MATERIAL_THICKNESS_EXTRA_LINES;
                    break;

                case 'Y': case 'y':
                    if (CheckLaserInstalledAndWarn()) {
                        ToggleMaterialThicknessMode();
                    }
                    else {
                        std::this_thread::sleep_for(std::chrono::seconds(ERROR_DISPLAY_SECONDS));
                    }
                    lineCount++;
                    break;

                case 'F': case 'f':
                    if (CheckLaserInstalledAndWarn()) {
                        UpdateMaterialThicknessCustomCoordinates();
                    }
                    else {
                        std::this_thread::sleep_for(std::chrono::seconds(ERROR_DISPLAY_SECONDS));
                    }
                    lineCount = 0;
                    break;

                case 'A': case 'a':
                    if (CheckMachineHomedAndWarn() && CheckLaserInstalledAndWarn()) {
                        AutoRefreshLoop(cam, imgFile, img, w, h, channels);
                    }
                    lineCount = 0;
                    break;

                case 'B': case 'b':
                    std::cout << GetTimeStamp() << "Reset base position requested..." << std::endl;
                    lineCount++;
                    ResetBasePositionToDefault();
                    lineCount += RESET_BASE_POSITION_EXTRA_LINES;
                    break;

                case 'G': case 'g':
                    SendCustomGCodeMenu(false);
                    lineCount = 0;
                    break;

                case 'L': case 'l':
                    LaunchLightBurn();
                    lineCount++;
                    break;

                case 'M': case 'm':
                    std::cout << GetTimeStamp() << "Machine setup requested..." << std::endl;
                    lineCount++;
                    PerformMachineSetup();
                    lineCount += MACHINE_SETUP_EXTRA_LINES;
                    break;

                case 'P': case 'p':
                    HostContext::instance().updateConfig([](UserConfig& c) {
                        c.autoStartPrint = !c.autoStartPrint;
                        return true;
                        });
                    SaveConfig(HostContext::instance().getConfigPath().string());
                    ClearAndDrawHeader(true);
                    break;

                case 'R': case 'r':
                    ResetImageAdjustment();
                    lineCount += ADJUSTMENT_EXTRA_LINES;
                    break;

                case 'S': case 's':
                    DisplayMachineStatusPage(false);
                    lineCount = 0;
                    break;

                case 'T': case 't':
                    ToggleImageAdjustment();
                    lineCount += ADJUSTMENT_EXTRA_LINES;
                    break;

                case 'U': case 'u':
                    UpdateBasePosition();
                    lineCount = 0;
                    break;

                case 'C': case 'c':
                    ClearAndDrawHeader(true);
                    lineCount = 0;
                    break;

                case 'K': case 'k':
                    KasaInteractiveMode(false);
                    lineCount = 0;
                    break;

                case 'O': case 'o':
                    OpenLatestImage();
                    lineCount++;
                    break;

                case 'V': case 'v':
                    CameraInteractiveMode(false);
                    lineCount = 0;
                    break;

                case 'X': case 'x':
                case ESC_ASCII:
                    HostContext::instance().requestShutdown();
                    break;
                }
            }
            if (lineCount > MAIN_MENU_LINE_LIMIT) {
                ClearAndDrawHeader(true);
                lineCount = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
        }
        SaveConfig(HostContext::instance().getConfigPath().string());
        HostContext::instance().shutdown();
        std::cout << GetTimeStamp() << "Shutdown complete." << std::endl;
        return 0;
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[FATAL] Out of memory: " << e.what() << std::endl;
        std::cout << "\nThe application ran out of memory. Please close other applications and try again.\n";
        std::cout << "Press any key to exit...\n";
        (void)_getch();
        return 1;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[FATAL] Filesystem error: " << e.what() << std::endl;
        std::cout << "\nA filesystem error occurred. Check disk space and permissions.\n";
        std::cout << "Press any key to exit...\n";
        (void)_getch();
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        std::cout << "\nPress any key to exit...\n";
        (void)_getch();
        return 1;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[FATAL] Unknown fatal error" << std::endl;
        std::cout << "\nAn unknown fatal error occurred. The application will exit.\n";
        std::cout << "Press any key to exit...\n";
        (void)_getch();
        return 1;
    }
}