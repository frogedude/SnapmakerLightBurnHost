// config_manager.cpp - Load/Save config.json, LoadLuban
#include "host.h"

bool LoadLuban(UserConfig& cfg)
{
	try {
		const char* appdata = getenv("APPDATA");
		if (!appdata) return false;

		fs::path path;
		try {
			path = fs::path(appdata) / LUBAN_CONFIG_PATH;
			if (!fs::exists(path)) return false;
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cout << GetTimeStamp() << "[ERR] Filesystem error accessing Luban config: " << e.what() << std::endl;
			return false;
		}
		catch (const std::exception& e) {
			std::cout << GetTimeStamp() << "[ERR] Unexpected error checking Luban config path: " << e.what() << std::endl;
			return false;
		}

		std::ifstream f(path);
		if (!f.is_open()) {
			std::cout << GetTimeStamp() << "[ERR] Could not open Luban config file: " << path.string() << std::endl;
			return false;
		}

		json j;
		try {
			j = json::parse(f);
		}
		catch (const json::parse_error& e) {
			std::cout << GetTimeStamp() << "[ERR] Failed to parse Luban config JSON: " << e.what() << std::endl;
			return false;
		}

		if (j.contains("state") && j["state"].is_object() &&
			j["state"].contains("server") && j["state"]["server"].is_object() &&
			j["state"]["server"].contains("address") && j["state"]["server"]["address"].is_string() &&
			j["state"]["server"].contains("token") && j["state"]["server"]["token"].is_string()) {

			cfg.ipAddress = j["state"]["server"]["address"];
			cfg.token = j["state"]["server"]["token"];

			std::cout << GetTimeStamp() << "[OK] Loaded Luban config" << std::endl;
			std::cout << GetTimeStamp() << "Snapmaker IP: " << cfg.ipAddress << " & Token: " << cfg.token << std::endl;

			if (j.contains("state") && j["state"].is_object() &&
				j["state"].contains("machine") && j["state"]["machine"].is_object() &&
				j["state"]["machine"].contains("series") && j["state"]["machine"]["series"].is_string()) {

				std::string series = j["state"]["machine"]["series"].get<std::string>();
				if (series.find("A350") != std::string::npos) {
					cfg.model = MODEL_A350;
					cfg.basePositionX = A350_BASE_POS_X;
					cfg.basePositionY = A350_BASE_POS_Y;
					cfg.basePositionZ = A350_BASE_POS_Z;
					std::cout << GetTimeStamp() << "[OK] Detected model: Snapmaker 2.0 A350" << std::endl;
				}
				else if (series.find("A250") != std::string::npos) {
					cfg.model = MODEL_A250;
					cfg.basePositionX = A250_BASE_POS_X;
					cfg.basePositionY = A250_BASE_POS_Y;
					cfg.basePositionZ = A250_BASE_POS_Z;
					std::cout << GetTimeStamp() << "[OK] Detected model: Snapmaker 2.0 A250" << std::endl;
				}
				else {
					std::cout << GetTimeStamp() << "[WARN] Unknown model series: " << series << std::endl;
				}
			}
			HostContext::instance().updateConfig([&cfg](UserConfig& globalCfg) {
				globalCfg.ipAddress = cfg.ipAddress;
				globalCfg.token = cfg.token;
				globalCfg.model = cfg.model;
				globalCfg.basePositionX = cfg.basePositionX;
				globalCfg.basePositionY = cfg.basePositionY;
				globalCfg.basePositionZ = cfg.basePositionZ;
				return true;
				});
			return true;
		}
		return false;
	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[ERR] Error loading Luban config: " << e.what() << std::endl;
		return false;
	}
}

