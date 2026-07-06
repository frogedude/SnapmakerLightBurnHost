// tray.cpp - System tray with folder monitoring, interruptible wait, manual process
#define WIN32_LEAN_AND_MEAN
#include "host.h"
#include "resource.h"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <shlwapi.h>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <iostream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

// ----------------------------------------------------------------------
// Helper function for Recycle Bin
// ----------------------------------------------------------------------
bool MoveToRecycleBin(const std::string& filePath) {
    std::string doubleNullPath = filePath + '\0' + '\0';
    SHFILEOPSTRUCTA sh = { 0 };
    sh.wFunc = FO_DELETE;
    sh.pFrom = doubleNullPath.c_str();
    sh.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    int result = SHFileOperationA(&sh);
    return (result == 0);
}

// Constants
#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY_EXIT               1001
#define ID_TRAY_OPEN_URL           1002
#define ID_TRAY_START_SERVER       1003
#define ID_TRAY_STOP_SERVER        1004
#define ID_TRAY_RESTART            1005
#define ID_TRAY_STATUS             1006
#define ID_TRAY_OPEN_FOLDER        1007
#define ID_TRAY_PROCESS_UPLOADS    1008
#define ID_TRAY_LAUNCH_CAMERA      1009
#define ID_TRAY_LAUNCH_KASA        1010
#define ID_TRAY_LAUNCH_MONITOR     1011
#define ID_TRAY_LAUNCH_LIGHTBURN   1012
#define ID_TRAY_LAUNCH_ORCA        1013
#define ID_TRAY_LAUNCH_LUBAN       1014
#define ID_TRAY_LAUNCH_CURA        1015
#define ID_TRAY_WEB_CAPTURE        1016
#define ID_TRAY_WEB_CONFIG         1017
#define ID_TRAY_WEB_CONTROL        1018
#define ID_TRAY_WEB_DISCOVER       1019
#define ID_TRAY_WEB_GCODE          1020
#define ID_TRAY_WEB_MONITOR        1021
#define ID_TRAY_WEB_THICKNESS      1022
#define ID_TRAY_WEB_UPLOAD         1023
#define ID_TRAY_WEB_XYZCAL         1024
#define ID_TRAY_WEB_ABOUT          1025

static NOTIFYICONDATAW nid = {};
static HWND hMsgWnd = NULL;
static std::string orcaPath;
static std::string lubanPath;
static std::string curaPath;

// Folder monitoring globals
static std::atomic<bool> monitorRunning{ false };
static std::thread monitorThread;
static HANDLE hDir = INVALID_HANDLE_VALUE;
static HANDLE hCompletionEvent = NULL;
static OVERLAPPED overlapped = { 0 };
static char buffer[65536];
static std::string watchedFolder;
static std::string noteFolder;

struct QueuedFile {
    std::string path;
    bool wasBusyOnArrival;
};
static std::queue<QueuedFile> fileQueue;
static std::mutex queueMutex;
static std::condition_variable queueCV;
static std::thread workerThread;
static HANDLE hWakeEvent = NULL;

// Deduplication cache
static std::map<std::string, std::chrono::steady_clock::time_point> recentlyQueued;
static std::mutex recentMutex;

// Processing flag and per-file lock
static std::atomic<bool> processingInProgress{ false };
static std::set<std::string> filesInProgress;
static std::mutex filesInProgressMutex;

static void EnsureWatchedFolder() {
    if (!watchedFolder.empty()) return;
    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(NULL, exePathW, MAX_PATH);
    fs::path exeDir = fs::path(exePathW).parent_path();
    watchedFolder = (exeDir / "Upload").string();
    if (!fs::exists(watchedFolder))
        fs::create_directories(watchedFolder);
}

