// process_handlers.cpp - File upload, print preparation, Kasa commands, LightBurn, email, beep, shortcuts
#include "host.h"

// ---------- File Processing Helpers ----------
struct FileSizeInfo {
    bool isLargeFile;
    bool isFirmware;
    size_t sizeBytes;
    double sizeMB;
    double sizeKB;
};

static FileSizeInfo GetFileSizeInfo(const std::string& filepath) {
    FileSizeInfo info = { false, false, 0, 0.0, 0.0 };

    try {
        if (!fs::exists(filepath)) {
            return info;
        }

        info.isFirmware = IsFirmwareFile(filepath);
        info.sizeBytes = fs::file_size(filepath);
        info.sizeKB = static_cast<double>(info.sizeBytes) / 1024.0;
        info.sizeMB = static_cast<double>(info.sizeBytes) / (1024.0 * 1024.0);

        if (!info.isFirmware) {
            info.isLargeFile = (info.sizeBytes > LARGE_FILE_THRESHOLD_BYTES);
        }
        else {
            // Firmware files are always considered "large" for status checker purposes
            info.isLargeFile = true;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[WARN] Could not get file size for " << filepath << ": " << e.what() << std::endl;
        info.isLargeFile = false;
        info.isFirmware = false;
        info.sizeBytes = 0;
        info.sizeKB = 0.0;
        info.sizeMB = 0.0;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[WARN] Unexpected error getting file size: " << e.what() << std::endl;
        info.isLargeFile = false;
    }

    return info;
}

static void LogFileSizeInfo(const FileSizeInfo& info, const std::string& operation) {
    if (info.isLargeFile || info.isFirmware) {
        std::cout << std::endl << GetTimeStamp() << "[INFO] " << operation << " file detected (";
        if (info.sizeMB >= 1.0) {
            std::cout << std::fixed << std::setprecision(2) << info.sizeMB << " MB";
        }
        else {
            std::cout << std::fixed << std::setprecision(1) << info.sizeKB << " KB";
        }
        std::cout << ") - starting status checker keep alive." << std::endl;
    }
}

// ---------- File Upload Functions ----------
CURLcode PreparePrintOnSnapmaker(const std::string& filepath, const std::string& printType, std::string& response, long timeout) {
    try {
        if (!fs::exists(filepath)) {
            std::cout << GetTimeStamp() << "[ERR] File not found: " << filepath << std::endl;
            return CURLE_HTTP_RETURNED_ERROR;
        }

        UserConfig cfg = HostContext::instance().snapshotConfig();
        if (cfg.token.empty()) {
            std::cout << GetTimeStamp() << "[ERR] No token available for upload." << std::endl;
            return CURLE_HTTP_RETURNED_ERROR;
        }

        FileSizeInfo fileInfo = GetFileSizeInfo(filepath);
        LogFileSizeInfo(fileInfo, "Large");

        std::optional<StatusCheckerGuard> statusGuard;
        if (fileInfo.isLargeFile || fileInfo.isFirmware) {
            statusGuard.emplace(cfg.ipAddress, cfg.token);
        }

        CurlSession session;
        if (!session) return CURLE_FAILED_INIT;
        CURL* curl = session.get();
        curl_easy_reset(curl);

        struct curl_httppost* formpost = NULL;
        struct curl_httppost* lastptr = NULL;

        curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "token",
            CURLFORM_COPYCONTENTS, cfg.token.c_str(),
            CURLFORM_END);
        curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "file",
            CURLFORM_FILE, filepath.c_str(),
            CURLFORM_CONTENTTYPE, "application/octet-stream",
            CURLFORM_END);

        FormPostGuard formGuard(formpost);

        std::string url = BuildPreparePrintUrl(cfg.ipAddress, cfg.token, printType);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        ProgressData prog;
        prog.filename = fs::path(filepath).filename().string();
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CurlSlistGuard headers;
        headers.append("Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK && res != CURLE_OPERATION_TIMEDOUT) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            std::cout << GetTimeStamp() << "[ERR] Prepare print failed: " << curl_easy_strerror(res)
                << " (HTTP " << http_code << ")" << std::endl;
        }
        return res;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Filesystem error in PreparePrintOnSnapmaker: " << e.what() << std::endl;
        return CURLE_HTTP_RETURNED_ERROR;
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[ERR] Out of memory in PreparePrintOnSnapmaker: " << e.what() << std::endl;
        return CURLE_OUT_OF_MEMORY;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Exception in PreparePrintOnSnapmaker: " << e.what() << std::endl;
        return CURLE_FAILED_INIT;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] Unknown exception in PreparePrintOnSnapmaker" << std::endl;
        return CURLE_FAILED_INIT;
    }
}

