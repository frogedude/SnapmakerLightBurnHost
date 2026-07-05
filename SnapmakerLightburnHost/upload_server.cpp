// upload_server.cpp
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "host.h"
#include "upload_server.hpp"

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

bool CheckBasicAuth(const std::string& headers, const UserConfig& cfg) {
    if (!cfg.webAuthEnabled || cfg.webPassword.empty()) return true;

    std::string authHeader;
    size_t pos = headers.find("Authorization:");
    if (pos != std::string::npos) {
        size_t start = pos + 14;
        size_t end = headers.find("\r\n", start);
        if (end != std::string::npos) {
            authHeader = headers.substr(start, end - start);
            // trim
            authHeader.erase(0, authHeader.find_first_not_of(" \t"));
            authHeader.erase(authHeader.find_last_not_of(" \t") + 1);
        }
    }

    if (authHeader.empty() || authHeader.find("Basic ") != 0) return false;

    std::string base64 = authHeader.substr(6);
    std::string decoded = base64_decode(base64);
    decoded.erase(0, decoded.find_first_not_of(" \t\n\r"));
    decoded.erase(decoded.find_last_not_of(" \t\n\r") + 1);
    std::string expected = cfg.webUser + ":" + cfg.webPassword;
    return decoded == expected;
}

std::string base64_decode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size() * 3 / 4);

    std::array<unsigned char, 4> quad{};
    size_t quad_pos = 0;

    for (char c : encoded) {
        if (c == '=') break;
        if (c == '\r' || c == '\n') continue;
        if (c == '-') c = '+';
        else if (c == '_') c = '/';

        size_t idx = base64_chars.find(c);
        if (idx == std::string::npos) continue;

        quad[quad_pos++] = static_cast<unsigned char>(idx);

        if (quad_pos == 4) {
            // Decode 4 characters into 3 bytes
            unsigned char byte1 = (quad[0] << 2) | ((quad[1] & 0x30) >> 4);
            unsigned char byte2 = ((quad[1] & 0x0F) << 4) | ((quad[2] & 0x3C) >> 2);
            unsigned char byte3 = ((quad[2] & 0x03) << 6) | quad[3];

            decoded.push_back(byte1);
            decoded.push_back(byte2);
            decoded.push_back(byte3);

            quad_pos = 0;
        }
    }

    // Handle remaining characters (1–3)
    if (quad_pos > 0) {
        for (size_t j = quad_pos; j < 4; ++j) {
            quad[j] = 0;
        }
        unsigned char byte1 = (quad[0] << 2) | ((quad[1] & 0x30) >> 4);
        unsigned char byte2 = ((quad[1] & 0x0F) << 4) | ((quad[2] & 0x3C) >> 2);
        decoded.push_back(byte1);
        if (quad_pos >= 3) decoded.push_back(byte2);
    }
    return decoded;
}

void SendAuthRequired(SOCKET clientSocket) {
    std::string body = "Authentication required";
    std::string response = "HTTP/1.1 401 Unauthorized\r\n";
    response += "WWW-Authenticate: Basic realm=\"Snapmaker Bridge\"\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
}

// Define globals
std::string g_selectedFilename = "";
std::mutex g_selectedFileMutex;
std::chrono::steady_clock::time_point g_lastPrintStart;
std::mutex g_lastPrintMutex;
std::string g_lastUploadedFilename;
std::mutex g_lastUploadedFilenameMutex;
std::mutex g_cameraFeedMutex;

// Camera feed - static to this file
static std::unique_ptr<CameraSession> g_httpCameraFeed;

// Accessor functions
std::unique_ptr<CameraSession>& GetHttpCameraFeed() {
    return g_httpCameraFeed;
}

bool IsFileExtensionAllowed(const std::string& filename, const std::string& toolhead, bool printAfterUpload) {
    std::string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Firmware always allowed
    if (ext == ".bin") return true;

    // 3D Printer: only .gcode, regardless of print flag
    if (toolhead == "3D Printer" || toolhead == "3DP") {
        return (ext == ".gcode");
    }

    // Laser module
    if (toolhead == "Laser") {
        if (printAfterUpload) {
            // Upload + print: accept .nc or .gcode
            return (ext == ".nc" || ext == ".gcode");
        }
        else {
            // Upload only: accept .nc only
            return (ext == ".nc");
        }
    }

    // CNC module
    if (toolhead == "CNC") {
        if (printAfterUpload) {
            // Upload + print: accept .cnc or .gcode
            return (ext == ".cnc" || ext == ".gcode");
        }
        else {
            // Upload only: accept .cnc only
            return (ext == ".cnc");
        }
    }
    return false;
}