// ----------------------------------------------------------------------
// Helper: launch monitor window (SnapmakerLightBurnHost.exe /monitor)
// ----------------------------------------------------------------------
static void LaunchMonitorWindow() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = "\"" + std::string(exePath) + "\" /monitor";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ----------------------------------------------------------------------
// Helper: launch camera window (SnapmakerLightBurnHost.exe /camera)
// ----------------------------------------------------------------------
static void LaunchCamera() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = "\"" + std::string(exePath) + "\" /camera";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ----------------------------------------------------------------------
// Helper: launch Kasa control batch file (hs100.bat)
// ----------------------------------------------------------------------
static void LaunchKasa() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();
    fs::path batchPath = exeDir / "hs100" / "hs100.bat";
    if (fs::exists(batchPath)) {
        ShellExecuteA(NULL, "open", batchPath.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

// ----------------------------------------------------------------------
// Helper: get OrcaSlicer executable path
// ----------------------------------------------------------------------
static std::string GetOrcaSlicerPath() {
    HKEY hKey;
    std::string path;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\orcaslicer\\shell\\open\\command",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[MAX_PATH] = { 0 };
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        if (RegQueryValueExA(hKey, NULL, NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS && type == REG_SZ) {
            std::string cmd = buffer;
            size_t firstQuote = cmd.find('"');
            if (firstQuote != std::string::npos) {
                size_t secondQuote = cmd.find('"', firstQuote + 1);
                if (secondQuote != std::string::npos) {
                    path = cmd.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                }
                else {
                    path = cmd;
                }
            }
        }
        RegCloseKey(hKey);
    }
    if (path.empty() || !std::filesystem::exists(path)) {
        path = "C:\\Program Files\\OrcaSlicer\\orca-slicer.exe";
        if (!std::filesystem::exists(path)) path.clear();
    }
    return path;
}

// ----------------------------------------------------------------------
// Helper: get Snapmaker Luban executable path
// ----------------------------------------------------------------------
static std::string GetLubanPath() {
    HKEY hKey;
    std::string path;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char subKeyName[256];
        DWORD subKeySize = sizeof(subKeyName);
        while (RegEnumKeyExA(hKey, index++, subKeyName, &subKeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            HKEY hSubKey;
            if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                char displayName[256] = { 0 };
                DWORD size = sizeof(displayName);
                if (RegQueryValueExA(hSubKey, "DisplayName", NULL, NULL, (LPBYTE)displayName, &size) == ERROR_SUCCESS) {
                    std::string name = displayName;
                    if (name.find("Snapmaker Luban") != std::string::npos) {
                        char iconPath[MAX_PATH] = { 0 };
                        size = sizeof(iconPath);
                        if (RegQueryValueExA(hSubKey, "DisplayIcon", NULL, NULL, (LPBYTE)iconPath, &size) == ERROR_SUCCESS) {
                            path = iconPath;
                            size_t comma = path.find(',');
                            if (comma != std::string::npos) path = path.substr(0, comma);
                        }
                        else {
                            char installPath[MAX_PATH] = { 0 };
                            size = sizeof(installPath);
                            if (RegQueryValueExA(hSubKey, "InstallLocation", NULL, NULL, (LPBYTE)installPath, &size) == ERROR_SUCCESS) {
                                path = std::string(installPath) + "Snapmaker Luban.exe";
                            }
                        }
                        RegCloseKey(hSubKey);
                        if (!path.empty() && std::filesystem::exists(path)) break;
                        else path.clear();
                    }
                }
                RegCloseKey(hSubKey);
            }
            subKeySize = sizeof(subKeyName);
        }
        RegCloseKey(hKey);
    }
    if (path.empty() || !std::filesystem::exists(path)) {
        path = "C:\\Program Files\\Snapmaker Luban\\Snapmaker Luban.exe";
        if (!std::filesystem::exists(path)) path.clear();
    }
    return path;
}

// ----------------------------------------------------------------------
// Helper: get UltiMaker Cura executable path
// ----------------------------------------------------------------------
static std::string GetCuraPath() {
    HKEY hKey;
    std::string path;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char subKeyName[256];
        DWORD subKeySize = sizeof(subKeyName);
        while (RegEnumKeyExA(hKey, index++, subKeyName, &subKeySize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            HKEY hSubKey;
            if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                char displayName[256] = { 0 };
                DWORD size = sizeof(displayName);
                if (RegQueryValueExA(hSubKey, "DisplayName", NULL, NULL, (LPBYTE)displayName, &size) == ERROR_SUCCESS) {
                    std::string name = displayName;
                    if (name.find("UltiMaker Cura") != std::string::npos) {
                        char iconPath[MAX_PATH] = { 0 };
                        size = sizeof(iconPath);
                        if (RegQueryValueExA(hSubKey, "DisplayIcon", NULL, NULL, (LPBYTE)iconPath, &size) == ERROR_SUCCESS) {
                            path = iconPath;
                            size_t comma = path.find(',');
                            if (comma != std::string::npos) path = path.substr(0, comma);
                        }
                        else {
                            char uninstallPath[MAX_PATH] = { 0 };
                            size = sizeof(uninstallPath);
                            if (RegQueryValueExA(hSubKey, "UninstallString", NULL, NULL, (LPBYTE)uninstallPath, &size) == ERROR_SUCCESS) {
                                std::string uninst = uninstallPath;
                                size_t exePos = uninst.find("uninstall.exe");
                                if (exePos != std::string::npos) {
                                    path = uninst.substr(0, exePos);
                                    if (std::filesystem::exists(path + "UltiMaker-Cura.exe"))
                                        path += "UltiMaker-Cura.exe";
                                    else if (std::filesystem::exists(path + "cura.exe"))
                                        path += "cura.exe";
                                    else path.clear();
                                }
                            }
                        }
                        RegCloseKey(hSubKey);
                        if (!path.empty() && std::filesystem::exists(path)) break;
                        else path.clear();
                    }
                }
                RegCloseKey(hSubKey);
            }
            subKeySize = sizeof(subKeyName);
        }
        RegCloseKey(hKey);
    }
    if (path.empty() || !std::filesystem::exists(path)) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator("C:\\Program Files")) {
                if (entry.is_directory() && entry.path().filename().string().find("UltiMaker Cura") != std::string::npos) {
                    std::string exe = (entry.path() / "UltiMaker-Cura.exe").string();
                    if (std::filesystem::exists(exe)) { path = exe; break; }
                }
            }
            if (path.empty()) {
                for (const auto& entry : std::filesystem::directory_iterator("C:\\Program Files (x86)")) {
                    if (entry.is_directory() && entry.path().filename().string().find("UltiMaker Cura") != std::string::npos) {
                        std::string exe = (entry.path() / "UltiMaker-Cura.exe").string();
                        if (std::filesystem::exists(exe)) { path = exe; break; }
                    }
                }
            }
        }
        catch (...) {}
    }
    return path;
}