CURLcode UploadFileToSnapmaker(const std::string& filepath, std::string& response, long timeout) {
    try {
        if (!fs::exists(filepath)) {
            std::cout << GetTimeStamp() << "[ERR] File not found: " << filepath << std::endl;
            return CURLE_HTTP_RETURNED_ERROR;
        }

        UserConfig cfg = HostContext::instance().snapshotConfig();
        if (cfg.token.empty()) {
            std::cout << GetTimeStamp() << "[ERR] No token available for upload." << std::endl;
            return CURLE_HTTP_RETURNED_ERROR;
        }

        FileSizeInfo fileInfo = GetFileSizeInfo(filepath);
        LogFileSizeInfo(fileInfo, "Large");

        std::unique_ptr<StatusCheckerGuard> statusGuard;
        if (fileInfo.isLargeFile || fileInfo.isFirmware) {
            statusGuard = std::make_unique<StatusCheckerGuard>(cfg.ipAddress, cfg.token);
        }

        CurlSession session;
        if (!session) return CURLE_FAILED_INIT;
        CURL* curl = session.get();
        curl_easy_reset(curl);

        struct curl_httppost* formpost = NULL;
        struct curl_httppost* lastptr = NULL;

        curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "file",
            CURLFORM_FILE, filepath.c_str(),
            CURLFORM_CONTENTTYPE, "application/octet-stream",
            CURLFORM_END);

        if (!formpost) {
            std::cout << GetTimeStamp() << "[ERR] Failed to create form data." << std::endl;
            return CURLE_FAILED_INIT;
        }
        FormPostGuard formGuard(formpost);

        std::string url = BuildUploadUrl(cfg.ipAddress, cfg.token);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

        ProgressData prog;
        prog.filename = fs::path(filepath).filename().string();
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CurlSlistGuard headers;
        headers.append("Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            std::cout << GetTimeStamp() << "[ERR] Upload failed: " << curl_easy_strerror(res)
                << " (HTTP " << http_code << ")" << std::endl;
        }
        return res;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Filesystem error in UploadFileToSnapmaker: " << e.what() << std::endl;
        return CURLE_HTTP_RETURNED_ERROR;
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[ERR] Out of memory in UploadFileToSnapmaker: " << e.what() << std::endl;
        return CURLE_OUT_OF_MEMORY;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Exception in UploadFileToSnapmaker: " << e.what() << std::endl;
        return CURLE_FAILED_INIT;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] Unknown exception in UploadFileToSnapmaker" << std::endl;
        return CURLE_FAILED_INIT;
    }
}

