// camera_capture.cpp - Softcam capture, image download, RTSP camera
#include "host.h"

// ---------- CameraSession ----------
CameraSession::CameraSession() : cam(nullptr) {}
CameraSession::CameraSession(int width, int height, int fps) : cam(nullptr) {
#pragma warning(push)
#pragma warning(disable: 4244)
    cam = scCreateCamera(static_cast<float>(width), static_cast<float>(height), static_cast<float>(fps));
#pragma warning(pop)
}
CameraSession::~CameraSession() { if (cam) scDeleteCamera(cam); }
CameraSession::CameraSession(CameraSession&& other) noexcept : cam(other.cam) { other.cam = nullptr; }
CameraSession& CameraSession::operator=(CameraSession&& other) noexcept {
    if (this != &other) {
        if (cam) scDeleteCamera(cam);
        cam = other.cam;
        other.cam = nullptr;
    }
    return *this;
}
bool CameraSession::create(int width, int height, int fps) {
    if (cam) scDeleteCamera(cam);
#pragma warning(push)
#pragma warning(disable: 4244)
    cam = scCreateCamera(static_cast<float>(width), static_cast<float>(height), static_cast<float>(fps));
#pragma warning(pop)
    return cam != nullptr;
}
void* CameraSession::get() const { return cam; }
CameraSession::operator bool() const { return cam != nullptr; }
bool CameraSession::sendFrame(const unsigned char* frame) const {
    if (cam && frame) {
        scSendFrame(cam, frame);
        return true;
    }
    return false;
}
void CameraSession::release() {
    if (cam) {
        scDeleteCamera(cam);
        cam = nullptr;
    }
}

// ---------- ImageBuffer ----------
ImageBuffer::ImageBuffer() : data(nullptr), size(0) {}
ImageBuffer::ImageBuffer(size_t bufferSize) : size(bufferSize) {
    if (bufferSize > 0) data = std::make_unique<unsigned char[]>(bufferSize);
}
ImageBuffer::ImageBuffer(ImageBuffer&& other) noexcept : data(std::move(other.data)), size(other.size) { other.size = 0; }
ImageBuffer& ImageBuffer::operator=(ImageBuffer&& other) noexcept {
    if (this != &other) {
        data = std::move(other.data);
        size = other.size;
        other.size = 0;
    }
    return *this;
}
ImageBuffer::operator bool() const noexcept { return data != nullptr && size > 0; }
unsigned char* ImageBuffer::get() const { return data.get(); }
size_t ImageBuffer::getSize() const { return size; }
void ImageBuffer::reset(size_t newSize) {
    data.reset();
    size = 0;
    if (newSize > 0) {
        data = std::make_unique<unsigned char[]>(newSize);
        size = newSize;
    }
}

// ---------- Camera Functions ----------
std::string CaptureURL() {
    UserConfig cfg = HostContext::instance().snapshotConfig();
    return BuildCapturePhotoUrl(cfg.ipAddress, cfg.basePositionX, cfg.basePositionY, cfg.basePositionZ, CAMERA_CAPTURE_FEED_RATE);
}

std::string DownloadURL() {
    UserConfig cfg = HostContext::instance().snapshotConfig();
    return BuildGetCameraImageUrl(cfg.ipAddress);
}