// ----------------------------------------------------------------------
// Helper: check if LightBurn is installed
// ----------------------------------------------------------------------
static bool IsLightBurnInstalled() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "LightBurn.LightBurn.1\\Shell\\Open\\Command",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[MAX_PATH] = { 0 };
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        if (RegQueryValueExA(hKey, NULL, NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS && type == REG_SZ) {
            std::string cmd = buffer;
            size_t firstQuote = cmd.find('"');
            if (firstQuote != std::string::npos) {
                size_t secondQuote = cmd.find('"', firstQuote + 1);
                if (secondQuote != std::string::npos) {
                    std::string path = cmd.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                    if (std::filesystem::exists(path)) {
                        RegCloseKey(hKey);
                        return true;
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }
    const char* defaultPaths[] = {
        "C:\\Program Files\\LightBurn\\LightBurn.exe",
        "C:\\Program Files (x86)\\LightBurn\\LightBurn.exe"
    };
    for (const char* path : defaultPaths) {
        if (std::filesystem::exists(path)) return true;
    }
    return false;
}

// ----------------------------------------------------------------------
// Helper: delete all files older than 10 minutes, but preserve Readme.txt
// ----------------------------------------------------------------------
static void CleanupOldFiles() {
    if (watchedFolder.empty()) return;
    try {
        auto now = std::chrono::system_clock::now();
        for (const auto& entry : std::filesystem::directory_iterator(watchedFolder)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Skip Readme.txt (case-insensitive)
                if (_stricmp(filename.c_str(), "Readme.txt") == 0)
                    continue;
                auto ftime = std::filesystem::last_write_time(entry.path());
                auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                auto age = std::chrono::duration_cast<std::chrono::minutes>(now - sys_time).count();
                if (age > 10) {
                    std::filesystem::remove(entry.path());
                    std::cout << GetTimeStamp() << "[CLEANUP] Deleted old file: " << filename << std::endl;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[WARN] Cleanup failed: " << e.what() << std::endl;
    }
}

// ----------------------------------------------------------------------
// Launch the main executable with the file as argument (no command switch)
// ----------------------------------------------------------------------
static void LaunchMainExeWithFile(const std::string& filePath) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = "\"" + std::string(exePath) + "\" \"" + filePath + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    std::cout << GetTimeStamp() << "[LAUNCH] Command: " << cmdLine << std::endl;

    if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        std::cout << GetTimeStamp() << "[LAUNCH] Process created (PID: " << pi.dwProcessId << ")" << std::endl;
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        DWORD err = GetLastError();
        std::cerr << GetTimeStamp() << "[LAUNCH] CreateProcess failed with error: " << err << std::endl;
    }
}

// ----------------------------------------------------------------------
// Interruptible wait: waits up to 10 minutes OR until hWakeEvent is signaled.
// ----------------------------------------------------------------------
static void WaitForPrinterIdle() {
    const int WAIT_MS = 10 * 60 * 1000; // 10 * minutes
    while (true) {
        MachineStatusData status = GetMachineStatus(false);
        std::cout << GetTimeStamp() << "[FOLDER] Machine status: " << status.status
            << ", isBusy=" << status.isBusy()
            << ", homed=" << status.homed << std::endl;
        if (!status.isBusy()) {
            std::cout << GetTimeStamp() << "[FOLDER] Printer idle." << std::endl;
            return;
        }
        std::cout << GetTimeStamp() << "[FOLDER] Printer busy. Waiting up to 10 minutes (or until new file arrives)..." << std::endl;
        DWORD waitResult = WaitForSingleObject(hWakeEvent, WAIT_MS);
        if (waitResult == WAIT_OBJECT_0) {
            std::cout << GetTimeStamp() << "[FOLDER] Woken by new file event. Rechecking printer." << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[FOLDER] 10 minutes elapsed. Rechecking printer." << std::endl;
        }
        // Loop continues – recheck printer state
    }
}

// ----------------------------------------------------------------------
// Process a single file: wait for idle, confirm if needed, launch, wait again
// ----------------------------------------------------------------------
static void ProcessNewFile(const std::string& filepath, bool wasBusyOnArrival) {
    std::cout << GetTimeStamp() << "[FOLDER] ProcessNewFile called: " << filepath << " (busy=" << wasBusyOnArrival << ")" << std::endl;

    // Per-file lock to prevent duplicates
    {
        std::lock_guard<std::mutex> lock(filesInProgressMutex);
        if (filesInProgress.find(filepath) != filesInProgress.end()) {
            std::cout << GetTimeStamp() << "[FOLDER] File already being processed, skipping: " << filepath << std::endl;
            return;
        }
        filesInProgress.insert(filepath);
    }

    // RAII unlock
    struct Unlocker {
        std::string path;
        ~Unlocker() {
            std::lock_guard<std::mutex> lock(filesInProgressMutex);
            filesInProgress.erase(path);
        }
    } unlocker{ filepath };

    Sleep(500);
    {
        std::lock_guard<std::mutex> lock(recentMutex);
        recentlyQueued.erase(filepath);
    }

    if (!std::filesystem::exists(filepath)) {
        std::cout << GetTimeStamp() << "[FOLDER] File no longer exists, skipping: " << filepath << std::endl;
        return;
    }

    std::string ext = fs::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::cout << GetTimeStamp() << "[FOLDER] File extension: " << ext << std::endl;

    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::string action = cfg.fileAction;
    std::transform(action.begin(), action.end(), action.begin(), ::tolower);
    bool useRecycleBin = (action != "delete");

    if (ext != ".gcode" && ext != ".nc" && ext != ".cnc" && ext != ".bin") {
        std::cout << GetTimeStamp() << "[FOLDER] Ignoring unsupported file: " << filepath << std::endl;
        if (useRecycleBin) {
            if (MoveToRecycleBin(filepath)) {
                std::cout << GetTimeStamp() << "[FOLDER] Moved unsupported file to Recycle Bin" << std::endl;
            }
            else {
                std::filesystem::remove(filepath);
                std::cout << GetTimeStamp() << "[FOLDER] Deleted unsupported file" << std::endl;
            }
        }
        else {
            std::filesystem::remove(filepath);
            std::cout << GetTimeStamp() << "[FOLDER] Deleted unsupported file" << std::endl;
        }
        return;
    }

    std::cout << GetTimeStamp() << "[FOLDER] Calling WaitForPrinterIdle()..." << std::endl;
    WaitForPrinterIdle();

    // Always show confirmation dialog if autoStartPrint is enabled
    if (cfg.autoStartPrint) {
        PlayAlertChime();
        std::wstring wfilename(filepath.begin(), filepath.end());
        std::wstring msg = L"Start print for " + wfilename + L"?\n\n"
            L"  Yes      - Start this print\n"
            L"  No       - Delete this file only\n"
            L"  Cancel   - Delete ALL pending files and stop processing";
        int result = MessageBoxW(hMsgWnd, msg.c_str(), L"Confirm Print Start", MB_YESNOCANCEL | MB_ICONQUESTION);

        if (result == IDNO) {
            std::cout << GetTimeStamp() << "[FOLDER] User declined print. ";
            if (useRecycleBin) {
                if (MoveToRecycleBin(filepath)) {
                    std::cout << "Moved to Recycle Bin" << std::endl;
                }
                else {
                    std::filesystem::remove(filepath);
                    std::cout << "Deleted" << std::endl;
                }
            }
            else {
                std::filesystem::remove(filepath);
                std::cout << "Deleted" << std::endl;
            }
            return;
        }
        else if (result == IDCANCEL) {
            std::cout << GetTimeStamp() << "[FOLDER] User cancelled. Clearing queue." << std::endl;

            if (useRecycleBin) {
                if (!MoveToRecycleBin(filepath)) {
                    std::filesystem::remove(filepath);
                }
            }
            else {
                std::filesystem::remove(filepath);
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                while (!fileQueue.empty()) {
                    QueuedFile qf = fileQueue.front();
                    fileQueue.pop();
                    if (useRecycleBin) {
                        if (!MoveToRecycleBin(qf.path)) {
                            std::filesystem::remove(qf.path);
                        }
                    }
                    else {
                        std::filesystem::remove(qf.path);
                    }
                }
            }
            return;
        }
        // If IDYES, fall through to launch
    }

    std::cout << GetTimeStamp() << "[FOLDER] Launching main executable with file: " << filepath << std::endl;
    LaunchMainExeWithFile(filepath);

    // Process source file after print
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
                std::cout << GetTimeStamp() << "[INFO] Permanently deleted: " << srcPath.filename().string() << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[WARN] Could not process source file: " << e.what() << std::endl;
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!fileQueue.empty()) {
            WaitForPrinterIdle();
        }
    }
}

// ----------------------------------------------------------------------
// Manually process all .gcode, .nc, .cnc files in Upload folder (synchronous)
// ----------------------------------------------------------------------
static void ProcessAllExistingFiles() {
    EnsureWatchedFolder();
    if (watchedFolder.empty()) {
        std::cout << GetTimeStamp() << "[FOLDER] Upload folder path could not be determined." << std::endl;
        return;
    }

    // Clear queue and cache (fresh start)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!fileQueue.empty()) fileQueue.pop();
    }
    {
        std::lock_guard<std::mutex> lock(recentMutex);
        recentlyQueued.clear();
    }
    std::cout << GetTimeStamp() << "[FOLDER] Queue and cache cleared." << std::endl;

    // Capture current machine status
    MachineStatusData status = GetMachineStatus(false);
    bool busyNow = status.isBusy();
    std::cout << GetTimeStamp() << "[FOLDER] Printer busy status: " << (busyNow ? "BUSY" : "IDLE") << std::endl;

    std::cout << GetTimeStamp() << "[FOLDER] Manual processing started." << std::endl;
    try {
        for (const auto& entry : fs::directory_iterator(watchedFolder)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".gcode" || ext == ".nc" || ext == ".cnc" || ext == ".bin") {
                    std::string fullPath = entry.path().string();
                    std::cout << GetTimeStamp() << "[FOLDER] Processing file: " << fullPath << std::endl;
                    ProcessNewFile(fullPath, busyNow);
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[FOLDER] Error scanning folder: " << e.what() << std::endl;
    }
    std::cout << GetTimeStamp() << "[FOLDER] Manual processing finished." << std::endl;
}

// ----------------------------------------------------------------------
// Worker thread: processes queued files one by one
// ----------------------------------------------------------------------
static void WorkerThreadFunc() {
    while (monitorRunning) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCV.wait(lock, [] { return !fileQueue.empty() || !monitorRunning; });
        if (!monitorRunning) break;
        QueuedFile qf = fileQueue.front();
        fileQueue.pop();
        lock.unlock();
        ProcessNewFile(qf.path, qf.wasBusyOnArrival);
    }
}

