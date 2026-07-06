// upload_get.cpp
#include "upload_server.hpp"
#include <atomic>
#include <mutex>

// ============================================================================
// Local Circuit Breaker for Web Polling (prevents thread starvation)
// ============================================================================
static std::chrono::steady_clock::time_point lastOfflineTime = std::chrono::steady_clock::now();
static bool isCooldown = false;
static std::mutex cooldownMutex;
const int OFFLINE_COOLDOWN_SECONDS = 10;

static MachineStatusData GetMachineStatusWithCooldown() {
    std::lock_guard<std::mutex> lock(cooldownMutex);
    auto now = std::chrono::steady_clock::now();

    // If we are in cooldown, return cached OFFLINE instantly (0ms delay)
    if (isCooldown) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastOfflineTime).count();
        if (elapsed < OFFLINE_COOLDOWN_SECONDS) {
            // Return cached offline status – no network call!
            return MachineStatusData::CreateOffline();
        }
        else {
            // Cooldown expired – allow a fresh attempt
            isCooldown = false;
        }
    }

    // Perform the actual network request (may block up to 3 seconds)
    MachineStatusData status = GetMachineStatus(false);

    // Update cooldown state based on result
    if (IsMachineOffline(status)) {
        isCooldown = true;
        lastOfflineTime = std::chrono::steady_clock::now();
    }
    else {
        // Machine is online – reset cooldown
        isCooldown = false;
    }
    return status;
}

static bool DoCaptureAndSendResponse(SOCKET clientSocket) {
	MachineStatusData status = GetMachineStatus(false);
	if (IsMachineOffline(status)) {
		SendSimpleResponse(clientSocket, 503, "Machine offline");
		return false;
	}
	if (!status.homed) {
		SendSimpleResponse(clientSocket, 400, "Machine not homed");
		return false;
	}
	if (!status.isLaser() || !status.laserCamera) {
		SendSimpleResponse(clientSocket, 400, "Laser with camera required");
		return false;
	}

	int w = 0, h = 0;
	ImageBuffer dummy;
	if (!CaptureAndUpdateImage("latest.jpg", dummy, w, h, false)) {
		SendSimpleResponse(clientSocket, 500, "Capture failed");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(g_cameraFeedMutex);
		if (FileUploadServer::EnsureHttpCameraFeed()) {
			int imgW, imgH, channels;
			unsigned char* imgData = stbi_load("latest.jpg", &imgW, &imgH, &channels, 3);
			if (imgData) {
				if (imgW != CAMERA_WIDTH || imgH != CAMERA_HEIGHT) {
					ImageBuffer scaled = ResizeImage(imgData, imgW, imgH, CAMERA_WIDTH, CAMERA_HEIGHT);
					if (scaled) GetHttpCameraFeed()->sendFrame(scaled.get());
				}
				else {
					GetHttpCameraFeed()->sendFrame(imgData);
				}
				stbi_image_free(imgData);
			}
		}
	}

	std::ifstream file("latest.jpg", std::ios::binary);
	if (!file) {
		SendSimpleResponse(clientSocket, 500, "Image file not found after capture");
		return false;
	}
	std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	SendHttpResponse(clientSocket, 200, "image/jpeg", std::string(data.begin(), data.end()));
	return true;
}

static bool GetThicknessAtCoords(const std::string& ip, double x, double y, double& outThickness) {
	std::string url = BuildMaterialThicknessUrl(ip, x, y, MATERIAL_THICKNESS_FEED_RATE);
	std::string response;
	CurlSession session;
	if (!session) return false;
	CURL* curl = session.get();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortable_progress_wrapper);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (res != CURLE_OK || http_code != 200) return false;

	try {
		auto data = json::parse(response);
		if (data.contains("status") && data["status"].is_boolean() && data["status"].get<bool>()) {
			if (data.contains("thickness") && data["thickness"].is_number()) {
				outThickness = data["thickness"].get<double>();
				return true;
			}
		}
	}
	catch (...) {}
	return false;
}

