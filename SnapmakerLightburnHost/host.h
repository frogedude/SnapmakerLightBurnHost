// host.h - Master header - all declarations, constants, structs, RAII classes
#pragma once

#define NOMINMAX

// ============================================================================
// Standard C/C++ Libraries
// ============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <cctype>
#include <regex>
#include <mutex>
#include <functional>
#include <conio.h>
#include <cstring>
#include <limits>

// ============================================================================
// Third-party Libraries
// ============================================================================
#include <curl/curl.h>

// ============================================================================
// Windows-specific Headers
// ============================================================================
#include <windows.h>
#include <shlobj.h>
#include <signal.h>

// ============================================================================
// Project-specific Headers (with warning suppression)
// ============================================================================
#pragma warning(push, 0)
#include "softcam.h"
#include "stb_image.h"
#include "stb_image_resize.h"
#include "nlohmann/json.hpp"
#pragma warning(pop)

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Constants
// ============================================================================

// Keyboard input
constexpr char ENTER_ASCII = 13;
constexpr char ESC_ASCII = 27;
constexpr char SPACE_ASCII = 32;
constexpr int ARROW_PREFIX = 224;
constexpr char LEFT_ARROW_KEY = 75;
constexpr char RIGHT_ARROW_KEY = 77;
constexpr char UP_ARROW_KEY = 72;
constexpr char DOWN_ARROW_KEY = 80;

// Timing & delays
constexpr int DEFAULT_FPS = 5;
constexpr int FRAME_DELAY_MS = 16;
constexpr int AUTO_REFRESH_INTERVAL_SECONDS = 1;
constexpr int GCODE_DELAY_MS = 200;

// CURL timeouts
constexpr int CURL_TIMEOUT = 30;
constexpr int CURL_HOMING_TIMEOUT = 45;
constexpr int CURL_CONNECT_TIMEOUT = 3;
constexpr int CURL_UPLOAD_TIMEOUT = 60;

// Retry & confirmation
constexpr int MAX_RETRY_ATTEMPTS = 1;
constexpr int HOMING_RETRY_ATTEMPTS = 1;
constexpr int CONFIRMATION_TIMEOUT_SECONDS = 60;

// Snapmaker A350 defaults
constexpr double A350_BASE_POS_X = 232.0;
constexpr double A350_BASE_POS_Y = 178.0;
constexpr double A350_BASE_POS_Z = 290.0;

// Snapmaker A250 defaults
constexpr double A250_BASE_POS_X = 160.0;
constexpr double A250_BASE_POS_Y = 120.0;
constexpr double A250_BASE_POS_Z = 175.0;

// Material thickness
constexpr double DEFAULT_MATERIAL_THICKNESS_CUSTOM_X = 99.0;
constexpr double DEFAULT_MATERIAL_THICKNESS_CUSTOM_Y = 22.0;
constexpr double MATERIAL_THICKNESS_FEED_RATE = 3000;

// Camera
constexpr double CAMERA_CAPTURE_FEED_RATE = 3000;
constexpr int CAMERA_WIDTH = 1024;
constexpr int CAMERA_HEIGHT = 1280;
constexpr int MAX_ADJUST_PIXELS = 96;
constexpr int ADJUST_STEP_PIXELS = 1;

// File paths & registry keys
constexpr char CONFIG_FILE[] = "config.json";
constexpr char SOFTCAM_REGISTRY_KEY1[] = "CLSID\\{AEF3B972-5FA5-4647-9571-358EB472BC9E}\\InprocServer32";
constexpr char SOFTCAM_REGISTRY_KEY2[] = "SOFTWARE\\Classes\\CLSID\\{AEF3B972-5FA5-4647-9571-358EB472BC9E}\\InprocServer32";
constexpr char LightBurn_REGISTRY_KEY[] = "LightBurn.LightBurn.1\\Shell\\Open\\Command";
constexpr char LightBurn_PREFS_PATH[] = "LightBurn\\prefs.ini";
constexpr char LUBAN_CONFIG_PATH[] = "snapmaker-luban\\machine.json";
constexpr char MACROS_FOLDER[] = "macros";

// UI & display limits
constexpr int DEFAULT_POLL_INTERVAL_SECONDS = 2;
constexpr int AUTO_REFRESH_LINE_LIMIT = 15;
constexpr int MAIN_MENU_LINE_LIMIT = 15;
constexpr int INITIAL_FRAME_DISPLAY_SECONDS = 5;
constexpr int STATUS_REFRESH_COOLDOWN_SECONDS = 2;
constexpr int MACHINE_SETUP_DELAY_SECONDS = 1;
constexpr int POST_HOMING_DELAY_SECONDS = 1;
constexpr int POST_CONNECTION_DELAY_MS = 100;
constexpr int AUTO_REFRESH_CAPTURE_DELAY_SECONDS = 2;
constexpr int RETRY_DELAY_SECONDS = 2;
constexpr int ERROR_DISPLAY_SECONDS = 3;

