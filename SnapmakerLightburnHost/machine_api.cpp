// machine_api.cpp - All Snapmaker communication and RAII class implementations
#include "host.h"

// ============================================================================
// ProcessHandle Implementation
// ============================================================================

ProcessHandle::ProcessHandle() : hProcess(NULL), hThread(NULL), hPipe(NULL), processId(0), isValid(false) {}

ProcessHandle::ProcessHandle(const std::string& commandLine, bool createWindow, bool captureOutput, HANDLE* outReadPipe)
	: hProcess(NULL), hThread(NULL), hPipe(NULL), processId(0), isValid(false)
{
	STARTUPINFOA si = { 0 };
	si.cb = sizeof(si);

	HANDLE hReadPipe = NULL;
	HANDLE hWritePipe = NULL;

	if (captureOutput) {
		SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
			hWritePipe = NULL;
			return;
		}
		SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	}

	if (!createWindow) {
		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}

	PROCESS_INFORMATION pi = { 0 };
	std::vector<char> cmdLine(commandLine.begin(), commandLine.end());
	cmdLine.push_back('\0');

	DWORD creationFlags = CREATE_NO_WINDOW;
	if (!createWindow) {
		creationFlags |= DETACHED_PROCESS;
	}

	BOOL success = CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE,
		creationFlags, NULL, NULL, &si, &pi);

	if (outReadPipe) {
		*outReadPipe = hReadPipe;
	}

	if (hWritePipe) {
		CloseHandle(hWritePipe);
		hWritePipe = NULL;
	}

	if (success) {
		hProcess = pi.hProcess;
		hThread = pi.hThread;
		hPipe = hReadPipe;
		processId = pi.dwProcessId;
		isValid = true;
	}
	else {
		if (hReadPipe) CloseHandle(hReadPipe);
		if (hWritePipe) CloseHandle(hWritePipe);
		hReadPipe = NULL;
		hWritePipe = NULL;
	}
}

ProcessHandle::~ProcessHandle() {
	if (hProcess != NULL) {
		DWORD waitResult = WaitForSingleObject(hProcess, 3000);
		if (waitResult != WAIT_OBJECT_0) {
			TerminateProcess(hProcess, 1);
			WaitForSingleObject(hProcess, 1000);
		}
		CloseHandle(hProcess);
		hProcess = NULL;
	}
	if (hThread != NULL) {
		CloseHandle(hThread);
		hThread = NULL;
	}
	if (hPipe != NULL) {
		CloseHandle(hPipe);
		hPipe = NULL;
	}
}

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept
	: hProcess(other.hProcess), hThread(other.hThread), hPipe(other.hPipe),
	processId(other.processId), isValid(other.isValid)
{
	other.hProcess = NULL;
	other.hThread = NULL;
	other.hPipe = NULL;
	other.processId = 0;
	other.isValid = false;
}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept {
	if (this != &other) {
		if (hProcess != NULL) {
			DWORD waitResult = WaitForSingleObject(hProcess, 3000);
			if (waitResult != WAIT_OBJECT_0) {
				TerminateProcess(hProcess, 1);
				WaitForSingleObject(hProcess, 1000);
			}
			CloseHandle(hProcess);
		}
		if (hThread != NULL) CloseHandle(hThread);
		if (hPipe != NULL) CloseHandle(hPipe);

		hProcess = other.hProcess;
		hThread = other.hThread;
		hPipe = other.hPipe;
		processId = other.processId;
		isValid = other.isValid;

		other.hProcess = NULL;
		other.hThread = NULL;
		other.hPipe = NULL;
		other.processId = 0;
		other.isValid = false;
	}
	return *this;
}

HANDLE ProcessHandle::get() const { return hProcess; }
HANDLE ProcessHandle::getThread() const { return hThread; }
HANDLE ProcessHandle::getPipe() const { return hPipe; }
DWORD ProcessHandle::getId() const { return processId; }
bool ProcessHandle::valid() const { return isValid; }
ProcessHandle::operator bool() const { return isValid; }

bool ProcessHandle::readPipeOutput(char* buffer, DWORD bufferSize, DWORD& bytesRead, DWORD timeoutMs) {
	if (hPipe == NULL) return false;

	DWORD bytesAvail = 0;
	if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL)) {
		return false;
	}

	if (bytesAvail == 0) {
		DWORD waitResult = WaitForSingleObject(hProcess, timeoutMs);
		if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_TIMEOUT) {
			return false;
		}
		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL) || bytesAvail == 0) {
			return false;
		}
	}

	DWORD toRead = (std::min)(bufferSize, bytesAvail);
	return ReadFile(hPipe, buffer, toRead, &bytesRead, NULL) == TRUE;
}

DWORD ProcessHandle::wait(DWORD timeoutMs) const {
	if (!isValid) return WAIT_FAILED;
	return WaitForSingleObject(hProcess, timeoutMs);
}

bool ProcessHandle::isRunning() const {
	if (!isValid) return false;
	DWORD exitCode = 0;
	if (GetExitCodeProcess(hProcess, &exitCode)) {
		return exitCode == STILL_ACTIVE;
	}
	return false;
}

bool ProcessHandle::terminate(DWORD exitCode) {
	if (!isValid) return false;
	return TerminateProcess(hProcess, exitCode) == TRUE;
}

void ProcessHandle::terminateFast() {
	if (hProcess != NULL) {
		TerminateProcess(hProcess, 1);
		WaitForSingleObject(hProcess, 100);
		CloseHandle(hProcess);
		hProcess = NULL;
	}
	if (hThread != NULL) {
		CloseHandle(hThread);
		hThread = NULL;
	}
	if (hPipe != NULL) {
		CloseHandle(hPipe);
		hPipe = NULL;
	}
	isValid = false;
}

bool ProcessHandle::getExitCode(DWORD& exitCode) const {
	if (!isValid) return false;
	return GetExitCodeProcess(hProcess, &exitCode) == TRUE;
}

// ============================================================================
// RegKey Implementation
// ============================================================================

RegKey::RegKey() : hKey(NULL), isValid(false) {}

RegKey::RegKey(HKEY rootKey, const std::string& subKey, REGSAM samDesired)
	: hKey(NULL), isValid(false)
{
	LONG result = RegOpenKeyExA(rootKey, subKey.c_str(), 0, samDesired, &hKey);
	isValid = (result == ERROR_SUCCESS && hKey != NULL);
}