// ----------------------------------------------------------------------
// Completion routine for ReadDirectoryChangesW (APC)
// ----------------------------------------------------------------------
static VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped) {
    if (dwErrorCode != ERROR_SUCCESS) return;
    if (dwNumberOfBytesTransfered == 0) return;

    FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer;
    while (true) {
        if (pNotify->Action == FILE_ACTION_ADDED) {
            std::wstring wname(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
            std::string name(wname.begin(), wname.end());

            // Skip temporary files
            if (name.size() >= 4 && (name.substr(name.size() - 4) == ".tmp" || name.find("~") != std::string::npos))
                continue;

            // Check file extension - only process .gcode, .nc, .cnc
            std::string ext = fs::path(name).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".gcode" || ext == ".nc" || ext == ".cnc") {
                std::string fullPath = watchedFolder + "\\" + name;

                // Deduplication: ignore if same file was queued within last 2 seconds
                bool duplicate = false;
                {
                    std::lock_guard<std::mutex> lock(recentMutex);
                    auto now = std::chrono::steady_clock::now();
                    auto it = recentlyQueued.find(fullPath);
                    if (it != recentlyQueued.end()) {
                        if (now - it->second < std::chrono::seconds(2)) {
                            duplicate = true;
                        }
                        else {
                            it->second = now;
                        }
                    }
                    else {
                        recentlyQueued[fullPath] = now;
                    }
                }
                if (duplicate) {
                    std::cout << GetTimeStamp() << "[FOLDER] Duplicate event ignored: " << fullPath << std::endl;
                    continue;
                }

                MachineStatusData status = GetMachineStatus(false);
                bool busyNow = status.isBusy();
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    fileQueue.push({ fullPath, busyNow });
                    queueCV.notify_one();
                }
                if (hWakeEvent) {
                    SetEvent(hWakeEvent);
                }
            }
        }
        if (pNotify->NextEntryOffset == 0) break;
        pNotify = (FILE_NOTIFY_INFORMATION*)((BYTE*)pNotify + pNotify->NextEntryOffset);
    }
    if (monitorRunning && hDir != INVALID_HANDLE_VALUE) {
        ZeroMemory(&overlapped, sizeof(overlapped));
        overlapped.hEvent = hCompletionEvent;
        ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME,
            NULL, &overlapped, FileIOCompletionRoutine);
    }
}

