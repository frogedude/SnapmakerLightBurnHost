// string_utils.cpp - GetTimeStamp, WStringToString, validation, etc.
#include "host.h"

std::string GetTimeStamp() {
    try {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        tm tmBuf{};
        localtime_s(&tmBuf, &t);
        std::stringstream ss;
        ss << std::put_time(&tmBuf, "[%H:%M:%S] ");
        return ss.str();
    }
    catch (const std::exception&) {
        return "[ERR:TIMESTAMP] ";
    }
}

std::string WStringToString(const std::wstring& wstr) {
    try {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
            nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) return std::string();
        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
            &result[0], size_needed, nullptr, nullptr);
        return result;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] WStringToString: " << e.what() << std::endl;
        return std::string();
    }
}

std::string CleanFilePath(const std::string& path) {
    try {
        std::string cleaned = path;
        if (cleaned.size() >= 2 && cleaned.front() == '"' && cleaned.back() == '"') {
            cleaned = cleaned.substr(1, cleaned.size() - 2);
        }
        return cleaned;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] CleanFilePath: " << e.what() << std::endl;
        return path;
    }
}

void FlushInputBuffer() {
    ConsoleGuard::FlushInputBuffers();
}

bool IsValidIPAddress(const std::string& ip) {
    try {
        std::regex ipPattern(R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
        return std::regex_match(ip, ipPattern);
    }
    catch (const std::regex_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Regex error in IP validation: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Unexpected error in IP validation: " << e.what() << std::endl;
        return false;
    }
}