// status_page.cpp - Machine status display, Z/X/Y calibration UI
#include "host.h"

struct G92Offsets {
    double x;
    double y;
    double z;
    bool found;
};

static G92Offsets ParseG92Line(const std::string& filepath) {
    G92Offsets result = { 0.0, 0.0, 0.0, false };

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return result;

        std::string line;
        std::regex g92_pattern(R"(G92\s+X([-+]?\d*\.?\d+)\s+Y([-+]?\d*\.?\d+)\s+Z([-+]?\d*\.?\d+))");

        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, g92_pattern) && match.size() == 4) {
                try {
                    result.x = std::stod(match[1].str());
                    result.y = std::stod(match[2].str());
                    result.z = std::stod(match[3].str());
                    result.found = true;
                    break;
                }
                catch (const std::invalid_argument& e) {
                    std::cerr << GetTimeStamp() << "[WARN] Invalid number in G92 line: " << e.what() << std::endl;
                    continue;
                }
                catch (const std::out_of_range& e) {
                    std::cerr << GetTimeStamp() << "[WARN] Number out of range: " << e.what() << std::endl;
                    continue;
                }
            }
        }
        file.close();
    }
    catch (const std::regex_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Regex error in ParseG92Line: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] ParseG92Line exception: " << e.what() << std::endl;
    }
    return result;
}

static bool UpdateG92XYInFile(const std::string& filepath, double newX, double newY) {
    try {
        std::ifstream infile(filepath);
        if (!infile.is_open()) {
            std::cerr << GetTimeStamp() << "[ERR] Cannot open file for reading: " << filepath << std::endl;
            return false;
        }

        std::string content;
        std::string line;
        bool replaced = false;

        std::regex g92_pattern(R"(G92\s+X[-+]?\d*\.?\d+\s+Y[-+]?\d*\.?\d+\s+Z[-+]?\d*\.?\d+)");
        std::regex z_pattern(R"(Z([-+]?\d*\.?\d+))");

        while (std::getline(infile, line)) {
            if (std::regex_search(line, g92_pattern)) {
                std::smatch z_match;
                if (std::regex_search(line, z_match, z_pattern) && z_match.size() >= 2) {
                    try {
                        double zValue = std::stod(z_match[1].str());
                        (void)zValue;

                        std::ostringstream newLine;
                        newLine << "G92 X" << std::fixed << std::setprecision(1) << newX
                            << " Y" << newY
                            << " Z" << z_match[1].str();
                        line = newLine.str();
                        replaced = true;
                    }
                    catch (const std::invalid_argument& e) {
                        std::cerr << GetTimeStamp() << "[WARN] Invalid Z value format: " << e.what() << std::endl;
                    }
                    catch (const std::out_of_range& e) {
                        std::cerr << GetTimeStamp() << "[WARN] Z value out of range: " << e.what() << std::endl;
                    }
                }
            }
            content += line + "\n";
        }
        infile.close();

        if (!replaced) {
            std::cerr << GetTimeStamp() << "[WARN] No G92 line found in file: " << filepath << std::endl;
            return false;
        }

        std::ofstream outfile(filepath);
        if (!outfile.is_open()) {
            std::cerr << GetTimeStamp() << "[ERR] Cannot open file for writing: " << filepath << std::endl;
            return false;
        }

        outfile << content;
        outfile.close();
        return true;
    }
    catch (const std::regex_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Regex error in UpdateG92XYInFile: " << e.what() << std::endl;
        return false;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Filesystem error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] UpdateG92XYInFile exception: " << e.what() << std::endl;
        return false;
    }
}

static std::pair<double, double> CalculateNewXYOffsets(double measuredX, double measuredY,
    double currentX, double currentY) {
    double deltaX = measuredX - 20.0;
    double deltaY = measuredY - 20.0;
    double newX = currentX + deltaX;
    double newY = currentY + deltaY;
    newX = std::round(newX * 10.0) / 10.0;
    newY = std::round(newY * 10.0) / 10.0;
    return { newX, newY };
}

static double ParseEndGCodeZ(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return 0.0;
        std::string line;
        std::regex z_pattern(R"(G0\s+Z([-+]?\d*\.?\d+))");
        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, z_pattern) && match.size() == 2) {
                try {
                    return std::stod(match[1].str());
                }
                catch (const std::exception& e) {
                    std::cerr << GetTimeStamp() << "[WARN] Failed to parse Z value: " << e.what() << std::endl;
                    continue;
                }
            }
        }
        return 0.0;
    }
    catch (const std::regex_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Regex error in ParseEndGCodeZ: " << e.what() << std::endl;
        return 0.0;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] ParseEndGCodeZ exception: " << e.what() << std::endl;
        return 0.0;
    }
}