// Line count thresholds for screen clearing
constexpr int MANUAL_CAPTURE_EXTRA_LINES = 1;
constexpr int MATERIAL_THICKNESS_EXTRA_LINES = 2;
constexpr int RESET_BASE_POSITION_EXTRA_LINES = 3;
constexpr int MACHINE_SETUP_EXTRA_LINES = 4;
constexpr int ADJUSTMENT_EXTRA_LINES = 1;
constexpr int STATUS_DISPLAY_EXTRA_LINES = 4;
constexpr int UPLOAD_PROGRESS_BAR_WIDTH = 40;
constexpr int SHIFT_DETECTION_TIMEOUT_SECONDS = 3;

// File operation modes
constexpr int FILEOP_UPLOAD_ONLY = 0;
constexpr int FILEOP_AUTO_START = 1;
constexpr int FILEOP_SMART = 2;

// Image adjustment saves
static constexpr int SAVE_DEBOUNCE_SECONDS = 5;

// Kasa integration constants
constexpr char HS100_EXE_PATH[] = "hs100\\hs100.exe";
constexpr int KASA_WAIT_FOR_POWER_SWITCH_MILLISECONDS = 500;
constexpr int KASA_COMMAND_TIMEOUT_MS = 5000;
constexpr int MAX_WAIT_SECONDS = 60;
constexpr int POLL_INTERVAL_SECONDS = 5;

// Status checker keep alive constants
constexpr int STATUS_CHECKER_SHUTDOWN_TIMEOUT_SECONDS = 2;
constexpr int STATUS_CHECKER_POLL_INTERVAL_MS = 2000;
constexpr int LARGE_FILE_THRESHOLD_MB = 15;
constexpr size_t LARGE_FILE_THRESHOLD_BYTES = LARGE_FILE_THRESHOLD_MB * 1024ULL * 1024ULL;

// Print monitoring
constexpr int POLL_INTERVAL_PRINTING_SECONDS = 2;
constexpr int POLL_INTERVAL_PAUSED_SECONDS = 360;

// HTTP Upload Server
constexpr int UPLOAD_SERVER_DEFAULT_PORT = 8081;
constexpr char TEMP_UPLOAD_FOLDER[] = "snapmaker_upload";

// Web Authentication
constexpr char DEFAULT_WEB_USER[] = "admin";
constexpr char DEFAULT_WEB_PASSWORD[] = "Snapmaker_Bridge";

// ============================================================================
// Supported File Extensions
// ============================================================================
inline const std::vector<std::string> MACRO_EXTENSIONS = { ".txt" };
inline const std::vector<std::string> SUPPORTED_EXTENSIONS_LASER = { ".nc" };
inline const std::vector<std::string> SUPPORTED_EXTENSIONS_3DP = { ".gcode" };
inline const std::vector<std::string> SUPPORTED_EXTENSIONS_CNC = { ".cnc" };
inline const std::vector<std::string> SUPPORTED_EXTENSIONS_FIRMWARE = { ".bin" };
inline const std::vector<std::string> SUPPORTED_EXTENSIONS = [] {
    std::vector<std::string> all;
    for (const auto& ext : SUPPORTED_EXTENSIONS_LASER) all.push_back(ext);
    for (const auto& ext : SUPPORTED_EXTENSIONS_3DP) all.push_back(ext);
    for (const auto& ext : SUPPORTED_EXTENSIONS_CNC) all.push_back(ext);
    for (const auto& ext : SUPPORTED_EXTENSIONS_FIRMWARE) all.push_back(ext);
    return all;
    }();

// ============================================================================
// Enums
// ============================================================================
enum SnapmakerModel { MODEL_A350, MODEL_A250, MODEL_UNKNOWN };
enum MachineStatus { STATUS_UNKNOWN, STATUS_IDLE, STATUS_RUNNING, STATUS_PAUSED, STATUS_OFFLINE };
enum MaterialThicknessMode { MODE_DEFAULT, MODE_CUSTOM };
enum ModuleType { MODULE_LASER, MODULE_3DP, MODULE_CNC, MODULE_UNKNOWN };

