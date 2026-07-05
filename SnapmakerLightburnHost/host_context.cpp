// host_context.cpp - HostContext singleton (app state, config, dropped files)
#include "host.h"

// ---------- HostContext Singleton Implementation ----------
HostContext& HostContext::instance() {
    static HostContext instance;
    return instance;
}

// ---------- Configuration ----------
UserConfig HostContext::snapshotConfig() const {
    std::lock_guard<std::mutex> lock(mtx);
    return config;
}

bool HostContext::updateConfig(std::function<bool(UserConfig&)> updater) {
    if (!updater) return false;
    std::lock_guard<std::mutex> lock(mtx);
    UserConfig configCopy = config;
    if (!updater(configCopy)) {
        return false;
    }
    config = std::move(configCopy);
    return true;
}

// ---------- Config File Path ----------
void HostContext::setConfigPath(const fs::path& path) {
    std::lock_guard<std::mutex> lock(mtx);
    configFile = path;
}

fs::path HostContext::getConfigPath() const {
    std::lock_guard<std::mutex> lock(mtx);
    return configFile;
}

// ---------- Dropped Files ----------
std::vector<std::string> HostContext::acquireDroppedFiles() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> result = std::move(droppedFiles);
    droppedFiles.clear();
    return result;
}

bool HostContext::hasDroppedFiles() const {
    std::lock_guard<std::mutex> lock(mtx);
    return !droppedFiles.empty();
}

size_t HostContext::droppedFileCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return droppedFiles.size();
}

std::string HostContext::peekDroppedFile(size_t index) const {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < droppedFiles.size()) return droppedFiles[index];
    return "";
}

void HostContext::addDroppedFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx);
    droppedFiles.push_back(filename);
}

void HostContext::clearDroppedFiles() {
    std::lock_guard<std::mutex> lock(mtx);
    droppedFiles.clear();
}

// ---------- Machine Status ----------
MachineStatusData HostContext::snapshotStatus() const {
    std::lock_guard<std::mutex> lock(mtx);
    return lastStatus;
}

void HostContext::setLastStatus(const MachineStatusData& status) {
    std::lock_guard<std::mutex> lock(mtx);
    lastStatus = status;
    lastStatusUpdate = std::chrono::steady_clock::now();
}

bool HostContext::isStatusStale() const {
    std::lock_guard<std::mutex> lock(mtx);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - lastStatusUpdate).count();
    return elapsed >= STATUS_REFRESH_COOLDOWN_SECONDS;
}

std::chrono::steady_clock::time_point HostContext::getLastStatusUpdate() const {
    std::lock_guard<std::mutex> lock(mtx);
    return lastStatusUpdate;
}

// ---------- Lifecycle (simplified, no background threads) ----------
bool HostContext::isRunning() const {
    return run.load();
}

void HostContext::stopRunning() {
    run.store(false);
}

void HostContext::requestShutdown() {
    run.store(false);
}

bool HostContext::isShutdownRequested() const {
    return !run.load();
}

void HostContext::waitForShutdown() {
    // No background threads, nothing to wait for.
    // Kept for API compatibility.
}

void HostContext::shutdown() {
    // Minimal cleanup. No threads to join, no condition variables.
    stopCamera();
    run.store(false);
}

// ---------- Save Debounce Time ----------
void HostContext::updateLastSaveTime() {
    std::lock_guard<std::mutex> lock(adjustmentMtx);
    lastSaveTime = std::chrono::steady_clock::now();
}

bool HostContext::shouldDebounceSave() const {
    std::lock_guard<std::mutex> lock(adjustmentMtx);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - lastSaveTime).count();
    return elapsed >= SAVE_DEBOUNCE_SECONDS;
}

std::chrono::steady_clock::time_point HostContext::getLastSaveTime() const {
    std::lock_guard<std::mutex> lock(adjustmentMtx);
    return lastSaveTime;
}