RegKey::RegKey(HKEY rootKey, const std::string& subKey, const std::string& valueName,
	DWORD type, const void* data, DWORD dataSize)
	: hKey(NULL), isValid(false)
{
	HKEY createdKey = NULL;
	LONG result = RegCreateKeyExA(rootKey, subKey.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE,
		NULL, &createdKey, NULL);
	if (result == ERROR_SUCCESS && createdKey != NULL) {
		result = RegSetValueExA(createdKey, valueName.c_str(), 0, type,
			static_cast<const BYTE*>(data), dataSize);
		isValid = (result == ERROR_SUCCESS);
		RegCloseKey(createdKey);
	}
}

RegKey::~RegKey() {
	if (hKey != NULL) {
		RegCloseKey(hKey);
		hKey = NULL;
	}
}

RegKey::RegKey(RegKey&& other) noexcept
	: hKey(other.hKey), isValid(other.isValid)
{
	other.hKey = NULL;
	other.isValid = false;
}

RegKey& RegKey::operator=(RegKey&& other) noexcept {
	if (this != &other) {
		if (hKey != NULL) RegCloseKey(hKey);
		hKey = other.hKey;
		isValid = other.isValid;
		other.hKey = NULL;
		other.isValid = false;
	}
	return *this;
}

HKEY RegKey::get() const { return hKey; }
bool RegKey::valid() const { return isValid; }
RegKey::operator bool() const { return isValid; }
RegKey::operator HKEY() const { return hKey; }

std::string RegKey::getStringValue(const std::string& valueName) const {
	if (!isValid) return "";

	char buffer[1024] = { 0 };
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;

	LONG result = RegQueryValueExA(hKey, valueName.c_str(), NULL, &type,
		(LPBYTE)buffer, &bufferSize);
	if (result == ERROR_SUCCESS && type == REG_SZ) {
		return std::string(buffer);
	}
	return "";
}

DWORD RegKey::getDWordValue(const std::string& valueName) const {
	if (!isValid) return 0;

	DWORD value = 0;
	DWORD size = sizeof(DWORD);
	DWORD type = 0;

	LONG result = RegQueryValueExA(hKey, valueName.c_str(), NULL, &type,
		(LPBYTE)&value, &size);
	if (result == ERROR_SUCCESS && type == REG_DWORD) {
		return value;
	}
	return 0;
}

// ============================================================================
// ConsoleGuard Implementation
// ============================================================================

ConsoleGuard::ConsoleGuard()
	: hStdOut(GetStdHandle(STD_OUTPUT_HANDLE)),
	hStdIn(GetStdHandle(STD_INPUT_HANDLE)),
	originalOutMode(0),
	originalInMode(0),
	outModeSaved(false),
	inModeSaved(false),
	vtEnabled(false)
{
	if (hStdOut != INVALID_HANDLE_VALUE) {
		outModeSaved = !!GetConsoleMode(hStdOut, &originalOutMode);
		if (outModeSaved) {
			DWORD newMode = originalOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			if (SetConsoleMode(hStdOut, newMode)) {
				vtEnabled = true;
			}
			else {
				vtEnabled = false;
			}
		}
	}

	if (hStdIn != INVALID_HANDLE_VALUE) {
		inModeSaved = !!GetConsoleMode(hStdIn, &originalInMode);
		if (inModeSaved) {
			DWORD newMode = originalInMode & ~ENABLE_QUICK_EDIT_MODE;
			newMode |= ENABLE_EXTENDED_FLAGS;
			SetConsoleMode(hStdIn, newMode);
		}
	}
}

ConsoleGuard::~ConsoleGuard() {
	if (outModeSaved && hStdOut != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hStdOut, originalOutMode);
	}
	if (inModeSaved && hStdIn != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hStdIn, originalInMode);
	}
	FlushInputBuffers();
	fflush(stdout);
	fflush(stderr);
}

void ConsoleGuard::setTitle(const std::string& title) {
	SetConsoleTitleA(title.c_str());
}

void ConsoleGuard::setColor(WORD attributes) {
	if (hStdOut != INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(hStdOut, attributes);
	}
}

void ConsoleGuard::clearScreen() {
	if (hStdOut == INVALID_HANDLE_VALUE) return;
	if (vtEnabled) {
		std::cout << "\033[2J\033[H" << std::flush;
	}
	else {
		ClearScreenWin32();
	}
}

void ConsoleGuard::setWindowSize(int cols, int rows) {
	if (vtEnabled) {
		printf("\x1b[8;%d;%dt", rows, cols);
		fflush(stdout);
	}
	else {
		if (hStdOut == INVALID_HANDLE_VALUE) return;
		COORD bufferSize = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
		SetConsoleScreenBufferSize(hStdOut, bufferSize);
		SMALL_RECT windowRect = { 0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1) };
		SetConsoleWindowInfo(hStdOut, TRUE, &windowRect);
	}
}

void ConsoleGuard::flushInputBuffer() {
	FlushInputBufferInternal();
}

bool ConsoleGuard::hasVT() const { return vtEnabled; }

void ConsoleGuard::ClearScreenWin32() {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE) return;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
	DWORD written;
	COORD topLeft = { 0, 0 };
	DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
	FillConsoleOutputCharacter(hConsole, ' ', size, topLeft, &written);
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, size, topLeft, &written);
	SetConsoleCursorPosition(hConsole, topLeft);
}

void ConsoleGuard::FlushInputBuffers() {
	std::cin.clear();
	if (std::cin.rdbuf() && std::cin.rdbuf()->in_avail() > 0) {
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
	if (hConsole != INVALID_HANDLE_VALUE) {
		FlushConsoleInputBuffer(hConsole);
	}
}

void ConsoleGuard::SetConsoleTitleStatic(const std::string& title) {
	SetConsoleTitleA(title.c_str());
}

void ConsoleGuard::SetConsoleColor(WORD attributes) {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(hOut, attributes);
	}
}

void ConsoleGuard::SetConsoleWindowSize(int cols, int rows) {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return;
	COORD bufferSize = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
	SetConsoleScreenBufferSize(hOut, bufferSize);
	SMALL_RECT windowRect = { 0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1) };
	SetConsoleWindowInfo(hOut, TRUE, &windowRect);
}

void ConsoleGuard::FlushInputBufferInternal() {
	FlushInputBuffers();
}

// ============================================================================
// MachineStatusData Methods
// ============================================================================