// ============================================================================
// Shortcut Enums and Structures
// ============================================================================
enum class ShortcutAction {
    NONE,
    HOME,
    LASERSETUP,
    PRINTERSETUP,
    STATUS,
    KASA,
    KASA_ON,
    KASA_OFF,
    SETKASAAUTOOFF,
    MACROS,
    GCODE,
    THICKNESS,
    CAPTURE,
    AUTOCAPTURE,
    RESETCONFIG,
    RESETTOKEN,
    UPLOAD,
    PRINT,
    CONNECT,
    SETTOKEN,
    SETKASAIP,
    SETKASAAUTO,
    SETAUTOSTART,
    AUTOMONITOR_ON,
    AUTOMONITOR_OFF,
    SETFILEACTION,
    FULLCONFIGURE,
    HELP,
    ABOUT,
    ENCLOSUREFAN,
    ENCLOSURELED,
    STOP,
    PAUSE,
    RESUME,
    MONITOR,
    SOFTCAMCHECK,
    LIGHTBURN,
    LIGHTBURNCAMERA,
    CAMERA,
    CAMERASETTINGS,
    BEEP_ON,
    BEEP_OFF,
    EMAILCONFIG,
    EMAIL_ON,
    EMAIL_OFF,
    UPLOADSERVER_START,
    TRAY,
    SERVER_BIND_LOCALHOST,
    SERVER_BIND_GLOBAL,
    SETWEBAUTH,
    SETWEBUSER,
    SETWEBPASSWORD
};

struct ShortcutParams {
    ShortcutAction action;
    std::string ipAddress;
    std::string token;
    std::string kasaIp;
    std::string filename;
    std::string parameter;
    bool autoPowerOn;
    bool hasAutoPowerOn;
    bool hasFilename;
    bool hasParameter;

    ShortcutParams() : action(ShortcutAction::NONE), autoPowerOn(false),
        hasAutoPowerOn(false), hasFilename(false), hasParameter(false) {
    }
};

// ============================================================================
// Config Structures
// ============================================================================
struct ImageAdjustmentConfig {
    int pixelsX = 0;
    int pixelsY = 0;
    bool useAdjustment = false;
};

struct MaterialThicknessConfig {
    MaterialThicknessMode mode = MODE_DEFAULT;
    double customX = DEFAULT_MATERIAL_THICKNESS_CUSTOM_X;
    double customY = DEFAULT_MATERIAL_THICKNESS_CUSTOM_Y;
};

struct KasaConfig {
    std::string ipAddress;
    bool autoPowerOn = false;
    bool autoPowerOff = false;
};

struct RtspCameraConfig {
    bool autoLaunch = false;
    std::string rtspUrl;
};

struct UserConfig {
    // Machine connection
    std::string ipAddress;
    std::string token;
    SnapmakerModel model = MODEL_UNKNOWN;

    // Base positions
    double basePositionX = 0.0;
    double basePositionY = 0.0;
    double basePositionZ = 0.0;

    // Feature configs
    ImageAdjustmentConfig adjustment;
    MaterialThicknessConfig thickness;
    double laserOffsetZ = 0.0;
    KasaConfig kasa;
    bool autoStartPrint = false;
    bool autoMonitorPrint = true;
    RtspCameraConfig rtspCamera;
    std::string fileAction;

    // Notifications
    bool enableEmailAlerts = false;
    bool enableSoundAlerts = true;
    std::string emailRecipient;
    std::string emailSender;
    std::string emailAppPassword;
    std::string emailSmtpServer = "smtp.gmail.com";
    int emailSmtpPort = 587;

    bool webAuthEnabled = true;
    std::string webUser = DEFAULT_WEB_USER;
    std::string webPassword = DEFAULT_WEB_PASSWORD;
    bool bindLocalhost = true;
};

// ============================================================================
// Machine Status Data
// ============================================================================
struct MachineStatusData {
    // Core status
    std::string status = "UNKNOWN";
    double x = 0, y = 0, z = 0;
    bool homed = false;
    double offsetX = 0, offsetY = 0, offsetZ = 0;
    std::string toolHead = "Unknown";

    // Laser specific
    int laserFocalLength = 0;
    int laserPower = 0;
    bool laserCamera = false;
    int laser10WErrorState = 0;

    // CNC specific
    int spindleSpeed = 0;
    int workSpeed = 0;

    // 3D Printer specific
    std::string printStatus = "Unknown";
    int nozzleTemperature1 = 0, nozzleTargetTemperature1 = 0;
    int nozzleTemperature2 = 0, nozzleTargetTemperature2 = 0;
    int heatedBedTemperature = 0, heatedBedTargetTemperature = 0;
    bool isFilamentOut = false;

