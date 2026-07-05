// camera_ui.cpp - Auto-refresh mode, camera interactive mode
#include "host.h"

void AutoRefreshLoop(CameraSession& cam, std::string& imgFile, ImageBuffer& currentImg, int& w, int& h, int& channels) {
    ClearAndDrawHeader(true);
    std::cout << GetTimeStamp() << "[INFO] Auto-refresh mode started." << std::endl;
    std::cout << GetTimeStamp() << "[INFO] Press 'S' to stop, ENTER for immediate capture." << std::endl;
    auto lastCapture = std::chrono::steady_clock::now();
    auto lastFrame = std::chrono::steady_clock::now();
    bool autoRefresh = true;
    int lineCount = 3;

    if (w <= 0 || h <= 0) {
        std::cout << GetTimeStamp() << "[ERR] Invalid image dimensions: w=" << w << ", h=" << h << std::endl;
        std::cout << GetTimeStamp() << "[INFO] Using default dimensions " << CAMERA_WIDTH << "x" << CAMERA_HEIGHT << std::endl;
        w = CAMERA_WIDTH;
        h = CAMERA_HEIGHT;
        channels = 3;

        if (!currentImg) {
            currentImg = CreateEnhancedTestPattern(w, h);
            if (!currentImg) {
                std::cout << GetTimeStamp() << "[ERR] Failed to create test pattern" << std::endl;
                ClearAndDrawHeader(true);
                return;
            }
        }
    }

    while (IsRunning() && autoRefresh && !g_shutdownRequested.load(std::memory_order_acquire)) {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame).count() >= (1000 / DEFAULT_FPS)) {
            if (currentImg) {
                if (w > 0 && h > 0 && channels > 0) {
                    UserConfig cfg = HostContext::instance().snapshotConfig();
                    if (cfg.adjustment.useAdjustment && (cfg.adjustment.pixelsX != 0 || cfg.adjustment.pixelsY != 0)) {
                        ImageBuffer adj = ApplyImageAdjustment(currentImg.get(), w, h, channels,
                            cfg.adjustment.pixelsX, cfg.adjustment.pixelsY);
                        if (adj) cam.sendFrame(adj.get());
                        else cam.sendFrame(currentImg.get());
                    }
                    else {
                        cam.sendFrame(currentImg.get());
                    }
                }
                else {
                    cam.sendFrame(currentImg.get());
                }
            }
            lastFrame = now;
        }

        if (_kbhit()) {
            int key = _getch();
            if (key == ARROW_PREFIX) {
                key = _getch();
                switch (key) {
                case LEFT_ARROW_KEY:  AdjustImageLeft();   lineCount += ADJUSTMENT_EXTRA_LINES; break;
                case RIGHT_ARROW_KEY: AdjustImageRight();  lineCount += ADJUSTMENT_EXTRA_LINES; break;
                case UP_ARROW_KEY:    AdjustImageUp();     lineCount += ADJUSTMENT_EXTRA_LINES; break;
                case DOWN_ARROW_KEY:  AdjustImageDown();   lineCount += ADJUSTMENT_EXTRA_LINES; break;
                }
                continue;
            }

            char ch = static_cast<char>(key);
            switch (ch) {
            case ENTER_ASCII:
                std::cout << GetTimeStamp() << "Manual capture triggered..." << std::endl;
                lineCount += MANUAL_CAPTURE_EXTRA_LINES;
                if (CaptureAndUpdateImage(imgFile, currentImg, w, h)) {
                    channels = 3;
                    lineCount++;
                }
                else {
                    lineCount++;
                }
                lastCapture = now;
                break;

            case 'R': case 'r':
                ResetImageAdjustment();
                lineCount += ADJUSTMENT_EXTRA_LINES;
                break;

            case 'T': case 't':
                ToggleImageAdjustment();
                lineCount += ADJUSTMENT_EXTRA_LINES;
                break;

            case 'L': case 'l':
                LaunchLightBurn();
                lineCount++;
                break;

            case 'O': case 'o':
                OpenLatestImage();
                lineCount++;
                break;

            case 'S': case 's':
                std::cout << GetTimeStamp() << "[INFO] Auto-refresh stopped." << std::endl;
                autoRefresh = false;
                break;

            case ESC_ASCII:
            case 'X': case 'x':
                HostContext::instance().requestShutdown();
                autoRefresh = false;
                break;

            case 'C': case 'c':
                ClearAndDrawHeader(true);
                std::cout << GetTimeStamp() << "[INFO] Auto-refresh mode active." << std::endl;
                lineCount = 2;
                break;
            }
        }

        if (autoRefresh && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastCapture).count() >= AUTO_REFRESH_INTERVAL_SECONDS) {
            std::cout << GetTimeStamp() << "Auto-refresh capture..." << std::endl;
            lineCount++;
            if (CaptureAndUpdateImage(imgFile, currentImg, w, h)) {
                channels = 3;
                lineCount++;
            }
            else {
                lineCount++;
            }
            lastCapture = std::chrono::steady_clock::now();
        }

        if (lineCount > AUTO_REFRESH_LINE_LIMIT) {
            ClearAndDrawHeader(true);
            std::cout << GetTimeStamp() << "[INFO] Auto-refresh mode active." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Press 'S' to stop." << std::endl;
            lineCount = 2;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }
    ClearAndDrawHeader(true);
    std::cout << GetTimeStamp() << "[INFO] Returned to manual mode." << std::endl;
}