MachineStatusData MachineStatusData::CreateOffline() {
	MachineStatusData result;
	result.status = "OFFLINE";
	result.homed = false;
	result.toolHead = "Unknown";
	return result;
}

MachineStatusData MachineStatusData::CreateError(const std::string& errorMsg) {
	MachineStatusData result;
	result.status = errorMsg;
	result.homed = false;
	return result;
}

void MachineStatusData::reset() {
	status = "UNKNOWN";
	x = y = z = 0;
	homed = false;
	offsetX = offsetY = offsetZ = 0;
	toolHead = "Unknown";
	laserFocalLength = 0;
	laserPower = 0;
	laserCamera = false;
	laser10WErrorState = 0;
	spindleSpeed = 0;
	workSpeed = 0;
	printStatus = "Unknown";
	nozzleTemperature1 = nozzleTargetTemperature1 = 0;
	nozzleTemperature2 = nozzleTargetTemperature2 = 0;
	heatedBedTemperature = heatedBedTargetTemperature = 0;
	isFilamentOut = false;
	enclosure = rotaryModule = emergencyStopButton = airPurifier = isEnclosureDoorOpen = false;
	doorSwitchCount = 0;
}

MachineStatus MachineStatusData::getMachineStatus() const {
	if (status.find("IDLE") != std::string::npos) return STATUS_IDLE;
	if (status.find("RUNNING") != std::string::npos) return STATUS_RUNNING;
	if (status.find("PAUSED") != std::string::npos) return STATUS_PAUSED;
	if (status.find("OFFLINE") != std::string::npos || status.empty()) return STATUS_OFFLINE;
	return STATUS_UNKNOWN;
}

std::string MachineStatusData::getStatusDisplay() const {
	switch (getMachineStatus()) {
	case STATUS_IDLE: return "IDLE";
	case STATUS_RUNNING: return "RUNNING";
	case STATUS_PAUSED: return "PAUSED";
	case STATUS_OFFLINE: return "OFFLINE";
	default: return "UNKNOWN";
	}
}

std::string MachineStatusData::getToolHeadDisplay() const {
	if (toolHead.find("LASER") != std::string::npos) return "Laser";
	if (toolHead.find("3DP") != std::string::npos) return "3D Printer";
	if (toolHead.find("CNC") != std::string::npos) return "CNC";
	return toolHead;
}

bool MachineStatusData::isBusy() const {
	MachineStatus s = getMachineStatus();
	return (s == STATUS_RUNNING || s == STATUS_PAUSED);
}

bool MachineStatusData::is3DPrinter() const { return toolHead.find("3DP") != std::string::npos; }
bool MachineStatusData::isLaser() const { return toolHead.find("LASER") != std::string::npos; }
bool MachineStatusData::isCNC() const { return toolHead.find("CNC") != std::string::npos; }

std::string MachineStatusData::getFormattedTimeRemaining() const {
	if (remainingTime <= 0) return "Unknown";

	int hours = static_cast<int>(remainingTime) / 3600;
	int minutes = (static_cast<int>(remainingTime) % 3600) / 60;
	int seconds = static_cast<int>(remainingTime) % 60;

	std::stringstream ss;
	if (hours > 0) ss << hours << "h ";
	if (minutes > 0 || hours > 0) ss << minutes << "m ";
	ss << seconds << "s";
	return ss.str();
}

// ============================================================================
// Snapmaker API Functions
// ============================================================================

CURLcode ConnectAndMaintainConnection() {
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		std::cout << GetTimeStamp() << "[CONNECT] Failed - Response code: " << connectCode << std::endl;
		if (!connectResponse.empty()) {
			std::cout << GetTimeStamp() << "[CONNECT] Response: " << connectResponse << std::endl;
		}
	}
	else {
		std::cout << GetTimeStamp() << "[CONNECT] Success" << std::endl;
	}

	return (connectRes == CURLE_OK && connectCode == 200) ? CURLE_OK : CURLE_HTTP_RETURNED_ERROR;
}

CURLcode ConnectForGCode() {
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string url = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(url, "", response, CURL_TIMEOUT);
	return (res == CURLE_OK && code == 200) ? CURLE_OK : CURLE_HTTP_RETURNED_ERROR;
}

CURLcode ConnectAndSendGCode(const std::string& gcode, bool isHoming) {
	long timeout = isHoming ? CURL_HOMING_TIMEOUT : CURL_TIMEOUT;
	CURLcode connectRes = ConnectForGCode();
	if (connectRes != CURLE_OK) return connectRes;
	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	CurlSession session;
	if (!session) return CURLE_FAILED_INIT;
	CURL* curl = session.get();
	char* encoded = curl_easy_escape(curl, gcode.c_str(), static_cast<int>(gcode.length()));
	if (!encoded) return CURLE_FAILED_INIT;
	std::unique_ptr<char, decltype(&curl_free)> encodedPtr(encoded, curl_free);
	std::string encodedGCode(encodedPtr.get());

	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string url = BuildExecuteCodeUrl(cfg.ipAddress, cfg.token, encodedGCode);
	std::string response;
	std::cout << GetTimeStamp() << "Sending " << gcode << "... " << std::flush;
	auto [res, code] = PerformHttpPostWithCode(url, "", response, timeout);
	if (res == CURLE_OK && (code == 200 || code == 202 || code == 204)) {
		std::cout << "[OK]" << std::endl;
		if (isHoming) std::this_thread::sleep_for(std::chrono::seconds(POST_HOMING_DELAY_SECONDS));
		return CURLE_OK;
	}
	std::cout << "[FAIL]" << std::endl;
	return CURLE_HTTP_RETURNED_ERROR;
}

CURLcode SendGCodeWithRetry(const std::string& gcode, int maxRetries, bool isHoming) {
	for (int attempt = 1; attempt <= maxRetries; ++attempt) {
		if (attempt > 1) {
			std::cout << GetTimeStamp() << "Retry attempt " << attempt << " for " << gcode << "..." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
		}
		CURLcode result = ConnectAndSendGCode(gcode, isHoming);
		if (result == CURLE_OK) return CURLE_OK;
		if (isHoming && result == CURLE_OPERATION_TIMEDOUT) {
			std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
		}
	}
	return CURLE_HTTP_RETURNED_ERROR;
}

