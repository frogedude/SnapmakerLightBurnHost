// console_ui.cpp - ClearConsole, PrintHeader, main menu display
#include "host.h"

void ClearConsole() {
    ConsoleGuard::ClearScreenWin32();
}

void PrintHeader(const std::string& text, int width, char fillChar) {
    if (text.empty()) {
        std::cout << std::string(width, fillChar) << "\n";
        return;
    }

    std::string content = " " + text + " ";
    int leftPadding = (width - static_cast<int>(content.length())) / 2;
    int rightPadding = width - static_cast<int>(content.length()) - leftPadding;

    std::cout << std::string(leftPadding, fillChar)
        << content
        << std::string(rightPadding, fillChar)
        << "\n";
}

void ClearAndDrawHeader(bool showHeaderMenu) {
    ClearConsole();
    PrintHeader("SNAPMAKER LIGHTBURN HOST");
    if (showHeaderMenu) {
        UserConfig cfg = HostContext::instance().snapshotConfig();
        std::cout << "Current Configuration:" << std::endl;
        std::cout << "  Model: " << GetModelName(cfg.model) << " | " << "IP Address: " << cfg.ipAddress << " | " << "Token: " << cfg.token << std::endl;
        std::cout << "  Image Base Position: X=" << std::fixed << std::setprecision(1) << cfg.basePositionX << " mm, Y=" << cfg.basePositionY << " mm, Z=" << cfg.basePositionZ << " mm" << std::endl;
        std::cout << "  Material Thickness: " << GetMaterialThicknessModeDetails() << std::endl;
        std::cout << "  ";
        DisplayAdjustmentStatus();
        std::cout << "  Kasa: " << (cfg.kasa.ipAddress.empty() ? "None" : cfg.kasa.ipAddress)
            << " - Auto (Power-On: " << (cfg.kasa.autoPowerOn ? "ON" : "OFF")
            << ", Power-Off: " << (cfg.kasa.autoPowerOff ? "ON" : "OFF") << ")"
            << " | Auto-Start Print: " << (cfg.autoStartPrint ? "ON" : "OFF")
            << " | Auto-Start Camera: " << (cfg.rtspCamera.autoLaunch ? "ON" : "OFF")
            << std::endl;
        PrintHeader("");
        std::cout << "SNAPMAKER CONTROLS:" << std::endl;
        std::cout << "  [ENTER]  - Capture Image (moves bed to base position)" << std::endl;
        std::cout << "  [SPACE]  - Measure Material Thickness" << std::endl;
        std::cout << "  [Y]      - Toggle Material Thickness Mode (Default (Center) <-> Custom (X/Y))" << std::endl;
        std::cout << "  [F]      - Set/Reset Custom Material Thickness Coordinates" << std::endl;
        std::cout << "  [M]      - Machine Setup: Home (G28) + Set Absolute Positioning (G90) + Switch to Work Coordinates (G54)" << std::endl;
        std::cout << "  [S]      - Show Machine Status Page" << std::endl;
        std::cout << "  [G]      - G-code Macros (select macro or manual entry)" << std::endl;
        std::cout << "  [A]      - Auto-Refresh Image Capture Mode" << std::endl;
        std::cout << "  [U]      - Update Image Base Position Coordinates" << std::endl;
        std::cout << "  [B]      - Reset Image Base Position to Default Values" << std::endl;
        std::cout << "  [P]      - Toggle Auto-Start Print (ON/OFF)" << std::endl;
        std::cout << "ADJUSTMENT CONTROLS:" << std::endl;
        std::cout << "  [LEFT]   - Adjust Image Left (-1 pixel)" << std::endl;
        std::cout << "  [RIGHT]  - Adjust Image Right (+1 pixel)" << std::endl;
        std::cout << "  [UP]     - Adjust Image Up (-1 pixel)" << std::endl;
        std::cout << "  [DOWN]   - Adjust Image Down (+1 pixel)" << std::endl;
        std::cout << "  [R]      - Reset Image Adjustment to Zero (in Adjustment Controls)" << std::endl;
        std::cout << "  [T]      - Toggle Image Adjustment ON/OFF" << std::endl;
        std::cout << "GENERAL CONTROLS:" << std::endl;
        std::cout << "  [L]      - Launch LightBurn" << std::endl;
        std::cout << "  [K]      - Kasa Smart Plug Control (TP-Link HS100/HS110/HS300/KP115)" << std::endl;
        std::cout << "  [V]      - RTSP Camera Menu" << std::endl;
        std::cout << "  [C]      - Clear Screen / Redisplay Menu" << std::endl;
        std::cout << "  [ESC/X]  - Exit Program" << std::endl;
        std::cout << "DRAG AND DROP:" << std::endl;
        std::cout << "  Drag files onto the SnapmakerLightBurnHost.exe to auto upload or auto-start a print job." << std::endl;
        std::cout << "  Press SHIFT during the " << SHIFT_DETECTION_TIMEOUT_SECONDS << "-second window to toggle upload and auto-start." << std::endl;
    }
    PrintHeader("");
    std::cout << "ACTIVITY LOG:" << std::endl;
    PrintHeader("", 108, '-');
}

std::string GetModelName(SnapmakerModel model) {
    switch (model) {
    case MODEL_A350: return "Snapmaker 2.0 A350";
    case MODEL_A250: return "Snapmaker 2.0 A250";
    default: return "Unknown Model";
    }
}

bool SelectSnapmakerModel(SnapmakerModel& model) {
    std::cout << "\nSelect Snapmaker 2.0 Model:" << std::endl;
    std::cout << "  1. A350 (X: 232.0 Y: 178.0 Z: 290.0)" << std::endl;
    std::cout << "  2. A250 (X: 160.0 Y: 120.0 Z: 175.0)" << std::endl;
    std::cout << "Enter choice (1 or 2): " << std::flush;

    while (true) {
        if (!IsRunning()) {
            return false;
        }

        if (_kbhit()) {
            int ch = _getch();
            if (ch == '1') { model = MODEL_A350; std::cout << "1" << std::endl; return true; }
            if (ch == '2') { model = MODEL_A250; std::cout << "2" << std::endl; return true; }
            if (ch == 0 || ch == 224) { (void)_getch(); continue; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }
}