void CameraInteractiveMode(bool fromShortcut) {
    ClearConsole();
    PrintHeader("RTSP CAMERA MENU");
    std::cout << "Configure and control the RTSP camera.\n\n";

    bool keep_going = true;
    while (keep_going && IsRunning() && !g_shutdownRequested.load(std::memory_order_acquire)) {
        UserConfig cfg = HostContext::instance().snapshotConfig();

        ClearConsole();
        PrintHeader("RTSP CAMERA MENU");
        std::cout << "Current settings:\n";
        std::cout << "  Auto-launch on print start: " << (cfg.rtspCamera.autoLaunch ? "YES" : "NO") << "\n";
        std::cout << "  RTSP URL: " << (cfg.rtspCamera.rtspUrl.empty() ? "(not set)" : cfg.rtspCamera.rtspUrl) << "\n";
        std::cout << "\nCamera process status: ";
        if (HostContext::instance().isCameraRunning())
            std::cout << "RUNNING (attached mode)";
        else
            std::cout << "NOT RUNNING";
        std::cout << "\n\n";

        PrintHeader("");
        std::cout << "Options:\n";
        std::cout << "  [A] Toggle Auto-Launch on print start\n";
        std::cout << "  [S] Start Camera (attached) - closes when program exits\n";
        std::cout << "  [T] Stop Camera (attached) - terminates the camera window\n";
        std::cout << "  [D] Start Camera (detached) - stays open after exit\n";
        std::cout << "  [C] Configure RTSP URL / Settings\n";
        std::cout << "  [ESC] Return to main menu\n";
        std::cout << "\nEnter choice: " << std::flush;

        int ch = _getch();
        std::cout << std::endl;

        switch (ch) {
        case 'A': case 'a': {
            bool newValue = !cfg.rtspCamera.autoLaunch;
            HostContext::instance().updateConfig([newValue](UserConfig& c) {
                c.rtspCamera.autoLaunch = newValue;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Auto-launch " << (newValue ? "ENABLED" : "DISABLED") << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
        case 'S': case 's': {
            if (!IsRtspCameraConfigured(cfg.rtspCamera)) {
                std::cout << GetTimeStamp() << "[ERR] Camera not configured. Use [C] to set RTSP URL.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            }
            if (HostContext::instance().isCameraRunning()) {
                std::cout << GetTimeStamp() << "[INFO] Camera is already running in attached mode.\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            }
            std::cout << GetTimeStamp() << "Starting camera (attached mode)...\n";
            if (HostContext::instance().startCamera(cfg.rtspCamera.rtspUrl))
                std::cout << GetTimeStamp() << "[OK] Camera started. It will close when this program exits.\n";
            else
                std::cout << GetTimeStamp() << "[ERR] Failed to start camera.\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        }
        case 'T': case 't': {
            if (!HostContext::instance().isCameraRunning()) {
                std::cout << GetTimeStamp() << "[INFO] No camera is currently running in attached mode.\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            }
            std::cout << GetTimeStamp() << "Stopping camera (attached mode)...\n";
            HostContext::instance().stopCamera();
            std::cout << GetTimeStamp() << "[OK] Camera stopped.\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
        case 'D': case 'd': {
            if (!IsRtspCameraConfigured(cfg.rtspCamera)) {
                std::cout << GetTimeStamp() << "[ERR] Camera not configured. Use [C] to set RTSP URL.\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;
            }
            std::cout << GetTimeStamp() << "Starting camera (detached mode)...\n";
            if (HostContext::instance().startCameraDetached(cfg.rtspCamera.rtspUrl))
                std::cout << GetTimeStamp() << "[OK] Camera launched. It will remain open after program exit.\n";
            else
                std::cout << GetTimeStamp() << "[ERR] Failed to launch camera.\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        }
        case 'C': case 'c': {
            ConfigureRtspCamera();
            break;
        }
        case ESC_ASCII:
            keep_going = false;
            break;
        default:
            std::cout << GetTimeStamp() << "[INFO] Invalid option.\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
    }
    if (!fromShortcut) {
        ClearConsole();
        ClearAndDrawHeader(true);
        std::cout << std::flush;
    }
}