CURLcode SendRawGCode(const std::string& gcode) {
	CURLcode connectRes = ConnectForGCode();
	if (connectRes != CURLE_OK) return connectRes;
	CurlSession session;
	if (!session) return CURLE_FAILED_INIT;
	CURL* curl = session.get();
	char* encoded = curl_easy_escape(curl, gcode.c_str(), static_cast<int>(gcode.length()));
	if (!encoded) return CURLE_FAILED_INIT;
	std::unique_ptr<char, decltype(&curl_free)> encodedPtr(encoded, curl_free);
	std::string encodedGCode(encodedPtr.get());
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string url = BuildExecuteCodeUrl(cfg.ipAddress, cfg.token, encodedGCode);
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(url, "", response, CURL_TIMEOUT);
	return (res == CURLE_OK && (code == 200 || code == 202 || code == 204)) ? CURLE_OK : CURLE_HTTP_RETURNED_ERROR;
}

CURLcode SendRawGCodeWithResponse(const std::string& gcode, std::string& response) {
	CURLcode connectRes = ConnectForGCode();
	if (connectRes != CURLE_OK) return connectRes;
	CurlSession session;
	if (!session) return CURLE_FAILED_INIT;
	CURL* curl = session.get();
	char* encoded = curl_easy_escape(curl, gcode.c_str(), static_cast<int>(gcode.length()));
	if (!encoded) return CURLE_FAILED_INIT;
	std::unique_ptr<char, decltype(&curl_free)> encodedPtr(encoded, curl_free);
	std::string encodedGCode(encodedPtr.get());
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string url = BuildExecuteCodeUrl(cfg.ipAddress, cfg.token, encodedGCode);
	auto [res, code] = PerformHttpPostWithCode(url, "", response, CURL_TIMEOUT);
	return (res == CURLE_OK && (code == 200 || code == 202 || code == 204)) ? CURLE_OK : CURLE_HTTP_RETURNED_ERROR;
}

MachineStatusData GetMachineStatus(bool useCache) {
	if (useCache && !HostContext::instance().isStatusStale())
		return HostContext::instance().snapshotStatus();

	MachineStatusData result;
	UserConfig cfg = HostContext::instance().snapshotConfig();

	if (cfg.ipAddress.empty()) {
		result.status = "OFFLINE";
		HostContext::instance().setLastStatus(result);
		return result;
	}

	if (!IsRunning() || g_shutdownRequested.load(std::memory_order_acquire)) {
		result.status = "SHUTDOWN";
		return result;
	}

	auto parseStatusResponse = [&result](const std::string& response) -> bool {
		try {
			auto data = json::parse(response);
			result.status = data.value("status", "UNKNOWN");
			result.x = data.value("x", 0.0);
			result.y = data.value("y", 0.0);
			result.z = data.value("z", 0.0);
			result.homed = data.value("homed", false);
			result.offsetX = data.value("offsetX", 0.0);
			result.offsetY = data.value("offsetY", 0.0);
			result.offsetZ = data.value("offsetZ", 0.0);
			result.toolHead = data.value("toolHead", "Unknown");
			result.laserFocalLength = data.value("laserFocalLength", 0);
			result.laserPower = data.value("laserPower", 0);
			result.laserCamera = data.value("laserCamera", false);
			result.laser10WErrorState = data.value("laser10WErrorState", 0);
			result.spindleSpeed = data.value("spindleSpeed", 0);
			result.workSpeed = data.value("workSpeed", 0);
			result.printStatus = data.value("printStatus", "Unknown");
			result.nozzleTemperature1 = data.value("nozzleTemperature1", 0);
			result.nozzleTargetTemperature1 = data.value("nozzleTargetTemperature1", 0);
			result.nozzleTemperature2 = data.value("nozzleTemperature2", 0);
			result.nozzleTargetTemperature2 = data.value("nozzleTargetTemperature2", 0);
			result.heatedBedTemperature = data.value("heatedBedTemperature", 0);
			result.heatedBedTargetTemperature = data.value("heatedBedTargetTemperature", 0);
			result.isFilamentOut = data.value("isFilamentOut", false);
			result.enclosure = data.value("enclosure", false);
			result.rotaryModule = data.value("rotaryModule", false);
			result.emergencyStopButton = data.value("emergencyStopButton", false);
			result.airPurifier = data.value("airPurifier", false);
			result.isEnclosureDoorOpen = data.value("isEnclosureDoorOpen", false);
			result.doorSwitchCount = data.value("doorSwitchCount", 0);

			result.fileName = data.value("fileName", "");
			result.totalLines = data.value("totalLines", 0);
			result.currentLine = data.value("currentLine", 0);
			result.estimatedTime = data.value("estimatedTime", 0.0);
			result.elapsedTime = data.value("elapsedTime", 0.0);
			result.remainingTime = data.value("remainingTime", 0.0);
			result.progress = data.value("progress", 0.0);

			if (data.contains("moduleList") && data["moduleList"].is_object()) {
				auto& mod = data["moduleList"];
				result.enclosure = mod.value("enclosure", result.enclosure);
				result.rotaryModule = mod.value("rotaryModule", result.rotaryModule);
				result.emergencyStopButton = mod.value("emergencyStopButton", result.emergencyStopButton);
				result.airPurifier = mod.value("airPurifier", result.airPurifier);
			}
			return true;
		}
		catch (const std::exception& e) {
			std::cout << GetTimeStamp() << "[ERR] Failed to parse status: " << e.what() << std::endl;
			result.status = "PARSE_ERROR";
			return false;
		}
		};

	if (!cfg.token.empty()) {
		std::string connectUrl = BuildConnectUrl(cfg.ipAddress);
		std::string connectResponse;
		auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "token=" + cfg.token, connectResponse, 5);
		if (connectCode == 401) {
			std::cout << GetTimeStamp() << "[WARN] Token invalid. Clearing..." << std::endl;
			HostContext::instance().updateConfig([](UserConfig& c) { c.token.clear(); return true; });
			cfg.token.clear();
		}
	}

	std::string baseStatusUrl = BuildStatusUrl(cfg.ipAddress, cfg.token);
	std::string statusUrl = baseStatusUrl + "&" +
		std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count());

	std::string response;
	auto [res, code] = PerformHttpGetWithCode(statusUrl, response, 5);

	if (res != CURLE_OK || code != 200) {
		if (code == 401) {
			static std::atomic<bool> tokenRefreshInProgress{ false };

			if (!tokenRefreshInProgress.exchange(true)) {
				std::cout << GetTimeStamp() << "[WARN] Token invalid. Attempting to renew..." << std::endl;

				HostContext::instance().updateConfig([](UserConfig& c) {
					c.token.clear();
					return true;
					});

				UserConfig currentCfg = HostContext::instance().snapshotConfig();
				bool refreshSuccess = false;

				if (ConnectionSetup(currentCfg.ipAddress, true)) {
					currentCfg = HostContext::instance().snapshotConfig();
					if (!currentCfg.token.empty()) {
						std::string retryUrl = BuildStatusUrl(currentCfg.ipAddress, currentCfg.token);
						std::string retryResponse;
						auto [retryRes, retryCode] = PerformHttpGetWithCode(retryUrl, retryResponse, 5);

						if (retryRes == CURLE_OK && retryCode == 200 && parseStatusResponse(retryResponse)) {
							refreshSuccess = true;
						}
					}
				}

				tokenRefreshInProgress = false;

				if (refreshSuccess) {
					HostContext::instance().setLastStatus(result);
					return result;
				}

				result.status = "OFFLINE";
				HostContext::instance().setLastStatus(result);
				return result;
			}
			else {
				int waited = 0;
				while (tokenRefreshInProgress && waited < 50) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					waited++;
				}

				cfg = HostContext::instance().snapshotConfig();
				if (!cfg.token.empty()) {
					std::string retryUrl = BuildStatusUrl(cfg.ipAddress, cfg.token);
					std::string retryResponse;
					auto [retryRes, retryCode] = PerformHttpGetWithCode(retryUrl, retryResponse, 5);
					if (retryRes == CURLE_OK && retryCode == 200 && parseStatusResponse(retryResponse)) {
						HostContext::instance().setLastStatus(result);
						return result;
					}
				}

				result.status = "OFFLINE";
				HostContext::instance().setLastStatus(result);
				return result;
			}
		}

		result.status = "OFFLINE";
		HostContext::instance().setLastStatus(result);
		return result;
	}

	if (!parseStatusResponse(response)) {
		result.status = "PARSE_ERROR";
	}
	HostContext::instance().setLastStatus(result);
	return result;
}