    // Modules
    bool enclosure = false, rotaryModule = false, emergencyStopButton = false;
    bool airPurifier = false, isEnclosureDoorOpen = false;
    int doorSwitchCount = 0;

    // Print monitoring
    std::string fileName = "";
    int totalLines = 0;
    int currentLine = 0;
    double estimatedTime = 0.0;
    double elapsedTime = 0.0;
    double remainingTime = 0.0;
    double progress = 0.0;

    // Constructors
    MachineStatusData() = default;
    static MachineStatusData CreateOffline();
    static MachineStatusData CreateError(const std::string& errorMsg = "ERROR");
    void reset();

    // Member functions
    MachineStatus getMachineStatus() const;
    std::string getStatusDisplay() const;
    std::string getToolHeadDisplay() const;
    bool isBusy() const;
    bool is3DPrinter() const;
    bool isLaser() const;
    bool isCNC() const;
    int getProgressPercent() const { return static_cast<int>(progress * 100); }
    std::string getFormattedTimeRemaining() const;
};

// ============================================================================
// Progress Data
// ============================================================================
struct ProgressData {
    std::string filename;
    double lastPercent = 0;
};

// ============================================================================
// StatusChecker Keep Alive Class (RAII)
// ============================================================================
class StatusChecker {
private:
    std::thread worker;
    std::atomic<bool> stopFlag;
    std::string ip;
    std::string token;
    std::mutex mutex;

public:
    StatusChecker();
    ~StatusChecker();
    StatusChecker(const StatusChecker&) = delete;
    StatusChecker& operator=(const StatusChecker&) = delete;
    StatusChecker(StatusChecker&&) = delete;
    StatusChecker& operator=(StatusChecker&&) = delete;

    void start(const std::string& machineIp, const std::string& machineToken);
    void stop();
    bool isActive() const;
};

// ============================================================================
// StatusCheckerGuard RAII Wrapper
// ============================================================================
class StatusCheckerGuard {
private:
    std::unique_ptr<StatusChecker> checker;
    std::string ip;
    std::string token;
    bool active;

public:
    StatusCheckerGuard(const std::string& machineIp, const std::string& machineToken);
    ~StatusCheckerGuard();
    void stop();
    bool isActive() const;
    StatusChecker* operator->();
    const StatusChecker* operator->() const;

    StatusCheckerGuard(const StatusCheckerGuard&) = delete;
    StatusCheckerGuard& operator=(const StatusCheckerGuard&) = delete;
    StatusCheckerGuard(StatusCheckerGuard&& other) noexcept;
    StatusCheckerGuard& operator=(StatusCheckerGuard&& other) noexcept;
};

// ============================================================================
// Registry Key RAII
// ============================================================================
class RegKey {
private:
    HKEY hKey;
    bool isValid;

public:
    RegKey();
    RegKey(HKEY rootKey, const std::string& subKey, REGSAM samDesired = KEY_READ);
    RegKey(HKEY rootKey, const std::string& subKey, const std::string& valueName,
        DWORD type, const void* data, DWORD dataSize);
    ~RegKey();

    RegKey(RegKey&& other) noexcept;
    RegKey& operator=(RegKey&& other) noexcept;
    RegKey(const RegKey&) = delete;
    RegKey& operator=(const RegKey&) = delete;

    HKEY get() const;
    bool valid() const;
    explicit operator bool() const;
    operator HKEY() const;

    std::string getStringValue(const std::string& valueName = "") const;
    DWORD getDWordValue(const std::string& valueName = "") const;
};

// ============================================================================
// Process Handle RAII
// ============================================================================
class ProcessHandle {
private:
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hPipe;
    DWORD processId;
    bool isValid;

public:
    ProcessHandle();
    ProcessHandle(const std::string& commandLine, bool createWindow = false,
        bool captureOutput = false, HANDLE* outReadPipe = nullptr);
    ~ProcessHandle();

    ProcessHandle(ProcessHandle&& other) noexcept;
    ProcessHandle& operator=(ProcessHandle&& other) noexcept;
    ProcessHandle(const ProcessHandle&) = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;

    HANDLE get() const;
    HANDLE getThread() const;
    HANDLE getPipe() const;
    DWORD getId() const;
    bool valid() const;
    explicit operator bool() const;

    bool readPipeOutput(char* buffer, DWORD bufferSize, DWORD& bytesRead, DWORD timeoutMs = 100);
    DWORD wait(DWORD timeoutMs = INFINITE) const;
    bool isRunning() const;
    bool terminate(DWORD exitCode = 1);
    void terminateFast();
    bool getExitCode(DWORD& exitCode) const;
};

