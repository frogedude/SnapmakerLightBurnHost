// network_client.cpp - CURL wrappers, HTTP requests, callbacks, StatusChecker
#include "host.h"

// ============================================================================
// CurlSlistGuard Implementation
// ============================================================================

CurlSlistGuard::CurlSlistGuard() : list(nullptr) {}

CurlSlistGuard::~CurlSlistGuard() {
    if (list) curl_slist_free_all(list);
}

CurlSlistGuard::CurlSlistGuard(CurlSlistGuard&& other) noexcept : list(other.list) {
    other.list = nullptr;
}

CurlSlistGuard& CurlSlistGuard::operator=(CurlSlistGuard&& other) noexcept {
    if (this != &other) {
        if (list) curl_slist_free_all(list);
        list = other.list;
        other.list = nullptr;
    }
    return *this;
}

void CurlSlistGuard::append(const char* str) {
    list = curl_slist_append(list, str);
}

curl_slist* CurlSlistGuard::get() const {
    return list;
}

// ============================================================================
// CurlProgressGuard Implementation
// ============================================================================

CurlProgressGuard::CurlProgressGuard(CURL* c, void* callback, void* userdata)
    : curl(c), active(false)
{
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, userdata);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        active = true;
    }
}

CurlProgressGuard::~CurlProgressGuard() {
    if (active && curl) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, nullptr);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, nullptr);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }
}

// ============================================================================
// FormPostGuard Implementation
// ============================================================================

FormPostGuard::FormPostGuard() : form(nullptr) {}

FormPostGuard::FormPostGuard(struct curl_httppost* f) : form(f) {}

FormPostGuard::~FormPostGuard() {
    if (form) {
        curl_formfree(form);
        form = nullptr;
    }
}

FormPostGuard::FormPostGuard(FormPostGuard&& other) noexcept : form(other.form) {
    other.form = nullptr;
}

FormPostGuard& FormPostGuard::operator=(FormPostGuard&& other) noexcept {
    if (this != &other) {
        if (form) curl_formfree(form);
        form = other.form;
        other.form = nullptr;
    }
    return *this;
}

struct curl_httppost* FormPostGuard::get() const {
    return form;
}

void FormPostGuard::reset(struct curl_httppost* f) {
    if (form) curl_formfree(form);
    form = f;
}

// ============================================================================
// Static Callbacks
// ============================================================================

int CurlSession::abort_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    auto* flag = static_cast<std::atomic<bool>*>(clientp);
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    if (flag && flag->load(std::memory_order_acquire)) {
        return 1;
    }
    return 0;
}

// ============================================================================
// RAII CurlSession
// ============================================================================

CurlSession::CurlSession() : curl(curl_easy_init()), abortFlag(nullptr) {}

CurlSession::~CurlSession() {
    if (curl) curl_easy_cleanup(curl);
}

CURL* CurlSession::get() const { return curl; }

CurlSession::operator CURL* () const { return curl; }

CurlSession::operator bool() const { return curl != nullptr; }

CurlSession::CurlSession(CurlSession&& other) noexcept
    : curl(other.curl), abortFlag(other.abortFlag) {
    other.curl = nullptr;
    other.abortFlag = nullptr;
}

CurlSession& CurlSession::operator=(CurlSession&& other) noexcept {
    if (this != &other) {
        if (curl) curl_easy_cleanup(curl);
        curl = other.curl;
        abortFlag = other.abortFlag;
        other.curl = nullptr;
        other.abortFlag = nullptr;
    }
    return *this;
}

void CurlSession::setAbortFlag(std::atomic<bool>* flag) {
    abortFlag = flag;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abort_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, abortFlag);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 1L);
    }
}

CurlGlobalInit::CurlGlobalInit() { curl_global_init(CURL_GLOBAL_ALL); }

CurlGlobalInit::~CurlGlobalInit() { curl_global_cleanup(); }

// ============================================================================
// CURL Callbacks
// ============================================================================

size_t json_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string& data = *(std::string*)userdata;
    data.append(ptr, size * nmemb);
    return size * nmemb;
}