// ----------------------------------------------------------------------
// Start monitoring the "Upload" folder (relative to executable)
// ----------------------------------------------------------------------
static void StartFolderMonitor() {
    if (monitorRunning) return;

    try {
        wchar_t exePathW[MAX_PATH];
        GetModuleFileNameW(NULL, exePathW, MAX_PATH);
        fs::path exeDir = fs::path(exePathW).parent_path();
        watchedFolder = (exeDir / "Upload").string();

        if (!std::filesystem::exists(watchedFolder))
            std::filesystem::create_directories(watchedFolder);

        noteFolder = (exeDir / "Upload/Files placed here will be recycled or deleted after processing.").string();
        if (!std::filesystem::exists(noteFolder))
            std::filesystem::create_directories(noteFolder);

        fs::path readmePath = fs::path(watchedFolder) / "Readme.txt";
        std::ofstream readme(readmePath, std::ios::trunc);
        if (readme.is_open()) {
            readme << "Snapmaker LightBurn Host - Upload Folder\n";
            readme << "=======================================\n\n";
            readme << "Files dropped here will be automatically processed:\n";
            readme << "  - .gcode, .nc, .cnc files are sent to the printer.\n";
            readme << "  - After a successful upload, the file is sent to the recycle bin or DELETED.\n";
            readme << "  - Files older than 10 minutes are removed on program start/stop (except Readme.txt).\n\n";
            readme << "Queue behavior when printer is busy:\n";
            readme << "  - If the printer is already printing, new files are queued.\n";
            readme << "  - The system will wait for the printer to become idle, re-checking every 5 minutes.\n";
            readme << "  - It will wait indefinitely; no print will be started while the printer is busy.\n";
            readme << "  - You can start an immediate check by right-clicking the tray icon and selecting 'Process Upload Folder Now'.\n\n";
            readme.close();
        }

        hDir = CreateFileW(std::wstring(watchedFolder.begin(), watchedFolder.end()).c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);
        if (hDir == INVALID_HANDLE_VALUE) {
            std::cerr << GetTimeStamp() << "[FOLDER] Cannot open directory: " << watchedFolder << std::endl;
            return;
        }

        hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        ZeroMemory(&overlapped, sizeof(overlapped));
        overlapped.hEvent = hCompletionEvent;

        hWakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);   // auto-reset event

        monitorRunning = true;

        workerThread = std::thread(WorkerThreadFunc);
        monitorThread = std::thread([]() {
            ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME,
                NULL, &overlapped, FileIOCompletionRoutine);
            while (monitorRunning) {
                DWORD waitRes = WaitForSingleObjectEx(hCompletionEvent, INFINITE, TRUE);
                if (waitRes == WAIT_IO_COMPLETION) continue;
                else if (waitRes == WAIT_OBJECT_0) break;
            }
            });
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[FOLDER] Exception during folder monitor startup: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[FOLDER] Unknown exception during folder monitor startup." << std::endl;
    }
}

