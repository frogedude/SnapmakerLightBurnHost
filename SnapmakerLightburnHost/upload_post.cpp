// upload_post.cpp
#include "upload_server.hpp"
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>
#include <optional>

// Static variables for condition variable (used for file upload notification)
static std::condition_variable cv;
static std::mutex cv_mutex;
static std::atomic<bool> fileReceived{ false };

// Helper function to build header from content string (no file I/O)
static bool BuildFullHeaderFromContent(const std::string& content, std::string& header, long long totalLines) {
	try {
		std::istringstream inFile(content);
		std::string line;

		// Variables to collect - start with defaults
		double estimatedTime = 0;
		double nozzleTemp = 205;
		double bedTemp = 65;
		double workSpeed = 60;
		double minX = 999999, minY = 999999, minZ = 999999;
		double maxX = -999999, maxY = -999999, maxZ = -999999;
		bool hasDualExtruder = false;
		bool hasExtruder1Temp = false;
		double extruder1Temp = 205;

		// Track what we've found
		bool foundNozzleTemp = false;
		bool foundBedTemp = false;
		bool foundWorkSpeed = false;
		bool foundExtruder1Temp = false;

		int linesRead = 0;
		while (std::getline(inFile, line)) {
			linesRead++;

			// Early exit check: if we have everything we need
			bool haveAllEssential = (foundNozzleTemp && foundBedTemp && foundWorkSpeed);
			bool haveBounds = (minX != 999999 && minY != 999999 && minZ != 999999 &&
				maxX != -999999 && maxY != -999999 && maxZ != -999999);

			// Stop after ~1000 lines if we have all essential info and bounds
			if (haveAllEssential && haveBounds && linesRead > 1000) {
				break;
			}

			// Also stop after 2000 lines as safety
			if (linesRead > 2000) {
				break;
			}

			// Stop if we hit end marker
			if (line.find(";End of Gcode") != std::string::npos) {
				break;
			}

			// Extract extruder temperature (M104 Sxxx) - only first one
			if (!foundNozzleTemp && line.find("M104 S") != std::string::npos && line.find("T1") == std::string::npos) {
				size_t pos = line.find("M104 S");
				if (pos != std::string::npos) {
					std::string tempStr = line.substr(pos + 6);
					std::string num;
					for (char c : tempStr) {
						if (isdigit(c) || c == '.') num += c;
						else break;
					}
					if (!num.empty()) {
						nozzleTemp = std::stod(num);
						foundNozzleTemp = true;
					}
				}
			}

			// Check for dual extruder (T1 command)
			if (!hasDualExtruder && (line.find("T1") != std::string::npos || line.find("M104 T1") != std::string::npos)) {
				hasDualExtruder = true;
			}

			// Extract extruder 1 temperature (M104 T1 Sxxx)
			if (!foundExtruder1Temp && line.find("M104 T1 S") != std::string::npos) {
				size_t pos = line.find("M104 T1 S");
				if (pos != std::string::npos) {
					std::string tempStr = line.substr(pos + 9);
					std::string num;
					for (char c : tempStr) {
						if (isdigit(c) || c == '.') num += c;
						else break;
					}
					if (!num.empty()) {
						extruder1Temp = std::stod(num);
						foundExtruder1Temp = true;
					}
				}
			}

			// Extract bed temperature (M140 Sxxx)
			if (!foundBedTemp && line.find("M140 S") != std::string::npos) {
				size_t pos = line.find("M140 S");
				if (pos != std::string::npos) {
					std::string tempStr = line.substr(pos + 6);
					std::string num;
					for (char c : tempStr) {
						if (isdigit(c) || c == '.') num += c;
						else break;
					}
					if (!num.empty()) {
						bedTemp = std::stod(num);
						foundBedTemp = true;
					}
				}
			}

			// Extract work speed (F value from G1/G0 commands)
			if (!foundWorkSpeed && (line.find("G1") != std::string::npos || line.find("G0") != std::string::npos) && line.find("F") != std::string::npos) {
				size_t pos = line.find("F");
				if (pos != std::string::npos) {
					size_t endPos = line.find(" ", pos + 1);
					if (endPos == std::string::npos) endPos = line.length();
					std::string speedStr = line.substr(pos + 1, endPos - pos - 1);
					std::string num;
					for (char c : speedStr) {
						if (isdigit(c) || c == '.') num += c;
						else break;
					}
					if (!num.empty()) {
						workSpeed = std::stod(num);
						foundWorkSpeed = true;
					}
				}
			}

			// Extract min/max coordinates from G1/G0 moves
			if (line.find("G1") != std::string::npos || line.find("G0") != std::string::npos) {
				// Parse X
				size_t xPos = line.find("X");
				if (xPos != std::string::npos) {
					size_t spacePos = line.find(" ", xPos + 1);
					if (spacePos == std::string::npos) spacePos = line.length();
					std::string xStr = line.substr(xPos + 1, spacePos - xPos - 1);
					try {
						double xVal = std::stod(xStr);
						minX = std::min(minX, xVal);
						maxX = std::max(maxX, xVal);
					}
					catch (...) {}
				}
				// Parse Y
				size_t yPos = line.find("Y");
				if (yPos != std::string::npos) {
					size_t spacePos = line.find(" ", yPos + 1);
					if (spacePos == std::string::npos) spacePos = line.length();
					std::string yStr = line.substr(yPos + 1, spacePos - yPos - 1);
					try {
						double yVal = std::stod(yStr);
						minY = std::min(minY, yVal);
						maxY = std::max(maxY, yVal);
					}
					catch (...) {}
				}
				// Parse Z
				size_t zPos = line.find("Z");
				if (zPos != std::string::npos) {
					size_t spacePos = line.find(" ", zPos + 1);
					if (spacePos == std::string::npos) spacePos = line.length();
					std::string zStr = line.substr(zPos + 1, spacePos - zPos - 1);
					try {
						double zVal = std::stod(zStr);
						minZ = std::min(minZ, zVal);
						maxZ = std::max(maxZ, zVal);
					}
					catch (...) {}
				}
			}
		}

		// Fix min/max if no moves found
		if (minX == 999999) { minX = 0; maxX = 0; }
		if (minY == 999999) { minY = 0; maxY = 0; }
		if (minZ == 999999) { minZ = 0; maxZ = 0; }

		// Get printer model from config
		UserConfig cfg = HostContext::instance().snapshotConfig();
		std::string printerModel = (cfg.model == MODEL_A350) ? "Snapmaker 2.0 A350" : "Snapmaker 2.0 A250";

		// Build the header using the passed totalLines (accurate count)
		header = ";Header Start\n";
		header += ";header_type: 3dp\n";
		header += ";file_total_lines: " + std::to_string(totalLines) + "\n";
		header += ";estimated_time(s): " + std::to_string((int)estimatedTime) + "\n";
		header += ";nozzle_temperature(C): " + std::to_string((int)nozzleTemp) + "\n";
		header += ";build_plate_temperature(C): " + std::to_string((int)bedTemp) + "\n";
		header += ";work_speed(mm/minute): " + std::to_string((int)workSpeed) + "\n";
		header += ";Printer:" + printerModel + "\n";
		header += ";Extruder 0 Nozzle Size:0.4\n";
		header += ";Extruder 0 Material:PLA\n";
		header += ";Extruder 0 Print Temperature:" + std::to_string((int)nozzleTemp) + ".0\n";
		header += ";Extruder 0 Retraction Distance:0.5\n";
		header += ";Extruder 0 Switch Retraction Distance:0.5\n";

		if (hasDualExtruder) {
			header += ";Extruder 1 Nozzle Size:0.4\n";
			header += ";Extruder 1 Material:PLA\n";
			header += ";Extruder 1 Print Temperature:" + std::to_string((int)(foundExtruder1Temp ? extruder1Temp : nozzleTemp)) + ".0\n";
			header += ";Extruder 1 Retraction Distance:0.5\n";
			header += ";Extruder 1 Switch Retraction Distance:16\n";
		}

		header += ";min_x(mm): " + std::to_string(minX) + "\n";
		header += ";min_y(mm): " + std::to_string(minY) + "\n";
		header += ";min_z(mm): " + std::to_string(minZ) + "\n";
		header += ";max_x(mm): " + std::to_string(maxX) + "\n";
		header += ";max_y(mm): " + std::to_string(maxY) + "\n";
		header += ";max_z(mm): " + std::to_string(maxZ) + "\n";
		//header += ";thumbnail: data:image/png;base64,\n"; //This could crash the touchscreen
		header += ";Header End\n";

		return true;

	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[HEADER] Error building header: " << e.what() << std::endl;
		return false;
	}
}