// ============================================================================
// Console State RAII
// ============================================================================
class ConsoleGuard {
private:
    HANDLE hStdOut;
    HANDLE hStdIn;
    DWORD originalOutMode;
    DWORD originalInMode;
    bool outModeSaved;
    bool inModeSaved;
    bool vtEnabled;

public:
    ConsoleGuard();
    ~ConsoleGuard();

    ConsoleGuard(const ConsoleGuard&) = delete;
    ConsoleGuard& operator=(const ConsoleGuard&) = delete;
    ConsoleGuard(ConsoleGuard&&) = delete;
    ConsoleGuard& operator=(ConsoleGuard&&) = delete;

    // Instance methods
    void setTitle(const std::string& title);
    void setColor(WORD attributes);
    void clearScreen();
    void setWindowSize(int cols, int rows);
    void flushInputBuffer();
    bool hasVT() const;

    // Static utility methods
    static void ClearScreenWin32();
    static void FlushInputBuffers();
    static void SetConsoleTitleStatic(const std::string& title);
    static void SetConsoleColor(WORD attributes);
    static void SetConsoleWindowSize(int cols, int rows);

private:
    void FlushInputBufferInternal();
};

// ============================================================================
// HostContext Singleton
// ============================================================================
class HostContext {
private:
    HostContext() = default;
    ~HostContext() = default;
    HostContext(const HostContext&) = delete;
    HostContext& operator=(const HostContext&) = delete;

    mutable std::mutex mtx;
    mutable std::mutex adjustmentMtx;
    UserConfig config;
    std::vector<std::string> droppedFiles;
    std::atomic<bool> run{ true };
    MachineStatusData lastStatus;
    std::chrono::steady_clock::time_point lastStatusUpdate = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastSaveTime = std::chrono::steady_clock::now();
    fs::path configFile;
    std::unique_ptr<class ProcessHandle> cameraProcess;

public:
    static HostContext& instance();

    // Configuration
    UserConfig snapshotConfig() const;
    bool updateConfig(std::function<bool(UserConfig&)> updater);
    void setConfigPath(const fs::path& path);
    fs::path getConfigPath() const;

    // Dropped files
    std::vector<std::string> acquireDroppedFiles();
    bool hasDroppedFiles() const;
    size_t droppedFileCount() const;
    std::string peekDroppedFile(size_t index) const;
    void addDroppedFile(const std::string& filename);
    void clearDroppedFiles();

    // Machine status
    MachineStatusData snapshotStatus() const;
    void setLastStatus(const MachineStatusData& status);
    bool isStatusStale() const;
    std::chrono::steady_clock::time_point getLastStatusUpdate() const;

    // Lifecycle
    bool isRunning() const;
    void stopRunning();
    void requestShutdown();
    void waitForShutdown();
    bool isShutdownRequested() const;
    void shutdown();

    // Save debounce
    void updateLastSaveTime();
    bool shouldDebounceSave() const;
    std::chrono::steady_clock::time_point getLastSaveTime() const;

    // Camera management
    bool startCamera(const std::string& rtspUrl);
    bool startCameraDetached(const std::string& rtspUrl);
    void stopCamera();
    bool isCameraRunning() const;

    // HTTP Upload Server Management
    bool startUploadServer(int port = 8081);
    void stopUploadServer();
    bool isUploadServerRunning() const;
};

// ============================================================================
// Global Flags
// ============================================================================
extern std::atomic<bool> g_kasaModeActive;
extern std::atomic<bool> g_shutdownRequested;

inline bool IsKasaModeActive() {
    return g_kasaModeActive.load(std::memory_order_acquire);
}

inline void SetKasaModeActive(bool active) {
    g_kasaModeActive.store(active, std::memory_order_release);
}

inline bool IsRunning() {
    return HostContext::instance().isRunning() &&
        !g_shutdownRequested.load(std::memory_order_acquire);
}

// ============================================================================
// KasaModeGuard RAII
// ============================================================================
class KasaModeGuard {
public:
    KasaModeGuard() { SetKasaModeActive(true); }
    ~KasaModeGuard() { SetKasaModeActive(false); }
    KasaModeGuard(const KasaModeGuard&) = delete;
    KasaModeGuard& operator=(const KasaModeGuard&) = delete;
};

// ============================================================================
// URL Builders (inline)
// ============================================================================
inline std::string BuildApiUrl(const std::string& ip, const std::string& path) {
    return "http://" + ip + ":8080" + path;
}