bool IsMachineOffline(const MachineStatusData& status) {
	return status.status == "OFFLINE" || status.status == "ERROR" || status.status.empty();
}

bool CheckMachineHomed() { return GetMachineStatus(true).homed; }

std::string GetMachineToolHead() { return GetMachineStatus(true).getToolHeadDisplay(); }

void PerformLaserSetup() {
	std::cout << GetTimeStamp() << "Starting laser setup sequence..." << std::endl;
	std::cout << GetTimeStamp() << "[INFO] This will: Home Machine (G28), Set Absolute Positioning (G90), & Set Workspace Coordinates (G54)." << std::endl;

	HomeMachine();
	std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

	std::cout << GetTimeStamp() << "Setting absolute positioning..." << std::endl;
	if (SendGCodeWithRetry("G90", MAX_RETRY_ATTEMPTS, false) != CURLE_OK) {
		std::cout << GetTimeStamp() << "[ERR] Setting absolute positioning failed." << std::endl;
		return;
	}
	std::cout << GetTimeStamp() << "[OK] Absolute positioning set." << std::endl;
	std::cout << GetTimeStamp() << "Switch to work coordinates (G54)..." << std::endl;
	if (SendGCodeWithRetry("G54", MAX_RETRY_ATTEMPTS, false) != CURLE_OK) {
		std::cout << GetTimeStamp() << "[ERR] Switch to work coordinates failed." << std::endl;
		return;
	}
	std::cout << GetTimeStamp() << "[OK] Laser setup finished successfully!" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));
}

void PerformPrintSetup() {
	std::cout << GetTimeStamp() << "Starting 3DP/CNC setup sequence..." << std::endl;
	std::cout << GetTimeStamp() << "[INFO] This will: Home Machine (G28) and Set Absolute Positioning (G90)" << std::endl;

	HomeMachine();
	std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));

	std::cout << GetTimeStamp() << "Setting absolute positioning (G90)..." << std::endl;
	if (SendGCodeWithRetry("G90", MAX_RETRY_ATTEMPTS, false) != CURLE_OK) {
		std::cout << GetTimeStamp() << "[ERR] Setting absolute positioning failed." << std::endl;
		return;
	}
	std::cout << GetTimeStamp() << "[OK] Absolute positioning set." << std::endl;

	std::cout << GetTimeStamp() << "[OK] Print setup finished successfully!" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(GCODE_DELAY_MS));
}

void PerformMachineSetup() {
	MachineStatusData status = GetMachineStatus(false);
	if (IsMachineOffline(status)) {
		std::cout << GetTimeStamp() << "[WARN] Machine is offline or token is invalid. Cannot perform setup." << std::endl;
		return;
	}
	if (status.isBusy()) {
		std::cout << GetTimeStamp() << "[INFO] Machine is busy (Status: " << status.getStatusDisplay() << "). Skipping setup." << std::endl;
		return;
	}
	std::string toolHead = status.getToolHeadDisplay();
	std::cout << GetTimeStamp() << "Detected tool head: " << toolHead << std::endl;
	if (toolHead.find("Laser") != std::string::npos) PerformLaserSetup();
	else if (toolHead.find("3D") != std::string::npos || toolHead.find("CNC") != std::string::npos) PerformPrintSetup();
	else PerformLaserSetup();
}

std::string GetMaterialThicknessModeName() {
	UserConfig cfg = HostContext::instance().snapshotConfig();
	return cfg.thickness.mode == MODE_DEFAULT ? "Default (Base Position)" : "Custom";
}

std::string GetMaterialThicknessModeDetails() {
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::stringstream ss;
	ss << std::fixed << std::setprecision(1);
	if (cfg.thickness.mode == MODE_DEFAULT) ss << "Default (Center) Mode (X=" << cfg.basePositionX << ", Y=" << cfg.basePositionY << ")";
	else ss << "Custom (X/Y) Mode (X=" << cfg.thickness.customX << ", Y=" << cfg.thickness.customY << ")";
	return ss.str();
}