// Single function: Process file once - add header AND strip after
static bool ProcessGcodeFile(const std::string& filepath) {
	try {
		std::ifstream inFile(filepath, std::ios::binary);
		if (!inFile.is_open()) {
			std::cout << GetTimeStamp() << "[PROCESS] Failed to open file: " << filepath << std::endl;
			return false;
		}

		std::string content((std::istreambuf_iterator<char>(inFile)),
			std::istreambuf_iterator<char>());
		inFile.close();

		// 1. Remove any existing header (always do this)
		size_t headerStartPos = content.find(";Header Start");
		if (headerStartPos != std::string::npos) {
			size_t headerEndPos = content.find(";Header End", headerStartPos);
			if (headerEndPos != std::string::npos) {
				size_t afterHeader = content.find("\n", headerEndPos);
				if (afterHeader == std::string::npos) afterHeader = content.length();
				else afterHeader++;
				content = content.substr(afterHeader);
				std::cout << GetTimeStamp() << "[PROCESS] Removed existing header" << std::endl;
			}
		}

		// 2. Strip markers (whichever comes first)
		size_t endMarkerPos = content.find(";End of Gcode");
		size_t smfixPos = content.find("; DON'T REMOVE these lines if you're using the smfix");
		if (smfixPos != std::string::npos && (endMarkerPos == std::string::npos || smfixPos < endMarkerPos)) {
			endMarkerPos = smfixPos;
		}
		if (endMarkerPos != std::string::npos) {
			content = content.substr(0, endMarkerPos);
			while (!content.empty() && (content.back() == '\n' || content.back() == '\r')) {
				content.pop_back();
			}
			std::cout << GetTimeStamp() << "[PROCESS] Stripped after marker" << std::endl;
		}

		// 3. Remove leading blank lines
		size_t startPos = 0;
		while (startPos < content.length() && (content[startPos] == '\n' || content[startPos] == '\r')) {
			startPos++;
		}
		if (startPos > 0) content = content.substr(startPos);

		// 4. Count total lines in the stripped content
		long long totalLines = std::count(content.begin(), content.end(), '\n');
		if (!content.empty() && content.back() != '\n') {
			totalLines++;
		}

		// 5. Build header from clean G-code, passing the correct totalLines
		std::string header;
		if (!BuildFullHeaderFromContent(content, header, totalLines)) {
			std::cout << GetTimeStamp() << "[PROCESS] Failed to build header" << std::endl;
			return false;
		}

		// 6. Write final file
		std::string newContent = header + content;
		std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);
		if (!outFile.is_open()) {
			std::cout << GetTimeStamp() << "[PROCESS] Failed to write file" << std::endl;
			return false;
		}
		outFile.write(newContent.data(), newContent.size());
		outFile.close();

		std::cout << GetTimeStamp() << "[PROCESS] Successfully processed file (line count: " << totalLines << ")" << std::endl;
		return true;

	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[PROCESS] Error: " << e.what() << std::endl;
		return false;
	}
}