inline std::string BuildConnectUrl(const std::string& ip, const std::string& token = "") {
    std::string url = BuildApiUrl(ip, "/api/v1/connect");
    if (!token.empty()) url += "?token=" + token;
    return url;
}

inline std::string BuildStatusUrl(const std::string& ip, const std::string& token) {
    return BuildApiUrl(ip, "/api/v1/status?token=" + token);
}

inline std::string BuildExecuteCodeUrl(const std::string& ip, const std::string& token,
    const std::string& encodedGCode) {
    return BuildApiUrl(ip, "/api/v1/execute_code?token=" + token + "&code=" + encodedGCode);
}

inline std::string BuildUploadUrl(const std::string& ip, const std::string& token) {
    return BuildApiUrl(ip, "/api/v1/upload?token=" + token);
}

inline std::string BuildPreparePrintUrl(const std::string& ip, const std::string& token,
    const std::string& printType) {
    return BuildApiUrl(ip, "/api/v1/prepare_print?token=" + token + "&type=" + printType);
}

inline std::string BuildStartPrintUrl(const std::string& ip, const std::string& token) {
    return BuildApiUrl(ip, "/api/v1/start_print?token=" + token);
}

inline std::string BuildCapturePhotoUrl(const std::string& ip, double x, double y, double z,
    double feedRate = CAMERA_CAPTURE_FEED_RATE) {
    return BuildApiUrl(ip, "/api/request_capture_photo?index=0&x=" + std::to_string(static_cast<int>(x)) +
        "&y=" + std::to_string(static_cast<int>(y)) +
        "&z=" + std::to_string(static_cast<int>(z)) +
        "&feedRate=" + std::to_string(static_cast<int>(feedRate)) + "&photoQuality=0");
}

inline std::string BuildGetCameraImageUrl(const std::string& ip) {
    return BuildApiUrl(ip, "/api/get_camera_image?index=0");
}

inline std::string BuildMaterialThicknessUrl(const std::string& ip, double x, double y,
    double feedRate = MATERIAL_THICKNESS_FEED_RATE) {
    return BuildApiUrl(ip, "/api/request_Laser_Material_Thickness?x=" + std::to_string(static_cast<int>(x)) +
        "&y=" + std::to_string(static_cast<int>(y)) + "&feedRate=" + std::to_string(static_cast<int>(feedRate)));
}

// ============================================================================
// RTSP Camera Helpers
// ============================================================================
std::string GetVlcPath();
bool CheckVlcInstalled();

inline bool IsRtspCameraConfigured(const RtspCameraConfig& cfg) {
    return !cfg.rtspUrl.empty() && CheckVlcInstalled();
}

// ============================================================================
// CURL RAII Helpers
// ============================================================================
class CurlSession {
private:
    CURL* curl;
    std::atomic<bool>* abortFlag;
    static int abort_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
        curl_off_t ultotal, curl_off_t ulnow);
public:
    CurlSession();
    ~CurlSession();
    CURL* get() const;
    operator CURL* () const;
    explicit operator bool() const;
    CurlSession(const CurlSession&) = delete;
    CurlSession& operator=(const CurlSession&) = delete;
    CurlSession(CurlSession&& other) noexcept;
    CurlSession& operator=(CurlSession&& other) noexcept;
    void setAbortFlag(std::atomic<bool>* flag);
};

struct CurlSlistGuard {
    curl_slist* list;
    CurlSlistGuard();
    ~CurlSlistGuard();
    CurlSlistGuard(const CurlSlistGuard&) = delete;
    CurlSlistGuard& operator=(const CurlSlistGuard&) = delete;
    CurlSlistGuard(CurlSlistGuard&& other) noexcept;
    CurlSlistGuard& operator=(CurlSlistGuard&& other) noexcept;
    void append(const char* str);
    curl_slist* get() const;
};

struct CurlProgressGuard {
    CURL* curl;
    bool active;
    CurlProgressGuard(CURL* c, void* callback, void* userdata = nullptr);
    ~CurlProgressGuard();
    CurlProgressGuard(const CurlProgressGuard&) = delete;
    CurlProgressGuard& operator=(const CurlProgressGuard&) = delete;
};

struct FormPostGuard {
    struct curl_httppost* form;
    FormPostGuard();
    explicit FormPostGuard(struct curl_httppost* f);
    ~FormPostGuard();
    FormPostGuard(const FormPostGuard&) = delete;
    FormPostGuard& operator=(const FormPostGuard&) = delete;
    FormPostGuard(FormPostGuard&& other) noexcept;
    FormPostGuard& operator=(FormPostGuard&& other) noexcept;
    struct curl_httppost* get() const;
    void reset(struct curl_httppost* f = nullptr);
};

