// upload_server.hpp
#pragma once

#include "host.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

// MultipartData structure
struct MultipartData {
    std::string filename;
    std::vector<char> content;
    bool printAfterUpload = false;
};

// Forward declarations
class FileUploadServer;

// Shared globals - defined in upload_server.cpp
extern std::string g_selectedFilename;
extern std::mutex g_selectedFileMutex;
extern std::chrono::steady_clock::time_point g_lastPrintStart;
extern std::mutex g_lastPrintMutex;
extern std::string g_lastUploadedFilename;
extern std::mutex g_lastUploadedFilenameMutex;
extern std::mutex g_cameraFeedMutex;

// Camera feed accessor (defined in upload_server.cpp)
class CameraSession;
std::unique_ptr<CameraSession>& GetHttpCameraFeed();
void SetHttpCameraFeed(std::unique_ptr<CameraSession> feed);

// Helper functions
void SendHttpResponse(SOCKET clientSocket, int statusCode, const std::string& contentType, const std::string& body);
void SendSimpleResponse(SOCKET clientSocket, int statusCode, const std::string& message);
void SendOctoPrintVersionResponse(SOCKET clientSocket);
void SendOctoPrintConnectionResponse(SOCKET clientSocket);
void LaunchProcess(const std::string& command, const std::string& filePath);
void LaunchMonitorWindow();
std::string ExtractApiKeyFromHeaders(const std::string& headers);
bool ParseMultipartForm(const std::string& request, const std::vector<char>& rawBuffer, int bytesReceived, MultipartData& outData);
bool ReadCompleteRequest(SOCKET clientSocket, std::vector<char>& fullRequest, std::string& headers, size_t& contentLength);
void CleanupLeftoverTempFiles();

// HTML page handlers
void ServeUploadPage(SOCKET clientSocket);
void ServeMonitorPage(SOCKET clientSocket);
void ServeConfigPage(SOCKET clientSocket);
void ServeControlPage(SOCKET clientSocket);
void ServeCapturePage(SOCKET clientSocket);
void ServeThicknessPage(SOCKET clientSocket);
void ServeAboutPage(SOCKET clientSocket);
void ServeGCodePage(SOCKET clientSocket);
void ServeRootPage(SOCKET clientSocket, const std::string& lastUploadedFilename, const std::string& ipAddress);
void ServeMobilePage(SOCKET clientSocket);

// Upload module helper
bool IsFileExtensionAllowed(const std::string& filename, const std::string& toolhead, bool printAfterUpload);

// Helper functions for authentication
std::string base64_decode(const std::string& encoded);
void SendAuthRequired(SOCKET clientSocket);
bool CheckBasicAuth(const std::string& headers, const UserConfig& cfg);

// GET request handlers
void HandleGetRequest(SOCKET clientSocket, const std::string& path, const std::string& headers,
    const std::string& requestStr, const std::string& clientIP,
    const std::string& lastUploadedFilename);

// POST request handlers
void HandlePostRequest(SOCKET clientSocket, const std::string& path, const std::string& headers,
    const std::string& requestStr, const std::string& clientIP,
    const std::vector<char>& fullRequest);

void ServeXYZCalibrationPage(SOCKET clientSocket);

class FileUploadServer {
private:
    // Original members
    std::thread serverThread;
    std::atomic<bool> running{ false };
    int port;
    SOCKET listenSocket;
    std::string lastUploadedFilename;

    // Thread pool members
    std::vector<std::thread> workers;
    std::queue<SOCKET> pendingSockets;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopPool = false;

    void log(const std::string& msg) {
        std::cout << GetTimeStamp() << " " << msg << std::endl;
    }

    void handleRequest(SOCKET clientSocket) const;

    // Worker function for thread pool
    void workerFunction() {
        while (!stopPool) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !pendingSockets.empty() || stopPool; });
            if (stopPool) break;
            SOCKET clientSocket = pendingSockets.front();
            pendingSockets.pop();
            lock.unlock();

            handleRequest(clientSocket);
            closesocket(clientSocket);
        }
    }

public:
    FileUploadServer();
    ~FileUploadServer();

    // Prevent copying or moving the server
    FileUploadServer(const FileUploadServer&) = delete;
    FileUploadServer& operator=(const FileUploadServer&) = delete;
    FileUploadServer(FileUploadServer&&) = delete;
    FileUploadServer& operator=(FileUploadServer&&) = delete;

    bool start(int serverPort = 8081);
    void stop();
    bool isRunning() const;
    static bool EnsureHttpCameraFeed();

    // Thread pool management
    void startThreadPool(size_t threadCount = 4) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(&FileUploadServer::workerFunction, this);
        }
    }

    void stopThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopPool = true;
        }
        cv.notify_all();
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }
};