void DisplayMachineStatusPage(bool fromShortcut)
{
    ClearAndDrawHeader(false);

    bool inStatusPage = true;
    static double staticBufferZ = 0.0;
    static double staticBufferX = 0.0;
    static double staticBufferY = 0.0;
    static double zFocusHeight = 0.0;
    static bool zHeightCalculated = false;
    static bool xyCalibrated = false;
    static bool homingAttempted = false;

    struct LaserOffsets {
        double x;
        double y;
        double z;
        bool valid;
    };
    static LaserOffsets offsets10w = { 0.0, 0.0, 0.0, false };
    static LaserOffsets offsets2040wIR = { 0.0, 0.0, 0.0, false };
    static double endZLift = 0.0;

    auto readOffsetsFromFile = [](const fs::path& filepath, LaserOffsets& out) {
        out.valid = false;
        if (!fs::exists(filepath)) return;
        G92Offsets parsed = ParseG92Line(filepath.string());
        if (parsed.found) {
            out.x = parsed.x;
            out.y = parsed.y;
            out.z = parsed.z;
            out.valid = true;
        }
        };

    fs::path gcodeFolder = "G-code";
    fs::path file10w = gcodeFolder / "Start G-code Sample - 10W Laser.txt";
    fs::path file2040wIR = gcodeFolder / "Start G-code Sample - 20W, 40W and IR Lasers.txt";
    fs::path endFile = gcodeFolder / "End G-code Sample - All Lasers.txt";

    readOffsetsFromFile(file10w, offsets10w);
    readOffsetsFromFile(file2040wIR, offsets2040wIR);
    endZLift = ParseEndGCodeZ(endFile.string());

    staticBufferZ = 0.0;
    staticBufferX = 0.0;
    staticBufferY = 0.0;
    zFocusHeight = 0.0;
    zHeightCalculated = false;
    xyCalibrated = false;
    homingAttempted = false;

    auto lastFile10wTime = fs::exists(file10w) ? fs::last_write_time(file10w) : fs::file_time_type();
    auto lastFile2040wTime = fs::exists(file2040wIR) ? fs::last_write_time(file2040wIR) : fs::file_time_type();
    auto lastEndFileTime = fs::exists(endFile) ? fs::last_write_time(endFile) : fs::file_time_type();

    LaserOffsets lastDisplayed10w = offsets10w;
    LaserOffsets lastDisplayed2040w = offsets2040wIR;
    double lastDisplayedEndZLift = endZLift;
    MachineStatusData lastDisplayedStatus;
    bool firstRun = true;

    auto listBackupsForFile = [](const fs::path& targetFile) -> std::vector<fs::path> {
        std::vector<fs::path> backups;
        try {
            fs::path folder = targetFile.parent_path();
            std::string baseName = targetFile.stem().string();
            std::regex pattern(baseName + "\\.txt_\\d{8}_\\d{6}\\.old");
            int idx = 1;
            std::cout << "Backup files for " << targetFile.filename().string() << ":" << std::endl;
            for (const auto& entry : fs::directory_iterator(folder)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (std::regex_match(filename, pattern)) {
                        std::cout << "  " << idx++ << ". " << filename << std::endl;
                        backups.push_back(entry.path());
                    }
                }
            }
            if (backups.empty()) {
                std::cout << "  No backup files found." << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[ERR] " << e.what() << std::endl;
        }
        return backups;
        };

    auto deleteOldBackups = [](const fs::path& targetFile) -> int {
        try {
            if (!fs::exists(targetFile.parent_path()) || !fs::is_directory(targetFile.parent_path())) return 0;
            fs::path folder = targetFile.parent_path();
            std::string baseName = targetFile.stem().string();
            std::regex pattern(baseName + "\\.txt_\\d{8}_\\d{6}\\.old");
            int count = 0;
            for (const auto& entry : fs::directory_iterator(folder)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (std::regex_match(filename, pattern)) {
                        try {
                            fs::remove(entry.path());
                            count++;
                        }
                        catch (const std::exception& e) {
                            std::cerr << GetTimeStamp() << "[ERR] Failed to delete: " << e.what() << std::endl;
                        }
                    }
                }
            }
            return count;
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[ERR] " << e.what() << std::endl;
            return 0;
        }
        };

    auto restoreFromBackup = [](const fs::path& targetFile, int backupIndex = -1) -> bool {
        try {
            fs::path folder = targetFile.parent_path();
            std::string baseName = targetFile.stem().string();
            std::vector<fs::path> backups;
            std::regex pattern(baseName + "\\.txt_\\d{8}_\\d{6}\\.old");

            for (const auto& entry : fs::directory_iterator(folder)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (std::regex_match(filename, pattern)) {
                        backups.push_back(entry.path());
                    }
                }
            }

            if (backups.empty()) {
                std::cout << GetTimeStamp() << "[WARN] No backup files found for " << targetFile.filename().string() << std::endl;
                return false;
            }

            fs::path backupToRestore;
            if (backupIndex >= 1 && backupIndex <= static_cast<int>(backups.size())) {
                std::sort(backups.begin(), backups.end());
                backupToRestore = backups[backupIndex - 1];
                std::cout << GetTimeStamp() << "Restoring from selected backup: " << backupToRestore.filename().string() << std::endl;
            }
            else {
                std::sort(backups.begin(), backups.end());
                backupToRestore = backups.back();
                std::cout << GetTimeStamp() << "Restoring from latest backup: " << backupToRestore.filename().string() << std::endl;
            }

            fs::copy(backupToRestore, targetFile, fs::copy_options::overwrite_existing);
            std::cout << GetTimeStamp() << "[OK] Restored from backup: " << backupToRestore.filename().string() << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[ERR] Restore failed: " << e.what() << std::endl;
            return false;
        }
        };

    while (inStatusPage && IsRunning() && !g_shutdownRequested.load(std::memory_order_acquire)) {

        bool needsRedraw = false;

        auto currentFile10wTime = fs::exists(file10w) ? fs::last_write_time(file10w) : fs::file_time_type();
        auto currentFile2040wTime = fs::exists(file2040wIR) ? fs::last_write_time(file2040wIR) : fs::file_time_type();
        auto currentEndFileTime = fs::exists(endFile) ? fs::last_write_time(endFile) : fs::file_time_type();

        bool file10wChanged = (currentFile10wTime != lastFile10wTime);
        bool file2040wChanged = (currentFile2040wTime != lastFile2040wTime);
        bool endFileChanged = (currentEndFileTime != lastEndFileTime);

        if (file10wChanged || file2040wChanged || endFileChanged) {
            if (file10wChanged) {
                readOffsetsFromFile(file10w, offsets10w);
                lastFile10wTime = currentFile10wTime;
            }
            if (file2040wChanged) {
                readOffsetsFromFile(file2040wIR, offsets2040wIR);
                lastFile2040wTime = currentFile2040wTime;
            }
            if (endFileChanged) {
                endZLift = ParseEndGCodeZ(endFile.string());
                lastEndFileTime = currentEndFileTime;
            }
            needsRedraw = true;
        }

        MachineStatusData status = GetMachineStatus(false);

        if (status.x != lastDisplayedStatus.x || status.y != lastDisplayedStatus.y || status.z != lastDisplayedStatus.z ||
            status.status != lastDisplayedStatus.status || status.homed != lastDisplayedStatus.homed ||
            status.toolHead != lastDisplayedStatus.toolHead) {
            needsRedraw = true;
            lastDisplayedStatus = status;
        }

        if (offsets10w.x != lastDisplayed10w.x || offsets10w.y != lastDisplayed10w.y || offsets10w.z != lastDisplayed10w.z) {
            needsRedraw = true;
            lastDisplayed10w = offsets10w;
        }
        if (offsets2040wIR.x != lastDisplayed2040w.x || offsets2040wIR.y != lastDisplayed2040w.y || offsets2040wIR.z != lastDisplayed2040w.z) {
            needsRedraw = true;
            lastDisplayed2040w = offsets2040wIR;
        }
        if (endZLift != lastDisplayedEndZLift) {
            needsRedraw = true;
            lastDisplayedEndZLift = endZLift;
        }

        if (needsRedraw || firstRun) {
            ClearConsole();
            firstRun = false;

            PrintHeader("SNAPMAKER STATUS");
            std::cout << "The appropriate manual G-code command may yield more accurate information than some values displayed below." << std::endl;
            if (status.isLaser()) {
                std::cout << "IMPORTANT: Complete the LightBurn Z-calibration [Z] for each laser prior to performing X/Y calibration [O]." << std::endl;
                std::cout << "Refer to the LightBurn Guide for detailed instructions." << std::endl;
            }
            PrintHeader("");

            bool isOffline = IsMachineOffline(status);

            if (isOffline) {
                std::cout << "                                   [ MACHINE OFFLINE ]" << std::endl;
                std::cout << std::endl;
                UserConfig configCopy = HostContext::instance().snapshotConfig();
                std::cout << "  Unable to connect to Snapmaker at: " << configCopy.ipAddress << std::endl;
                std::cout << std::endl;
                std::cout << "  Possible causes:" << std::endl;
                std::cout << "  * Machine is powered off" << std::endl;
                std::cout << "  * Network connection issues" << std::endl;
                std::cout << "  * Incorrect IP address in config" << std::endl;
            }
            else {
                std::cout << "  Status: " << status.getStatusDisplay();
                if (status.isBusy()) {
                    std::cout << " (Busy)";
                }
                std::cout << "    Homed: " << (status.homed ? "Yes" : "No") << std::endl;
                std::cout << std::endl;

                std::cout << "  Position:" << std::endl;
                std::cout << "    X: " << std::setw(8) << std::fixed << std::setprecision(1) << status.x << " mm";
                std::cout << "    Y: " << std::setw(8) << std::fixed << std::setprecision(1) << status.y << " mm";
                std::cout << "    Z: " << std::setw(8) << std::fixed << std::setprecision(1) << status.z << " mm" << std::endl;
                std::cout << std::endl;

                std::cout << "  Tool Head: " << status.getToolHeadDisplay() << std::endl;

                if (status.isLaser()) {
                    std::cout << "    Laser Power: " << status.laserPower << " %" << std::endl;
                    std::cout << "    Focal Length: " << status.laserFocalLength << " mm" << std::endl;
                    std::cout << "    Laser Camera: " << (status.laserCamera ? "Yes" : "No") << std::endl;
                }

                if (status.is3DPrinter()) {
                    std::cout << "    Nozzle 1: " << status.nozzleTemperature1 << "C / "
                        << status.nozzleTargetTemperature1 << "C" << std::endl;
                    std::cout << "    Nozzle 2: " << status.nozzleTemperature2 << "C / "
                        << status.nozzleTargetTemperature2 << "C" << std::endl;
                    std::cout << "    Bed: " << status.heatedBedTemperature << "C / "
                        << status.heatedBedTargetTemperature << "C" << std::endl;
                    std::cout << "    Filament: " << (status.isFilamentOut ? "Out" : "Loaded") << std::endl;
                    std::cout << "    Print Status: " << status.printStatus << std::endl;
                }

                if (status.isCNC()) {
                    std::cout << "    Spindle Speed: " << status.spindleSpeed << " RPM" << std::endl;
                    std::cout << "    Work Speed: " << status.workSpeed << " mm/min" << std::endl;
                }
                std::cout << std::endl;

                if (zHeightCalculated && status.isLaser()) {
                    std::cout << "  LightBurn Z Focal Calculations:" << std::endl;
                    std::cout << "    Static Buffer Z (Homed Position): " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                    std::cout << "    Laser Focal Length: " << std::fixed << std::setprecision(1) << status.laserFocalLength << " mm" << std::endl;
                    std::cout << "    Z Focus Height: " << std::fixed << std::setprecision(1) << zFocusHeight << " mm" << std::endl;
                    std::cout << std::endl;

                    if (status.laserFocalLength > 0) {
                        double lightburnZ_10w = staticBufferZ - status.laserFocalLength;
                        std::cout << "    LightBurn Z Focal (10W Laser): " << std::fixed << std::setprecision(1) << lightburnZ_10w << " mm" << std::endl;
                        std::cout << "    Formula ([Static Buffer Z] - Laser Focal Length): " << std::fixed << std::setprecision(1) << staticBufferZ << " - " << std::fixed << std::setprecision(1) << status.laserFocalLength << std::endl;
                        std::cout << std::endl;

                        double lightburnZ_2040wIR = staticBufferZ - zFocusHeight;
                        std::cout << "    LightBurn Z Focal (20W/40W/IR Laser): " << std::fixed << std::setprecision(1) << lightburnZ_2040wIR << " mm" << std::endl;
                        std::cout << "    Formula ([Static Buffer Z] - Z Focus Height): " << std::fixed << std::setprecision(1) << staticBufferZ << " - " << std::fixed << std::setprecision(1) << zFocusHeight << std::endl;

                        std::cout << std::endl;
                        std::cout << "    Sample LightBurn G-code files generated in 'G-code' folder:" << std::endl;
                        std::cout << "      - Start G-code Sample - 10W Laser.txt (uses Laser Focal Length)" << std::endl;
                        std::cout << "      - Start G-code Sample - 20W, 40W and IR Lasers.txt (uses Z Focus Height)" << std::endl;
                        std::cout << "      - End G-code Sample - All Lasers.txt" << std::endl;
                        std::cout << std::endl;
                    }
                }

                if (status.isLaser() && (offsets10w.valid || offsets2040wIR.valid || (fs::exists(endFile) && endZLift != 0.0))) {
                    std::cout << "    LightBurn X/Y/Z offsets (front-left corner origin) loaded from sample G-code file(s):" << std::endl;
                }
                if (offsets10w.valid && status.isLaser()) {
                    std::cout << "    LightBurn X/Y/Z (10W Laser):    X=" << std::fixed << std::setprecision(1) << offsets10w.x
                        << " mm, Y=" << offsets10w.y << " mm, Z=" << offsets10w.z << " mm" << std::endl;
                }
                if (offsets2040wIR.valid && status.isLaser()) {
                    std::cout << "    LightBurn X/Y/Z (20W/40W/IR):   X=" << std::fixed << std::setprecision(1) << offsets2040wIR.x
                        << " mm, Y=" << offsets2040wIR.y << " mm, Z=" << offsets2040wIR.z << " mm" << std::endl;
                }
                if (fs::exists(endFile) && endZLift != 0.0 && status.isLaser()) {
                    std::cout << "    LightBurn Z Lift (All Lasers): " << std::fixed << std::setprecision(1) << endZLift
                        << " mm (tool lift after job)" << std::endl;
                }
                if (status.isLaser() && (offsets10w.valid || offsets2040wIR.valid || (fs::exists(endFile) && endZLift != 0.0))) {
                    std::cout << std::endl;
                }

                std::cout << "  Work Offsets:" << std::endl;
                std::cout << "    X: " << std::setw(8) << std::fixed << std::setprecision(1) << status.offsetX << " mm";
                std::cout << "    Y: " << std::setw(8) << std::fixed << std::setprecision(1) << status.offsetY << " mm";
                std::cout << "    Z: " << std::setw(8) << std::fixed << std::setprecision(1) << status.offsetZ << " mm" << std::endl;
                std::cout << std::endl;

                std::cout << "  Modules:" << std::endl;
                std::cout << "    Enclosure: " << (status.enclosure ? "Yes" : "No") << std::endl;
                std::cout << "    Rotary: " << (status.rotaryModule ? "Yes" : "No") << std::endl;
                std::cout << "    E-Stop: " << (status.emergencyStopButton ? "Yes" : "No") << std::endl;
                std::cout << "    Air Purifier: " << (status.airPurifier ? "Yes" : "No") << std::endl;
                std::cout << "    Door Open: " << (status.isEnclosureDoorOpen ? "Yes" : "No") << std::endl;
                std::cout << "    Door Count: " << status.doorSwitchCount << std::endl;
            }

            PrintHeader("");

            std::cout << "Controls: [M] Main Menu  |  [ENTER] Refresh Machine Status Display  |  [ESC] Exit";

            std::cout << std::endl;

            if (status.isLaser() && !homingAttempted) {
                std::cout << "          [Z] Machine Setup for LightBurn Z Focal Calculations";
            }
            else if (status.isLaser() && homingAttempted && !zHeightCalculated) {
                std::cout << "          [Z] Run LightBurn Z Focal Calculations";
            }
            else if (status.isLaser() && zHeightCalculated) {
                std::cout << "          [Z] Re-run LightBurn Z Focal Calculations";
            }
            if (status.isLaser()) {
                std::cout << "  |  [O] LightBurn X/Y Offset Calibration" << std::endl << "          [F] Manage G-code Files  |  [B] Backup Management  |  [G] Open LightBurn Guide in Browser";
            }
            std::cout << std::endl;
        }

        if (_kbhit()) {
            int statusKey = _getch();

            if (statusKey == ENTER_ASCII) {
                if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                needsRedraw = true;
                continue;
            }
            else if ((statusKey == 'Z' || statusKey == 'z') && status.isLaser()) {
                try {
                    if (!homingAttempted) {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Starting Z-Height calculation for laser module..." << std::endl;
                        std::cout << GetTimeStamp() << "[NOTICE] Continuing will home machine and switch to machine coordinates." << std::endl;
                        std::cout << GetTimeStamp() << "Ready to proceed? (y/N): " << std::flush;
                        int proceed = _getch();
                        std::cout << std::endl;

                        if (proceed != 'y' && proceed != 'Y') {
                            std::cout << GetTimeStamp() << "[INFO] LightBurn Z Focal setup cancelled." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        std::cout << GetTimeStamp() << "Homing machine to capture homed Z (G28)..." << std::endl;
                        CURLcode homingResult = SendGCodeWithRetry("G28", HOMING_RETRY_ATTEMPTS, true);

                        if (homingResult != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Homing failed." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        std::cout << GetTimeStamp() << "[OK] Homing completed to capture homed Z." << std::endl;

                        std::cout << GetTimeStamp() << "Switching to machine coordinates (G53)..." << std::endl;
                        CURLcode machineCoordsResult = SendGCodeWithRetry("G53", MAX_RETRY_ATTEMPTS, false);

                        if (machineCoordsResult != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Failed to switch to machine coordinates." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        std::cout << GetTimeStamp() << "[OK] Switched to machine coordinates." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

                        MachineStatusData homedStatus = GetMachineStatus(false);
                        staticBufferZ = homedStatus.z;
                        std::cout << GetTimeStamp() << "Homed Z position captured: " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "[VERIFY] The Z value should match machine coordinates shown on touchscreen interface." << std::endl;
                        std::cout << GetTimeStamp() << "[ACTION] If Z value do not match, exit status page and re-execute LightBurn Z Focal machine setup procedure." << std::endl;
                        homingAttempted = true;

                        std::cout << GetTimeStamp() << "[OK] Completed LightBurn Z Focal setup!" << std::endl;
                        std::cout << GetTimeStamp() << "Press [Z] again to run LightBurn Z Focal calculations." << std::endl;
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();
                        continue;
                    }
                    else {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Running LightBurn Z Focal calculations..." << std::endl;

                        if (staticBufferZ == 0.0) {
                            MachineStatusData currentStatus = GetMachineStatus(false);
                            staticBufferZ = currentStatus.z;
                            std::cout << GetTimeStamp() << "Homed position captured: " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                        }

                        std::cout << GetTimeStamp() << "Set the focus bar directly on the bed." << std::endl;
                        std::cout << GetTimeStamp() << "[INFO] This step can be skipped for the 10W Laser." << std::endl;
                        std::cout << GetTimeStamp() << "[INFO] The touchscreen will disconnect momentarily." << std::endl;
                        std::cout << GetTimeStamp() << "Press ENTER when ready to capture Z Focus Height (or ESC to skip)..." << std::endl;

                        int keyPress = _getch();
                        if (keyPress == ENTER_ASCII) {
                            std::cout << GetTimeStamp() << "Switching to machine coordinates to capture Z (G53)..." << std::endl;
                            CURLcode machineCoordsResult = SendGCodeWithRetry("G53", MAX_RETRY_ATTEMPTS, false);

                            if (machineCoordsResult != CURLE_OK) {
                                std::cout << GetTimeStamp() << "[ERR] Failed to switch to machine coordinates." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                                continue;
                            }

                            std::cout << GetTimeStamp() << "[OK] Switched to machine coordinates." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

                            std::cout << GetTimeStamp() << "Refreshing machine status to capture Z Focus Height..." << std::endl;
                            MachineStatusData bedStatus = GetMachineStatus(false);
                            zFocusHeight = bedStatus.z;

                            std::cout << GetTimeStamp() << "Z Focus Height captured: " << std::fixed << std::setprecision(1) << zFocusHeight << " mm" << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Z Focus Height not captured. Using Z Focus Height = 0 mm" << std::endl;
                            zFocusHeight = 0.0;
                        }

                        zHeightCalculated = true;

                        MachineStatusData currentStatus = GetMachineStatus(false);
                        double laserFocalLength = currentStatus.laserFocalLength;

                        double lightburnZ_10w = staticBufferZ - laserFocalLength;
                        double lightburnZ_2040wIR = staticBufferZ - zFocusHeight;

                        std::cout << GetTimeStamp();
                        PrintHeader("", 40);
                        std::cout << GetTimeStamp() << "LightBurn Z Focal Results:" << std::endl;
                        std::cout << GetTimeStamp();
                        PrintHeader("", 40);
                        std::cout << GetTimeStamp() << "Static Buffer Z (Homed Position): " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "Laser Focal Length: " << std::fixed << std::setprecision(1) << laserFocalLength << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "Z Focus Height: " << std::fixed << std::setprecision(1) << zFocusHeight << " mm" << std::endl;
                        std::cout << GetTimeStamp();
                        PrintHeader("", 40, '-');
                        std::cout << GetTimeStamp() << "LightBurn Z Focal (10W Laser): " << std::fixed << std::setprecision(1) << lightburnZ_10w << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "  Formula: " << std::fixed << std::setprecision(1) << staticBufferZ << " - " << std::fixed << std::setprecision(1) << laserFocalLength << std::endl;
                        std::cout << GetTimeStamp() << "LightBurn Z Focal (20W/40W/IR Laser): " << std::fixed << std::setprecision(1) << lightburnZ_2040wIR << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "  Formula: " << std::fixed << std::setprecision(1) << staticBufferZ << " - " << std::fixed << std::setprecision(1) << zFocusHeight << std::endl;
                        std::cout << GetTimeStamp();
                        PrintHeader("", 40);

                        auto createBackupWithTimestamp = [](const fs::path& filepath) -> fs::path {
                            if (!fs::exists(filepath)) return fs::path();
                            auto now = std::chrono::system_clock::now();
                            auto tt = std::chrono::system_clock::to_time_t(now);
                            tm tmBuf{};
                            localtime_s(&tmBuf, &tt);
                            std::stringstream ss;
                            ss << std::put_time(&tmBuf, "_%Y%m%d_%H%M%S");
                            fs::path backupPath = filepath;
                            backupPath.replace_extension(".txt" + ss.str() + ".old");
                            fs::copy(filepath, backupPath, fs::copy_options::overwrite_existing);
                            std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;
                            return backupPath;
                            };

                        RegenerateGCodeFiles(staticBufferZ, laserFocalLength, zFocusHeight);

                        std::cout << GetTimeStamp() << "[NOTE] All three sample G-code files have been generated or updated and a backup created." << std::endl;
                        std::cout << GetTimeStamp() << "[NOTE] Select the file that matches your laser module:" << std::endl;
                        std::cout << GetTimeStamp() << "       - 10W Laser        -> 'Start G-code Sample - 10W Laser.txt'" << std::endl;
                        std::cout << GetTimeStamp() << "       - 20W/40W/IR Laser -> 'Start G-code Sample - 20W, 40W and IR Lasers.txt'" << std::endl;
                        std::cout << GetTimeStamp() << "       - End G-code       -> 'End G-code Sample - All Lasers.txt' (applies to all)" << std::endl;
                        std::cout << GetTimeStamp() << "Do you want to open the updated G-code files in Notepad? (Y/n): ";
                        std::cout.flush();

                        int openFiles = 0;
                        try {
                            openFiles = _getch();
                        }
                        catch (const std::exception& e) {
                            std::cerr << GetTimeStamp() << "[ERR] Failed to read input: " << e.what() << std::endl;
                            openFiles = 'n';
                        }
                        std::cout << std::endl;

                        if (openFiles != 'n' && openFiles != 'N') {
                            std::cout << GetTimeStamp() << "[NOTE] Opening all three files - use the one matching your laser module." << std::endl;
                            std::cout.flush();

                            try {
                                if (fs::exists(file10w)) {
                                    INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", file10w.string().c_str(), NULL, SW_SHOWNORMAL);
                                    if (result <= 32) {
                                        std::cerr << GetTimeStamp() << "[WARN] Failed to open " << file10w.filename().string() << " (error: " << result << ")" << std::endl;
                                    }
                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                }
                            }
                            catch (const std::exception& e) {
                                std::cerr << GetTimeStamp() << "[ERR] Failed to open 10W Laser file: " << e.what() << std::endl;
                            }

                            try {
                                if (fs::exists(file2040wIR)) {
                                    INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", file2040wIR.string().c_str(), NULL, SW_SHOWNORMAL);
                                    if (result <= 32) {
                                        std::cerr << GetTimeStamp() << "[WARN] Failed to open " << file2040wIR.filename().string() << " (error: " << result << ")" << std::endl;
                                    }
                                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                }
                            }
                            catch (const std::exception& e) {
                                std::cerr << GetTimeStamp() << "[ERR] Failed to open 20W/40W/IR Laser file: " << e.what() << std::endl;
                            }

                            try {
                                if (fs::exists(endFile)) {
                                    INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", endFile.string().c_str(), NULL, SW_SHOWNORMAL);
                                    if (result <= 32) {
                                        std::cerr << GetTimeStamp() << "[WARN] Failed to open " << endFile.filename().string() << " (error: " << result << ")" << std::endl;
                                    }
                                }
                            }
                            catch (const std::exception& e) {
                                std::cerr << GetTimeStamp() << "[ERR] Failed to open End G-code file: " << e.what() << std::endl;
                            }
                        }

                        CURLcode workCoordsResult = SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false);
                        if (workCoordsResult != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Failed to restore to work coordinates." << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[OK] Restored work coordinates (G54)." << std::endl;
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

                        std::cout << GetTimeStamp() << "Do you want to return the laser head to home (G28)? (Y/n): ";
                        int returnHome = _getch();
                        std::cout << std::endl;

                        if (returnHome == 'y' || returnHome == 'Y' || returnHome == ENTER_ASCII) {
                            HomeMachine();
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Skipping return to home position." << std::endl;
                        }

                        std::cout << GetTimeStamp() << "[OK] Calculations complete!" << std::endl;
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();

                        if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                        if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                        if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                        needsRedraw = true;
                        continue;
                    }
                }
                catch (const std::exception& e) {
                    std::cout << GetTimeStamp() << "[ERR] Z-calibration error: " << e.what() << std::endl;
                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                    (void)_getch();
                    continue;
                }
                catch (...) {
                    std::cout << GetTimeStamp() << "[ERR] Z-calibration unknown error" << std::endl;
                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                    (void)_getch();
                    continue;
                }
            }
            else if ((statusKey == 'O' || statusKey == 'o') && status.isLaser()) {
                try {
                    if (!fs::exists(file10w) || !fs::exists(file2040wIR)) {
                        std::cout << std::endl << GetTimeStamp() << "[ERR] Start G-code files not found. Please run Z Focal Calculations first." << std::endl;
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();
                        continue;
                    }

                    const double DEFAULT_X = 0.0;
                    const double DEFAULT_Y = 0.0;

                    auto createBackupForFile = [](const fs::path& filepath) -> fs::path {
                        if (!fs::exists(filepath)) return fs::path();
                        auto now = std::chrono::system_clock::now();
                        auto tt = std::chrono::system_clock::to_time_t(now);
                        tm tmBuf{};
                        localtime_s(&tmBuf, &tt);
                        std::stringstream ss;
                        ss << std::put_time(&tmBuf, "_%Y%m%d_%H%M%S");
                        fs::path backupPath = filepath;
                        backupPath.replace_extension(".txt" + ss.str() + ".old");
                        fs::copy(filepath, backupPath, fs::copy_options::overwrite_existing);
                        std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;
                        return backupPath;
                        };

                    std::cout << std::endl;
                    PrintHeader("", 40);
                    PrintHeader("LIGHTBURN X/Y OFFSET CALIBRATION", 40);
                    PrintHeader("", 40);
                    std::cout << std::endl;

                    std::cout << "Select calibration method:" << std::endl;
                    std::cout << "  1. Auto-calibrate from current machine position (get X/Y values)" << std::endl;
                    std::cout << "  2. Fine-tune after test run (measure 20mm square)" << std::endl;
                    std::cout << "  3. Enter custom X/Y coordinates (manual input)" << std::endl;
                    std::cout << "  4. Reset files to defaults (X=0, Y=0)" << std::endl;
                    std::cout << "  ESC. Cancel" << std::endl;
                    std::cout << "Enter choice: ";

                    int choice = _getch();
                    std::cout << std::endl;
                    if (choice == ESC_ASCII) continue;

                    if (choice == '1') {
                        std::cout << std::endl;
                        PrintHeader("", 40);
                        PrintHeader("AUTO CALIBRATION MODE", 40);
                        PrintHeader("", 40);
                        std::cout << std::endl;

                        std::cout << "Select laser to calibrate:" << std::endl;
                        std::cout << "  1. 10W Laser (file: " << file10w.filename().string() << ")" << std::endl;
                        std::cout << "  2. 20W/40W/IR Laser (file: " << file2040wIR.filename().string() << ")" << std::endl;
                        std::cout << "  ESC. Cancel" << std::endl;
                        std::cout << "Enter choice: ";

                        int laserChoice = _getch();
                        std::cout << std::endl;
                        if (laserChoice == ESC_ASCII) continue;

                        fs::path targetFile;
                        LaserOffsets* targetOffsets = nullptr;
                        std::string laserName;

                        if (laserChoice == '1') {
                            targetFile = file10w;
                            targetOffsets = &offsets10w;
                            laserName = "10W Laser";
                        }
                        else if (laserChoice == '2') {
                            targetFile = file2040wIR;
                            targetOffsets = &offsets2040wIR;
                            laserName = "20W/40W/IR Laser";
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            continue;
                        }

                        createBackupForFile(targetFile);

                        G92Offsets currentOffsets = ParseG92Line(targetFile.string());
                        double currentZ = currentOffsets.found ? currentOffsets.z : 0.0;

                        if (!xyCalibrated) {
                            std::cout << GetTimeStamp() << "[INFO] Performing machine setup to capture static position..." << std::endl;
                            PerformMachineSetup();

                            std::cout << GetTimeStamp() << "Switching to machine coordinates (G53) to capture static position..." << std::endl;
                            CURLcode machineCoordsResult = SendGCodeWithRetry("G53", MAX_RETRY_ATTEMPTS, false);

                            if (machineCoordsResult != CURLE_OK) {
                                std::cout << GetTimeStamp() << "[ERR] Failed to switch to machine coordinates." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                                continue;
                            }

                            std::cout << GetTimeStamp() << "[OK] Switched to machine coordinates (G53)." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

                            std::cout << GetTimeStamp() << "Reading static machine position..." << std::endl;
                            MachineStatusData staticStatus = GetMachineStatus(false);

                            CURLcode workCoordsResult = SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false);
                            if (workCoordsResult != CURLE_OK) {
                                std::cout << GetTimeStamp() << "[ERR] Failed to restore work coordinates." << std::endl;
                            }
                            else {
                                std::cout << GetTimeStamp() << "[OK] Restored work coordinates (G54)." << std::endl;
                            }

                            if (IsMachineOffline(staticStatus)) {
                                std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot capture static position." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                                continue;
                            }

                            staticBufferX = staticStatus.x;
                            staticBufferY = staticStatus.y;
                            staticBufferZ = staticStatus.z;
                            xyCalibrated = true;

                            std::cout << GetTimeStamp() << "Static position captured:" << std::endl;
                            std::cout << "  X = " << std::fixed << std::setprecision(1) << staticBufferX << " mm" << std::endl;
                            std::cout << "  Y = " << std::fixed << std::setprecision(1) << staticBufferY << " mm" << std::endl;
                            std::cout << "  Z = " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                            std::cout << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Using previously captured static position:" << std::endl;
                            std::cout << "  X = " << std::fixed << std::setprecision(1) << staticBufferX << " mm" << std::endl;
                            std::cout << "  Y = " << std::fixed << std::setprecision(1) << staticBufferY << " mm" << std::endl;
                            std::cout << "  Z = " << std::fixed << std::setprecision(1) << staticBufferZ << " mm" << std::endl;
                            std::cout << std::endl;
                        }

                        std::cout << GetTimeStamp() << "Current Z offset from G-code file:" << std::endl;
                        std::cout << "  G92 Z from file: " << std::fixed << std::setprecision(1) << currentZ << " mm" << std::endl;
                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "[INFO] This Z value will be used as the focus position in LightBurn." << std::endl;
                        std::cout << GetTimeStamp() << "[INFO] To change this value, edit the G92 line in the start G-code file." << std::endl;
                        std::cout << std::endl;

                        std::cout << GetTimeStamp() << "Do you want to move the laser to X0 Y0? (Y/n): ";
                        int moveToOrigin = _getch();
                        std::cout << std::endl;

                        if (moveToOrigin == 'y' || moveToOrigin == 'Y' || moveToOrigin == ENTER_ASCII) {
                            std::string gcodeXY = "G0 X0 Y0";
                            if (SendGCodeWithRetry(gcodeXY, MAX_RETRY_ATTEMPTS, false) != CURLE_OK) {
                                std::cout << GetTimeStamp() << "[WARN] Failed to move to X0 Y0." << std::endl;
                            }
                            else {
                                std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));
                                std::cout << GetTimeStamp() << "[OK] Moved to X0 Y0" << std::endl;
                            }
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Skipping X0 Y0 move." << std::endl;
                        }

                        G92Offsets latestOffsets = ParseG92Line(targetFile.string());
                        if (latestOffsets.found) {
                            currentZ = latestOffsets.z;
                        }

                        double focusZPosition;
                        if (laserChoice == '1') {
                            MachineStatusData currentMachineStatus = GetMachineStatus(false);
                            double machineFocalZLength = currentMachineStatus.laserFocalLength;
                            double fileZ = latestOffsets.found ? latestOffsets.z : 0.0;

                            focusZPosition = machineFocalZLength;

                            if (std::abs(machineFocalZLength - fileZ) > 0.1) {
                                std::cout << GetTimeStamp() << "[WARNING] Focal length mismatch detected!" << std::endl;
                                std::cout << "  Machine reported Z focal length: " << std::fixed << std::setprecision(1) << machineFocalZLength << " mm" << std::endl;
                                std::cout << "  Z value in G-code file: " << std::fixed << std::setprecision(1) << fileZ << " mm" << std::endl;
                                std::cout << std::endl;
                                std::cout << "Which value do you want to use for focus Z position?" << std::endl;
                                std::cout << "[NOTE] This will not modify the G-code file." << std::endl;
                                std::cout << "  1. Machine Z value (" << machineFocalZLength << " mm)" << std::endl;
                                std::cout << "  2. G-code file Z value (" << fileZ << " mm)" << std::endl;
                                std::cout << "  ESC. Cancel calibration" << std::endl;
                                std::cout << "Enter choice: ";

                                int choice = _getch();
                                std::cout << std::endl;

                                if (choice == ESC_ASCII) {
                                    std::cout << GetTimeStamp() << "[INFO] Calibration cancelled." << std::endl;
                                    continue;
                                }
                                else if (choice == '2') {
                                    focusZPosition = fileZ;
                                    std::cout << GetTimeStamp() << "[INFO] Using file value: " << focusZPosition << " mm" << std::endl;
                                }
                                else {
                                    std::cout << GetTimeStamp() << "[INFO] Using machine value: " << focusZPosition << " mm" << std::endl;
                                }
                            }
                            else {
                                std::cout << GetTimeStamp() << "Focus Z position from machine: " << focusZPosition << " mm" << std::endl;
                                std::cout << std::endl;
                            }
                        }
                        else {
                            focusZPosition = currentZ;
                            std::cout << GetTimeStamp() << "Z focus position (20W/40W/IR Laser):" << std::endl;
                            std::cout << "  Focus Z position from file: " << std::fixed << std::setprecision(1) << focusZPosition << " mm" << std::endl;
                        }
                        std::cout << std::endl;

                        std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));
                        std::cout << GetTimeStamp() << "Do you want to move the laser to the focus Z position (" << std::fixed << std::setprecision(1) << focusZPosition << " mm)? (Y/n): ";
                        int moveFocusZ = _getch();
                        std::cout << std::endl;

                        if (moveFocusZ == 'y' || moveFocusZ == 'Y' || moveFocusZ == ENTER_ASCII) {
                            std::string gcodeZ = "G0 Z" + std::to_string(static_cast<int>(focusZPosition));
                            std::cout << GetTimeStamp() << "Moving to Z" << std::fixed << std::setprecision(1) << focusZPosition << " mm" << std::endl;

                            if (SendGCodeWithRetry("G53", MAX_RETRY_ATTEMPTS, false) == CURLE_OK) {
                                std::cout << GetTimeStamp() << "[OK] Switched to machine coordinates (G53)." << std::endl;
                                if (SendGCodeWithRetry(gcodeZ, MAX_RETRY_ATTEMPTS, false) == CURLE_OK) {
                                    std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));
                                    std::cout << GetTimeStamp() << "[OK] Moved to target position." << std::endl;
                                }
                                else {
                                    std::cout << GetTimeStamp() << "[WARN] Failed to move Z." << std::endl;
                                }

                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                if (SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false) == CURLE_OK) {
                                    std::cout << GetTimeStamp() << "[OK] Restored work coordinates (G54)." << std::endl;
                                }
                                else {
                                    std::cout << GetTimeStamp() << "[WARN] Failed to restore work coordinates." << std::endl;
                                }
                            }
                            else {
                                std::cout << GetTimeStamp() << "[WARN] Failed to switch to machine coordinates." << std::endl;
                            }
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Skipping Z move." << std::endl;
                        }

                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "[INSTRUCTIONS]" << std::endl;
                        std::cout << "  1. Use the Snapmaker touchscreen to move the laser head." << std::endl;
                        std::cout << "  2. Position the laser 'spot' just beyond the front-left corner (where X=0, Y=0 should be)." << std::endl;
                        std::cout << "  3. The laser can be up to 20mm off the bed (within the -20mm X/Y range)." << std::endl;
                        std::cout << "  4. The touchscreen will disconnect momentarily." << std::endl;
                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "Press ENTER when the laser is in position (or ESC to cancel): ";

                        bool cancelled = false;
                        while (true) {
                            if (!IsRunning()) {
                                cancelled = true;
                                break;
                            }
                            if (_kbhit()) {
                                int ch = _getch();
                                if (ch == ENTER_ASCII) {
                                    break;
                                }
                                else if (ch == ESC_ASCII) {
                                    cancelled = true;
                                    break;
                                }
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }

                        if (cancelled) {
                            std::cout << std::endl << GetTimeStamp() << "[INFO] Calibration cancelled by user." << std::endl;
                            continue;
                        }

                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "Switching to machine coordinates to capture X/Y (G53)..." << std::endl;

                        CURLcode machineCoordsResult = SendGCodeWithRetry("G53", MAX_RETRY_ATTEMPTS, false);

                        if (machineCoordsResult != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Failed to switch to machine coordinates." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        std::cout << GetTimeStamp() << "[OK] Switched to machine coordinates (G53)." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

                        std::cout << GetTimeStamp() << "Reading current machine position..." << std::endl;

                        MachineStatusData currentStatus = GetMachineStatus(false);

                        CURLcode workCoordsResult = SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false);
                        if (workCoordsResult != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Failed to restore work coordinates." << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[OK] Restored work coordinates (G54)." << std::endl;
                        }

                        if (IsMachineOffline(currentStatus)) {
                            std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot read position." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        double capturedX = currentStatus.x;
                        double capturedY = currentStatus.y;

                        std::cout << GetTimeStamp() << "Machine coordinates captured:" << std::endl;
                        std::cout << "  X = " << std::fixed << std::setprecision(1) << capturedX << " mm" << std::endl;
                        std::cout << "  Y = " << std::fixed << std::setprecision(1) << capturedY << " mm" << std::endl;
                        std::cout << "  Static X = " << std::fixed << std::setprecision(1) << staticBufferX << " mm" << std::endl;
                        std::cout << "  Static Y = " << std::fixed << std::setprecision(1) << staticBufferY << " mm" << std::endl;
                        std::cout << std::endl;

                        double newX = -(capturedX - staticBufferX);
                        double newY = -(capturedY - staticBufferY);

                        newX = std::round(newX * 10.0) / 10.0;
                        newY = std::round(newY * 10.0) / 10.0;

                        std::cout << GetTimeStamp() << "Calculated G92 X/Y offsets:" << std::endl;
                        std::cout << "  Formula: newX = -(capturedX - staticBufferX)" << std::endl;
                        std::cout << "  newX = -(" << capturedX << " - " << staticBufferX << ") = " << newX << " mm" << std::endl;
                        std::cout << "  newY = -(" << capturedY << " - " << staticBufferY << ") = " << newY << " mm" << std::endl;
                        std::cout << std::endl;

                        if (UpdateG92XYInFile(targetFile.string(), newX, newY)) {
                            std::cout << GetTimeStamp() << "[OK] G92 X/Y updated in " << targetFile.filename().string() << std::endl;
                            targetOffsets->x = newX;
                            targetOffsets->y = newY;
                            targetOffsets->valid = true;
                            std::cout << std::endl;
                            PrintHeader("", 40);
                            PrintHeader("AUTO CALIBRATION COMPLETE", 40);
                            PrintHeader("", 40);
                            std::cout << "The G92 X/Y offsets have been updated." << std::endl;
                            std::cout << "X = " << newX << " mm, Y = " << newY << " mm" << std::endl;
                            std::cout << "Z = " << currentZ << " mm (preserved)" << std::endl;
                            std::cout << std::endl;
                            std::cout << "This position will now be treated as the origin (X=0, Y=0)" << std::endl;
                            std::cout << "in LightBurn when using the Start G-code file." << std::endl;
                            std::cout << std::endl;
                            std::cout << GetTimeStamp() << "[INFO] You can now run the 20mm square test to fine-tune." << std::endl;
                            std::cout << GetTimeStamp() << "[INFO] Follow the guide at:" << std::endl;
                            std::cout << "  https://forum.snapmaker.com/t/1064nm-ir-20-40w-lightburn-guide/35954" << std::endl;

                            std::cout << GetTimeStamp() << "Do you want to open the updated file in Notepad? (Y/n): ";
                            std::cout.flush();
                            int openFile = _getch();
                            if (openFile != 'n' && openFile != 'N') {
                                std::cout << GetTimeStamp() << "[NOTE] Opening file in Notepad..." << std::endl;
                                std::cout.flush();

                                try {
                                    if (fs::exists(targetFile)) {
                                        INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", targetFile.string().c_str(), NULL, SW_SHOWNORMAL);
                                        if (result <= 32) {
                                            std::cerr << GetTimeStamp() << "[WARN] Failed to open " << targetFile.filename().string() << " (error: " << result << ")" << std::endl;
                                        }
                                    }
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                }
                            }

                            std::cout << std::endl;
                            std::cout << GetTimeStamp() << "Do you want to return the laser head home (G28)? (Y/n): ";
                            int returnHome = _getch();
                            std::cout << std::endl;

                            if (returnHome == 'y' || returnHome == 'Y' || returnHome == ENTER_ASCII) {
                                HomeMachine();
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Skipping return to home position." << std::endl;
                            }

                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();

                            if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                            if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                            if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                            needsRedraw = true;
                            continue;
                        }
                    }
                    else if (choice == '2') {
                        std::cout << std::endl;
                        PrintHeader("", 40);
                        PrintHeader("FINE-TUNE X/Y OFFSETS", 40);
                        PrintHeader("", 40);
                        std::cout << std::endl;

                        std::cout << "Select laser to fine-tune:" << std::endl;
                        std::cout << "  1. 10W Laser (file: " << file10w.filename().string() << ")" << std::endl;
                        std::cout << "  2. 20W/40W/IR Laser (file: " << file2040wIR.filename().string() << ")" << std::endl;
                        std::cout << "  ESC. Cancel" << std::endl;
                        std::cout << "Enter choice: ";

                        int laserChoice = _getch();
                        std::cout << std::endl;
                        if (laserChoice == ESC_ASCII) continue;

                        fs::path targetFile;
                        LaserOffsets* targetOffsets = nullptr;

                        if (laserChoice == '1') {
                            targetFile = file10w;
                            targetOffsets = &offsets10w;
                        }
                        else if (laserChoice == '2') {
                            targetFile = file2040wIR;
                            targetOffsets = &offsets2040wIR;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            continue;
                        }

                        auto now = std::chrono::system_clock::now();
                        auto tt = std::chrono::system_clock::to_time_t(now);
                        tm tmBuf{};
                        localtime_s(&tmBuf, &tt);
                        std::stringstream ss;
                        ss << std::put_time(&tmBuf, "_%Y%m%d_%H%M%S");
                        fs::path backupPath = targetFile;
                        backupPath.replace_extension(".txt" + ss.str() + ".old");
                        fs::copy(targetFile, backupPath, fs::copy_options::overwrite_existing);
                        std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;

                        G92Offsets currentOffsets = ParseG92Line(targetFile.string());
                        if (!currentOffsets.found) {
                            std::cout << GetTimeStamp() << "[ERR] Could not read current G92 offsets from file." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        double currentX = currentOffsets.x;
                        double currentY = currentOffsets.y;
                        double currentZ = currentOffsets.z;

                        std::cout << GetTimeStamp() << "Current G92 offsets:" << std::endl;
                        std::cout << "  X = " << std::fixed << std::setprecision(1) << currentX << " mm" << std::endl;
                        std::cout << "  Y = " << std::fixed << std::setprecision(1) << currentY << " mm" << std::endl;
                        std::cout << "  Z = " << std::fixed << std::setprecision(1) << currentZ << " mm (preserved)" << std::endl;
                        std::cout << std::endl;

                        std::cout << GetTimeStamp() << "[INSTRUCTIONS]" << std::endl;
                        std::cout << "  1. In LightBurn, create a 10x10 mm square at X=20, Y=20 mm" << std::endl;
                        std::cout << "  2. Set it as a line engrave and run the job on your material" << std::endl;
                        std::cout << "  3. Measure the actual distances from the left and front edges" << std::endl;
                        std::cout << "  4. Enter the measured values below" << std::endl;
                        std::cout << std::endl;

                        auto readDouble = [&](const std::string& prompt, double& out) -> bool {
                            while (true) {
                                if (!IsRunning()) return false;
                                std::cout << GetTimeStamp() << prompt;
                                std::string input;
                                bool escPressed = false;
                                while (true) {
                                    if (!IsRunning()) return false;
                                    int key = _getch();
                                    if (key == ESC_ASCII) {
                                        escPressed = true;
                                        break;
                                    }
                                    else if (key == ENTER_ASCII) {
                                        break;
                                    }
                                    else if (key == 8) {
                                        if (!input.empty()) {
                                            input.pop_back();
                                            std::cout << "\b \b";
                                        }
                                    }
                                    else if (key >= 32 && key <= 126) {
                                        char c = static_cast<char>(key);
                                        if (isdigit(c) || c == '.' || (c == '-' && input.empty())) {
                                            if (c == '.' && input.find('.') != std::string::npos) {
                                                continue;
                                            }
                                            input += c;
                                            std::cout << c;
                                        }
                                    }
                                }
                                std::cout << std::endl;
                                if (escPressed) return false;
                                if (input.empty()) {
                                    std::cout << GetTimeStamp() << "[ERR] Input cannot be empty. Please enter a number (or ESC to cancel)." << std::endl;
                                    continue;
                                }
                                try {
                                    out = std::stod(input);
                                    return true;
                                }
                                catch (...) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid number. Please enter a numeric value." << std::endl;
                                }
                            }
                            };

                        double measuredX, measuredY;
                        if (!readDouble("Enter measured X distance (from left side to square) in mm: ", measuredX)) {
                            std::cout << GetTimeStamp() << "[INFO] Fine-tune cancelled by user." << std::endl;
                            continue;
                        }
                        if (!readDouble("Enter measured Y distance (from front side to square) in mm: ", measuredY)) {
                            std::cout << GetTimeStamp() << "[INFO] Fine-tune cancelled by user." << std::endl;
                            continue;
                        }

                        auto [newX, newY] = CalculateNewXYOffsets(measuredX, measuredY, currentX, currentY);

                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "Calculated new G92 offsets:" << std::endl;
                        std::cout << "  X: " << currentX << " -> " << newX << " mm" << std::endl;
                        std::cout << "  Y: " << currentY << " -> " << newY << " mm" << std::endl;
                        std::cout << "  Z: " << currentZ << " mm (preserved)" << std::endl;
                        std::cout << std::endl;

                        if (UpdateG92XYInFile(targetFile.string(), newX, newY)) {
                            std::cout << GetTimeStamp() << "[OK] G92 X/Y updated in " << targetFile.filename().string() << std::endl;
                            targetOffsets->x = newX;
                            targetOffsets->y = newY;
                            targetOffsets->valid = true;

                            std::cout << std::endl;
                            PrintHeader("", 40);
                            PrintHeader("FINE-TUNE COMPLETE", 40);
                            PrintHeader("", 40);
                            std::cout << "The G92 X/Y offsets have been updated." << std::endl;
                            std::cout << "X = " << newX << " mm, Y = " << newY << " mm" << std::endl;
                            std::cout << std::endl;

                            std::cout << GetTimeStamp() << "Do you want to open the updated file in Notepad? (Y/n): ";
                            std::cout.flush();
                            int openFile = _getch();
                            std::cout << std::endl;
                            if (openFile != 'n' && openFile != 'N') {
                                std::cout << GetTimeStamp() << "[NOTE] Opening file in Notepad..." << std::endl;
                                std::cout.flush();

                                try {
                                    if (fs::exists(targetFile)) {
                                        INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", targetFile.string().c_str(), NULL, SW_SHOWNORMAL);
                                        if (result <= 32) {
                                            std::cerr << GetTimeStamp() << "[WARN] Failed to open " << targetFile.filename().string() << " (error: " << result << ")" << std::endl;
                                        }
                                    }
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                }
                            }

                            std::cout << GetTimeStamp() << "[INFO] Repeat the test to verify or fine-tune further." << std::endl;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[ERR] Failed to update G-code file." << std::endl;
                        }

                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();

                        if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                        if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                        if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                        needsRedraw = true;
                        continue;
                    }
                    else if (choice == '3') {
                        std::cout << "Enter custom X/Y coordinates for the selected laser." << std::endl;
                        std::cout << GetTimeStamp() << "These values are used in the Start G-code G92 command." << std::endl;
                        std::cout << std::endl;

                        std::cout << "Select laser to update:" << std::endl;
                        std::cout << "  1. 10W Laser (file: " << file10w.filename().string() << ")" << std::endl;
                        std::cout << "  2. 20W/40W/IR Laser (file: " << file2040wIR.filename().string() << ")" << std::endl;
                        std::cout << "  ESC. Cancel" << std::endl;
                        std::cout << "Enter choice: ";

                        int laserChoice = _getch();
                        std::cout << std::endl;
                        if (laserChoice == ESC_ASCII) continue;

                        fs::path targetFile;
                        LaserOffsets* targetOffsets = nullptr;

                        if (laserChoice == '1') {
                            targetFile = file10w;
                            targetOffsets = &offsets10w;
                        }
                        else if (laserChoice == '2') {
                            targetFile = file2040wIR;
                            targetOffsets = &offsets2040wIR;
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            continue;
                        }

                        auto now = std::chrono::system_clock::now();
                        auto tt = std::chrono::system_clock::to_time_t(now);
                        tm tmBuf{};
                        localtime_s(&tmBuf, &tt);
                        std::stringstream ss;
                        ss << std::put_time(&tmBuf, "_%Y%m%d_%H%M%S");
                        fs::path backupPath = targetFile;
                        backupPath.replace_extension(".txt" + ss.str() + ".old");
                        fs::copy(targetFile, backupPath, fs::copy_options::overwrite_existing);
                        std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;

                        G92Offsets currentOffsets = ParseG92Line(targetFile.string());
                        double currentZ = currentOffsets.found ? currentOffsets.z : 0.0;

                        double currentX = targetOffsets->valid ? targetOffsets->x : DEFAULT_X;
                        double currentY = targetOffsets->valid ? targetOffsets->y : DEFAULT_Y;

                        std::cout << GetTimeStamp() << "Current X: " << currentX << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "Current Y: " << currentY << " mm" << std::endl;
                        std::cout << GetTimeStamp() << "Current Z: " << currentZ << " mm (will be preserved)" << std::endl;
                        std::cout << std::endl;

                        std::cout << GetTimeStamp() << "Enter new X coordinate (or press Enter to keep " << currentX << "): ";
                        std::string xInput;
                        std::getline(std::cin, xInput);
                        double newX = currentX;
                        if (!xInput.empty()) {
                            try {
                                newX = std::stod(xInput);
                            }
                            catch (...) {
                                std::cout << GetTimeStamp() << "[ERR] Invalid X coordinate. Keeping current value." << std::endl;
                            }
                        }

                        std::cout << GetTimeStamp() << "Enter new Y coordinate (or press Enter to keep " << currentY << "): ";
                        std::string yInput;
                        std::getline(std::cin, yInput);
                        double newY = currentY;
                        if (!yInput.empty()) {
                            try {
                                newY = std::stod(yInput);
                            }
                            catch (...) {
                                std::cout << GetTimeStamp() << "[ERR] Invalid Y coordinate. Keeping current value." << std::endl;
                            }
                        }

                        if (UpdateG92XYInFile(targetFile.string(), newX, newY)) {
                            std::cout << GetTimeStamp() << "[OK] G92 X/Y updated in " << targetFile.filename().string() << std::endl;
                            std::cout << GetTimeStamp() << "  X: " << currentX << " -> " << newX << " mm" << std::endl;
                            std::cout << GetTimeStamp() << "  Y: " << currentY << " -> " << newY << " mm" << std::endl;
                            std::cout << GetTimeStamp() << "  Z: " << currentZ << " mm (preserved)" << std::endl;
                            targetOffsets->x = newX;
                            targetOffsets->y = newY;
                            targetOffsets->valid = true;

                            std::cout << GetTimeStamp() << "Do you want to open the updated file in Notepad? (Y/n): ";
                            std::cout.flush();
                            int openFile = _getch();
                            std::cout << std::endl;
                            if (openFile != 'n' && openFile != 'N') {
                                std::cout << GetTimeStamp() << "[NOTE] Opening file in Notepad..." << std::endl;
                                std::cout.flush();

                                try {
                                    if (fs::exists(targetFile)) {
                                        INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", targetFile.string().c_str(), NULL, SW_SHOWNORMAL);
                                        if (result <= 32) {
                                            std::cerr << GetTimeStamp() << "[WARN] Failed to open " << targetFile.filename().string() << " (error: " << result << ")" << std::endl;
                                        }
                                    }
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                }
                            }
                        }
                        else {
                            std::cout << GetTimeStamp() << "[ERR] Failed to update G-code file." << std::endl;
                        }
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();

                        if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                        if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                        if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                        needsRedraw = true;
                        continue;
                    }
                    else if (choice == '4') {
                        std::cout << "Reset which file(s) to factory default X=0, Y=0?" << std::endl;
                        std::cout << "  1. 10W Laser only" << std::endl;
                        std::cout << "  2. 20W/40W/IR Laser only" << std::endl;
                        std::cout << "  3. Both files" << std::endl;
                        std::cout << "  ESC. Cancel" << std::endl;
                        std::cout << "Enter choice: ";

                        int resetChoice = _getch();
                        std::cout << std::endl;
                        if (resetChoice == ESC_ASCII) continue;

                        auto resetFileToZero = [&](fs::path filepath, LaserOffsets& offsets, const std::string& name) -> bool {
                            auto now = std::chrono::system_clock::now();
                            auto tt = std::chrono::system_clock::to_time_t(now);
                            tm tmBuf{};
                            localtime_s(&tmBuf, &tt);
                            std::stringstream ss;
                            ss << std::put_time(&tmBuf, "_%Y%m%d_%H%M%S");
                            fs::path backupPath = filepath;
                            backupPath.replace_extension(".txt" + ss.str() + ".old");
                            fs::copy(filepath, backupPath, fs::copy_options::overwrite_existing);
                            std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;

                            G92Offsets existing = ParseG92Line(filepath.string());
                            double existingZ = existing.found ? existing.z : 0.0;

                            if (UpdateG92XYInFile(filepath.string(), 0.0, 0.0)) {
                                std::cout << GetTimeStamp() << "[OK] " << name << " reset to X=0, Y=0 (Z preserved: " << existingZ << ")" << std::endl;
                                offsets.x = 0.0;
                                offsets.y = 0.0;
                                offsets.valid = true;

                                std::cout << GetTimeStamp() << "Do you want to open the reset file in Notepad? (Y/n): ";
                                std::cout.flush();
                                int openFile = _getch();
                                std::cout << std::endl;
                                if (openFile != 'n' && openFile != 'N') {
                                    std::cout << GetTimeStamp() << "[NOTE] Opening reset file." << std::endl;
                                    std::cout.flush();

                                    try {
                                        if (fs::exists(filepath)) {
                                            INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", filepath.string().c_str(), NULL, SW_SHOWNORMAL);
                                            if (result <= 32) {
                                                std::cerr << GetTimeStamp() << "[WARN] Failed to open " << filepath.filename().string() << " (error: " << result << ")" << std::endl;
                                            }
                                        }
                                    }
                                    catch (const std::exception& e) {
                                        std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                    }
                                }
                                return true;
                            }
                            else {
                                std::cout << GetTimeStamp() << "[ERR] Failed to reset " << name << std::endl;
                                return false;
                            }
                            };

                        if (resetChoice == '1') {
                            resetFileToZero(file10w, offsets10w, "10W Laser file");
                        }
                        else if (resetChoice == '2') {
                            resetFileToZero(file2040wIR, offsets2040wIR, "20W/40W/IR Laser file");
                        }
                        else if (resetChoice == '3') {
                            resetFileToZero(file10w, offsets10w, "10W Laser file");
                            resetFileToZero(file2040wIR, offsets2040wIR, "20W/40W/IR Laser file");
                        }
                        else {
                            std::cout << GetTimeStamp() << "[INFO] Invalid choice. No files reset." << std::endl;
                        }
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();

                        if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                        if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                        if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                        needsRedraw = true;
                        continue;
                    }
                    else {
                        std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
                }
                catch (const std::exception& e) {
                    std::cout << GetTimeStamp() << "[ERR] X/Y calibration error: " << e.what() << std::endl;
                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                    (void)_getch();
                    continue;
                }
                catch (...) {
                    std::cout << GetTimeStamp() << "[ERR] X/Y calibration unknown error" << std::endl;
                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                    (void)_getch();
                    continue;
                }
            }
            else if ((statusKey == 'B' || statusKey == 'b') && status.isLaser()) {
                while (true) {
                    std::cout << std::endl;
                    PrintHeader("", 40);
                    PrintHeader("BACKUP FILE MANAGEMENT", 40);
                    PrintHeader("", 40);
                    std::cout << std::endl;
                    std::cout << "Select laser to manage:" << std::endl;
                    std::cout << "  1. 10W Laser (file: " << file10w.filename().string() << ")" << std::endl;
                    std::cout << "  2. 20W/40W/IR Laser (file: " << file2040wIR.filename().string() << ")" << std::endl;
                    std::cout << "  3. Both lasers" << std::endl;
                    std::cout << "  ESC. Return to status page" << std::endl;
                    std::cout << "Enter choice: ";

                    int backupChoice = _getch();
                    std::cout << std::endl;
                    if (backupChoice == ESC_ASCII) break;

                    if (backupChoice == '1') {
                        while (true) {
                            std::cout << std::endl;
                            std::cout << "Managing backups for: 10W Laser" << std::endl;
                            std::cout << "  1. Restore from latest backup" << std::endl;
                            std::cout << "  2. Restore from specific backup (select by number)" << std::endl;
                            std::cout << "  3. Open selected backup in Notepad (select by number)" << std::endl;
                            std::cout << "  4. Delete all old backup files for selected laser" << std::endl;
                            std::cout << "  ESC. Return to main menu" << std::endl;
                            std::cout << "Enter choice: ";

                            int action = _getch();
                            std::cout << std::endl;

                            if (action == ESC_ASCII) break;

                            if (action == '1') {
                                if (restoreFromBackup(file10w)) {
                                    G92Offsets parsed = ParseG92Line(file10w.string());
                                    if (parsed.found) {
                                        offsets10w.x = parsed.x;
                                        offsets10w.y = parsed.y;
                                        offsets10w.z = parsed.z;
                                        offsets10w.valid = true;
                                    }
                                    std::cout << GetTimeStamp() << "Do you want to open the restored file in Notepad? (Y/n): ";
                                    std::cout.flush();
                                    int openFile = _getch();
                                    std::cout << std::endl;
                                    if (openFile != 'n' && openFile != 'N') {
                                        try {
                                            if (fs::exists(file10w)) {
                                                ShellExecuteA(NULL, "open", "notepad.exe", file10w.string().c_str(), NULL, SW_SHOWNORMAL);
                                            }
                                        }
                                        catch (const std::exception& e) {
                                            std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                            else if (action == '2') {
                                std::vector<fs::path> backups = listBackupsForFile(file10w);
                                if (backups.empty()) {
                                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                    (void)_getch();
                                    continue;
                                }

                                std::cout << GetTimeStamp() << "Enter backup number to restore (1-" << backups.size() << "): ";
                                std::string input;
                                while (true) {
                                    int ch = _getch();
                                    if (ch == ENTER_ASCII) break;
                                    else if (ch == ESC_ASCII) {
                                        std::cout << std::endl << GetTimeStamp() << "[INFO] Cancelled." << std::endl;
                                        goto next_10w;
                                    }
                                    else if (ch >= '0' && ch <= '9') {
                                        input += static_cast<char>(ch);
                                        std::cout << static_cast<char>(ch);
                                    }
                                    else if (ch == 8 && !input.empty()) {
                                        input.pop_back();
                                        std::cout << "\b \b";
                                    }
                                }
                                std::cout << std::endl;

                                int backupNum = 0;
                                try {
                                    backupNum = std::stoi(input);
                                }
                                catch (...) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid number." << std::endl;
                                    continue;
                                }

                                if (backupNum < 1 || backupNum > static_cast<int>(backups.size())) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid backup number. Choose 1-" << backups.size() << std::endl;
                                    continue;
                                }

                                if (restoreFromBackup(file10w, backupNum)) {
                                    G92Offsets parsed = ParseG92Line(file10w.string());
                                    if (parsed.found) {
                                        offsets10w.x = parsed.x;
                                        offsets10w.y = parsed.y;
                                        offsets10w.z = parsed.z;
                                        offsets10w.valid = true;
                                    }
                                    std::cout << GetTimeStamp() << "Do you want to open the restored file in Notepad? (Y/n): ";
                                    std::cout.flush();
                                    int openFile = _getch();
                                    std::cout << std::endl;
                                    if (openFile != 'n' && openFile != 'N') {
                                        try {
                                            if (fs::exists(file10w)) {
                                                ShellExecuteA(NULL, "open", "notepad.exe", file10w.string().c_str(), NULL, SW_SHOWNORMAL);
                                            }
                                        }
                                        catch (const std::exception& e) {
                                            std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                            else if (action == '3') {
                                try {
                                    std::vector<fs::path> backups = listBackupsForFile(file10w);
                                    if (backups.empty()) {
                                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                        (void)_getch();
                                        continue;
                                    }

                                    std::cout << GetTimeStamp() << "Enter backup number to open (1-" << backups.size() << "): ";
                                    std::string input;
                                    while (true) {
                                        int ch = _getch();
                                        if (ch == ENTER_ASCII) break;
                                        else if (ch == ESC_ASCII) {
                                            std::cout << std::endl << GetTimeStamp() << "[INFO] Cancelled." << std::endl;
                                            goto next_10w;
                                        }
                                        else if (ch >= '0' && ch <= '9') {
                                            input += static_cast<char>(ch);
                                            std::cout << static_cast<char>(ch);
                                        }
                                        else if (ch == 8 && !input.empty()) {
                                            input.pop_back();
                                            std::cout << "\b \b";
                                        }
                                    }
                                    std::cout << std::endl;

                                    int backupNum = 0;
                                    try {
                                        backupNum = std::stoi(input);
                                    }
                                    catch (const std::exception& e) {
                                        std::cout << GetTimeStamp() << "[ERR] Invalid number: " << e.what() << std::endl;
                                        continue;
                                    }

                                    if (backupNum < 1 || backupNum > static_cast<int>(backups.size())) {
                                        std::cout << GetTimeStamp() << "[ERR] Invalid backup number. Choose 1-" << backups.size() << std::endl;
                                        continue;
                                    }

                                    fs::path backupToOpen = backups[backupNum - 1];
                                    std::cout << GetTimeStamp() << "Opening backup file: " << backupToOpen.filename().string() << std::endl;

                                    try {
                                        if (fs::exists(backupToOpen)) {
                                            INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", backupToOpen.string().c_str(), NULL, SW_SHOWNORMAL);
                                            if (result <= 32) {
                                                std::cerr << GetTimeStamp() << "[WARN] Failed to open " << backupToOpen.filename().string() << " (error: " << result << ")" << std::endl;
                                            }
                                        }
                                        else {
                                            std::cerr << GetTimeStamp() << "[WARN] Backup file not found: " << backupToOpen.filename().string() << std::endl;
                                        }
                                    }
                                    catch (const std::exception& e) {
                                        std::cerr << GetTimeStamp() << "[ERR] Failed to open backup: " << e.what() << std::endl;
                                    }
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to list backups: " << e.what() << std::endl;
                                }
                            }
                            else if (action == '4') {
                                int count = deleteOldBackups(file10w);
                                std::cout << GetTimeStamp() << "[OK] Deleted " << count << " old backup files for 10W Laser." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            }
                        next_10w:
                            continue;
                        }
                    }
                    else if (backupChoice == '2') {
                        while (true) {
                            std::cout << std::endl;
                            std::cout << "Managing backups for: 20W/40W/IR Laser" << std::endl;
                            std::cout << "  1. Restore from latest backup" << std::endl;
                            std::cout << "  2. Restore from specific backup (select by number)" << std::endl;
                            std::cout << "  3. Open selected backup in Notepad (select by number)" << std::endl;
                            std::cout << "  4. Delete all old backup files for selected laser" << std::endl;
                            std::cout << "  ESC. Return to main menu" << std::endl;
                            std::cout << "Enter choice: ";

                            int action = _getch();
                            std::cout << std::endl;

                            if (action == ESC_ASCII) break;

                            if (action == '1') {
                                if (restoreFromBackup(file2040wIR)) {
                                    G92Offsets parsed = ParseG92Line(file2040wIR.string());
                                    if (parsed.found) {
                                        offsets2040wIR.x = parsed.x;
                                        offsets2040wIR.y = parsed.y;
                                        offsets2040wIR.z = parsed.z;
                                        offsets2040wIR.valid = true;
                                    }
                                    std::cout << GetTimeStamp() << "Do you want to open the restored file in Notepad? (Y/n): ";
                                    std::cout.flush();
                                    int openFile = _getch();
                                    std::cout << std::endl;
                                    if (openFile != 'n' && openFile != 'N') {
                                        try {
                                            if (fs::exists(file2040wIR)) {
                                                ShellExecuteA(NULL, "open", "notepad.exe", file2040wIR.string().c_str(), NULL, SW_SHOWNORMAL);
                                            }
                                        }
                                        catch (const std::exception& e) {
                                            std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                            else if (action == '2') {
                                std::vector<fs::path> backups = listBackupsForFile(file2040wIR);
                                if (backups.empty()) {
                                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                    (void)_getch();
                                    continue;
                                }

                                std::cout << GetTimeStamp() << "Enter backup number to restore (1-" << backups.size() << "): ";
                                std::string input;
                                while (true) {
                                    int ch = _getch();
                                    if (ch == ENTER_ASCII) break;
                                    else if (ch == ESC_ASCII) {
                                        std::cout << std::endl << GetTimeStamp() << "[INFO] Cancelled." << std::endl;
                                        goto next_2040w;
                                    }
                                    else if (ch >= '0' && ch <= '9') {
                                        input += static_cast<char>(ch);
                                        std::cout << static_cast<char>(ch);
                                    }
                                    else if (ch == 8 && !input.empty()) {
                                        input.pop_back();
                                        std::cout << "\b \b";
                                    }
                                }
                                std::cout << std::endl;

                                int backupNum = 0;
                                try {
                                    backupNum = std::stoi(input);
                                }
                                catch (...) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid number." << std::endl;
                                    continue;
                                }

                                if (backupNum < 1 || backupNum > static_cast<int>(backups.size())) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid backup number. Choose 1-" << backups.size() << std::endl;
                                    continue;
                                }

                                if (restoreFromBackup(file2040wIR, backupNum)) {
                                    G92Offsets parsed = ParseG92Line(file2040wIR.string());
                                    if (parsed.found) {
                                        offsets2040wIR.x = parsed.x;
                                        offsets2040wIR.y = parsed.y;
                                        offsets2040wIR.z = parsed.z;
                                        offsets2040wIR.valid = true;
                                    }
                                    std::cout << GetTimeStamp() << "Do you want to open the restored file in Notepad? (Y/n): ";
                                    std::cout.flush();
                                    int openFile = _getch();
                                    std::cout << std::endl;
                                    if (openFile != 'n' && openFile != 'N') {
                                        try {
                                            if (fs::exists(file2040wIR)) {
                                                ShellExecuteA(NULL, "open", "notepad.exe", file2040wIR.string().c_str(), NULL, SW_SHOWNORMAL);
                                            }
                                        }
                                        catch (const std::exception& e) {
                                            std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                                        }
                                    }
                                }
                            }
                            else if (action == '3') {
                                std::vector<fs::path> backups = listBackupsForFile(file2040wIR);
                                if (backups.empty()) {
                                    std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                    (void)_getch();
                                    continue;
                                }

                                std::cout << GetTimeStamp() << "Enter backup number to open (1-" << backups.size() << "): ";
                                std::string input;
                                while (true) {
                                    int ch = _getch();
                                    if (ch == ENTER_ASCII) break;
                                    else if (ch == ESC_ASCII) {
                                        std::cout << std::endl << GetTimeStamp() << "[INFO] Cancelled." << std::endl;
                                        goto next_2040w;
                                    }
                                    else if (ch >= '0' && ch <= '9') {
                                        input += static_cast<char>(ch);
                                        std::cout << static_cast<char>(ch);
                                    }
                                    else if (ch == 8 && !input.empty()) {
                                        input.pop_back();
                                        std::cout << "\b \b";
                                    }
                                }
                                std::cout << std::endl;

                                int backupNum = 0;
                                try {
                                    backupNum = std::stoi(input);
                                }
                                catch (...) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid number." << std::endl;
                                    continue;
                                }

                                if (backupNum < 1 || backupNum > static_cast<int>(backups.size())) {
                                    std::cout << GetTimeStamp() << "[ERR] Invalid backup number. Choose 1-" << backups.size() << std::endl;
                                    continue;
                                }

                                fs::path backupToOpen = backups[backupNum - 1];
                                std::cout << GetTimeStamp() << "Opening backup file: " << backupToOpen.filename().string() << std::endl;
                                try {
                                    if (fs::exists(backupToOpen)) {
                                        INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", backupToOpen.string().c_str(), NULL, SW_SHOWNORMAL);
                                        if (result <= 32) {
                                            std::cerr << GetTimeStamp() << "[WARN] Failed to open " << backupToOpen.filename().string() << " (error: " << result << ")" << std::endl;
                                        }
                                    }
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to open backup: " << e.what() << std::endl;
                                }
                            }
                            else if (action == '4') {
                                int count = deleteOldBackups(file2040wIR);
                                std::cout << GetTimeStamp() << "[OK] Deleted " << count << " old backup files for 20W/40W/IR Laser." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            }
                        next_2040w:
                            continue;
                        }
                    }
                    else if (backupChoice == '3') {
                        while (true) {
                            std::cout << std::endl;
                            std::cout << "Managing backups for: Both lasers" << std::endl;
                            std::cout << "  1. Delete all old backup files (for both lasers)" << std::endl;
                            std::cout << "  2. List backup files (for both lasers)" << std::endl;
                            std::cout << "  ESC. Return to main menu" << std::endl;
                            std::cout << "Enter choice: ";

                            int action = _getch();
                            std::cout << std::endl;

                            if (action == ESC_ASCII) break;

                            if (action == '1') {
                                int count10w = deleteOldBackups(file10w);
                                int count2040w = deleteOldBackups(file2040wIR);
                                std::cout << GetTimeStamp() << "[OK] Deleted " << count10w << " backup(s) for 10W Laser" << std::endl;
                                std::cout << GetTimeStamp() << "[OK] Deleted " << count2040w << " backup(s) for 20W/40W/IR Laser" << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                            }
                            else if (action == '2') {
                                std::cout << std::endl;
                                listBackupsForFile(file10w);
                                std::cout << std::endl;
                                listBackupsForFile(file2040wIR);
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                            }
                        }
                    }
                }

                if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                needsRedraw = true;
                continue;
            }
            else if ((statusKey == 'F' || statusKey == 'f') && status.isLaser()) {
                while (true) {
                    std::cout << std::endl;
                    PrintHeader("", 40);
                    PrintHeader("MANAGE G-CODE FILES", 40);
                    PrintHeader("", 40);
                    std::cout << std::endl;

                    try {
                        std::cout << "G-code Folder: " << fs::absolute(gcodeFolder).string() << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cerr << GetTimeStamp() << "[ERR] Failed to get absolute path: " << e.what() << std::endl;
                        std::cout << "G-code Folder: " << gcodeFolder.string() << std::endl;
                    }

                    std::cout << std::endl;
                    std::cout << "  1. Open G-code Folder in Explorer" << std::endl;
                    std::cout << "  2. Open file in Notepad (select by number)" << std::endl;
                    std::cout << "  3. Delete file (select by number)" << std::endl;
                    std::cout << "  4. List all G-code files" << std::endl;
                    std::cout << "  ESC. Return to status page" << std::endl;
                    std::cout << "Enter choice: ";

                    int action = _getch();
                    std::cout << std::endl;
                    if (action == ESC_ASCII) break;

                    if (action == '1') {
                        try {
                            ShellExecuteA(NULL, "open", gcodeFolder.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                        catch (const std::exception& e) {
                            std::cerr << GetTimeStamp() << "[ERR] Failed to open folder: " << e.what() << std::endl;
                        }
                    }
                    else if (action == '2' || action == '3') {
                        std::vector<fs::path> files;
                        try {
                            if (!fs::exists(gcodeFolder)) {
                                std::cout << GetTimeStamp() << "[ERR] G-code folder does not exist." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                                continue;
                            }
                            for (const auto& entry : fs::directory_iterator(gcodeFolder)) {
                                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                                    files.push_back(entry.path());
                                }
                            }
                        }
                        catch (const std::exception& e) {
                            std::cerr << GetTimeStamp() << "[ERR] Failed to list files: " << e.what() << std::endl;
                            continue;
                        }

                        if (files.empty()) {
                            std::cout << GetTimeStamp() << "[INFO] No G-code files found in folder." << std::endl;
                            std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                            (void)_getch();
                            continue;
                        }

                        std::cout << std::endl;
                        std::cout << "G-code files in folder:" << std::endl;
                        for (size_t i = 0; i < files.size(); i++) {
                            std::cout << "  " << (i + 1) << ". " << files[i].filename().string() << std::endl;
                        }
                        std::cout << std::endl;

                        std::cout << GetTimeStamp() << "Enter file number (1-" << files.size() << "): ";
                        std::string input;
                        while (true) {
                            int ch = _getch();
                            if (ch == ENTER_ASCII) break;
                            else if (ch == ESC_ASCII) {
                                std::cout << std::endl << GetTimeStamp() << "[INFO] Cancelled." << std::endl;
                                goto next_f;
                            }
                            else if (ch >= '0' && ch <= '9') {
                                input += static_cast<char>(ch);
                                std::cout << static_cast<char>(ch);
                            }
                            else if (ch == 8 && !input.empty()) {
                                input.pop_back();
                                std::cout << "\b \b";
                            }
                        }
                        std::cout << std::endl;

                        int fileNum = 0;
                        try {
                            fileNum = std::stoi(input);
                        }
                        catch (const std::exception& e) {
                            std::cout << GetTimeStamp() << "[ERR] Invalid number: " << e.what() << std::endl;
                            continue;
                        }

                        if (fileNum < 1 || fileNum > static_cast<int>(files.size())) {
                            std::cout << GetTimeStamp() << "[ERR] Invalid file number. Choose 1-" << files.size() << std::endl;
                            continue;
                        }

                        if (action == '2') {
                            try {
                                if (fs::exists(files[fileNum - 1])) {
                                    INT_PTR result = (INT_PTR)ShellExecuteA(NULL, "open", "notepad.exe", files[fileNum - 1].string().c_str(), NULL, SW_SHOWNORMAL);
                                    if (result <= 32) {
                                        std::cerr << GetTimeStamp() << "[WARN] Failed to open file (error: " << result << ")" << std::endl;
                                    }
                                    else {
                                        std::cout << GetTimeStamp() << "[OK] Opening: " << files[fileNum - 1].filename().string() << std::endl;
                                    }
                                }
                                else {
                                    std::cerr << GetTimeStamp() << "[WARN] File not found: " << files[fileNum - 1].filename().string() << std::endl;
                                }
                            }
                            catch (const std::exception& e) {
                                std::cerr << GetTimeStamp() << "[ERR] Failed to open file: " << e.what() << std::endl;
                            }
                        }
                        else if (action == '3') {
                            std::cout << GetTimeStamp() << "Are you sure you want to delete " << files[fileNum - 1].filename().string() << "? (y/N): ";
                            int confirm = _getch();
                            std::cout << std::endl;
                            if (confirm == 'y' || confirm == 'Y') {
                                try {
                                    fs::remove(files[fileNum - 1]);
                                    std::cout << GetTimeStamp() << "[OK] Deleted: " << files[fileNum - 1].filename().string() << std::endl;
                                }
                                catch (const std::exception& e) {
                                    std::cerr << GetTimeStamp() << "[ERR] Failed to delete: " << e.what() << std::endl;
                                }
                            }
                            else {
                                std::cout << GetTimeStamp() << "[INFO] Deletion cancelled." << std::endl;
                            }
                        }
                    }
                    else if (action == '4') {
                        try {
                            if (!fs::exists(gcodeFolder)) {
                                std::cout << GetTimeStamp() << "[INFO] G-code folder does not exist yet." << std::endl;
                                std::cout << GetTimeStamp() << "Run Z Focal Calculations to create it." << std::endl;
                                std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                                (void)_getch();
                                continue;
                            }

                            int idx = 1;
                            std::cout << std::endl;
                            std::cout << "G-code files in folder:" << std::endl;
                            for (const auto& entry : fs::directory_iterator(gcodeFolder)) {
                                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                                    std::string timestamp = "";
                                    try {
                                        auto ftime = fs::last_write_time(entry.path());
                                        auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                                        std::time_t tt = std::chrono::system_clock::to_time_t(sys_time);
                                        tm tmBuf{};
                                        localtime_s(&tmBuf, &tt);
                                        std::stringstream ss;
                                        ss << std::put_time(&tmBuf, " (%Y-%m-%d %H:%M:%S)");
                                        timestamp = ss.str();
                                    }
                                    catch (const std::exception&) {
                                        timestamp = " (date unknown)";
                                    }
                                    std::cout << "  " << idx++ << ". " << entry.path().filename().string() << timestamp << std::endl;
                                }
                            }
                            if (idx == 1) {
                                std::cout << "  No G-code files found." << std::endl;
                            }
                        }
                        catch (const std::exception& e) {
                            std::cerr << GetTimeStamp() << "[ERR] Failed to list files: " << e.what() << std::endl;
                        }
                        std::cout << GetTimeStamp() << "Press any key to continue..." << std::endl;
                        (void)_getch();
                    }
                    else {
                        std::cout << GetTimeStamp() << "[INFO] Invalid choice." << std::endl;
                    }
                next_f:
                    continue;
                }

                if (fs::exists(file10w)) lastFile10wTime = fs::file_time_type();
                if (fs::exists(file2040wIR)) lastFile2040wTime = fs::file_time_type();
                if (fs::exists(endFile)) lastEndFileTime = fs::file_time_type();
                needsRedraw = true;
                continue;
            }
            else if (statusKey == 'M' || statusKey == 'm' || statusKey == ESC_ASCII) {
                inStatusPage = false;
                zHeightCalculated = false;
                staticBufferZ = 0.0;
                staticBufferX = 0.0;
                staticBufferY = 0.0;
                zFocusHeight = 0.0;
                xyCalibrated = false;
                homingAttempted = false;
                break;
            }
            else if ((statusKey == 'G' || statusKey == 'g') && status.isLaser()) {
                std::cout << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Opening Snapmaker 1064nm IR+20/40W + Lightburn Guide in default browser..." << std::endl;

                const char* url = "https://forum.snapmaker.com/t/1064nm-ir-20-40w-lightburn-guide/35954";
                HINSTANCE result = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);

                if ((INT_PTR)result <= 32) {
                    std::cout << GetTimeStamp() << "[WARN] Could not open browser automatically." << std::endl;
                    std::cout << GetTimeStamp() << "[INFO] Please manually open this URL in your browser:" << std::endl;
                    std::cout << "       " << url << std::endl;
                }
                else {
                    std::cout << GetTimeStamp() << "[OK] Browser opened." << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!fromShortcut) {
        ClearAndDrawHeader(true);
    }
}