class CurlGlobalInit {
public:
    CurlGlobalInit();
    ~CurlGlobalInit();
    CurlGlobalInit(const CurlGlobalInit&) = delete;
    CurlGlobalInit& operator=(const CurlGlobalInit&) = delete;
};

// ============================================================================
// Camera Classes
// ============================================================================
class ImageBuffer {
private:
    std::unique_ptr<unsigned char[]> data;
    size_t size;
public:
    ImageBuffer();
    explicit ImageBuffer(size_t bufferSize);
    ImageBuffer(ImageBuffer&& other) noexcept;
    ImageBuffer& operator=(ImageBuffer&& other) noexcept;
    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;
    explicit operator bool() const noexcept;
    unsigned char* get() const;
    size_t getSize() const;
    void reset(size_t newSize = 0);
};

class CameraSession {
private:
    using CameraHandle = void*;
    CameraHandle cam = nullptr;
public:
    CameraSession();
    CameraSession(int width, int height, int fps);
    ~CameraSession();
    CameraSession(const CameraSession&) = delete;
    CameraSession& operator=(const CameraSession&) = delete;
    CameraSession(CameraSession&& other) noexcept;
    CameraSession& operator=(CameraSession&& other) noexcept;
    bool create(int width, int height, int fps);
    void* get() const;
    explicit operator bool() const;
    bool sendFrame(const unsigned char* frame) const;
    void release();
};

// ============================================================================
// Callback Functions (CURL progress, abortable)
// ============================================================================
int abortable_progress(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);
int abortable_progress_wrapper(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);

// ============================================================================
// Function Declarations - Grouped by Category
// ============================================================================

// --- UI / Display -----------------------------------------------------------
void ClearConsole();
void PrintHeader(const std::string& text, int width = 108, char fillChar = '=');
void ClearAndDrawHeader(bool showHeaderMenu = false);
void DisplayMachineStatusPage(bool fromShortcut = false);
void AutoRefreshLoop(CameraSession& cam, std::string& imgFile, ImageBuffer& currentImg,
    int& w, int& h, int& channels);
void UpdateBasePosition();
void ResetBasePositionToDefault();
void KasaInteractiveMode(bool fromShortcut = false);
void CameraInteractiveMode(bool fromShortcut = false);
void DisplayAbout();
bool SelectSnapmakerModel(SnapmakerModel& model);
std::string GetModelName(SnapmakerModel model);
void ConfigureRtspCamera();

// --- Utils ------------------------------------------------------------------
std::string GetTimeStamp();
std::string WStringToString(const std::wstring& wstr);
std::string CleanFilePath(const std::string& path);
void FlushInputBuffer();
bool IsValidIPAddress(const std::string& ip);
bool IsSupportedFile(const std::string& filename);
bool IsMacroFile(const std::string& filename);
bool IsFirmwareFile(const std::string& filename);
void CheckCommandLineFiles();
bool CheckForShiftKey(int timeoutSeconds);

// --- Network ----------------------------------------------------------------
std::pair<CURLcode, long> PerformHttpPostWithCode(const std::string& url,
    const std::string& postData, std::string& response, long timeout = CURL_TIMEOUT);
std::pair<CURLcode, long> PerformHttpGetWithCode(const std::string& url,
    std::string& response, long timeout = CURL_TIMEOUT);
size_t json_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
size_t data_write(void* buf, size_t size, size_t nmemb, void* userp);
int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow);
bool ConnectionSetup(const std::string& preIp = "", bool isRenewal = false);
void ThrottledConnect();

// --- Machine API ------------------------------------------------------------
MachineStatusData GetMachineStatus(bool useCache = false);
bool IsMachineOffline(const MachineStatusData& status);
bool CheckMachineHomed();
std::string GetMachineToolHead();
void PerformMachineSetup();
void PerformLaserSetup();
void PerformPrintSetup();
void PerformFirstTimeWarning();
CURLcode GetMaterialThicknessFromSnapmaker(double& outThickness);
void ToggleMaterialThicknessMode();
void UpdateMaterialThicknessCustomCoordinates();
void ResetMaterialThicknessCustomCoordinates();
std::string GetMaterialThicknessModeName();
std::string GetMaterialThicknessModeDetails();
bool CheckMachineHomedAndWarn();
bool CheckLaserInstalledAndWarn();
std::pair<bool, std::string> SetEnclosureFanSpeed(const std::string& speedInput);
std::pair<bool, std::string> SetEnclosureLed(const std::string& brightnessInput);
CURLcode ConnectAndMaintainConnection();