bool LoadConfig(const std::string& file, UserConfig& cfg) {
	if (!fs::exists(file)) return false;
	try {
		std::ifstream f(file);
		if (!f.is_open()) {
			std::cout << GetTimeStamp() << "[ERR] Could not open config file: " << file << std::endl;
			return false;
		}
		json j = json::parse(f);

		auto trim = [](const std::string& s) {
			size_t start = s.find_first_not_of(" \t\n\r");
			if (start == std::string::npos) return std::string("");
			size_t end = s.find_last_not_of(" \t\n\r");
			return s.substr(start, end - start + 1);
			};

		// ========== MACHINE SETTINGS ==========
		if (j.contains("machine") && j["machine"].is_object()) {
			auto& machine = j["machine"];

			// Load IP address
			if (machine.contains("ipAddress") && machine["ipAddress"].is_string()) {
				std::string ip = machine["ipAddress"].get<std::string>();
				ip = trim(ip);
				if (!ip.empty() && IsValidIPAddress(ip)) {
					cfg.ipAddress = ip;
				}
				else if (!ip.empty()) {
					std::cout << GetTimeStamp() << "[WARN] Invalid IP address in config: " << ip << std::endl;
				}
			}

			// Load token
			if (machine.contains("token") && machine["token"].is_string()) {
				std::string token = machine["token"].get<std::string>();
				token = trim(token);
				if (!token.empty()) {
					cfg.token = token;
				}
			}

			// Load model
			if (machine.contains("model") && machine["model"].is_number()) {
				int modelVal = machine["model"].get<int>();
				switch (modelVal) {
				case 0: cfg.model = MODEL_A350; break;
				case 1: cfg.model = MODEL_A250; break;
				default:
					std::cout << GetTimeStamp() << "[WARN] Unknown model value " << modelVal
						<< " in " << CONFIG_FILE << ". Ignoring." << std::endl;
					break;
				}
			}

			// Load auto-start print
			if (machine.contains("autoStartPrint") && machine["autoStartPrint"].is_boolean())
				cfg.autoStartPrint = machine["autoStartPrint"].get<bool>();
			else
				cfg.autoStartPrint = false;

			// Load auto-monitor print
			if (machine.contains("autoMonitorPrint") && machine["autoMonitorPrint"].is_boolean())
				cfg.autoMonitorPrint = machine["autoMonitorPrint"].get<bool>();
			else
				cfg.autoMonitorPrint = true;

			// Load file action (delete or recycle)
			if (machine.contains("fileAction") && machine["fileAction"].is_string()) {
				cfg.fileAction = machine["fileAction"].get<std::string>();
			}

			// Load bindLocalhost
			if (machine.contains("bindLocalhost") && machine["bindLocalhost"].is_boolean())
				cfg.bindLocalhost = machine["bindLocalhost"].get<bool>();
			else
				cfg.bindLocalhost = true;
		}
		else {
			// Legacy flat format fallback
			if (j.contains("ipAddress") && j["ipAddress"].is_string()) {
				std::string ip = j["ipAddress"].get<std::string>();
				ip = trim(ip);
				if (!ip.empty() && IsValidIPAddress(ip)) {
					cfg.ipAddress = ip;
				}
			}
			if (j.contains("token") && j["token"].is_string()) {
				std::string token = j["token"].get<std::string>();
				token = trim(token);
				if (!token.empty()) cfg.token = token;
			}
			if (j.contains("model") && j["model"].is_number()) {
				int modelVal = j["model"].get<int>();
				switch (modelVal) {
				case 0: cfg.model = MODEL_A350; break;
				case 1: cfg.model = MODEL_A250; break;
				default: break;
				}
			}
			if (j.contains("autoStartPrint") && j["autoStartPrint"].is_boolean())
				cfg.autoStartPrint = j["autoStartPrint"].get<bool>();
			else
				cfg.autoStartPrint = false;

			if (j.contains("fileAction") && j["fileAction"].is_string()) {
				cfg.fileAction = j["fileAction"].get<std::string>();
			}
		}

		// Set default fileAction if empty
		if (cfg.fileAction.empty()) {
			cfg.fileAction = "recycle";
		}

		// ========== BASE POSITION SETTINGS ==========
		if (j.contains("basePosition") && j["basePosition"].is_object()) {
			auto& pos = j["basePosition"];
			if (pos.contains("x") && pos["x"].is_number())
				cfg.basePositionX = pos["x"].get<double>();
			if (pos.contains("y") && pos["y"].is_number())
				cfg.basePositionY = pos["y"].get<double>();
			if (pos.contains("z") && pos["z"].is_number())
				cfg.basePositionZ = pos["z"].get<double>();
		}
		else {
			// Legacy flat format fallback
			if (j.contains("basePositionX") && j["basePositionX"].is_number())
				cfg.basePositionX = j["basePositionX"].get<double>();
			if (j.contains("basePositionY") && j["basePositionY"].is_number())
				cfg.basePositionY = j["basePositionY"].get<double>();
			if (j.contains("basePositionZ") && j["basePositionZ"].is_number())
				cfg.basePositionZ = j["basePositionZ"].get<double>();
		}

		// ========== IMAGE SETTINGS ==========
		if (j.contains("image") && j["image"].is_object() && j["image"].contains("adjustment")) {
			auto& adj = j["image"]["adjustment"];
			if (adj.contains("pixelsX") && adj["pixelsX"].is_number()) {
				int val = adj["pixelsX"].get<int>();
				if (val < -MAX_ADJUST_PIXELS) val = -MAX_ADJUST_PIXELS;
				else if (val > MAX_ADJUST_PIXELS) val = MAX_ADJUST_PIXELS;
				cfg.adjustment.pixelsX = val;
			}
			if (adj.contains("pixelsY") && adj["pixelsY"].is_number()) {
				int val = adj["pixelsY"].get<int>();
				if (val < -MAX_ADJUST_PIXELS) val = -MAX_ADJUST_PIXELS;
				else if (val > MAX_ADJUST_PIXELS) val = MAX_ADJUST_PIXELS;
				cfg.adjustment.pixelsY = val;
			}
			if (adj.contains("useAdjustment") && adj["useAdjustment"].is_boolean())
				cfg.adjustment.useAdjustment = adj["useAdjustment"].get<bool>();
		}
		else if (j.contains("adjustment") && j["adjustment"].is_object()) {
			// Legacy flat format fallback
			auto& adj = j["adjustment"];
			if (adj.contains("pixelsX") && adj["pixelsX"].is_number()) {
				int val = adj["pixelsX"].get<int>();
				if (val < -MAX_ADJUST_PIXELS) val = -MAX_ADJUST_PIXELS;
				else if (val > MAX_ADJUST_PIXELS) val = MAX_ADJUST_PIXELS;
				cfg.adjustment.pixelsX = val;
			}
			if (adj.contains("pixelsY") && adj["pixelsY"].is_number()) {
				int val = adj["pixelsY"].get<int>();
				if (val < -MAX_ADJUST_PIXELS) val = -MAX_ADJUST_PIXELS;
				else if (val > MAX_ADJUST_PIXELS) val = MAX_ADJUST_PIXELS;
				cfg.adjustment.pixelsY = val;
			}
			if (adj.contains("useAdjustment") && adj["useAdjustment"].is_boolean())
				cfg.adjustment.useAdjustment = adj["useAdjustment"].get<bool>();
		}

		// ========== MATERIAL SETTINGS ==========
		if (j.contains("material") && j["material"].is_object() && j["material"].contains("thickness")) {
			auto& thick = j["material"]["thickness"];
			if (thick.contains("mode") && thick["mode"].is_number())
				cfg.thickness.mode = static_cast<MaterialThicknessMode>(thick["mode"].get<int>());
			if (thick.contains("customX") && thick["customX"].is_number())
				cfg.thickness.customX = thick["customX"].get<double>();
			if (thick.contains("customY") && thick["customY"].is_number())
				cfg.thickness.customY = thick["customY"].get<double>();
		}
		else if (j.contains("thickness") && j["thickness"].is_object()) {
			// Legacy flat format fallback
			auto& thick = j["thickness"];
			if (thick.contains("mode") && thick["mode"].is_number())
				cfg.thickness.mode = static_cast<MaterialThicknessMode>(thick["mode"].get<int>());
			if (thick.contains("customX") && thick["customX"].is_number())
				cfg.thickness.customX = thick["customX"].get<double>();
			if (thick.contains("customY") && thick["customY"].is_number())
				cfg.thickness.customY = thick["customY"].get<double>();
		}

		// ========== KASA SETTINGS ==========
		if (j.contains("kasa") && j["kasa"].is_object()) {
			auto& k = j["kasa"];
			if (k.contains("ipAddress") && k["ipAddress"].is_string()) {
				std::string kasaIp = k["ipAddress"].get<std::string>();
				kasaIp = trim(kasaIp);
				if (!kasaIp.empty()) cfg.kasa.ipAddress = kasaIp;
			}
			if (k.contains("autoPowerOn") && k["autoPowerOn"].is_boolean())
				cfg.kasa.autoPowerOn = k["autoPowerOn"].get<bool>();
			if (k.contains("autoPowerOff") && k["autoPowerOff"].is_boolean())
				cfg.kasa.autoPowerOff = k["autoPowerOff"].get<bool>();
		}

		// ========== CAMERA SETTINGS ==========
		if (j.contains("camera") && j["camera"].is_object() && j["camera"].contains("rtsp")) {
			auto& rc = j["camera"]["rtsp"];
			cfg.rtspCamera.autoLaunch = rc.value("autoLaunch", false);
			cfg.rtspCamera.rtspUrl = rc.value("rtspUrl", "");
		}
		else if (j.contains("rtspCamera") && j["rtspCamera"].is_object()) {
			// Legacy flat format fallback
			auto& rc = j["rtspCamera"];
			cfg.rtspCamera.autoLaunch = rc.value("autoLaunch", false);
			cfg.rtspCamera.rtspUrl = rc.value("rtspUrl", "");
		}

		// ========== NOTIFICATION SETTINGS ==========
		if (j.contains("notifications") && j["notifications"].is_object()) {
			auto& notify = j["notifications"];

			// Global sound alerts (controls all beeps/chimes)
			if (notify.contains("soundAlerts") && notify["soundAlerts"].is_boolean())
				cfg.enableSoundAlerts = notify["soundAlerts"].get<bool>();
			else
				cfg.enableSoundAlerts = true;

			// Global email alerts (controls all email notifications)
			if (notify.contains("emailAlerts") && notify["emailAlerts"].is_boolean())
				cfg.enableEmailAlerts = notify["emailAlerts"].get<bool>();
			else
				cfg.enableEmailAlerts = false;

			// Email configuration (SMTP, credentials)
			if (notify.contains("email") && notify["email"].is_object()) {
				auto& email = notify["email"];
				cfg.emailRecipient = email.value("recipient", "");
				cfg.emailSender = email.value("sender", "");
				cfg.emailAppPassword = email.value("appPassword", "");
				cfg.emailSmtpServer = email.value("smtpServer", "smtp.gmail.com");
				cfg.emailSmtpPort = email.value("smtpPort", 587);
			}
		}

		// ========== WEB AUTHENTICATION SETTINGS ==========
		if (j.contains("web") && j["web"].is_object()) {
			auto& web = j["web"];
			cfg.webAuthEnabled = web.value("authEnabled", true);
			cfg.webUser = web.value("user", DEFAULT_WEB_USER);
			std::string pass = web.value("password", DEFAULT_WEB_PASSWORD);
			pass = trim(pass);
			cfg.webPassword = pass;
		}
		else {
			// Default values if web block is missing
			cfg.webAuthEnabled = true;
			cfg.webUser = DEFAULT_WEB_USER;
			cfg.webPassword = DEFAULT_WEB_PASSWORD;
		}

		bool hasIp = !cfg.ipAddress.empty();
		bool hasToken = !cfg.token.empty();

		if (hasIp && hasToken) {
			std::cout << GetTimeStamp() << "[OK] Loaded configuration for " << GetModelName(cfg.model) << " at " << cfg.ipAddress << std::endl;
			std::cout << GetTimeStamp() << "[OK] File action: " << cfg.fileAction << std::endl;
			return true;
		}
		else {
			if (!hasIp && !hasToken)
				std::cout << GetTimeStamp() << "[WARN] Config file missing both IP address and token." << std::endl;
			else if (!hasIp)
				std::cout << GetTimeStamp() << "[WARN] Config file missing valid IP address." << std::endl;
			else
				std::cout << GetTimeStamp() << "[WARN] Config file missing valid token." << std::endl;
			return false;
		}
	}
	catch (const json::parse_error& e) {
		std::cout << GetTimeStamp() << "[ERR] JSON parse error in config: " << e.what() << std::endl;
		return false;
	}
	catch (const std::filesystem::filesystem_error& e) {
		std::cout << GetTimeStamp() << "[ERR] Filesystem error loading config: " << e.what() << std::endl;
		return false;
	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[ERR] Error parsing config: " << e.what() << std::endl;
		return false;
	}
	catch (...) {
		std::cout << GetTimeStamp() << "[ERR] Unknown error loading config" << std::endl;
		return false;
	}
}