void ToggleMaterialThicknessMode() {
	HostContext::instance().updateConfig([](UserConfig& cfg) {
		if (cfg.thickness.mode == MODE_DEFAULT) {
			cfg.thickness.mode = MODE_CUSTOM;
			std::cout << GetTimeStamp() << "[OK] Material thickness mode changed to: Custom" << std::endl;
		}
		else {
			cfg.thickness.mode = MODE_DEFAULT;
			std::cout << GetTimeStamp() << "[OK] Material thickness mode changed to: Default (Base Position)" << std::endl;
		}
		return true;
		});
	SaveConfig(HostContext::instance().getConfigPath().string());
	ClearAndDrawHeader(true);
}

void ResetMaterialThicknessCustomCoordinates() {
	double oldX = 0.0, oldY = 0.0;
	HostContext::instance().updateConfig([&oldX, &oldY](UserConfig& cfg) {
		oldX = cfg.thickness.customX;
		oldY = cfg.thickness.customY;
		cfg.thickness.customX = DEFAULT_MATERIAL_THICKNESS_CUSTOM_X;
		cfg.thickness.customY = DEFAULT_MATERIAL_THICKNESS_CUSTOM_Y;
		return true;
		});
	SaveConfig(HostContext::instance().getConfigPath().string());
	std::cout << GetTimeStamp() << "[OK] Custom material thickness coordinates reset to defaults." << std::endl;
	std::cout << GetTimeStamp() << "     Previous: X=" << oldX << ", Y=" << oldY << std::endl;
	std::cout << GetTimeStamp() << "     New: X=" << DEFAULT_MATERIAL_THICKNESS_CUSTOM_X << ", Y=" << DEFAULT_MATERIAL_THICKNESS_CUSTOM_Y << std::endl;
}