bool CaptureImage(const std::string& file) {
    std::string response;
    std::string captureUrl = CaptureURL();
    std::cout << GetTimeStamp() << "Requesting image capture..." << std::flush;

    CurlSession session;
    if (!session) {
        std::cout << "[FAIL]" << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Failed to initialize CURL." << std::endl;
        return false;
    }
    CURL* curl = session.get();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, captureUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortable_progress_wrapper);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode captureRes = curl_easy_perform(curl);
    long captureCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &captureCode);

    if (captureRes != CURLE_OK || captureCode != 200) {
        std::cout << "[FAIL]" << std::endl;
        return false;
    }
    try {
        auto json_response = json::parse(response);

        if (!json_response.is_object()) {
            std::cout << "[FAIL] Response is not a JSON object." << std::endl;
            return false;
        }

        if (json_response.contains("status")) {
            if (!json_response["status"].is_boolean()) {
                std::cout << "[FAIL] Status field is not a boolean." << std::endl;
                return false;
            }
            if (!json_response["status"].get<bool>()) {
                std::cout << "[FAIL] Status indicates failure." << std::endl;
                return false;
            }
        }
        else {
            std::string errorMsg = "Unknown error";
            if (json_response.contains("message") && json_response["message"].is_string()) {
                errorMsg = json_response["message"].get<std::string>();
            }
            else if (json_response.contains("error") && json_response["error"].is_string()) {
                errorMsg = json_response["error"].get<std::string>();
            }
            std::cout << "[FAIL] API returned error: " << errorMsg << std::endl;
            return false;
        }
    }
    catch (const json::parse_error& e) {
        std::cout << "[FAIL] JSON parse error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cout << "[FAIL] Unexpected error parsing response: " << e.what() << std::endl;
        return false;
    }
    std::cout << "[OK]" << std::endl;
    std::cout << GetTimeStamp() << "Waiting for image to be ready..." << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(AUTO_REFRESH_CAPTURE_DELAY_SECONDS));
    std::cout << "[OK]" << std::endl;

    std::cout << GetTimeStamp() << "Downloading image..." << std::flush;
    std::string downloadUrl = DownloadURL();

    std::ofstream f(file, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "[FAIL]" << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Cannot create file: " << file << std::endl;
        return false;
    }

    CurlSession downloadSession;
    if (!downloadSession) {
        std::cout << "[FAIL]" << std::endl;
        f.close();
        return false;
    }
    CURL* download_curl = downloadSession.get();
    curl_easy_reset(download_curl);
    curl_easy_setopt(download_curl, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(download_curl, CURLOPT_WRITEFUNCTION, data_write);
    curl_easy_setopt(download_curl, CURLOPT_WRITEDATA, &f);
    curl_easy_setopt(download_curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(download_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(download_curl, CURLOPT_XFERINFOFUNCTION, abortable_progress_wrapper);
    curl_easy_setopt(download_curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode downloadRes = curl_easy_perform(download_curl);
    long downloadCode = 0;
    curl_easy_getinfo(download_curl, CURLINFO_RESPONSE_CODE, &downloadCode);
    f.close();

    if (downloadRes != CURLE_OK || downloadCode != 200) {
        std::cout << "[FAIL]" << std::endl;
        try {
            fs::remove(file);
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << GetTimeStamp() << "[WARN] Could not remove incomplete file: " << e.what() << std::endl;
        }
        return false;
    }

    try {
        std::ifstream checkFile(file, std::ios::binary | std::ios::ate);
        if (!checkFile.is_open() || checkFile.tellg() == 0) {
            std::cout << "[FAIL]" << std::endl;
            fs::remove(file);
            return false;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << GetTimeStamp() << "[ERR] Error checking downloaded file: " << e.what() << std::endl;
        return false;
    }

    std::cout << "[OK]" << std::endl;
    return true;
}

bool CaptureAndUpdateImage(const std::string& imgFile, ImageBuffer& currentImg, int& width, int& height, bool verbose) {
    if (!CaptureImage(imgFile)) {
        if (verbose) {
            std::cout << GetTimeStamp() << "[ERR] Image capture failed." << std::endl;
        }
        return false;
    }

    int newW, newH, newC;
    unsigned char* newImg = stbi_load(imgFile.c_str(), &newW, &newH, &newC, 3);
    if (!newImg) {
        if (verbose) {
            std::cout << GetTimeStamp() << "[ERR] Failed to load captured image (corrupt or unsupported format)." << std::endl;
        }
        return false;
    }

    if (newW <= 0 || newH <= 0 || newC <= 0) {
        if (verbose) {
            std::cout << GetTimeStamp() << "[ERR] Invalid image dimensions after capture." << std::endl;
        }
        stbi_image_free(newImg);
        return false;
    }

    if (newC != 3 && verbose) {
        std::cout << GetTimeStamp() << "[WARN] Unexpected channel count: " << newC
            << " (expected 3). Colors may be incorrect." << std::endl;
    }

    ImageBuffer processed;
    if (newW != CAMERA_WIDTH || newH != CAMERA_HEIGHT) {
        processed = ResizeImage(newImg, newW, newH, CAMERA_WIDTH, CAMERA_HEIGHT);
        if (!processed) {
            if (verbose) {
                std::cout << GetTimeStamp() << "[ERR] Failed to resize captured image." << std::endl;
            }
            stbi_image_free(newImg);
            return false;
        }
        width = CAMERA_WIDTH;
        height = CAMERA_HEIGHT;
    }
    else {
        size_t u_w = static_cast<size_t>(newW);
        size_t u_h = static_cast<size_t>(newH);
        if (u_w > SIZE_MAX / u_h) {
            if (verbose) {
                std::cout << GetTimeStamp() << "[ERR] Image dimensions too large (overflow)." << std::endl;
            }
            stbi_image_free(newImg);
            return false;
        }
        size_t wh = u_w * u_h;
        if (wh > SIZE_MAX / 3) {
            if (verbose) {
                std::cout << GetTimeStamp() << "[ERR] Image buffer size overflow." << std::endl;
            }
            stbi_image_free(newImg);
            return false;
        }
        size_t sz = wh * 3;
        try {
            processed = ImageBuffer(sz);
        }
        catch (const std::bad_alloc&) {
            if (verbose) {
                std::cout << GetTimeStamp() << "[ERR] Out of memory while allocating image buffer." << std::endl;
            }
            stbi_image_free(newImg);
            return false;
        }
        if (!processed) {
            if (verbose) {
                std::cout << GetTimeStamp() << "[ERR] Failed to allocate buffer for image." << std::endl;
            }
            stbi_image_free(newImg);
            return false;
        }
        memcpy(processed.get(), newImg, sz);
        width = newW;
        height = newH;
    }
    stbi_image_free(newImg);
    currentImg = std::move(processed);

    if (verbose) {
        std::cout << GetTimeStamp() << "[OK] Image loaded into camera feed." << std::endl;
        std::cout << GetTimeStamp() << "[INFO] Press 'O' to open captured image in default viewer." << std::endl;
    }
    return true;
}

// ---------- RTSP Camera Helpers ----------
std::string GetVlcPath() {
    try {
        RegKey key(HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC", KEY_READ);
        if (key.valid()) {
            std::string installDir = key.getStringValue("InstallDir");
            if (!installDir.empty()) {
                fs::path vlcExe = fs::path(installDir) / "vlc.exe";
                if (fs::exists(vlcExe))
                    return vlcExe.string();
            }
        }
        fs::path defaultPath = "C:\\Program Files\\VideoLAN\\VLC\\vlc.exe";
        if (fs::exists(defaultPath))
            return defaultPath.string();
        fs::path altPath = "C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe";
        if (fs::exists(altPath))
            return altPath.string();
        return "";
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] GetVlcPath: " << e.what() << std::endl;
        return "";
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] GetVlcPath: unknown exception" << std::endl;
        return "";
    }
}

bool CheckVlcInstalled() {
    try {
        return !GetVlcPath().empty();
    }
    catch (...) {
        return false;
    }
}

// ---------- RTSP Camera Management ----------
bool HostContext::startCamera(const std::string& rtspUrl) {
    try {
        stopCamera();
        std::string vlcPath = GetVlcPath();
        if (vlcPath.empty() || rtspUrl.empty()) {
            std::cerr << GetTimeStamp() << "[ERR] Cannot start camera: VLC not found or RTSP URL missing." << std::endl;
            return false;
        }
        if (!fs::exists(vlcPath)) {
            std::cerr << GetTimeStamp() << "[ERR] VLC executable not found: " << vlcPath << std::endl;
            return false;
        }
        std::string cmd = "\"" + vlcPath + "\" \"" + rtspUrl + "\"";
        cameraProcess = std::make_unique<ProcessHandle>(cmd, true, false);
        if (!cameraProcess->valid()) {
            std::cerr << GetTimeStamp() << "[ERR] Failed to launch VLC process" << std::endl;
            cameraProcess.reset();
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] startCamera: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] startCamera: unknown exception" << std::endl;
        return false;
    }
}

bool HostContext::startCameraDetached(const std::string& rtspUrl) {
    try {
        std::string vlcPath = GetVlcPath();
        if (vlcPath.empty() || rtspUrl.empty()) {
            std::cerr << GetTimeStamp() << "[ERR] Cannot launch camera: VLC not found or RTSP URL missing." << std::endl;
            return false;
        }
        if (!fs::exists(vlcPath)) {
            std::cerr << GetTimeStamp() << "[ERR] VLC executable not found: " << vlcPath << std::endl;
            return false;
        }
        // Use ShellExecute to launch VLC detached (no waiting, no handle)
        std::string cmd = "\"" + vlcPath + "\" \"" + rtspUrl + "\" --play-and-exit";
        HINSTANCE result = ShellExecuteA(NULL, "open", vlcPath.c_str(), rtspUrl.c_str(), NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            std::cerr << GetTimeStamp() << "[ERR] Failed to launch VLC (error code: " << (INT_PTR)result << ")" << std::endl;
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] launchCameraDetached: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] launchCameraDetached: unknown exception" << std::endl;
        return false;
    }
}

void HostContext::stopCamera() {
    try {
        if (cameraProcess && cameraProcess->valid()) {
            cameraProcess->terminateFast();
            cameraProcess.reset();
        }
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] stopCamera: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] stopCamera: unknown exception" << std::endl;
    }
}