void SaveConfig(const std::string& file, bool verbose) {
	UserConfig cfg = HostContext::instance().snapshotConfig();

	// Image adjustment settings
	json adjustmentJson = {
		{"pixelsX", cfg.adjustment.pixelsX},
		{"pixelsY", cfg.adjustment.pixelsY},
		{"useAdjustment", cfg.adjustment.useAdjustment}
	};

	// Material thickness settings
	json thicknessJson = {
		{"mode", static_cast<int>(cfg.thickness.mode)},
		{"customX", cfg.thickness.customX},
		{"customY", cfg.thickness.customY}
	};

	// Kasa smart plug settings
	json kasaJson = {
		{"ipAddress", cfg.kasa.ipAddress},
		{"autoPowerOn", cfg.kasa.autoPowerOn},
		{"autoPowerOff", cfg.kasa.autoPowerOff}
	};

	// RTSP camera settings
	json rtspJson = {
		{"autoLaunch", cfg.rtspCamera.autoLaunch},
		{"rtspUrl", cfg.rtspCamera.rtspUrl}
	};

	// Email configuration (credentials, SMTP settings)
	json emailJson = {
		{"recipient", cfg.emailRecipient},
		{"sender", cfg.emailSender},
		{"appPassword", cfg.emailAppPassword},
		{"smtpServer", cfg.emailSmtpServer},
		{"smtpPort", cfg.emailSmtpPort}
	};

	// Notification settings (global toggles)
	json notificationsJson = {
		{"soundAlerts", cfg.enableSoundAlerts},
		{"emailAlerts", cfg.enableEmailAlerts},
		{"email", emailJson}
	};

	// Web authentication settings
	json webJson = {
		{"authEnabled", cfg.webAuthEnabled},
		{"user", cfg.webUser},
		{"password", cfg.webPassword}
	};

	// Main configuration object - categorized
	json j = {
		// Machine connection
		{"machine", {
			{"ipAddress", cfg.ipAddress},
			{"token", cfg.token},
			{"model", static_cast<int>(cfg.model)},
			{"autoStartPrint", cfg.autoStartPrint},
			{"autoMonitorPrint", cfg.autoMonitorPrint},
			{"fileAction", cfg.fileAction.empty() ? "recycle" : cfg.fileAction},
			{"bindLocalhost", cfg.bindLocalhost}
		}},

		// Base positions
		{"basePosition", {
			{"x", cfg.basePositionX},
			{"y", cfg.basePositionY},
			{"z", cfg.basePositionZ}
		}},

		// Image settings
		{"image", {
			{"adjustment", adjustmentJson}
		}},

		// Material settings
		{"material", {
			{"thickness", thicknessJson}
		}},

		// Kasa smart plug
		{"kasa", kasaJson},

		// Camera settings
		{"camera", {
			{"rtsp", rtspJson}
		}},

		// Notifications
		{"notifications", notificationsJson},

		// Web authentication
		{"web", webJson}
	};

	// Backup creation - only show if verbose
	if (verbose) {
		try {
			fs::path configPath(file);
			if (fs::exists(configPath)) {
				fs::path backupPath = configPath;
				backupPath.replace_extension(".json.old");
				fs::copy(configPath, backupPath, fs::copy_options::overwrite_existing);
				std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << std::endl;
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cerr << GetTimeStamp() << "[WARN] Could not create config backup: " << e.what() << std::endl;
		}
		catch (const std::exception& e) {
			std::cerr << GetTimeStamp() << "[WARN] Unexpected error during backup: " << e.what() << std::endl;
		}
	}

	// File writing
	std::ofstream f(file);
	if (!f.is_open()) {
		std::cerr << GetTimeStamp() << "[ERR] Failed to open config file for writing: " << file << std::endl;
		std::cerr << GetTimeStamp() << "[ERR] Check permissions or disk space." << std::endl;
		return;
	}

	try {
		f << std::setw(4) << j << std::endl;
		if (f.fail()) {
			throw std::runtime_error("Write operation failed");
		}
		if (f.bad()) {
			throw std::runtime_error("Stream is in bad state");
		}
	}
	catch (const std::exception& e) {
		std::cerr << GetTimeStamp() << "[ERR] Failed to write configuration: " << e.what() << std::endl;
		return;
	}

	if (verbose) {
		std::cout << GetTimeStamp() << "[OK] Configuration saved to " << file << std::endl;
	}
}