size_t data_write(void* buf, size_t size, size_t nmemb, void* userp) {
    std::ostream& os = *(std::ostream*)userp;
    size_t totalBytes = size * nmemb;
    os.write(static_cast<char*>(buf), totalBytes);
    if (os.good()) {
        return totalBytes;
    }
    else {
        return 0;
    }
}

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {

    if (!IsRunning() || g_shutdownRequested.load(std::memory_order_acquire)) {
        return 1;
    }

    ProgressData* prog = (ProgressData*)clientp;
    if (ultotal > 0) {
        double percent = (ulnow * 100.0) / ultotal;
        if (percent - prog->lastPercent >= 1.0) {
            prog->lastPercent = percent;
            int pos = static_cast<int>(UPLOAD_PROGRESS_BAR_WIDTH * percent / 100.0);
            std::cout << "\r" << GetTimeStamp() << "Uploading: [";
            for (int i = 0; i < UPLOAD_PROGRESS_BAR_WIDTH; ++i) {
                if (i < pos) std::cout << "=";
                else if (i == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << std::fixed << std::setprecision(1) << percent << "%" << std::flush;
        }
        if (ulnow == ultotal && prog->lastPercent < 100.0) {
            prog->lastPercent = 100.0;
            std::cout << "\r" << GetTimeStamp() << "Uploading: [";
            for (int i = 0; i < UPLOAD_PROGRESS_BAR_WIDTH; ++i) std::cout << "=";
            std::cout << "] 100.0%" << std::endl;
        }
    }
    return 0;
}

int abortable_progress(void*, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    if (!IsRunning() || g_shutdownRequested.load(std::memory_order_acquire)) {
        return 1;
    }
    return 0;
}

int abortable_progress_wrapper(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;

    if (!IsRunning() || g_shutdownRequested.load(std::memory_order_acquire)) {
        return 1;
    }

    if (clientp) {
        auto* abortFlag = static_cast<std::atomic<bool>*>(clientp);
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) {
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// HTTP Functions
// ============================================================================

std::pair<CURLcode, long> PerformHttpPostWithCode(const std::string& url, const std::string& postData, std::string& response, long timeout) {
    try {
        CurlSession session;
        if (!session) return { CURLE_FAILED_INIT, 0 };
        CURL* curl = session.get();
        curl_easy_reset(curl);

        CurlSlistGuard headers;
        headers.append("Content-Type: application/x-www-form-urlencoded");
        headers.append("Accept: application/json");
        headers.append("Connection: close");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!postData.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.length());
        }
        else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CurlProgressGuard progressGuard(curl, abortable_progress_wrapper, nullptr);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        return { res, http_code };
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[ERR] Out of memory in PerformHttpPostWithCode: " << e.what() << std::endl;
        return { CURLE_OUT_OF_MEMORY, 0 };
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Exception in PerformHttpPostWithCode: " << e.what() << std::endl;
        return { CURLE_FAILED_INIT, 0 };
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] Unknown exception in PerformHttpPostWithCode" << std::endl;
        return { CURLE_FAILED_INIT, 0 };
    }
}

std::pair<CURLcode, long> PerformHttpGetWithCode(const std::string& url, std::string& response, long timeout) {
    try {
        CurlSession session;
        if (!session) return { CURLE_FAILED_INIT, 0 };
        CURL* curl = session.get();
        curl_easy_reset(curl);

        CurlSlistGuard headers;
        headers.append("Accept: application/json");
        headers.append("Connection: close");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CurlProgressGuard progressGuard(curl, abortable_progress_wrapper, nullptr);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        return { res, http_code };
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[ERR] Out of memory in PerformHttpGetWithCode: " << e.what() << std::endl;
        return { CURLE_OUT_OF_MEMORY, 0 };
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Exception in PerformHttpGetWithCode: " << e.what() << std::endl;
        return { CURLE_FAILED_INIT, 0 };
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] Unknown exception in PerformHttpGetWithCode" << std::endl;
        return { CURLE_FAILED_INIT, 0 };
    }
}

// ============================================================================
// StatusChecker Class Implementation
// ============================================================================

StatusChecker::StatusChecker() : stopFlag(false) {}

StatusChecker::~StatusChecker() { stop(); }