bool HostContext::isCameraRunning() const {
    try {
        return cameraProcess && cameraProcess->valid() && cameraProcess->isRunning();
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] isCameraRunning: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }
}

void OpenLatestImage() {
    std::string imgFile = "latest.jpg";

    try {
        if (!fs::exists(imgFile)) {
            std::cout << GetTimeStamp() << "[ERR] No captured image found. Press ENTER to capture first." << std::endl;
            return;
        }

        if (!fs::is_regular_file(imgFile)) {
            std::cout << GetTimeStamp() << "[ERR] Not a regular file: " << imgFile << std::endl;
            return;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << GetTimeStamp() << "[ERR] Cannot check image file: " << e.what() << std::endl;
        return;
    }

    std::ifstream testFile(imgFile, std::ios::binary);
    if (!testFile.is_open()) {
        std::cout << GetTimeStamp() << "[ERR] Image file exists but cannot be opened." << std::endl;
        return;
    }

    testFile.seekg(0, std::ios::end);
    if (testFile.tellg() == 0) {
        std::cout << GetTimeStamp() << "[ERR] Image file is empty." << std::endl;
        return;
    }

    HINSTANCE result = ShellExecuteA(NULL, "open", imgFile.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        std::cout << GetTimeStamp() << "[ERR] Could not open image viewer. Error code: " << (INT_PTR)result << std::endl;
    }
    else {
        std::cout << GetTimeStamp() << "[OK] Opening image in default viewer." << std::endl;
    }
}