// In upload_server.cpp or a shared header
fs::path GetSnapmakerTempPath() {
    fs::path tempDir = fs::temp_directory_path() / TEMP_UPLOAD_FOLDER;

    // Create the directory if it doesn't exist
    if (!fs::exists(tempDir)) {
        fs::create_directories(tempDir);
    }

    return tempDir;
}

void SetHttpCameraFeed(std::unique_ptr<CameraSession> feed) {
    g_httpCameraFeed = std::move(feed);
}

static std::chrono::steady_clock::time_point lastMonitorLaunch;
static std::mutex monitorMutex;

void SendHttpResponse(SOCKET clientSocket, int statusCode, const std::string& contentType, const std::string& body) {
    std::stringstream response;
    response << "HTTP/1.1 " << statusCode << " " << (statusCode == 200 ? "OK" : (statusCode == 201 ? "Created" : "Error")) << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    send(clientSocket, response.str().c_str(), static_cast<int>(response.str().length()), 0);
}

void SendSimpleResponse(SOCKET clientSocket, int statusCode, const std::string& message) {
    SendHttpResponse(clientSocket, statusCode, "text/plain", message + "\n");
}

void SendOctoPrintVersionResponse(SOCKET clientSocket) {
    std::string responseBody = "{\"api\":\"0.1\",\"server\":\"1.9.0\",\"text\":\"OctoPrint 1.9.0 (Snapmaker Bridge)\",\"version\":\"1.9.0\"}";
    SendHttpResponse(clientSocket, 200, "application/json", responseBody);
}

void SendOctoPrintConnectionResponse(SOCKET clientSocket) {
    std::string responseBody = "{\"current\":{\"state\":\"Operational\",\"port\":\"virtual\",\"baudrate\":115200,\"printerProfile\":\"_default\"},\"options\":{\"ports\":[],\"baudrates\":[],\"printerProfiles\":[{\"name\":\"Default\",\"id\":\"_default\"}]}}";
    SendHttpResponse(clientSocket, 200, "application/json", responseBody);
}