bool StatusChecker::isActive() const {
    return worker.joinable() && !stopFlag.load(std::memory_order_acquire);
}

void StatusChecker::start(const std::string& machineIp, const std::string& machineToken) {
    if (worker.joinable()) return;

    stopFlag.store(false, std::memory_order_release);
    ip = machineIp;
    token = machineToken;

    worker = std::thread([this]() {
        while (!stopFlag.load(std::memory_order_acquire) && IsRunning()) {
            std::string statusUrl = BuildStatusUrl(ip, token);
            std::string response;

            CurlSession session;
            if (session) {
                session.setAbortFlag(&stopFlag);
                CURL* curl = session.get();

                curl_easy_setopt(curl, CURLOPT_URL, statusUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1000L);
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 1L);
                curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 1L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortable_progress_wrapper);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stopFlag);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

                curl_easy_perform(curl);
            }

            for (int i = 0; i < STATUS_CHECKER_POLL_INTERVAL_MS / 100; ++i) {
                if (stopFlag.load(std::memory_order_acquire) || !IsRunning()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        });
}

void StatusChecker::stop() {
    stopFlag.store(true, std::memory_order_release);

    if (worker.joinable()) {
        auto startTime = std::chrono::steady_clock::now();
        const auto MAX_WAIT = std::chrono::seconds(STATUS_CHECKER_SHUTDOWN_TIMEOUT_SECONDS);

        while (worker.joinable() && std::chrono::steady_clock::now() - startTime < MAX_WAIT) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (worker.joinable()) {
            worker.join();
            std::cout << GetTimeStamp() << "[INFO] Status checker keep alive thread stopped." << std::endl;
        }
    }
}

// ============================================================================
// StatusCheckerGuard Implementation
// ============================================================================

StatusCheckerGuard::StatusCheckerGuard(const std::string& machineIp, const std::string& machineToken)
    : ip(machineIp), token(machineToken), active(false)
{
    checker = std::make_unique<StatusChecker>();
    if (checker) {
        checker->start(ip, token);
        active = true;
    }
}

StatusCheckerGuard::~StatusCheckerGuard() { stop(); }

void StatusCheckerGuard::stop() {
    if (checker && active) {
        std::cout << GetTimeStamp() << "[INFO] Stopping status checker keep alive thread (upload finished)..." << std::endl;
        checker->stop();
        active = false;
    }
}

bool StatusCheckerGuard::isActive() const {
    return active && checker && checker->isActive();
}

StatusChecker* StatusCheckerGuard::operator->() { return checker.get(); }

const StatusChecker* StatusCheckerGuard::operator->() const { return checker.get(); }

StatusCheckerGuard::StatusCheckerGuard(StatusCheckerGuard&& other) noexcept
    : checker(std::move(other.checker)), ip(std::move(other.ip)),
    token(std::move(other.token)), active(other.active)
{
    other.active = false;
}

StatusCheckerGuard& StatusCheckerGuard::operator=(StatusCheckerGuard&& other) noexcept {
    if (this != &other) {
        stop();
        checker = std::move(other.checker);
        ip = std::move(other.ip);
        token = std::move(other.token);
        active = other.active;
        other.active = false;
    }
    return *this;
}

static std::chrono::steady_clock::time_point lastConnectTime;
static std::mutex connectMutex;

void ThrottledConnect() {
    static std::chrono::steady_clock::time_point lastCallTime = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastConnectTime = std::chrono::steady_clock::now();
    static std::mutex throttleMutex;

    std::lock_guard<std::mutex> lock(throttleMutex);
    auto now = std::chrono::steady_clock::now();

    // Update last call time on every invocation
    auto elapsedSinceLastCall = std::chrono::duration_cast<std::chrono::seconds>(now - lastCallTime).count();
    lastCallTime = now;

    // Only connect if there was a gap of at least 5 seconds since the previous call
    if (elapsedSinceLastCall >= 5) {
        // Also ensure we don't connect too frequently
        auto elapsedSinceLastConnect = std::chrono::duration_cast<std::chrono::seconds>(now - lastConnectTime).count();
        if (elapsedSinceLastConnect >= 5) {
            lastConnectTime = now;
            ConnectAndMaintainConnection();
        }
    }
}