bool ProcessFileUpload(const std::string& filepath, int operation, bool interactive) {
    try {
        fs::path filePath(filepath);

        // ========== VALIDATION PHASE ==========
        try {
            if (!fs::exists(filepath)) {
                std::cout << GetTimeStamp() << "[ERR] File not found: " << filepath << std::endl;
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cout << GetTimeStamp() << "[ERR] Cannot access file: " << e.what() << std::endl;
            return false;
        }

        double sizeKB = 0.0;
        try {
            sizeKB = static_cast<double>(fs::file_size(filepath)) / 1024.0;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cout << GetTimeStamp() << "[ERR] Cannot determine file size: " << e.what() << std::endl;
            return false;
        }

        if (!IsSupportedFile(filepath)) {
            std::cout << GetTimeStamp() << "[ERR] Unsupported file type: " << fs::path(filepath).extension().string() << std::endl;
            return false;
        }

        bool isFirmware = IsFirmwareFile(filepath);

        if (isFirmware) {
            if (operation == FILEOP_AUTO_START) {
                std::cout << GetTimeStamp() << "[ERR] Firmware cannot be auto-started" << std::endl;
                return false;
            }

            if (interactive) {
                double sizeMB = 0.0;
                try {
                    sizeMB = static_cast<double>(fs::file_size(filepath)) / (1024.0 * 1024.0);
                }
                catch (...) {
                    sizeMB = 0.0;
                }
                std::cout << "[WARNING] Ensure this is the correct firmware for your model.\n";
                PrintHeader("FIRMWARE");
                std::cout << "Firmware File:\n";
                std::cout << "  Name: " << filePath.filename().string() << "\n";
                std::cout << "  Size: " << std::fixed << std::setprecision(2) << sizeMB << " MB\n\n";
                std::cout << "Do you want to continue with upload? (y/N): ";

                int ch = _getch();
                std::cout << std::endl;
                if (ch != 'y' && ch != 'Y') {
                    std::cout << GetTimeStamp() << "[INFO] Firmware upload cancelled." << std::endl;
                    return false;
                }
            }
            operation = FILEOP_UPLOAD_ONLY;
        }

        std::cout << GetTimeStamp() << "File: " << filePath.filename().string() << "\n";
        std::cout << GetTimeStamp() << "Size: " << std::fixed << std::setprecision(2) << sizeKB << " KB\n";

        // ========== MACHINE CONNECTION PHASE ==========
        MachineStatusData status = GetMachineStatus(false);

        if (IsMachineOffline(status)) {
            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.kasa.autoPowerOn && !cfg.kasa.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "[INFO] Machine offline. Attempting auto power-on via Kasa...\n";
                if (AutoPowerOnKasaIfNeeded()) {
                    std::cout << GetTimeStamp() << "[OK] Machine powered on. Waiting for boot...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    status = GetMachineStatus(false);
                }
            }

            if (IsMachineOffline(status)) {
                std::cout << GetTimeStamp() << "[ERR] Machine is offline and could not be powered on" << std::endl;
                return false;
            }
        }

        std::cout << GetTimeStamp() << "[OK] Machine is online.\n";

        if (status.isBusy()) {
            std::cout << GetTimeStamp() << "[ERR] Machine is busy (Status: " << status.getStatusDisplay() << ")" << std::endl;
            return false;
        }

        if (!isFirmware) {
            if (operation == FILEOP_AUTO_START && !status.homed) {
                std::cout << GetTimeStamp() << "[INFO] Machine not homed. Performing setup...\n";
                PerformMachineSetup();
                status = GetMachineStatus(false);

                if (!status.homed) {
                    std::cout << GetTimeStamp() << "[ERR] Machine could not be homed" << std::endl;
                    return false;
                }
            }

            ModuleType currentModule = GetModuleTypeFromToolHead(status.toolHead);
            if (currentModule == MODULE_UNKNOWN) {
                std::cout << GetTimeStamp() << "[ERR] Unknown tool head: " << status.toolHead << std::endl;
                return false;
            }

            bool isCompatible = IsFileCompatibleWithModule(filepath, currentModule);
            fs::path p(filepath);
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool isSecondary = false;
            if ((currentModule == MODULE_LASER || currentModule == MODULE_CNC) && ext == ".gcode") {
                isSecondary = true;
            }

            if (!isCompatible && !isSecondary) {
                std::string expected = GetExpectedExtensions(currentModule);
                std::cout << GetTimeStamp() << "[ERR] File type " << ext << " not compatible with " << status.getToolHeadDisplay() << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Expected format: " << expected << std::endl;
                return false;
            }

            if (isSecondary) {
                std::cout << GetTimeStamp() << "[OK] Secondary format accepted: " << ext << std::endl;
            }

            if (status.isLaser() && status.isEnclosureDoorOpen && operation == FILEOP_AUTO_START) {
                std::cout << GetTimeStamp() << "[ERR] Enclosure door is open. Close door before laser operation." << std::endl;
                return false;
            }

            if (status.is3DPrinter() && status.isFilamentOut && operation == FILEOP_AUTO_START) {
                std::cout << GetTimeStamp() << "[ERR] Filament is out. Please load filament first." << std::endl;
                return false;
            }
        }
        else {
            std::cout << GetTimeStamp() << "[INFO] Firmware update mode - skipping module compatibility check.\n";
        }

        // ========== CONNECTION PHASE ==========
        std::cout << GetTimeStamp() << "Connecting to Snapmaker...\n";
        if (ConnectAndMaintainConnection() != CURLE_OK) {
            std::cout << GetTimeStamp() << "[ERR] Failed to connect to Snapmaker" << std::endl;
            return false;
        }

        // ========== UPLOAD VS PREPARE PRINT PHASE ==========
        std::string response;
        bool success = false;

        if (operation == FILEOP_AUTO_START && !isFirmware) {
            std::string printType;
            ModuleType currentModule = GetModuleTypeFromToolHead(status.toolHead);
            switch (currentModule) {
            case MODULE_LASER: printType = "Laser"; break;
            case MODULE_3DP: printType = "3DP"; break;
            case MODULE_CNC: printType = "CNC"; break;
            default: printType = "Laser"; break;
            }

            std::cout << GetTimeStamp() << "Preparing print (uploading and preparing file)...";
            if (PreparePrintOnSnapmaker(filepath, printType, response, CURL_UPLOAD_TIMEOUT) == CURLE_OK) {
                success = true;
                std::cout << std::endl << GetTimeStamp() << "[OK] File prepared successfully...";
            }
            else {
                std::cout << std::endl << GetTimeStamp() << "[ERR] Prepare print failed.";
                return false;
            }
        }
        else {
            std::cout << GetTimeStamp() << "Uploading file...";
            if (UploadFileToSnapmaker(filepath, response, CURL_UPLOAD_TIMEOUT) == CURLE_OK) {
                success = true;
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Upload failed";
                return false;
            }
        }

        if (!success) {
            return false;
        }

        std::cout << "\n+----------------------------------------------------+\n";
        if (isFirmware) {
            std::cout << "|              FIRMWARE UPLOAD COMPLETE              |\n";
            std::cout << "|          Firmware uploaded successfully!           |\n";
            std::cout << "|         The file is now on your Snapmaker.         |\n";
            std::cout << "|      Start it manually from the touchscreen.       |\n";
            std::cout << "|     IMPORTANT: Do not power off during update!     |\n";
        }
        else {
            if (operation == FILEOP_AUTO_START) {
                std::cout << "|              FILE PREPARED FOR PRINT               |\n";
                std::cout << "|          File uploaded and ready to print!         |\n";
                std::cout << "|         The file is now on your Snapmaker.         |\n";
            }
            else {
                std::cout << "|                  UPLOAD COMPLETE                   |\n";
                std::cout << "|            File uploaded successfully!             |\n";
                std::cout << "|         The file is now on your Snapmaker.         |\n";
                std::cout << "|       Start it manually from the touchscreen.      |\n";
            }
        }
        std::cout << "+----------------------------------------------------+\n";

        // Get config for file action
        UserConfig cfg = HostContext::instance().snapshotConfig();
        std::string action = cfg.fileAction;
        std::transform(action.begin(), action.end(), action.begin(), ::tolower);
        bool useRecycleBin = (action != "delete");  // Default to recycle bin

        // ========== DELETE TEMPORARY FILE (HTTP server temp files) ==========
        // This runs for both upload-only and auto-start after successful upload/prepare.
        try {
            fs::path snapmakerTempDir = GetSnapmakerTempPath();  // Use your dedicated folder
            fs::path filePath(filepath);

            // Check if file is in the snapmaker_upload temp directory
            if (filePath.parent_path() == snapmakerTempDir) {
                if (fs::exists(filePath)) {
                    if (useRecycleBin) {
                        if (MoveToRecycleBin(filePath.string())) {
                            std::cout << GetTimeStamp() << "[INFO] Moved temp file to Recycle Bin: " << filePath.filename().string() << std::endl;
                        }
                        else {
                            fs::remove(filePath);
                            std::cout << GetTimeStamp() << "[INFO] Deleted temp file: " << filePath.filename().string() << std::endl;
                        }
                    }
                    else {
                        fs::remove(filePath);
                        std::cout << GetTimeStamp() << "[INFO] Deleted temp file: " << filePath.filename().string() << std::endl;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[WARN] Could not delete temp file: " << e.what() << std::endl;
        }

        // ========== DELETE OR RECYCLE SOURCE FILE IF IN "Upload" FOLDER ==========
        try {
            wchar_t exePathW[MAX_PATH];
            GetModuleFileNameW(NULL, exePathW, MAX_PATH);
            fs::path uploadDir = fs::path(exePathW).parent_path() / "Upload";
            fs::path srcPath(filepath);
            if (srcPath.parent_path() == uploadDir && fs::exists(srcPath)) {
                if (useRecycleBin) {
                    if (MoveToRecycleBin(srcPath.string())) {
                        std::cout << GetTimeStamp() << "[INFO] Moved to Recycle Bin: " << srcPath.filename().string() << std::endl;
                    }
                    else {
                        std::cerr << GetTimeStamp() << "[WARN] Failed to move to Recycle Bin, deleting permanently" << std::endl;
                        fs::remove(srcPath);
                    }
                }
                else {
                    fs::remove(srcPath);
                    std::cout << GetTimeStamp() << "[INFO] Permanently deleted from Upload folder: " << srcPath.filename().string() << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[WARN] Could not delete source file: " << e.what() << std::endl;
        }

        // ========== START PRINT PHASE ==========
        if (operation == FILEOP_AUTO_START && !isFirmware) {
             // ========== CONNECTION PHASE FOR PREPARE PRINT ==========
            std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

            std::cout << GetTimeStamp() << "Connecting to Snapmaker...\n";
            if (ConnectAndMaintainConnection() != CURLE_OK) {
                std::cout << GetTimeStamp() << "[ERR] Failed to connect to Snapmaker" << std::endl;
                return false;
            }
            std::cout << GetTimeStamp() << "Starting print..." << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

            UserConfig cfg = HostContext::instance().snapshotConfig();
            std::string startUrl = BuildStartPrintUrl(cfg.ipAddress, cfg.token);
            std::string startResp;
            auto [res, code] = PerformHttpPostWithCode(startUrl, "", startResp, 10);

            if (res == CURLE_OK && (code == 200 || code == 202 || code == 204 || code == 409)) {
                std::cout << "+----------------------------------------------------+\n";
                std::cout << "|                   PRINT STARTED                    |\n";
                std::cout << "|          Print job started successfully!           |\n";
                std::cout << "|     Keep this window open to monitor progress.     |\n";
                std::cout << "|     Close to monitor from touchscreen instead.     |\n";
                std::cout << "+----------------------------------------------------+\n";

                // Auto-launch camera
                if (IsRtspCameraConfigured(cfg.rtspCamera) && cfg.rtspCamera.autoLaunch && !HostContext::instance().isCameraRunning()) {
                    std::cout << GetTimeStamp() << "Auto-starting camera in 3 seconds (press SHIFT to cancel)... ";
                    std::cout.flush();
                    if (!CheckForShiftKey(3)) {
                        bool launched = false;
                        if (cfg.autoMonitorPrint) {
                            // Attached mode: camera closes when monitor exits
                            launched = HostContext::instance().startCamera(cfg.rtspCamera.rtspUrl);
                        }
                        else {
                            // Detached mode: camera stays open after program exits
                            launched = HostContext::instance().startCameraDetached(cfg.rtspCamera.rtspUrl);
                        }
                        if (launched) {
                            std::cout << "Camera started." << std::endl;
                        }
                        else {
                            std::cout << "Failed to start camera." << std::endl;
                        }
                    }
                    else {
                        std::cout << "Camera launch cancelled." << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                // Start monitoring with progress
                bool shouldMonitor = cfg.autoMonitorPrint;
                if (shouldMonitor) {
                    std::cout << GetTimeStamp() << "Auto-monitor is enabled. Press SHIFT within 3 seconds to skip monitor window." << std::endl;
                    if (CheckForShiftKey(3)) {
                        shouldMonitor = false;
                        std::cout << GetTimeStamp() << "Monitoring skipped by user." << std::endl;
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "Auto-monitor is disabled in config.json. No monitor window will appear." << std::endl;
                    std::cout << GetTimeStamp() << "Press SHIFT within 3 seconds to override and start monitor anyway." << std::endl;
                    if (CheckForShiftKey(3)) {
                        shouldMonitor = true;
                        std::cout << GetTimeStamp() << "Override: monitoring started." << std::endl;
                    }
                }

                if (shouldMonitor) {
                    MonitorPrintWithProgress(true);
                }
                else {
                    std::cout << GetTimeStamp() << "Monitor not started. The print continues on the printer." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            else {
                std::cout << GetTimeStamp() << "[WARN] File prepared but could not auto-start (HTTP " << code << ").\n";
                std::cout << GetTimeStamp() << "[INFO] Start manually from the touchscreen.\n";
            }
        }
        return true;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << GetTimeStamp() << "[ERR] File system error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cout << GetTimeStamp() << "[ERR] Unexpected error in file upload: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cout << GetTimeStamp() << "[ERR] Unknown error in file upload" << std::endl;
        return false;
    }
}

void ProcessFileWithMode(const std::string& filepath, int mode, bool waitForKey) {
    ClearConsole();

    // Check for firmware FIRST
    bool isFirmware = IsFirmwareFile(filepath);

    // Show appropriate banner
    switch (mode) {
    case FILEOP_UPLOAD_ONLY:
        PrintHeader("SNAPMAKER FILE UPLOAD");
        break;
    case FILEOP_AUTO_START:
        PrintHeader("SNAPMAKER AUTO-START PRINT");
        break;
    case FILEOP_SMART:
        PrintHeader("SNAPMAKER FILE UPLOAD");
        break;
    }

    int actualMode = mode;

    // Handle firmware: force upload-only mode and show warning
    if (isFirmware) {
        if (mode == FILEOP_AUTO_START) {
            std::cout << GetTimeStamp() << "[WARN] Firmware cannot be auto-started. Switching to UPLOAD-ONLY mode." << std::endl;
            actualMode = FILEOP_UPLOAD_ONLY;
        }
        else if (mode == FILEOP_SMART) {
            // For drag-and-drop firmware, force upload-only and don't show shift prompt
            std::cout << GetTimeStamp() << "[INFO] Firmware file detected - using UPLOAD-ONLY mode." << std::endl;
            actualMode = FILEOP_UPLOAD_ONLY;
        }
    }

    // Only show shift key prompt for non-firmware files in SMART mode and NOT from tray
    if (mode == FILEOP_SMART && !isFirmware) {
        UserConfig cfg = HostContext::instance().snapshotConfig();

        const char* defaultMode = cfg.autoStartPrint ? "AUTO-START" : "UPLOAD-ONLY";
        std::cout << GetTimeStamp() << "[INFO] Default mode: " << defaultMode << std::endl;

        std::cout << GetTimeStamp() << "Press Shift within " << SHIFT_DETECTION_TIMEOUT_SECONDS
            << " seconds to override to " << (cfg.autoStartPrint ? "UPLOAD-ONLY" : "AUTO-START") << "... ";
        std::cout.flush();

        bool shiftPressed = CheckForShiftKey(SHIFT_DETECTION_TIMEOUT_SECONDS);

        if (shiftPressed) {
            actualMode = cfg.autoStartPrint ? FILEOP_UPLOAD_ONLY : FILEOP_AUTO_START;
            std::cout << "[OVERRIDE]" << std::endl;
            const char* overrideMode = (actualMode == FILEOP_AUTO_START) ? "AUTO-START" : "UPLOAD-ONLY";
            std::cout << GetTimeStamp() << "[INFO] Override mode: " << overrideMode << std::endl;
        }
        else {
            std::cout << "[DEFAULT]" << std::endl;
            actualMode = cfg.autoStartPrint ? FILEOP_AUTO_START : FILEOP_UPLOAD_ONLY;
        }
    }

    ProcessFileUpload(filepath, actualMode, true);

    if (waitForKey && actualMode != FILEOP_AUTO_START) {
        std::cout << std::endl << "Press any key to exit..." << std::endl;
        (void)_getch();
    }
    HostContext::instance().requestShutdown();
    return;
}