static void StopFolderMonitor() {
    if (!monitorRunning) return;
    monitorRunning = false;
    if (hDir != INVALID_HANDLE_VALUE) {
        CancelIoEx(hDir, &overlapped);
        CloseHandle(hDir);
        hDir = INVALID_HANDLE_VALUE;
    }
    if (hCompletionEvent) {
        SetEvent(hCompletionEvent);
        CloseHandle(hCompletionEvent);
        hCompletionEvent = NULL;
    }
    if (hWakeEvent) {
        SetEvent(hWakeEvent);
        CloseHandle(hWakeEvent);
        hWakeEvent = NULL;
    }
    if (monitorThread.joinable()) monitorThread.join();
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queueCV.notify_all();
    }
    if (workerThread.joinable()) workerThread.join();
}

// ----------------------------------------------------------------------
// Tray UI functions
// ----------------------------------------------------------------------
// In tray.cpp (add includes if needed)
static std::vector<std::string> GetAllLocalIPAddresses() {
    std::vector<std::string> ips;
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return ips;
    }

    struct addrinfo hints, * res = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        return ips;
    }

    for (struct addrinfo* curr = res; curr != nullptr; curr = curr->ai_next) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)curr->ai_addr;
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);

        std::string checkIP(ipStr);

        // Filter strictly for private LAN/Wi-Fi subnets
        if (checkIP.rfind("192.168.", 0) == 0 ||
            checkIP.rfind("10.", 0) == 0 ||
            checkIP.rfind("172.", 0) == 0) {
            ips.push_back(checkIP);
        }
    }

    freeaddrinfo(res);

    // Remove duplicates (just in case)
    std::sort(ips.begin(), ips.end());
    ips.erase(std::unique(ips.begin(), ips.end()), ips.end());

    return ips;
}

static void UpdateStatusMenuItem(HMENU hMenu) {
    bool running = HostContext::instance().isUploadServerRunning();
    std::wstring statusText = L"Server status: " + std::wstring(running ? L"Running" : L"Stopped");
    ModifyMenuW(hMenu, ID_TRAY_STATUS, MF_BYCOMMAND | MF_STRING, ID_TRAY_STATUS, statusText.c_str());
}