void LaunchProcess(const std::string& command, const std::string& filePath) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string cmdLine = "\"" + std::string(exePath) + "\" " + command;
    if (!filePath.empty()) {
        cmdLine += " \"" + filePath + "\"";
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    std::vector<char> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back('\0');

    if (CreateProcessA(NULL, cmdLineBuf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void LaunchMonitorWindow() {
    std::lock_guard<std::mutex> lock(monitorMutex);
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMonitorLaunch).count() < 2000) {
        return;
    }
    lastMonitorLaunch = now;
    LaunchProcess("/monitor", "");
}

std::string ExtractApiKeyFromHeaders(const std::string& headers) {
    size_t authPos = headers.find("X-Api-Key:");
    if (authPos != std::string::npos) {
        size_t keyStart = authPos + 11;
        size_t keyEnd = headers.find("\r\n", keyStart);
        if (keyEnd != std::string::npos) {
            std::string key = headers.substr(keyStart, keyEnd - keyStart);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            return key;
        }
    }
    return "";
}

bool ReadCompleteRequest(SOCKET clientSocket, std::vector<char>& fullRequest, std::string& headers, size_t& contentLength) {
    std::vector<char> buffer(65536);
    int bytesReceived;
    bool headersComplete = false;
    fullRequest.clear();
    contentLength = 0;

    while (!headersComplete) {
        bytesReceived = recv(clientSocket, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (bytesReceived <= 0) return false;

        fullRequest.insert(fullRequest.end(), buffer.data(), buffer.data() + bytesReceived);
        std::string temp(fullRequest.data(), fullRequest.size());

        size_t headerEnd = temp.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            headersComplete = true;
            headers = temp.substr(0, headerEnd);

            size_t clPos = headers.find("Content-Length:");
            if (clPos != std::string::npos) {
                size_t numStart = clPos + 15;
                size_t numEnd = headers.find("\r\n", numStart);
                if (numEnd != std::string::npos) {
                    std::string lenStr = headers.substr(numStart, numEnd - numStart);
                    lenStr.erase(0, lenStr.find_first_not_of(" \t"));
                    lenStr.erase(lenStr.find_last_not_of(" \t") + 1);
                    contentLength = std::stoul(lenStr);
                }
            }

            size_t bodyStart = headerEnd + 4;
            size_t bodyReceived = fullRequest.size() - bodyStart;
            while (bodyReceived < contentLength) {
                bytesReceived = recv(clientSocket, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (bytesReceived <= 0) break;
                fullRequest.insert(fullRequest.end(), buffer.data(), buffer.data() + bytesReceived);
                bodyReceived += bytesReceived;
            }
            break;
        }
    }
    return !fullRequest.empty();
}

void CleanupLeftoverTempFiles() {
    try {
        fs::path snapmakerTempDir = GetSnapmakerTempPath();

        // Check if directory exists
        if (fs::exists(snapmakerTempDir) && fs::is_directory(snapmakerTempDir)) {
            auto now = std::chrono::system_clock::now();
            for (const auto& entry : fs::directory_iterator(snapmakerTempDir)) {
                if (entry.is_regular_file()) {
                    auto ftime = fs::last_write_time(entry.path());
                    auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    long long age = std::chrono::duration_cast<std::chrono::minutes>(now - sys_time).count();

                    // Delete files older than 10 minutes
                    if (age > 10) {
                        std::string filename = entry.path().filename().string();
                        if (fs::remove(entry.path())) {
                            std::cout << GetTimeStamp() << "[CLEANUP] Deleted old temp file: " << filename << std::endl;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[WARN] Cleanup failed: " << e.what() << std::endl;
    }
}

bool ParseMultipartForm(const std::string& request, const std::vector<char>& rawBuffer, int bytesReceived, MultipartData& outData) {
    size_t boundaryPos = request.find("boundary=");
    if (boundaryPos == std::string::npos) return false;

    size_t boundaryStart = boundaryPos + 9;
    size_t boundaryEnd = request.find_first_of("\r\n;", boundaryStart);
    if (boundaryEnd == std::string::npos) boundaryEnd = request.length();

    std::string boundary = request.substr(boundaryStart, boundaryEnd - boundaryStart);
    while (!boundary.empty() && (boundary.front() == '"' || boundary.front() == ' ')) boundary.erase(0, 1);
    while (!boundary.empty() && (boundary.back() == '"' || boundary.back() == ' ' || boundary.back() == '\r')) boundary.pop_back();

    std::string fullBoundary = "--" + boundary;
    std::string endBoundary = "--" + boundary + "--";
    std::string rawString(rawBuffer.data(), bytesReceived);

    size_t currentPos = 0;
    MultipartData bestMatch;
    bestMatch.printAfterUpload = false;
    size_t largestContent = 0;

    while (true) {
        size_t partStart = rawString.find(fullBoundary, currentPos);
        if (partStart == std::string::npos) break;

        size_t partEnd = rawString.find(fullBoundary, partStart + fullBoundary.length());
        if (partEnd == std::string::npos) {
            partEnd = rawString.find(endBoundary, partStart + fullBoundary.length());
            if (partEnd == std::string::npos) partEnd = rawString.length();
        }

        std::string part = rawString.substr(partStart, partEnd - partStart);
        size_t dispPos = part.find("Content-Disposition:");
        if (dispPos != std::string::npos) {
            std::string name;
            size_t namePos = part.find("name=\"", dispPos);
            if (namePos != std::string::npos) {
                size_t nameStart = namePos + 6;
                size_t nameEnd = part.find("\"", nameStart);
                if (nameEnd != std::string::npos) {
                    name = part.substr(nameStart, nameEnd - nameStart);
                }
            }

            if (name == "file") {
                std::string filename;
                size_t fnPos = part.find("filename=\"", dispPos);
                if (fnPos != std::string::npos) {
                    size_t fnStart = fnPos + 10;
                    size_t fnEnd = part.find("\"", fnStart);
                    if (fnEnd != std::string::npos) {
                        filename = part.substr(fnStart, fnEnd - fnStart);
                        size_t lastSlash = filename.find_last_of("/\\");
                        if (lastSlash != std::string::npos) filename = filename.substr(lastSlash + 1);
                    }
                }

                size_t headersEnd = part.find("\r\n\r\n");
                if (headersEnd != std::string::npos) {
                    headersEnd += 4;
                    size_t contentSize = part.length() - headersEnd;
                    while (contentSize > 0 &&
                        headersEnd + contentSize - 1 < part.length() &&
                        (part[headersEnd + contentSize - 1] == '\r' ||
                            part[headersEnd + contentSize - 1] == '\n')) {
                        contentSize--;
                    }
                    if (contentSize > largestContent) {
                        largestContent = contentSize;
                        bestMatch.filename = filename;
                        bestMatch.content.resize(contentSize);
                        memcpy(bestMatch.content.data(), part.data() + headersEnd, contentSize);
                    }
                }
            }
            else if (name == "print") {
                size_t headersEnd = part.find("\r\n\r\n");
                if (headersEnd != std::string::npos) {
                    headersEnd += 4;
                    std::string value;
                    for (size_t i = headersEnd; i < part.length(); ++i) {
                        char c = part[i];
                        if (c == '\r' || c == '\n') break;
                        value += c;
                    }
                    while (!value.empty() && std::isspace(value.front())) value.erase(0, 1);
                    while (!value.empty() && std::isspace(value.back())) value.pop_back();
                    bestMatch.printAfterUpload = (value == "true");
                }
            }
        }
        currentPos = partStart + fullBoundary.length();
        if (currentPos >= rawString.length()) break;
    }

    if (largestContent == 0) return false;
    if (bestMatch.filename.empty()) {
        auto now = std::time(nullptr);
        bestMatch.filename = "print_" + std::to_string(now) + ".gcode";
    }
    outData = std::move(bestMatch);
    return true;
}

void FileUploadServer::handleRequest(SOCKET clientSocket) const {
    std::vector<char> fullRequest;
    std::string headers;
    size_t contentLength = 0;
    if (!ReadCompleteRequest(clientSocket, fullRequest, headers, contentLength)) {
        SendSimpleResponse(clientSocket, 400, "Bad Request: No data received");
        return;
    }

    // Get client IP
    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);
    std::string clientIP = "127.0.0.1";
    if (getpeername(clientSocket, (sockaddr*)&clientAddr, &addrLen) == 0) {
        clientIP = inet_ntoa(clientAddr.sin_addr);
    }

    std::string requestStr(fullRequest.data(), fullRequest.size());
    std::string method, path;
    std::istringstream iss(requestStr);
    iss >> method >> path;

    if (method == "GET") {
        HandleGetRequest(clientSocket, path, headers, requestStr, clientIP, lastUploadedFilename);
    }
    else if (method == "POST") {
        HandlePostRequest(clientSocket, path, headers, requestStr, clientIP, fullRequest);
    }
    else if (method == "DELETE") {
        if (path.find("/api/files/local/") == 0) {
            SendSimpleResponse(clientSocket, 204, "");
        }
        else {
            SendSimpleResponse(clientSocket, 404, "Not Found");
        }
    }
    else {
        SendSimpleResponse(clientSocket, 404, "Not Found");
    }
}

FileUploadServer::FileUploadServer() : running(false), port(8081), listenSocket(INVALID_SOCKET) {}
FileUploadServer::~FileUploadServer() { stop(); }

bool FileUploadServer::start(int serverPort) {
    CleanupLeftoverTempFiles();
    if (running.load()) return true;
    port = serverPort;
    running.store(true);

    serverThread = std::thread([this]() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { running.store(false); return; }
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) { WSACleanup(); running.store(false); return; }
        int reuse = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
        UserConfig cfg = HostContext::instance().snapshotConfig();
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        if (cfg.bindLocalhost) {
            serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        }
        else {
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        }
        serverAddr.sin_port = htons(port);
        if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(listenSocket); WSACleanup(); running.store(false); return;
        }
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(listenSocket); WSACleanup(); running.store(false); return;
        }

        while (running.load()) {
            fd_set readSet = {};
            FD_ZERO(&readSet);
            FD_SET(listenSocket, &readSet);
            timeval timeout = { 1, 0 };
            if (select(0, &readSet, NULL, NULL, &timeout) > 0) {
                SOCKET clientSocket = accept(listenSocket, NULL, NULL);
                if (clientSocket != INVALID_SOCKET) {
                    std::thread([this, clientSocket]() {
                        handleRequest(clientSocket);
                        closesocket(clientSocket);
                        }).detach();
                }
            }
        }
        closesocket(listenSocket);
        WSACleanup();
        });
    return true;
}

void FileUploadServer::stop() {
    if (!running.load()) return;
    running.store(false);
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    if (serverThread.joinable()) serverThread.join();
    CleanupLeftoverTempFiles();
}

bool FileUploadServer::isRunning() const { return running.load(); }

bool FileUploadServer::EnsureHttpCameraFeed() {
    if (!GetHttpCameraFeed()) {
        auto feed = std::make_unique<CameraSession>(CAMERA_WIDTH, CAMERA_HEIGHT, DEFAULT_FPS);
        if (!*feed) {
            std::cerr << GetTimeStamp() << "[ERR] Failed to create softcam feed" << std::endl;
            SetHttpCameraFeed(nullptr);
            return false;
        }
        ImageBuffer testImg = CreateEnhancedTestPattern(CAMERA_WIDTH, CAMERA_HEIGHT);
        if (testImg) feed->sendFrame(testImg.get());
        SetHttpCameraFeed(std::move(feed));
    }
    return true;
}

static FileUploadServer& GetServerInstance() {
    static FileUploadServer server;
    return server;
}

bool HostContext::startUploadServer(int serverPort) {
    return GetServerInstance().start(serverPort);
}

void HostContext::stopUploadServer() {
    GetServerInstance().stop();
}

bool HostContext::isUploadServerRunning() const {
    return GetServerInstance().isRunning();
}