void UpdateMaterialThicknessCustomCoordinates() {
	ClearAndDrawHeader(true);
	if (!IsRunning()) {
		return;
	}
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::cout << "\nCurrent material thickness custom coordinates:" << std::endl;
	std::cout << "  X: " << cfg.thickness.customX << " mm" << std::endl;
	std::cout << "  Y: " << cfg.thickness.customY << " mm" << std::endl;
	std::cout << "  Default: X=" << DEFAULT_MATERIAL_THICKNESS_CUSTOM_X << ", Y=" << DEFAULT_MATERIAL_THICKNESS_CUSTOM_Y << " mm" << std::endl;
	std::cout << "\nOptions:" << std::endl;
	std::cout << "  [1] - Enter new custom coordinates" << std::endl;
	std::cout << "  [2] - Reset to default custom coordinates" << std::endl;
	std::cout << "\nEnter choice (1-2) or ESC to return: " << std::flush;

	std::string choice;
	while (true) {
		if (!IsRunning()) {
			return;
		}
		if (_kbhit()) {
			int ch = _getch();
			if (ch == ENTER_ASCII) { std::cout << std::endl; break; }
			if (ch == ESC_ASCII) { std::cout << std::endl; ClearAndDrawHeader(true); return; }
			if (ch >= '1' && ch <= '2') { choice = static_cast<char>(ch); std::cout << choice << std::endl; break; }
			if (ch == 0 || ch == 224) { (void)_getch(); continue; }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
	}
	if (!IsRunning()) {
		return;
	}
	if (choice == "1") {
		FlushInputBuffer();
		std::string input;
		double newX = cfg.thickness.customX, newY = cfg.thickness.customY;
		std::cout << "\nEnter new custom coordinates (press Enter to keep current value):" << std::endl;
		std::cout << "New X coordinate [" << cfg.thickness.customX << "]: ";
		std::getline(std::cin, input);
		if (!input.empty()) try { newX = std::stod(input); }
		catch (...) { std::cout << "[ERR] Invalid input." << std::endl; }

		if (!IsRunning()) {
			return;
		}

		std::cout << "New Y coordinate [" << cfg.thickness.customY << "]: ";
		std::getline(std::cin, input);
		if (!input.empty()) try { newY = std::stod(input); }
		catch (...) { std::cout << "[ERR] Invalid input." << std::endl; }

		HostContext::instance().updateConfig([newX, newY](UserConfig& c) { c.thickness.customX = newX; c.thickness.customY = newY; return true; });
		SaveConfig(HostContext::instance().getConfigPath().string());
		std::cout << GetTimeStamp() << "[OK] Custom coordinates saved." << std::endl;
	}
	else if (choice == "2") {
		ResetMaterialThicknessCustomCoordinates();
	}
	std::this_thread::sleep_for(std::chrono::seconds(STATUS_REFRESH_COOLDOWN_SECONDS));
	ClearAndDrawHeader(true);
}

void PerformFirstTimeWarning() {
	static bool messageShown = false;
	if (!messageShown) {
		std::cout << GetTimeStamp() << "[WARNING] The first capture or measurement may fail if the laser head is far from the target position.\n";
		std::cout << GetTimeStamp() << "[NOTE] Simply retry the operation and subsequent attempts will succeed.\n";
		messageShown = true;
	}
}

CURLcode GetMaterialThicknessFromSnapmaker(double& outThickness) {
	std::string response;
	UserConfig cfg = HostContext::instance().snapshotConfig();
	std::string url;
	if (cfg.thickness.mode == MODE_DEFAULT) {
		url = BuildMaterialThicknessUrl(cfg.ipAddress, cfg.basePositionX, cfg.basePositionY, MATERIAL_THICKNESS_FEED_RATE);
	}
	else {
		url = BuildMaterialThicknessUrl(cfg.ipAddress, cfg.thickness.customX, cfg.thickness.customY, MATERIAL_THICKNESS_FEED_RATE);
	}

	std::cout << GetTimeStamp() << "Requesting material thickness at " << GetMaterialThicknessModeDetails() << "... " << std::flush;
	CurlSession session;
	if (!session) return CURLE_FAILED_INIT;
	CURL* curl = session.get();
	curl_easy_reset(curl);
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
	if (res != CURLE_OK || http_code != 200) {
		std::cout << "[FAIL]" << std::endl;
		return CURLE_HTTP_RETURNED_ERROR;
	}
	std::cout << "[OK]" << std::endl;
	try {
		auto data = json::parse(response);
		if (data.contains("status") && data["status"].is_boolean() && data["status"].get<bool>()) {
			if (data.contains("thickness") && data["thickness"].is_number()) {
				outThickness = data["thickness"].get<double>();
				std::cout << "+----------------------------------------------------------+" << std::endl;
				std::cout << "|                  MEASUREMENT COMPLETE                    |" << std::endl;
				std::cout << "|                                                          |" << std::endl;
				std::cout << "|  Use this thickness (" << std::fixed << std::setprecision(1) << data["thickness"].get<double>() << " mm) in LightBurn:               |" << std::endl;
				std::cout << "|  -> Open 'LightBurn' -> 'Cuts / Layers' tab.             |" << std::endl;
				std::cout << "|  -> Enter the value in the 'Material (mm)' field.        |" << std::endl;
				std::cout << "|                                                          |" << std::endl;
				std::cout << "|  Press space bar to measure again.                       |" << std::endl;
				std::cout << "+----------------------------------------------------------+" << std::endl;
			}
			else {
				std::cout << GetTimeStamp() << "[WARN] Thickness measurement completed but no thickness data." << std::endl;
			}
			return CURLE_OK;
		}
	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[WARN] JSON parse error: " << e.what() << std::endl;
	}
	catch (...) {
		std::cout << GetTimeStamp() << "[WARN] Unknown error parsing thickness response" << std::endl;
	}
	return CURLE_HTTP_RETURNED_ERROR;
}

void RegenerateGCodeFiles(double staticBufferZ, double laserFocalLength, double zFocusHeight) {
	try {
		fs::path gcodeFolder = "G-code";

		if (!fs::exists(gcodeFolder)) {
			if (!fs::create_directory(gcodeFolder)) {
				std::cerr << GetTimeStamp() << "[ERR] Failed to create 'G-code' directory. Check permissions." << std::endl;
				return;
			}
			std::cout << GetTimeStamp() << "[OK] Created 'G-code' directory." << std::endl;
		}

		double lightburnZ_10w = staticBufferZ - laserFocalLength;
		double lightburnZ_2040wIR = staticBufferZ - zFocusHeight;

		std::ofstream start10w(gcodeFolder / "Start G-code Sample - 10W Laser.txt");
		if (start10w.is_open()) {
			start10w << "M106 P0 S255\nM1010 S3 P100\nM1010 S4 P100\nG28\nG90\nG54\nG92 X0 Y0 Z"
				<< std::fixed << std::setprecision(1) << lightburnZ_10w << "\nM3 S0\n";
			start10w.close();
		}
		else {
			std::cerr << GetTimeStamp() << "[ERR] Could not write 10W laser start G-code file." << std::endl;
		}

		std::ofstream start2040(gcodeFolder / "Start G-code Sample - 20W, 40W and IR Lasers.txt");
		if (start2040.is_open()) {
			start2040 << "M106 P0 S255\nM1010 S3 P100\nM1010 S4 P100\nG28\nG90\nG54\nG92 X0 Y0 Z"
				<< std::fixed << std::setprecision(1) << lightburnZ_2040wIR << "\nM3 S0\n";
			start2040.close();
		}
		else {
			std::cerr << GetTimeStamp() << "[ERR] Could not write 20W/40W/IR start G-code file." << std::endl;
		}

		std::ofstream endAll(gcodeFolder / "End G-code Sample - All Lasers.txt");
		if (endAll.is_open()) {
			double endZ = staticBufferZ - 1.0;
			endAll << "M5\nG0 Z" << std::fixed << std::setprecision(1) << endZ << " F6000\nG28\n";
			endAll.close();
		}
		else {
			std::cerr << GetTimeStamp() << "[ERR] Could not write end G-code file." << std::endl;
		}

		std::cout << GetTimeStamp() << "[OK] Sample G-code files generated in 'G-code' folder." << std::endl;
	}
	catch (const std::exception& e) {
		std::cout << GetTimeStamp() << "[ERR] Failed to generate G-code files: " << e.what() << std::endl;
	}
}

bool CheckMachineHomedAndWarn() {
	MachineStatusData status = GetMachineStatus(false);

	if (IsMachineOffline(status)) {
		std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot proceed.\n";
		return false;
	}

	if (!status.homed) {
		std::cout << GetTimeStamp() << "[WARN] Machine is NOT homed!\n";
		std::cout << GetTimeStamp() << "[INFO] Homing is required before camera capture or thickness measurement.\n";
		std::cout << GetTimeStamp() << "Do you want to home the machine now? (Y/n): ";

		int ch = _getch();
		std::cout << std::endl;

		if (ch == 'n' || ch == 'N') {
			std::cout << GetTimeStamp() << "[INFO] Operation cancelled. Please home the machine first.\n";
			std::cout << GetTimeStamp() << "[INFO] You can home using: /home\n";
			return false;
		}

		HomeMachine();
		std::this_thread::sleep_for(std::chrono::seconds(MACHINE_SETUP_DELAY_SECONDS));
	}
	return true;
}

bool CheckLaserInstalledAndWarn() {
	MachineStatusData status = GetMachineStatus(false);

	if (IsMachineOffline(status)) {
		std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot verify tool head.\n";
		return false;
	}

	if (!status.isLaser()) {
		std::cout << GetTimeStamp() << "[ERR] This function requires the Laser module.\n";
		std::cout << GetTimeStamp() << "[INFO] Current tool head: " << status.getToolHeadDisplay() << "\n";
		std::cout << GetTimeStamp() << "[INFO] Camera capture and thickness measurement are laser-only features.\n";
		return false;
	}

	if (!status.laserCamera) {
		std::cout << GetTimeStamp() << "[ERR] This function requires the 10W Laser module with camera.\n";
		std::cout << GetTimeStamp() << "[INFO] Current laser does not have a camera.\n";
		std::cout << GetTimeStamp() << "[INFO] Camera capture and thickness measurement are only available on the 10W Laser.\n";
		return false;
	}

	return true;
}

std::pair<bool, std::string> SetEnclosureFanSpeed(const std::string& speedInput) {
	int speedPercent = -1;
	std::string inputLower = speedInput;
	std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);

	if (inputLower == "off") {
		speedPercent = 0;
	}
	else if (inputLower == "half") {
		speedPercent = 50;
	}
	else if (inputLower == "full") {
		speedPercent = 100;
	}
	else {
		try {
			speedPercent = std::stoi(speedInput);
			if (speedPercent < 0) speedPercent = 0;
			if (speedPercent > 100) speedPercent = 100;
		}
		catch (...) {
			return { false, "Invalid speed value. Use: off, half, full, or a number 0-100" };
		}
	}

	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string enclosureUrl = BuildApiUrl(cfg.ipAddress, "/api/v1/enclosure?token=" + cfg.token + "&fan=" + std::to_string(speedPercent));
	std::string enclosureResponse;
	auto [enclosureRes, enclosureCode] = PerformHttpPostWithCode(enclosureUrl, "", enclosureResponse, CURL_TIMEOUT);

	if (enclosureRes == CURLE_OK && enclosureCode == 200) {
		try {
			auto jsonResponse = json::parse(enclosureResponse);
			if (jsonResponse.contains("fan")) {
				int confirmedSpeed = jsonResponse["fan"].get<int>();
				return { true, "Fan speed set to " + std::to_string(confirmedSpeed) + "%" };
			}
		}
		catch (const std::exception&) {
			return { true, "Fan command accepted" };
		}
		return { true, "Fan speed set" };
	}
	else {
		return { false, "Failed to set enclosure fan speed (HTTP " + std::to_string(enclosureCode) + ")" };
	}
}