void HandleGetRequest(SOCKET clientSocket, const std::string& path, const std::string& headers,
    const std::string& requestStr, const std::string& clientIP,
    const std::string& lastUploadedFilename) {
    UserConfig cfg = HostContext::instance().snapshotConfig();

    // ----- Authentication check (only for remote clients) -----
    if (clientIP != "127.0.0.1" && clientIP != "::1") {
        if (!CheckBasicAuth(headers, cfg)) {
            SendAuthRequired(clientSocket);
            return;
        }
    }
    // ----- end authentication check -----

    // OctoPrint API endpoints
    if (path == "/api/version" || path.find("/api/version") == 0 ||
        path == "/api/server" || path.find("/api/server") == 0) {
        SendOctoPrintVersionResponse(clientSocket);
        return;
    }
    else if (path == "/api/connection" || path.find("/api/connection") == 0) {
        ThrottledConnect();

        json response = {
            {"current", {
                {"state", "Operational"},
                {"port", "virtual"},
                {"baudrate", 115200},
                {"printerProfile", "_default"}
            }},
            {"options", {
                {"ports", json::array()},
                {"baudrates", json::array()},
                {"printerProfiles", {
                    {{"name", "Default"}, {"id", "_default"}}
                }}
            }}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/printer" || path.find("/api/printer") == 0) {
        json response = {
            {"temperature", {
                {"tool0", {{"actual", 0}, {"target", 0}}},
                {"bed", {{"actual", 0}, {"target", 0}}}
            }},
            {"state", {
                {"text", "Operational"},
                {"flags", {
                    {"operational", true},
                    {"paused", false},
                    {"printing", false},
                    {"error", false},
                    {"ready", true}
                }}
            }}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/job" || path.find("/api/job") == 0) {
        json response = {
            {"job", {
                {"file", {{"name", json(nullptr)}, {"path", json(nullptr)}}}
            }},
            {"progress", {
                {"completion", json(nullptr)},
                {"printTime", json(nullptr)},
                {"printTimeLeft", json(nullptr)}
            }},
            {"state", "Operational"}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/live/job") {
        // Track state to avoid false notifications
        static bool wasRunning = false;
        // tracks progress of the last observed print
        static int lastPercent = -1;
        static std::mutex monitorMutex;

        // Lock the mutex for the entire state update
        std::lock_guard<std::mutex> lock(monitorMutex);

        //MachineStatusData status = GetMachineStatus(false);
        MachineStatusData status = GetMachineStatusWithCooldown();
        bool isRunning = status.isBusy();

        // Update progress if we are running
        if (isRunning) {
            if (status.progress > 0) {
                lastPercent = static_cast<int>(status.progress * 100);
            }
            else if (status.totalLines > 0 && status.currentLine > 0) {
                lastPercent = (status.currentLine * 100) / status.totalLines;
            }
        }

        // ====== FILAMENT RUNOUT NOTIFICATION ======
        static bool lastFilamentOut = false;
        if (!lastFilamentOut && status.isFilamentOut) {
            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.enableEmailAlerts) {
                SendEmailNotification("Filament Runout",
                    "Filament has run out on your Snapmaker.\n\n"
                    "Please load new filament and resume the print.");
            }

            if (cfg.enableSoundAlerts) {
                PlayAlertChime();
            }
        }
        lastFilamentOut = status.isFilamentOut;
        // =========================================

        // Transition from running to idle
        if (wasRunning && !isRunning) {
            // Only send notification if we actually had some progress
            if (lastPercent > 0) {
                UserConfig cfg = HostContext::instance().snapshotConfig();
                if (cfg.enableSoundAlerts) {
                    PlayCompletionBeep();
                }
                if (cfg.enableEmailAlerts) {
                    SendEmailNotification("Print completed", "Your Snapmaker print has finished.");
                }
            }
        }
        wasRunning = isRunning;

        double progress = status.progress;
        if (progress > 1.0) progress = 1.0;
        if (progress < 0.0) progress = 0.0;

        json response;
        if (IsMachineOffline(status)) {
            response = {
                {"fileName", ""},
                {"progress", 0},
                {"elapsedTime", 0},
                {"remainingTime", 0},
                {"totalLines", 0},
                {"currentLine", 0},
                {"state", "Offline"}
            };
        }
        else {
            response = {
                {"fileName", status.fileName},
                {"progress", progress},
                {"elapsedTime", status.elapsedTime},
                {"remainingTime", status.remainingTime},
                {"totalLines", status.totalLines},
                {"currentLine", status.currentLine},
                {"state", status.getStatusDisplay()}
            };
        }
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/live/printer") {
        MachineStatusData status = GetMachineStatusWithCooldown();
        //MachineStatusData status = GetMachineStatus(false);

        json response;
        if (IsMachineOffline(status)) {
            response = {
                {"toolHead", "Offline"},
                {"temperature", {{"tool0", {{"actual", 0}, {"target", 0}}}, {"bed", {{"actual", 0}, {"target", 0}}}}},
                {"custom", json::object()}
            };
        }
        else {
            // Toolhead specific data
            json custom;
            if (status.isLaser()) {
                custom["laser"] = {
                    {"power", status.laserPower},
                    {"focalLength", status.laserFocalLength},
                    {"camera", status.laserCamera},
                    {"workSpeed", status.workSpeed}
                };
            }
            else if (status.isCNC()) {
                custom["cnc"] = {
                    {"spindleSpeed", status.spindleSpeed},
                    {"workSpeed", status.workSpeed}
                };
            }
            else if (status.is3DPrinter()) {
                custom["3dp"] = {
                    {"nozzle2", {{"actual", status.nozzleTemperature2}, {"target", status.nozzleTargetTemperature2}}},
                    {"filamentOut", status.isFilamentOut},
                    {"printStatus", status.printStatus}
                };
            }
            custom["modules"] = {
                {"enclosure", status.enclosure},
                {"rotary", status.rotaryModule},
                {"doorOpen", status.isEnclosureDoorOpen},
                {"emergencyStop", status.emergencyStopButton},
                {"airPurifier", status.airPurifier}
            };
            custom["toolHead"] = status.getToolHeadDisplay();

            response = {
                {"toolHead", status.getToolHeadDisplay()},
                {"temperature", {
                    {"tool0", {{"actual", status.nozzleTemperature1}, {"target", status.nozzleTargetTemperature1}}},
                    {"bed", {{"actual", status.heatedBedTemperature}, {"target", status.heatedBedTargetTemperature}}}
                }},
                {"custom", custom}
            };
        }
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/server/info" || path.find("/server/info") == 0) {
        json response = {
            {"server", "1.9.0"},
            {"api", "0.1"},
            {"text", "OctoPrint 1.9.0 (Snapmaker Bridge)"},
            {"version", "1.9.0"}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/files" || path == "/api/files/local") {
        json response = {
            {"files", json::array()},
            {"free_space", 1000000000},
            {"total_space", 1000000000}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/settings" || path.find("/api/settings") == 0) {
        SendHttpResponse(clientSocket, 200, "application/json", "{}");
        return;
    }
    else if (path == "/api/printerprofiles" || path.find("/api/printerprofiles") == 0) {
        SendHttpResponse(clientSocket, 200, "application/json", "{\"profiles\":{}}");
        return;
    }
    else if (path == "/api/plugin/appkeys") {
        SendHttpResponse(clientSocket, 200, "application/json", "[{\"name\":\"Cura\",\"user\":\"snapmaker\",\"apikey\":\"SNAPMAKER_BRIDGE_KEY\"}]");
        return;
    }
    else if (path == "/api/plugin/appkeys/request") {
        SendHttpResponse(clientSocket, 200, "application/json", "{\"apikey\":\"SNAPMAKER_BRIDGE_KEY\"}");
        return;
    }
    else if (path == "/api/plugin/appkeys/verify" || path.find("/api/plugin/appkeys/verify") == 0) {
        SendHttpResponse(clientSocket, 200, "application/json", "{\"valid\":true}");
        return;
    }
    else if (path == "/plugin/appkeys/probe") {
        SendSimpleResponse(clientSocket, 200, "{\"ok\":true}");
        return;
    }
    else if (path == "/api/login" || path.find("/api/login") == 0) {
        SendSimpleResponse(clientSocket, 204, "");
        return;
    }
    else if (path == "/api/currentuser" || path.find("/api/currentuser") == 0) {
        SendHttpResponse(clientSocket, 200, "application/json", "{\"name\":\"snapmaker\",\"admin\":true,\"user\":true,\"apikey\":\"SNAPMAKER_BRIDGE_KEY\"}");
        return;
    }
    else if (path == "/api/kasa/status") {
        UserConfig cfg = HostContext::instance().snapshotConfig();
        if (cfg.kasa.ipAddress.empty()) {
            json error = { {"status", "error"}, {"message", "Kasa IP not configured"} };
            SendHttpResponse(clientSocket, 200, "application/json", error.dump());
            return;
        }
        bool isOn = false;
        if (CheckKasaPlugStatus(cfg.kasa.ipAddress, isOn)) {
            json response = { {"status", "ok"}, {"state", isOn} };
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        else {
            json error = { {"status", "error"}, {"message", "Failed to check plug status"} };
            SendHttpResponse(clientSocket, 200, "application/json", error.dump());
        }
        return;
    }
    else if (path.find("/api/thickness") == 0) {
        // Parse query parameters for custom x,y
        double customX = -1.0, customY = -1.0;
        size_t qpos = path.find('?');
        if (qpos != std::string::npos) {
            std::string query = path.substr(qpos + 1);
            size_t xpos = query.find("x=");
            if (xpos != std::string::npos) {
                size_t end = query.find('&', xpos);
                if (end == std::string::npos) end = query.length();
                std::string xstr = query.substr(xpos + 2, end - (xpos + 2));
                customX = std::stod(xstr);
            }
            size_t ypos = query.find("y=");
            if (ypos != std::string::npos) {
                size_t end = query.find('&', ypos);
                if (end == std::string::npos) end = query.length();
                std::string ystr = query.substr(ypos + 2, end - (ypos + 2));
                customY = std::stod(ystr);
            }
        }

        // Machine status checks
        MachineStatusData status = GetMachineStatus(false);
        if (IsMachineOffline(status)) {
            json error = { {"status","error"},{"message","Machine is offline"} };
            SendHttpResponse(clientSocket, 503, "application/json", error.dump());
            return;
        }
        if (!status.homed) {
            json error = { {"status","error"},{"message","Machine not homed"} };
            SendHttpResponse(clientSocket, 400, "application/json", error.dump());
            return;
        }
        if (!status.isLaser() || !status.laserCamera) {
            json error = { {"status","error"},{"message","Laser with camera required"} };
            SendHttpResponse(clientSocket, 400, "application/json", error.dump());
            return;
        }

        double thickness = 0.0;
        bool success = false;
        UserConfig cfg = HostContext::instance().snapshotConfig();

        if (customX >= 0 && customY >= 0) {
            success = GetThicknessAtCoords(cfg.ipAddress, customX, customY, thickness);
        }
        else {
            success = (GetMaterialThicknessFromSnapmaker(thickness) == CURLE_OK);
        }

        if (success && thickness > 0.0) {
            json response = { {"status","ok"},{"thickness_mm", thickness} };
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        else {
            json error = { {"status","error"},{"message","Measurement failed"} };
            SendHttpResponse(clientSocket, 500, "application/json", error.dump());
        }
        return;
    }
    else if (path == "/api/capture/latest") {
        std::ifstream file("latest.jpg", std::ios::binary);
        if (!file.is_open()) {
            SendSimpleResponse(clientSocket, 404, "No captured image yet. Run /api/capture first.");
            return;
        }
        std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        SendHttpResponse(clientSocket, 200, "image/jpeg", std::string(data.begin(), data.end()));
        return;
    }
    else if (path == "/api/capture") {
        DoCaptureAndSendResponse(clientSocket);
        return;
    }
    else if (path == "/api/machine/status") {
        MachineStatusData status = GetMachineStatus(false);
        json response = {
            {"online", !IsMachineOffline(status)},
            {"homed", status.homed},
            {"busy", status.isBusy()},
            {"toolhead", status.getToolHeadDisplay()},
            {"status_text", status.getStatusDisplay()},
            {"enclosure", status.enclosure},
            {"door_open", status.isEnclosureDoorOpen},
            {"filament_out", status.isFilamentOut},
            {"filename", status.fileName},
            {"progress", status.progress},
            {"elapsed", status.elapsedTime},
            {"remaining", status.remainingTime},
            {"x", status.x},
            {"y", status.y},
            {"z", status.z}
        };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/check/softcam") {
        json response;
        try {
            bool installed = CheckSoftcamRegistry();
            response["installed"] = installed;
            response["message"] = installed ? "Softcam is installed" : "Softcam is NOT installed";
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        catch (const std::exception&) {
            response["installed"] = false;
            response["message"] = "Error checking softcam";
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        return;
    }
    else if (path == "/api/check/lightburn") {
        json response;
        try {
            bool configured = CheckLightBurnCameraSetting();
            response["configured"] = configured;
            response["message"] = configured ? "LightBurn camera setting is correct" : "LightBurn camera setting is incorrect";
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        catch (const std::exception&) {
            response["configured"] = false;
            response["message"] = "Error checking LightBurn";
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        return;
    }
    // HTML pages
    else if (path == "/upload") {
        ServeUploadPage(clientSocket);
        return;
    }
    else if (path == "/monitor") {
        ServeMonitorPage(clientSocket);
        return;
    }
    else if (path == "/config") {
        ServeConfigPage(clientSocket);
        return;
    }
    else if (path == "/control") {
        ServeControlPage(clientSocket);
        return;
    }
    else if (path == "/capture") {
        ServeCapturePage(clientSocket);
        return;
    }
    else if (path == "/thickness") {
        ServeThicknessPage(clientSocket);
        return;
    }
    else if (path == "/about") {
        ServeAboutPage(clientSocket);
        return;
    }
    else if (path == "/gcode") {
        ServeGCodePage(clientSocket);
        return;
    }
    else if (path == "/xyzcal") {
        ServeXYZCalibrationPage(clientSocket);
        return;
    }
    else if (path == "/api/laser/focal") {
        MachineStatusData status = GetMachineStatus(false);
        json response;
        if (status.isLaser() && status.laserFocalLength > 0) {
            response = {
                {"success", true},
                {"focalLength", status.laserFocalLength}
            };
        }
        else {
            response = {
                {"success", false},
                {"message", "Laser module not detected or focal length not available"}
            };
        }
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/api/macros") {
        auto macros = LoadMacros(); // returns vector<pair<int, string>> (index, full path)
        json response = json::array();
        for (const auto& m : macros) {
            response.push_back({
                {"index", m.first},
                {"name", fs::path(m.second).filename().string()},
                {"path", m.second}
                });
        }
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
    else if (path == "/") {
        std::string lastFile;
        {
            std::lock_guard<std::mutex> lock(g_lastUploadedFilenameMutex);
            lastFile = g_lastUploadedFilename;
        }
        ServeRootPage(clientSocket, lastFile, cfg.ipAddress);
        return;
    }
    else if (path == "/launch_monitor") {
        LaunchMonitorWindow();
        SendSimpleResponse(clientSocket, 200, "Monitor window launched");
        return;
    }
    else if (path == "/mobile" || path == "/mobile/") {
        ServeMobilePage(clientSocket);
        return;
    }
    else if (path == "/discover") {
        ServeDiscoverPage(clientSocket);
    }
    else if (path == "/api/discover") {
        ServeDiscoverAPI(clientSocket);
    }
    else if (path == "/favicon.ico") {
        SendSimpleResponse(clientSocket, 404, "Not Found");
        return;
    }
    else if (path == "/config.json") {
        fs::path configPath = HostContext::instance().getConfigPath();
        if (configPath.empty()) {
            SendSimpleResponse(clientSocket, 500, "Config path is empty");
            return;
        }
        std::ifstream file(configPath);
        if (!file.is_open()) {
            SendHttpResponse(clientSocket, 200, "application/json", "{}");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        SendHttpResponse(clientSocket, 200, "application/json", content);
        return;
    }
    else {
        std::cout << GetTimeStamp() << "[DEBUG] UNHANDLED GET: " << path << std::endl;
        json response = { {"ok", true} };
        SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        return;
    }
}