static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool lightBurnInstalled = false;
    HICON hIcon = NULL;

    static UINT WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    switch (msg) {
    case WM_CREATE:
        orcaPath = GetOrcaSlicerPath();
        curaPath = GetCuraPath();
        lubanPath = GetLubanPath();
        lightBurnInstalled = IsLightBurnInstalled();
        memset(&nid, 0, sizeof(nid));
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
        if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
        nid.hIcon = hIcon;
        wcscpy_s(nid.szTip, L"Snapmaker LightBurn Host (HTTP Bridge)");
        Shell_NotifyIconW(NIM_ADD, &nid);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_URL, L"Open HTTP page in browser");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_STATUS, L"Server status: ...");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_START_SERVER, L"Start HTTP Server");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_STOP_SERVER, L"Stop HTTP Server");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTART, L"Restart HTTP Server");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            if (!watchedFolder.empty()) {
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_FOLDER, L"Open Upload folder");
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_PROCESS_UPLOADS, L"Process Upload Folder Now");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            }
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_CAMERA, L"Launch Camera");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_KASA, L"Launch Kasa (Console)");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_MONITOR, L"Launch Print Monitor (Console)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            if (lightBurnInstalled) {
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_LIGHTBURN, L"Launch LightBurn");
            }
            if (!orcaPath.empty()) {
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_ORCA, L"Launch OrcaSlicer");
            }
            if (!lubanPath.empty()) {
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_LUBAN, L"Launch Snapmaker Luban");
            }
            if (!curaPath.empty()) {
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_LAUNCH_CURA, L"Launch UltiMaker Cura");
            }
            // Web upload and monitor
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_CAPTURE, L"Web Capture");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_CONFIG, L"Web Configuration");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_CONTROL, L"Web Control");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_DISCOVER, L"Web Network Scanner");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_GCODE, L"Web G-code Console");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_MONITOR, L"Web Monitor");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_THICKNESS, L"Web Thickness");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_UPLOAD, L"Web Upload");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_XYZCAL, L"Web XYZ Laser Calibration");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_WEB_ABOUT, L"Web About");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            UpdateStatusMenuItem(hMenu);

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            PostMessage(hwnd, WM_NULL, 0, 0);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN_URL:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_STATUS: {
            bool running = HostContext::instance().isUploadServerRunning();
            std::wstring msg;

            if (running) {
                msg = L"HTTP server is RUNNING on port 8081\n\n";
                msg += L"Local Access: http://localhost:8081\n\n";

                // Get all matching physical LAN / Wi-Fi IP binds
                std::vector<std::string> lanIPs = GetAllLocalIPAddresses();

                if (!lanIPs.empty()) {
                    for (const auto& lanIP : lanIPs) {
                        std::wstring wlanIP(lanIP.begin(), lanIP.end());

                        msg += L"--- Network Adapter [" + wlanIP + L"] ---\n";
                        msg += L"LAN Access: http://" + wlanIP + L":8081\n";
                        msg += L"Mobile Page: http://" + wlanIP + L":8081/mobile\n\n";
                        // Generate QR code for this IP
                        std::string mobileUrl = "http://" + lanIP + ":8081/mobile";
                        std::string qrUrl = "https://api.qrserver.com/v1/create-qr-code/?size=250x250&data=" + mobileUrl;
                        ShellExecuteA(hwnd, "open", qrUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    }
                    //msg += L"Use the matching LAN URL from your mobile device.";
                    msg += L"QR codes generated in browser for all network adapters.\n";
                    msg += L"Scan the one that matches your device's network.";
                }
                else {
                    msg += L"LAN Access: No physical network adapter detected.\n";
                }
            }
            else {
                msg = L"HTTP server is STOPPED";
            }

            MessageBoxW(hwnd, msg.c_str(), L"Server Status", MB_OK | MB_ICONINFORMATION);
        } break;
        case ID_TRAY_STOP_SERVER:
            if (HostContext::instance().isUploadServerRunning()) {
                HostContext::instance().stopUploadServer();
                MessageBoxW(hwnd, L"HTTP server stopped", L"Server Stopped", MB_OK);
            }
            else {
                MessageBoxW(hwnd, L"Server was not running", L"Already Stopped", MB_OK);
            }
            break;
        case ID_TRAY_START_SERVER:
            if (!HostContext::instance().isUploadServerRunning()) {
                if (HostContext::instance().startUploadServer(8081)) {
                    MessageBoxW(hwnd, L"HTTP server started on port 8081", L"Server Started", MB_OK);
                }
                else {
                    MessageBoxW(hwnd, L"Failed to start server (port 8081 may be in use)", L"Error", MB_ICONERROR);
                }
            }
            else {
                MessageBoxW(hwnd, L"Server is already running", L"Already Running", MB_OK);
            }
            break;
        case ID_TRAY_RESTART:
            if (HostContext::instance().isUploadServerRunning()) {
                HostContext::instance().stopUploadServer();
                Sleep(500);
            }
            if (HostContext::instance().startUploadServer(8081)) {
                MessageBoxW(hwnd, L"HTTP server restarted on port 8081", L"Server Restarted", MB_OK);
            }
            else {
                MessageBoxW(hwnd, L"Failed to restart server (port 8081 may be in use)", L"Error", MB_ICONERROR);
            }
            break;
        case ID_TRAY_OPEN_FOLDER: {
            EnsureWatchedFolder();
            std::wstring wfolder(watchedFolder.begin(), watchedFolder.end());
            ShellExecuteW(NULL, L"open", L"explorer.exe", wfolder.c_str(), NULL, SW_SHOWNORMAL);
        } break;
        case ID_TRAY_PROCESS_UPLOADS: {
            if (processingInProgress.exchange(true)) {
                MessageBoxW(hwnd, L"Already processing uploads.", L"Snapmaker Tray", MB_OK);
                break;
            }

            // Wake any waiting loops so they recheck printer status immediately
            if (hWakeEvent) {
                SetEvent(hWakeEvent);
            }

            std::thread([hwnd]() {
                try {
                    ProcessAllExistingFiles();
                }
                catch (const std::exception& e) {
                    std::cerr << GetTimeStamp() << "[TRAY] Exception in processing thread: " << e.what() << std::endl;
                }
                processingInProgress = false;
                }).detach();
        } break;
        case ID_TRAY_LAUNCH_ORCA:
            if (!orcaPath.empty()) ShellExecuteA(NULL, "open", orcaPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_LAUNCH_CURA:
            if (!curaPath.empty()) ShellExecuteA(NULL, "open", curaPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_LAUNCH_LUBAN:
            if (!lubanPath.empty()) ShellExecuteA(NULL, "open", lubanPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_LAUNCH_LIGHTBURN:
            LaunchLightBurn();
            break;
        case ID_TRAY_LAUNCH_MONITOR:
            LaunchMonitorWindow();
            break;
        case ID_TRAY_LAUNCH_KASA:
            LaunchKasa();
            break;
        case ID_TRAY_LAUNCH_CAMERA:
            LaunchCamera();
            break;
        case ID_TRAY_WEB_UPLOAD:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/upload", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_MONITOR:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/monitor", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_CONFIG:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/config", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_THICKNESS:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/thickness", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_CAPTURE:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/capture", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_CONTROL:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/control", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_DISCOVER:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/discover", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_GCODE:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/gcode", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_ABOUT:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/about", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_WEB_XYZCAL:
            ShellExecuteW(NULL, L"open", L"http://localhost:8081/xyzcal", NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }
    if (msg == WM_TASKBARCREATED) {
        if (nid.hWnd) {
            Shell_NotifyIconW(NIM_ADD, &nid);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ----------------------------------------------------------------------
// Main tray entry point
// ----------------------------------------------------------------------
int RunTray() {
    // Single instance check
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"SnapmakerLightBurnHost_Tray_Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path configFile = fs::path(exePath).parent_path() / CONFIG_FILE;
    HostContext::instance().setConfigPath(configFile);

    UserConfig tempConfig;
    LoadLuban(tempConfig);
    UserConfig localConfig;
    LoadConfig(configFile.string(), localConfig);
    UserConfig mergedConfig = localConfig;
    if (mergedConfig.token.empty() && !tempConfig.token.empty())
        mergedConfig.token = tempConfig.token;
    if (mergedConfig.ipAddress.empty() && !tempConfig.ipAddress.empty())
        mergedConfig.ipAddress = tempConfig.ipAddress;
    if (mergedConfig.model == MODEL_UNKNOWN && tempConfig.model != MODEL_UNKNOWN)
        mergedConfig.model = tempConfig.model;

    if (mergedConfig.fileAction.empty()) {
        mergedConfig.fileAction = "recycle";
    }

    HostContext::instance().updateConfig([mergedConfig](UserConfig& cfg) {
        cfg = mergedConfig;
        return true;
        });
    SaveConfig(configFile.string(), false);

    StartFolderMonitor();
    CleanupOldFiles();

    FreeConsole();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = MsgWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SnapmakerTrayMsgClass";
    if (!RegisterClassW(&wc)) return 1;

    hMsgWnd = CreateWindowExW(0, L"SnapmakerTrayMsgClass", L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, wc.hInstance, NULL);
    if (!hMsgWnd) return 1;

    HostContext::instance().startUploadServer(8081);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    StopFolderMonitor();
    CleanupOldFiles();
    HostContext::instance().stopUploadServer();
    return 0;
}