std::pair<bool, std::string> SetEnclosureLed(const std::string& brightnessInput) {
	int brightnessPercent = -1;
	std::string inputLower = brightnessInput;
	std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);

	if (inputLower == "off") {
		brightnessPercent = 0;
	}
	else if (inputLower == "half") {
		brightnessPercent = 50;
	}
	else if (inputLower == "full") {
		brightnessPercent = 100;
	}
	else {
		try {
			brightnessPercent = std::stoi(brightnessInput);
			if (brightnessPercent < 0) brightnessPercent = 0;
			if (brightnessPercent > 100) brightnessPercent = 100;
		}
		catch (...) {
			return { false, "Invalid brightness value. Use: off, half, full, or a number 0-100" };
		}
	}

	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string enclosureUrl = BuildApiUrl(cfg.ipAddress, "/api/v1/enclosure?token=" + cfg.token + "&led=" + std::to_string(brightnessPercent));
	std::string enclosureResponse;
	auto [enclosureRes, enclosureCode] = PerformHttpPostWithCode(enclosureUrl, "", enclosureResponse, CURL_TIMEOUT);

	if (enclosureRes == CURLE_OK && enclosureCode == 200) {
		try {
			auto jsonResponse = json::parse(enclosureResponse);
			if (jsonResponse.contains("led")) {
				int confirmedBrightness = jsonResponse["led"].get<int>();
				return { true, "LED brightness set to " + std::to_string(confirmedBrightness) + "%" };
			}
		}
		catch (const std::exception&) {
			return { true, "LED command accepted" };
		}
		return { true, "LED brightness set" };
	}
	else {
		return { false, "Failed to set enclosure LED brightness (HTTP " + std::to_string(enclosureCode) + ")" };
	}
}

void HomeMachine() {
    UserConfig cfg = HostContext::instance().snapshotConfig();
        
    std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
    std::string connectResponse;
    auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);
    
    if (connectRes != CURLE_OK || connectCode != 200) {
        std::cout << GetTimeStamp() << "[ERR] Failed to connect to Snapmaker (HTTP " << connectCode << ")" << std::endl;
        return;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));
    
    std::cout << GetTimeStamp() << "Homing machine (G28)..." << std::endl;
    if (SendGCodeWithRetry("G28", HOMING_RETRY_ATTEMPTS, true) == CURLE_OK) {
        std::cout << GetTimeStamp() << "[OK] Machine homed successfully!\n";
    }
    else {
        std::cout << GetTimeStamp() << "[ERR] Homing failed. Check machine connection.\n";
    }
}

std::pair<bool, std::string> PausePrint() {
	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string pauseUrl = "http://" + cfg.ipAddress + ":8080/api/v1/pause_print?token=" + cfg.token;
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(pauseUrl, "", response, CURL_TIMEOUT);

	if (res == CURLE_OK && (code == 200 || code == 202)) {
		return { true, "Print paused" };
	}
	else if (code == 409) {
		return { false, "No active print to pause or printer is not printing" };
	}
	else {
		return { false, "Failed to pause (HTTP " + std::to_string(code) + ")" };
	}
}

std::pair<bool, std::string> ResumePrint() {
	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string resumeUrl = "http://" + cfg.ipAddress + ":8080/api/v1/resume_print?token=" + cfg.token;
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(resumeUrl, "", response, CURL_TIMEOUT);

	if (res == CURLE_OK && (code == 200 || code == 202)) {
		return { true, "Print resumed" };
	}
	else if (code == 401) {
		return { false, "Print may have been resumed from touch screen" };
	}
	else if (code == 409) {
		return { false, "Cannot resume - check door or filament" };
	}
	else {
		return { false, "Failed to resume (HTTP " + std::to_string(code) + ")" };
	}
}

std::pair<bool, std::string> StopPrint() {
	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string stopUrl = "http://" + cfg.ipAddress + ":8080/api/v1/stop_print?token=" + cfg.token;
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(stopUrl, "", response, CURL_TIMEOUT);

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	if (res == CURLE_OK && (code == 200 || code == 202 || code == 0)) {
		return { true, "Print stopped" };
	}
	else if (code == 409) {
		return { false, "No active print to stop or printer is busy." };
	}
	else {
		return { false, "Failed to stop print (HTTP " + std::to_string(code) + ")" };
	}
}

std::pair<bool, std::string> RePrint() {
	UserConfig cfg = HostContext::instance().snapshotConfig();

	std::string connectUrl = BuildConnectUrl(cfg.ipAddress, cfg.token);
	std::string connectResponse;
	auto [connectRes, connectCode] = PerformHttpPostWithCode(connectUrl, "", connectResponse, CURL_TIMEOUT);

	if (connectRes != CURLE_OK || connectCode != 200) {
		return { false, "Failed to connect to Snapmaker (HTTP " + std::to_string(connectCode) + ")" };
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	std::string stopUrl = "http://" + cfg.ipAddress + ":8080/api/v1/start_print?token=" + cfg.token;
	std::string response;
	auto [res, code] = PerformHttpPostWithCode(stopUrl, "", response, CURL_TIMEOUT);

	std::this_thread::sleep_for(std::chrono::milliseconds(POST_CONNECTION_DELAY_MS));

	if (res == CURLE_OK && (code == 200 || code == 202 || code == 0)) {
		return { true, "Reprint started" };
	}
	else if (code == 409) {
		return { false, "Printer is busy or not ready. Please check the machine." };
	}
	else {
		return { false, "Failed to restart print (HTTP " + std::to_string(code) + ")" };
	}
}