// input_utils.cpp - CheckForShiftKey, HasProblematicCharacters
#include "host.h"

bool CheckForShiftKey(int timeoutSeconds) {
    try {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < timeoutSeconds) {
            if (!IsRunning()) {
                return false;
            }
            if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] CheckForShiftKey: " << e.what() << std::endl;
        return false;
    }
}