static void CleanupTempFile(const std::string& filePath) {
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string action = cfg.fileAction;
	std::transform(action.begin(), action.end(), action.begin(), ::tolower);
	bool useRecycleBin = (action != "delete");

	try {
		if (std::filesystem::exists(filePath)) {
			if (useRecycleBin) {
				if (MoveToRecycleBin(filePath)) {
					std::cout << GetTimeStamp() << "[CLEANUP] Moved to Recycle Bin: "
						<< std::filesystem::path(filePath).filename().string() << std::endl;
				}
				else {
					std::filesystem::remove(filePath);
					std::cout << GetTimeStamp() << "[CLEANUP] Permanently deleted (Recycle Bin failed): "
						<< std::filesystem::path(filePath).filename().string() << std::endl;
				}
			}
			else {
				std::filesystem::remove(filePath);
				std::cout << GetTimeStamp() << "[CLEANUP] Permanently deleted: "
					<< std::filesystem::path(filePath).filename().string() << std::endl;
			}
		}
	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[CLEANUP] Failed to clean up temp file: " << e.what() << std::endl;
	}
}

void HandlePostRequest(SOCKET clientSocket, const std::string& path, const std::string& headers,
    const std::string& requestStr, const std::string& clientIP,
    const std::vector<char>& fullRequest) {
    UserConfig cfg = HostContext::instance().snapshotConfig();

    // ----- Authentication check (only for remote clients) -----
    if (clientIP != "127.0.0.1" && clientIP != "::1") {
        if (!CheckBasicAuth(headers, cfg)) {
            SendAuthRequired(clientSocket);
            return;
        }
    }
    // ----- end authentication check -----

    if (path == "/api/files/local" || path.find("/api/files/local") == 0) {
        {
            std::lock_guard<std::mutex> lock(cv_mutex);
            fileReceived = true;
        }
        cv.notify_one();

        MultipartData uploadData;
        if (!ParseMultipartForm(requestStr, fullRequest, (int)fullRequest.size(), uploadData)) {
            SendSimpleResponse(clientSocket, 400, "Failed to parse multipart form");
            return;
        }
        if (uploadData.content.empty()) {
            SendSimpleResponse(clientSocket, 400, "File is empty");
            return;
        }

        // -----------------------------------------------------------------
        // 1. Attempt to connect and power on via Kasa if needed
        // -----------------------------------------------------------------
        bool isOnline = (ConnectAndMaintainConnection() == CURLE_OK);
        if (!isOnline && cfg.kasa.autoPowerOn && !cfg.kasa.ipAddress.empty() && IsValidIPAddress(cfg.kasa.ipAddress)) {
            std::cout << GetTimeStamp() << "[POWER] Printer offline, powering on via Kasa..." << std::endl;
            if (SetKasaPlugState(cfg.kasa.ipAddress, true)) {
                std::cout << GetTimeStamp() << "[POWER] Kasa plug turned ON. Waiting for machine to boot..." << std::endl;
                bool booted = false;
                for (int i = 0; i < 60; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (ConnectAndMaintainConnection() == CURLE_OK) {
                        MachineStatusData testStatus = GetMachineStatus(false);
                        if (!IsMachineOffline(testStatus)) {
                            booted = true;
                            std::cout << GetTimeStamp() << "[POWER] Machine is now online." << std::endl;
                            break;
                        }
                    }
                }
                if (!booted) {
                    SendSimpleResponse(clientSocket, 503, "Machine offline - power-on succeeded but machine did not boot in time");
                    return;
                }
            }
            else {
                SendSimpleResponse(clientSocket, 503, "Machine offline - Kasa power-on failed");
                return;
            }
        }
        else if (!isOnline) {
            SendSimpleResponse(clientSocket, 503, "Machine offline - cannot connect and Kasa not available");
            return;
        }

        // Now the machine should be online
        MachineStatusData status = GetMachineStatus(false);
        if (IsMachineOffline(status)) {
            SendSimpleResponse(clientSocket, 503, "Machine offline - cannot verify module");
            return;
        }

        // -----------------------------------------------------------------
        // 2. Continue with module validation, homing, upload/print...
        // -----------------------------------------------------------------
        std::string toolhead = status.getToolHeadDisplay();
        if (!IsFileExtensionAllowed(uploadData.filename, toolhead, uploadData.printAfterUpload)) {
            std::string msg = "File type not supported for current module: " + toolhead;
            if (toolhead == "Laser")
                msg += uploadData.printAfterUpload ? " (allowed: .nc, .gcode)" : " (allowed: .nc)";
            else if (toolhead == "CNC")
                msg += uploadData.printAfterUpload ? " (allowed: .cnc, .gcode)" : " (allowed: .cnc)";
            else if (toolhead == "3D Printer" || toolhead == "3DP")
                msg += " (allowed: .gcode)";
            else
                msg += " (allowed: .gcode)";
            SendSimpleResponse(clientSocket, 400, msg);
            return;
        }

        fs::path tempFile = GetSnapmakerTempPath() / uploadData.filename;
        std::ofstream out(tempFile, std::ios::binary);
        if (!out.is_open()) {
            SendSimpleResponse(clientSocket, 500, "Could not create temp file");
            return;
        }
        out.write(uploadData.content.data(), uploadData.content.size());
        out.close();

        {
            std::lock_guard<std::mutex> lock(g_lastUploadedFilenameMutex);
            g_lastUploadedFilename = uploadData.filename;
        }
        std::cout << GetTimeStamp() << "[UPLOAD] Received " << uploadData.filename << std::endl;

        {
            std::lock_guard<std::mutex> lock(g_selectedFileMutex);
            g_selectedFilename = uploadData.filename;
        }

        std::string tempFilePath = tempFile.string();
        std::string uploadResponse;

        UserConfig cfg = HostContext::instance().snapshotConfig();

        // Process G-code
        ProcessGcodeFile(tempFilePath);

        if (uploadData.printAfterUpload) {
            std::cout << GetTimeStamp() << "[UPLOAD] Auto-print requested - starting print" << std::endl;
            {
                std::lock_guard<std::mutex> lock(g_lastPrintMutex);
                g_lastPrintStart = std::chrono::steady_clock::now();
            }

            // Get machine status and check homing
            MachineStatusData status = GetMachineStatus(false);
            if (!status.homed) {
                std::cout << GetTimeStamp() << "[PRINT] Machine not homed. Homing now..." << std::endl;
                HomeMachine();
                std::this_thread::sleep_for(std::chrono::seconds(5));
                status = GetMachineStatus(false);
                if (!status.homed) {
                    std::cout << GetTimeStamp() << "[PRINT] Failed to home machine. Aborting print." << std::endl;
                    SendSimpleResponse(clientSocket, 500, "Failed to home machine");
                    CleanupTempFile(tempFilePath);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(2));
                std::cout << GetTimeStamp() << "[PRINT] Machine homed successfully" << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[PRINT] Machine already homed" << std::endl;
            }

            // Check filament
            if (status.isFilamentOut) {
                std::cout << GetTimeStamp() << "[PRINT] WARNING: Filament is out!" << std::endl;
                MessageBoxA(NULL,
                    "Filament is out! Please load filament and try again.",
                    "Snapmaker Bridge - Filament Warning",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST);
                SendSimpleResponse(clientSocket, 409, "Filament is out");
                CleanupTempFile(tempFilePath);
                return;
            }

            // Check if machine is busy
            status = GetMachineStatus(false);
            if (status.isBusy()) {
                std::cout << GetTimeStamp() << "[PRINT] Machine is busy, aborting" << std::endl;
                SendSimpleResponse(clientSocket, 409, "Machine is busy");
                CleanupTempFile(tempFilePath);
                return;
            }
            std::cout << GetTimeStamp() << "[PRINT] Machine is idle and ready" << std::endl;

            // Refresh connection
            std::cout << GetTimeStamp() << "[PRINT] Refreshing connection..." << std::endl;
            ConnectAndMaintainConnection();
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Prepare print
            std::cout << GetTimeStamp() << "[PRINT] Preparing print..." << std::endl;
            std::string prepareResponse;
            if (PreparePrintOnSnapmaker(tempFilePath, "3DP", prepareResponse, CURL_UPLOAD_TIMEOUT) == CURLE_OK) {
                std::cout << std::endl;
                std::cout << GetTimeStamp() << "[PRINT] File prepared successfully" << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[PRINT] Prepare print failed." << std::endl;
                SendSimpleResponse(clientSocket, 500, "Failed to upload file to printer");
                CleanupTempFile(tempFilePath);
                return;
            }

            // Start the print
            if (ConnectAndMaintainConnection() == CURLE_OK) {
                std::string startUrl = BuildStartPrintUrl(cfg.ipAddress, cfg.token);
                std::string startResp;
                auto [res, code] = PerformHttpPostWithCode(startUrl, "", startResp, 10);
                if (res == CURLE_OK && (code == 200 || code == 202 || code == 204)) {
                    std::cout << GetTimeStamp() << "[PRINT] Started successfully" << std::endl;

                    // Launch camera
                    if (cfg.rtspCamera.autoLaunch && IsRtspCameraConfigured(cfg.rtspCamera) && !HostContext::instance().isCameraRunning()) {
                        std::cout << GetTimeStamp() << "Launching camera in detached mode..." << std::endl;
                        if (HostContext::instance().startCameraDetached(cfg.rtspCamera.rtspUrl)) {
                            std::cout << GetTimeStamp() << "Camera launched successfully. VLC will remain open after this program exits.\n";
                        }
                        else {
                            std::cout << GetTimeStamp() << "Failed to launch camera. Check RTSP URL and VLC path.\n";
                        }
                    }
                    else if (HostContext::instance().isCameraRunning()) {
                        std::cout << GetTimeStamp() << "Camera already running.\n";
                    }
                }
                else if (code == 409) {
                    std::cout << GetTimeStamp() << "Machine not ready. Check filament, door or homing.\n";
                    SendSimpleResponse(clientSocket, 409, "Machine not ready. Check filament, door or homing.");
                    CleanupTempFile(tempFilePath);
                    return;
                }
                else {
                    std::cout << GetTimeStamp() << "[PRINT] Failed to start, HTTP " << code << std::endl;
                    SendSimpleResponse(clientSocket, 500, "Failed to start print. HTTP " + std::to_string(code));
                    CleanupTempFile(tempFilePath);
                    return;
                }
            }
            else {
                std::cout << GetTimeStamp() << "[PRINT] Failed to connect" << std::endl;
                SendSimpleResponse(clientSocket, 503, "Lost connection to printer");
                CleanupTempFile(tempFilePath);
                return;
            }

            // Cleanup temp file
            std::cout << GetTimeStamp() << "[PRINT] Cleaning up temp file..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            CleanupTempFile(tempFilePath);

        }
        else {
            // Upload only
            std::cout << GetTimeStamp() << "[UPLOAD] Upload only - file uploaded but not printing" << std::endl;

            // Check if machine is busy
            MachineStatusData status = GetMachineStatus(false);
            if (status.isBusy()) {
                std::cout << GetTimeStamp() << "[PRINT] Machine is busy, aborting" << std::endl;
                SendSimpleResponse(clientSocket, 409, "Machine is busy");
                CleanupTempFile(tempFilePath);
                return;
            }
            std::cout << GetTimeStamp() << "[PRINT] Machine is idle and ready" << std::endl;

            // Upload file
            CURLcode uploadRes = UploadFileToSnapmaker(tempFilePath, uploadResponse, CURL_UPLOAD_TIMEOUT);
            std::cout << std::endl;
            if (uploadRes != CURLE_OK) {
                std::cout << GetTimeStamp() << "[UPLOAD] Failed to upload to printer" << std::endl;
                SendSimpleResponse(clientSocket, 500, "Failed to upload to printer");
                CleanupTempFile(tempFilePath);
                return;
            }

            std::cout << GetTimeStamp() << "[UPLOAD] File uploaded successfully" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            CleanupTempFile(tempFilePath);
        }

        // ========== SEND SUCCESS RESPONSE (ONLY HERE) ==========
        std::string jsonResponse = "{\"files\":{\"local\":{\"name\":\"" + uploadData.filename +
            "\",\"path\":\"" + uploadData.filename +
            "\",\"type\":\"machinecode\",\"size\":" + std::to_string(uploadData.content.size()) +
            ",\"date\":0,\"display\":\"" + uploadData.filename +
            "\"}},\"done\":true}";
        SendHttpResponse(clientSocket, 200, "application/json", jsonResponse);

        return;
    }
    else if (path == "/api/files/local/select" || path.find("/api/files/local/select") == 0) {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }
        try {
            json data = json::parse(body);
            std::string filename = data.value("filename", "");
            {
                std::lock_guard<std::mutex> lock(g_selectedFileMutex);
                if (!filename.empty()) g_selectedFilename = filename;
            }
            json response = {
                {"files", {
                    {"local", {
                        {"name", filename},
                        {"path", filename},
                        {"type", "machinecode"},
                        {"size", 0},
                        {"date", 0},
                        {"display", filename}
                    }}
                }},
                {"done", true}
            };
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        catch (const std::exception&) {
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
        }
        return;
    }
    else if (path == "/api/job" || path.find("/api/job") == 0) {
        // Parse the POST body for commands
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }

        try {
            json data = json::parse(body);
            std::string command = data.value("command", "");

            if (command == "start") {
                std::cout << GetTimeStamp() << "[JOB] Start command received" << std::endl;
                SendSimpleResponse(clientSocket, 202, "Print started");
                return;
            }
            else if (command == "pause") {
                auto [success, msg] = PausePrint();
                SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
                return;
            }
            else if (command == "cancel" || command == "stop") {
                auto [success, msg] = StopPrint();
                SendSimpleResponse(clientSocket, success ? 204 : 500, "");
                return;
            }
            else if (command == "restart") {
                auto [success, msg] = RePrint();
                SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
                return;
            }
        }
        catch (const std::exception& e) {
            std::cout << GetTimeStamp() << "[JOB] Error: " << e.what() << std::endl;
        }

        SendSimpleResponse(clientSocket, 204, "");
        return;
    }
    else if (path == "/api/job" && path.find("cancel") != std::string::npos) {
        auto [success, msg] = StopPrint();
        if (success) {
            std::cout << GetTimeStamp() << "[JOB] Print cancelled" << std::endl;
        }
        SendSimpleResponse(clientSocket, success ? 204 : 500, "");
        return;
    }
    else if (path == "/api/home") {
        CURLcode res = SendGCodeWithRetry("G28", HOMING_RETRY_ATTEMPTS, true);
        SendSimpleResponse(clientSocket, res == CURLE_OK ? 200 : 500, res == CURLE_OK ? "Machine homed" : "Homing failed");
        return;
    }
    else if (path == "/api/pause") {
        auto [success, msg] = PausePrint();
        // Don't track state variables
        if (success) {
            std::cout << GetTimeStamp() << "[PAUSE] Print paused" << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[PAUSE] Failed to pause: " << msg << std::endl;
        }
        SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        return;
    }
    else if (path == "/api/resume") {
        auto [success, msg] = ResumePrint();
        if (success) {
            std::cout << GetTimeStamp() << "[RESUME] Print resumed" << std::endl;
        }
        SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        return;
    }
    else if (path == "/api/stop") {
        auto [success, msg] = StopPrint();
        if (success) {
            std::cout << GetTimeStamp() << "[STOP] Print stopped" << std::endl;
        }
        SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        return;
    }
    else if (path == "/api/reprint" || path.find("/api/reprint") == 0) {
        auto [success, msg] = RePrint();
        if (success) {
            std::cout << GetTimeStamp() << "[REPRINT] Reprint started" << std::endl;
        }
        SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        return;
    }
    else if (path == "/api/enclosure/fan") {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }
        try {
            json data = json::parse(body);
            std::string speedStr = data.value("speed", "off");
            auto [success, msg] = SetEnclosureFanSpeed(speedStr);
            SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        }
        catch (const std::exception&) {
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
        }
        return;
    }
    else if (path == "/api/enclosure/led") {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }
        try {
            json data = json::parse(body);
            std::string brightStr = data.value("brightness", "off");
            auto [success, msg] = SetEnclosureLed(brightStr);
            SendSimpleResponse(clientSocket, success ? 200 : 500, msg);
        }
        catch (const std::exception&) {
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
        }
        return;
    }
    else if (path == "/api/gcode/send") {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }
        try {
            json data = json::parse(body);
            std::string gcode = data.value("gcode", "");
            if (gcode.empty()) {
                SendSimpleResponse(clientSocket, 400, "No G-code provided");
                return;
            }
            std::string response;
            CURLcode res = SendRawGCodeWithResponse(gcode, response);
            if (res == CURLE_OK) {
                try {
                    json respJson = json::parse(response);
                    std::string msg = respJson.value("message", response);
                    SendSimpleResponse(clientSocket, 200, msg);
                }
                catch (...) {
                    SendSimpleResponse(clientSocket, 200, response.empty() ? "OK" : response);
                }
            }
            else {
                SendSimpleResponse(clientSocket, 500, "G-code failed");
            }
        }
        catch (const std::exception&) {
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
        }
        return;
    }
    else if (path == "/api/kasa/on") {
        UserConfig cfg = HostContext::instance().snapshotConfig();
        if (cfg.kasa.ipAddress.empty()) {
            SendSimpleResponse(clientSocket, 400, "Kasa IP not configured");
            return;
        }
        if (SetKasaPlugState(cfg.kasa.ipAddress, true)) {
            SendSimpleResponse(clientSocket, 200, "Power ON");
        }
        else {
            SendSimpleResponse(clientSocket, 500, "Failed to turn ON");
        }
        return;
    }
    else if (path == "/api/kasa/off") {
        UserConfig cfg = HostContext::instance().snapshotConfig();
        if (cfg.kasa.ipAddress.empty()) {
            SendSimpleResponse(clientSocket, 400, "Kasa IP not configured");
            return;
        }
        if (SetKasaPlugState(cfg.kasa.ipAddress, false)) {
            SendSimpleResponse(clientSocket, 200, "Power OFF");
        }
        else {
            SendSimpleResponse(clientSocket, 500, "Failed to turn OFF");
        }
        return;
    }
    else if (path == "/api/camera/start") {
        UserConfig cfg = HostContext::instance().snapshotConfig();
        std::string rtspUrl = cfg.rtspCamera.rtspUrl;
        if (rtspUrl.empty()) {
            SendSimpleResponse(clientSocket, 400, "RTSP URL not configured");
            return;
        }
        if (!CheckVlcInstalled()) {
            SendSimpleResponse(clientSocket, 400, "VLC not installed");
            return;
        }
        if (HostContext::instance().startCameraDetached(rtspUrl)) {
            SendSimpleResponse(clientSocket, 200, "Camera launched");
        }
        else {
            SendSimpleResponse(clientSocket, 500, "Failed to launch camera");
        }
        return;
    }
    else if (path == "/api/machine/setup") {
        MachineStatusData status = GetMachineStatus(false);
        if (IsMachineOffline(status)) {
            json error = { {"status", "error"}, {"message", "Machine is offline"} };
            SendHttpResponse(clientSocket, 503, "application/json", error.dump());
            return;
        }
        if (status.isBusy()) {
            json error = { {"status", "error"}, {"message", "Machine is busy. Stop any running job first."} };
            SendHttpResponse(clientSocket, 409, "application/json", error.dump());
            return;
        }

        PerformMachineSetup();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        MachineStatusData newStatus = GetMachineStatus(false);

        if (newStatus.homed) {
            json response = { {"status", "ok"}, {"message", "Machine homed and calibrated"} };
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        else {
            json error = { {"status", "error"}, {"message", "Setup failed – machine not homed"} };
            SendHttpResponse(clientSocket, 500, "application/json", error.dump());
        }
        return;
    }
    else if (path == "/config") {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }
        try {
            json configJson = json::parse(body);
            fs::path configPath = HostContext::instance().getConfigPath();
            if (configPath.empty()) {
                SendSimpleResponse(clientSocket, 500, "Config path is empty");
                return;
            }
            fs::path configDir = configPath.parent_path();
            if (!configDir.empty() && !fs::exists(configDir)) {
                fs::create_directories(configDir);
            }
            std::string tempPath = configPath.string() + ".tmp";
            std::ofstream file(tempPath, std::ios::out | std::ios::trunc);
            if (!file.is_open()) {
                SendSimpleResponse(clientSocket, 500, "Failed to open config file");
                return;
            }
            file << std::setw(4) << configJson << std::endl;
            file.close();
            if (fs::exists(configPath)) {
                fs::remove(configPath);
            }
            fs::rename(tempPath, configPath);
            UserConfig newConfig;
            LoadConfig(configPath.string(), newConfig);
            HostContext::instance().updateConfig([newConfig](UserConfig& cfg) {
                cfg = newConfig;
                return true;
                });
            SendSimpleResponse(clientSocket, 200, "Configuration saved");
        }
        catch (const json::parse_error&) {
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
        }
        catch (const std::exception&) {
            SendSimpleResponse(clientSocket, 500, "Save failed");
        }
        return;
    }
    else if (path == "/upload") {
        // 1. Parse multipart data
        MultipartData uploadData;
        if (!ParseMultipartForm(requestStr, fullRequest, (int)fullRequest.size(), uploadData)) {
            SendSimpleResponse(clientSocket, 400, "Failed to parse multipart form");
            return;
        }

        // 2. Get machine status
        MachineStatusData status = GetMachineStatus(false);
        if (IsMachineOffline(status)) {
            SendSimpleResponse(clientSocket, 503, "Machine offline - cannot verify module");
            return;
        }

        // 3. Validate file extension with print flag
        std::string toolhead = status.getToolHeadDisplay();
        if (!IsFileExtensionAllowed(uploadData.filename, toolhead, uploadData.printAfterUpload)) {
            std::string msg = "File type not supported for current module with this operation.";
            SendSimpleResponse(clientSocket, 400, msg);
            return;
        }

        // 4. Save temp file and process
        fs::path tempFilePath = GetSnapmakerTempPath() / uploadData.filename;
        std::ofstream file(tempFilePath, std::ios::binary);
        if (!file.is_open()) {
            SendSimpleResponse(clientSocket, 500, "Failed to save file");
            return;
        }
        file.write(uploadData.content.data(), uploadData.content.size());
        file.close();

        int operation = uploadData.printAfterUpload ? FILEOP_AUTO_START : FILEOP_UPLOAD_ONLY;
        bool success = ProcessFileUpload(tempFilePath.string(), operation, false);
        if (success) {
            SendSimpleResponse(clientSocket, 200, uploadData.printAfterUpload ? "Print started" : "Upload complete");
        }
        else {
            SendSimpleResponse(clientSocket, 500, "Failed to process file");
        }
        return;
    }
    else if (path == "/api/printer/bed" || path.find("/api/printer/bed") == 0) {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }

        std::cout << GetTimeStamp() << "[BED] Bed command received: " << body << std::endl;

        try {
            json data = json::parse(body);

            if (data.contains("command")) {
                std::string command = data["command"];

                if (command == "target") {
                    if (data.contains("target")) {
                        int target = data["target"];
                        std::cout << GetTimeStamp() << "[BED] Setting bed target temperature to " << target << "C" << std::endl;

                        std::string gcode = "M140 S" + std::to_string(target);
                        CURLcode res = SendRawGCode(gcode);
                        SendSimpleResponse(clientSocket, res == CURLE_OK ? 204 : 500, "");
                        return;
                    }
                }
                else if (command == "off") {
                    std::cout << GetTimeStamp() << "[BED] Turning bed off" << std::endl;
                    std::string gcode = "M140 S0";
                    CURLcode res = SendRawGCode(gcode);
                    SendSimpleResponse(clientSocket, res == CURLE_OK ? 204 : 500, "");
                    return;
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << GetTimeStamp() << "[BED] Error: " << e.what() << std::endl;
        }
        SendSimpleResponse(clientSocket, 204, "");
        return;
    }
    else if (path == "/api/printer/command" || path.find("/api/printer/command") == 0) {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }

        std::cout << GetTimeStamp() << "[PRINTER] Command received: " << body << std::endl;

        try {
            json data = json::parse(body);

            if (data.contains("command")) {
                std::string command = data["command"];
                std::cout << GetTimeStamp() << "[PRINTER] Executing single command: " << command << std::endl;

                std::string response;
                CURLcode res = SendRawGCodeWithResponse(command, response);

                if (res == CURLE_OK) {
                    std::cout << GetTimeStamp() << "[PRINTER] Command successful, response: " << response << std::endl;
                    SendSimpleResponse(clientSocket, 204, "");
                }
                else {
                    std::cout << GetTimeStamp() << "[PRINTER] Command failed: " << curl_easy_strerror(res) << std::endl;
                    SendSimpleResponse(clientSocket, 500, "Command failed");
                }
                return;
            }
            else if (data.contains("commands") && data["commands"].is_array()) {
                const auto& commands = data["commands"];
                std::cout << GetTimeStamp() << "[PRINTER] Executing " << commands.size() << " commands" << std::endl;

                bool allSuccess = true;
                for (const auto& cmd : commands) {
                    std::string command = cmd;
                    std::cout << GetTimeStamp() << "[PRINTER] Executing: " << command << std::endl;

                    std::string response;
                    CURLcode res = SendRawGCodeWithResponse(command, response);

                    if (res != CURLE_OK) {
                        std::cout << GetTimeStamp() << "[PRINTER] Command failed: " << command << " - " << curl_easy_strerror(res) << std::endl;
                        allSuccess = false;
                        break;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                if (allSuccess) {
                    SendSimpleResponse(clientSocket, 204, "");
                }
                else {
                    SendSimpleResponse(clientSocket, 500, "One or more commands failed");
                }
                return;
            }
        }
        catch (const std::exception& e) {
            std::cout << GetTimeStamp() << "[PRINTER] Error parsing JSON: " << e.what() << std::endl;
        }

        SendSimpleResponse(clientSocket, 400, "Invalid command format");
        return;
    }
    else if (path == "/api/macros/run") {
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) body = requestStr.substr(bodyPos + 4);
        try {
            json data = json::parse(body);
            std::string macroName = data.value("macro", "");
            if (macroName.empty()) {
                SendSimpleResponse(clientSocket, 400, "No macro name provided");
                return;
            }

            // Find the macro file path
            auto macros = LoadMacros();
            fs::path macroPath;
            for (const auto& m : macros) {
                if (fs::path(m.second).filename().string() == macroName) {
                    macroPath = m.second;
                    break;
                }
            }
            if (macroPath.empty() || !fs::exists(macroPath)) {
                SendSimpleResponse(clientSocket, 404, "Macro not found");
                return;
            }

            auto results = ExecuteMacroWithResponse(macroPath.string());
            json response = {
                {"success", true},
                {"results", json::array()}
            };
            for (const auto& r : results) {
                response["results"].push_back({
                    {"command", r.first},
                    {"status", r.second}
                    });
            }
            SendHttpResponse(clientSocket, 200, "application/json", response.dump());
        }
        catch (const std::exception& e) {
            SendSimpleResponse(clientSocket, 400, std::string("Error: ") + e.what());
        }
        return;
    }
    else if (path == "/api/connection") {
        std::cout << GetTimeStamp() << "[CONNECTION] POST request received" << std::endl;

        ThrottledConnect();

        // Parse body to get command
        size_t bodyPos = requestStr.find("\r\n\r\n");
        std::string body;
        if (bodyPos != std::string::npos) {
            body = requestStr.substr(bodyPos + 4);
        }

        try {
            json data = json::parse(body);
            std::string command = data.value("command", "");

            if (command == "connect") {
                std::cout << GetTimeStamp() << "[CONNECTION] Connect command - establishing connection" << std::endl;
                CURLcode res = ConnectAndMaintainConnection();

                if (res == CURLE_OK) {
                    json response = {
                        {"current", {
                            {"state", "Operational"},
                            {"port", "virtual"},
                            {"baudrate", 115200},
                            {"printerProfile", "_default"}
                        }}
                    };
                    SendHttpResponse(clientSocket, 200, "application/json", response.dump());
                }
                else {
                    SendSimpleResponse(clientSocket, 500, "Connection failed");
                }
                return;
            }
            else if (command == "disconnect") {
                std::cout << GetTimeStamp() << "[CONNECTION] Disconnect command received" << std::endl;
                SendSimpleResponse(clientSocket, 204, "");
                return;
            }
            else if (command == "fake_ack") {
                SendSimpleResponse(clientSocket, 204, "");
                return;
            }
            else {
                std::cout << GetTimeStamp() << "[CONNECTION] Unknown command: " << command << std::endl;
                SendSimpleResponse(clientSocket, 204, "");
                return;
            }
        }
        catch (const std::exception& e) {
            std::cout << GetTimeStamp() << "[CONNECTION] Error parsing JSON: " << e.what() << std::endl;
            SendSimpleResponse(clientSocket, 400, "Invalid JSON");
            return;
        }
    }
    else if (path.find("/plugin/") == 0) {
        SendSimpleResponse(clientSocket, 204, "");
        return;
    }
    else {
        SendSimpleResponse(clientSocket, 404, "Not Found");
    }
}