// --- G-code Commands --------------------------------------------------------
CURLcode ConnectForGCode();
CURLcode ConnectAndSendGCode(const std::string& gcode, bool isHoming = false);
CURLcode SendGCodeWithRetry(const std::string& gcode, int maxRetries = MAX_RETRY_ATTEMPTS,
    bool isHoming = false);
CURLcode SendRawGCode(const std::string& gcode);
CURLcode SendRawGCodeWithResponse(const std::string& gcode, std::string& response);
void HomeMachine();
std::pair<bool, std::string> PausePrint();
std::pair<bool, std::string> ResumePrint();
std::pair<bool, std::string> StopPrint();
std::pair<bool, std::string> RePrint();
std::vector<std::pair<std::string, std::string>> ExecuteMacroWithResponse(const std::string& filepath);

// --- Print Monitoring -------------------------------------------------------
void MonitorPrintWithProgress(bool skipInitialCheck = false);
void PlayCompletionBeep();
void PlayAlertChime();
void SendEmailNotification(const std::string& subject, const std::string& body);
void ConfigureEmailSettings();

// --- Camera -----------------------------------------------------------------
std::string CaptureURL();
std::string DownloadURL();
bool CaptureImage(const std::string& file);
bool CaptureAndUpdateImage(const std::string& imgFile, ImageBuffer& currentImg,
    int& width, int& height, bool verbose = true);
ImageBuffer ResizeImage(const unsigned char* src, int srcW, int srcH, int dstW, int dstH);
ImageBuffer CreateEnhancedTestPattern(int width, int height);
ImageBuffer ApplyImageAdjustment(const unsigned char* src, int width, int height,
    int channels, int offsetX, int offsetY);
void OpenLatestImage();

// --- Image Adjustment -------------------------------------------------------
void AdjustImageLeft();
void AdjustImageRight();
void AdjustImageUp();
void AdjustImageDown();
void ResetImageAdjustment();
void ToggleImageAdjustment();
void DisplayAdjustmentStatus();

// --- Config -----------------------------------------------------------------
bool LoadLuban(UserConfig& cfg);
bool LoadConfig(const std::string& file, UserConfig& cfg);
void SaveConfig(const std::string& file, bool verbose = true);

// --- Macros -----------------------------------------------------------------
void CreateMacrosFolder();
std::vector<std::pair<int, std::string>> LoadMacros();
void SendCustomGCodeMenu(bool fromShortcut = false);
void SendMacroFile(const std::string& filepath);
void SendManualGCode();

// --- File Processing --------------------------------------------------------
bool ProcessFileUpload(const std::string& filepath, int operation, bool interactive = true);
void ProcessFileWithMode(const std::string& filepath, int mode, bool waitForKey = false);
CURLcode PreparePrintOnSnapmaker(const std::string& filepath, const std::string& printType,
    std::string& response, long timeout = CURL_UPLOAD_TIMEOUT);
CURLcode UploadFileToSnapmaker(const std::string& filepath, std::string& response, long timeout);

// --- Module Detection -------------------------------------------------------
ModuleType GetModuleTypeFromToolHead(const std::string& toolHead);
bool IsFileCompatibleWithModule(const std::string& filename, ModuleType module);
std::string GetExpectedExtensions(ModuleType module);

// --- G-code Generation ------------------------------------------------------
void RegenerateGCodeFiles(double staticBufferZ, double laserFocalLength, double zFocusHeight);

// --- LightBurn Integration --------------------------------------------------
bool LaunchLightBurn();
bool CheckLightBurnCameraSetting();
bool CheckSoftcamRegistry();

// --- Kasa Integration -------------------------------------------------------
bool ExecuteHs100Command(const std::string& args, std::string& output, int timeoutMs = KASA_COMMAND_TIMEOUT_MS);
bool CheckKasaPlugStatus(const std::string& ip, bool& isOn);
bool SetKasaPlugState(const std::string& ip, bool on);
bool AutoPowerOnKasaIfNeeded();

// --- Shortcuts --------------------------------------------------------------
ShortcutParams ParseShortcutWithParams(int argc, LPWSTR* argv);
void ExecuteShortcut(const ShortcutParams& params);
void DisplayHelp();
int RunTray();

// --- Console Handler --------------------------------------------------------
BOOL WINAPI ConsoleHandler(DWORD signal);

// --- Recycle Handler --------------------------------------------------------
bool MoveToRecycleBin(const std::string& filePath);
fs::path GetSnapmakerTempPath();
