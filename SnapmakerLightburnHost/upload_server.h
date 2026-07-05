// upload_server.h
#pragma once

#include "host.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <chrono>
#include <thread>

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

// GET request handlers
void HandleGetRequest(SOCKET clientSocket, const std::string& path, const std::string& headers, const std::string& requestStr, const std::string& lastUploadedFilename);

// POST request handlers
void HandlePostRequest(SOCKET clientSocket, const std::string& path, const std::string& headers, const std::string& requestStr, const std::vector<char>& fullRequest);

class FileUploadServer {
private:
    std::thread serverThread;
    std::atomic<bool> running{ false };
    int port;
    SOCKET listenSocket;
    std::string lastUploadedFilename;

    void log(const std::string& msg) {
        std::cout << GetTimeStamp() << " " << msg << std::endl;
    }

    void handleRequest(SOCKET clientSocket);

public:
    FileUploadServer();
    ~FileUploadServer();
    bool start(int serverPort = 8081);
    void stop();
    bool isRunning() const;
    static bool EnsureHttpCameraFeed();
};