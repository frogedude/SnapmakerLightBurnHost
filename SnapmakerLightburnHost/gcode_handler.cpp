// gcode_handler.cpp - Macros, manual entry, file upload/prepare/print
#include "host.h"

void CreateMacrosFolder() {
    try {
        if (!fs::exists(MACROS_FOLDER)) {
            fs::create_directory(MACROS_FOLDER);

            std::ofstream sample(MACROS_FOLDER + std::string("/Machine_Setup.txt"));
            if (sample.is_open()) {
                sample << "G28 ; Home all axes\nG90 ; Absolute positioning\nG54 ; Use work coordinates\n";
                sample.close();
                std::cout << GetTimeStamp() << "[OK] Created macros folder with sample file." << std::endl;
            }
            else {
                std::cout << GetTimeStamp() << "[WARN] Created macros folder but could not create sample file." << std::endl;
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Failed to create macros folder: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Unexpected error in CreateMacrosFolder: " << e.what() << std::endl;
    }
}

std::vector<std::pair<int, std::string>> LoadMacros() {
    std::vector<std::pair<int, std::string>> macros;
    int idx = 1;

    try {
        if (fs::exists(MACROS_FOLDER) && fs::is_directory(MACROS_FOLDER)) {
            try {
                for (const auto& entry : fs::directory_iterator(MACROS_FOLDER)) {
                    try {
                        if (entry.is_regular_file()) {
                            std::string filename = entry.path().filename().string();
                            if (IsMacroFile(filename)) {
                                macros.push_back({ idx++, entry.path().string() });
                            }
                        }
                    }
                    catch (const std::filesystem::filesystem_error& e) {
                        std::cerr << GetTimeStamp() << "[WARN] Cannot access file: "
                            << entry.path().string() << " - " << e.what() << std::endl;
                        continue;
                    }
                    catch (const std::exception& e) {
                        std::cerr << GetTimeStamp() << "[WARN] Unexpected error processing file: "
                            << e.what() << std::endl;
                        continue;
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                std::cerr << GetTimeStamp() << "[ERR] Cannot iterate macros directory: "
                    << e.what() << std::endl;
                return macros;
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Cannot access macros folder: "
            << e.what() << std::endl;
        return macros;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Unexpected error loading macros: "
            << e.what() << std::endl;
        return macros;
    }

    return macros;
}

void SendMacroFile(const std::string& filepath) {
    try {
        if (!fs::exists(filepath)) {
            std::cout << GetTimeStamp() << "[ERR] Macro file not found." << std::endl;
            return;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << GetTimeStamp() << "[ERR] Cannot access macro file: " << e.what() << std::endl;
        return;
    }
    catch (const std::exception& e) {
        std::cout << GetTimeStamp() << "[ERR] Unexpected error checking macro file: " << e.what() << std::endl;
        return;
    }

    if (!IsRunning()) {
        return;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << GetTimeStamp() << "[ERR] Could not open macro file." << std::endl;
        return;
    }

    std::cout << GetTimeStamp() << "Executing macro: " << fs::path(filepath).filename().string() << std::endl;
    std::string line;
    int lineNum = 0, success = 0;

    try {
        while (std::getline(file, line)) {
            if (!IsRunning()) {
                std::cout << GetTimeStamp() << "[INFO] Shutdown requested. Stopping macro execution." << std::endl;
                break;
            }
            lineNum++;
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty() || line[0] == ';' || line[0] == '(') continue;

            size_t comment = line.find(';');
            if (comment != std::string::npos) line = line.substr(0, comment);
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;

            std::transform(line.begin(), line.end(), line.begin(), ::toupper);
            std::cout << GetTimeStamp() << "[" << lineNum << "] Sending: " << line << "... " << std::flush;

            if (SendRawGCode(line) == CURLE_OK) {
                std::cout << "[OK]" << std::endl;
                success++;
            }
            else {
                std::cout << "[FAIL]" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(GCODE_DELAY_MS));
        }
    }
    catch (const std::exception& e) {
        std::cout << GetTimeStamp() << "[ERR] Error reading macro file: " << e.what() << std::endl;
    }

    std::cout << GetTimeStamp() << "[OK] Macro completed. " << success << " commands sent." << std::endl;
}

void SendManualGCode() {
    std::cout << GetTimeStamp() << "Enter G-code to send:" << std::endl;
    std::cout << GetTimeStamp() << "Examples: G28, G90, G54, G1 X10 Y20 F1000, M5, etc." << std::endl;
    std::cout << GetTimeStamp() << "UP/DOWN: Navigate history | LEFT/RIGHT: Move cursor" << std::endl;
    std::cout << GetTimeStamp() << "Press 'H' for history, 'C' to clear history." << std::endl;
    std::cout << GetTimeStamp() << "Type 'CLS' or 'CLEAR' to clear the screen." << std::endl;
    std::cout << GetTimeStamp() << "Note: All commands are converted to uppercase before sending." << std::endl;
    std::cout << GetTimeStamp() << "Press 'ESC' to exit or type 'exit' to return." << std::endl;
    PrintHeader("");
    std::cout << std::endl;

    auto toUpper = [](std::string s) -> std::string {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
        return s;
        };
    std::vector<std::string> history;
    size_t histPos = 0;
    bool exitReq = false;
    std::string input;
    size_t cursor = 0;

    while (!exitReq && IsRunning()) {
        std::cout << "G-code> ";
        input.clear(); cursor = 0; histPos = 0;
        while (true) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == ESC_ASCII) { exitReq = true; std::cout << std::endl; break; }
                if (ch == ENTER_ASCII) { std::cout << std::endl; break; }
                if (ch == ARROW_PREFIX) {
                    int arrow = _getch();
                    if (arrow == UP_ARROW_KEY && !history.empty() && histPos < history.size()) {
                        histPos++;
                        input = history[history.size() - histPos];
                        cursor = input.length();
                        std::cout << "\x1b[2K\rG-code> " << input << std::flush;
                    }
                    else if (arrow == DOWN_ARROW_KEY && histPos > 0) {
                        histPos--;
                        if (histPos == 0) {
                            input.clear();
                        }
                        else {
                            input = history[history.size() - histPos];
                        }
                        cursor = input.length();
                        std::cout << "\x1b[2K\rG-code> " << input << std::flush;
                    }
                    else if (arrow == LEFT_ARROW_KEY && cursor > 0) { cursor--; std::cout << "\b"; }
                    else if (arrow == RIGHT_ARROW_KEY && cursor < input.length()) { std::cout << input[cursor]; cursor++; }
                    continue;
                }
                if (ch == 8) {
                    if (cursor > 0) {
                        input.erase(cursor - 1, 1);
                        cursor--;
                        std::cout << "\b" << input.substr(cursor) << " ";
                        for (size_t i = 0; i < input.length() - cursor + 1; ++i) std::cout << "\b";
                    }
                    continue;
                }
                if (isprint(ch)) {
                    input.insert(cursor, 1, static_cast<char>(ch));
                    cursor++;
                    std::cout << static_cast<char>(ch) << input.substr(cursor);
                    for (size_t i = 0; i < input.length() - cursor; ++i) std::cout << "\b";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (exitReq) break;
        if (input.empty()) continue;
        std::string cmd = input;
        cmd.erase(0, cmd.find_first_not_of(" \t\r\n"));
        cmd.erase(cmd.find_last_not_of(" \t\r\n") + 1);
        if (cmd.empty()) continue;
        std::string upper = toUpper(cmd);

        if (upper == "CLS" || upper == "CLEAR") {
            ClearConsole();
            PrintHeader("SNAPMAKER MANUAL G-CODE");
            std::cout << GetTimeStamp() << "Enter G-code to send:" << std::endl;
            std::cout << GetTimeStamp() << "Examples: G28, G90, G54, G1 X10 Y20 F1000, M5, etc." << std::endl;
            std::cout << GetTimeStamp() << "UP/DOWN: Navigate history | LEFT/RIGHT: Move cursor" << std::endl;
            std::cout << GetTimeStamp() << "Press 'H' for history, 'C' to clear history." << std::endl;
            std::cout << GetTimeStamp() << "Type 'CLS' or 'CLEAR' to clear the screen." << std::endl;
            std::cout << GetTimeStamp() << "Note: All commands are converted to uppercase before sending." << std::endl;
            std::cout << GetTimeStamp() << "Press 'ESC' to exit or type 'exit' to return." << std::endl;
            PrintHeader("");
            std::cout << std::endl;
            continue;
        }

        if (upper == "EXIT" || upper == "QUIT" || upper == "Q") break;
        if (upper == "H") {
            if (history.empty()) std::cout << GetTimeStamp() << "No history." << std::endl;
            else for (size_t i = 0; i < history.size(); ++i) std::cout << "  " << i + 1 << ". " << history[i] << std::endl;
            std::cout << std::endl;
            continue;
        }
        if (upper == "C") {
            history.clear(); histPos = 0;
            std::cout << GetTimeStamp() << "History cleared." << std::endl << std::endl;
            continue;
        }
        if (cmd[0] == '!') {
            try {
                int idx = std::stoi(cmd.substr(1)) - 1;
                if (idx >= 0 && idx < (int)history.size()) {
                    cmd = history[idx];
                    std::cout << GetTimeStamp() << "Recalled: " << cmd << std::endl;
                    upper = toUpper(cmd);
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] Invalid index." << std::endl;
                    continue;
                }
            }
            catch (const std::invalid_argument& e) {
                std::cout << GetTimeStamp() << "[ERR] Invalid recall number: " << e.what() << std::endl;
                continue;
            }
            catch (const std::out_of_range& e) {
                std::cout << GetTimeStamp() << "[ERR] Number out of range: " << e.what() << std::endl;
                continue;
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[ERR] Recall error: " << e.what() << std::endl;
                continue;
            }
            catch (...) {
                std::cout << GetTimeStamp() << "[ERR] Unknown recall error" << std::endl;
                continue;
            }
        }
        if (upper == "!!") {
            if (history.empty()) { std::cout << GetTimeStamp() << "[ERR] No history." << std::endl; continue; }
            cmd = history.back();
            std::cout << GetTimeStamp() << "Recalled: " << cmd << std::endl;
            upper = toUpper(cmd);
        }
        if (history.empty() || history.back() != cmd) {
            history.push_back(cmd);
            histPos = 0;
        }
        std::string resp;
        auto start = std::chrono::steady_clock::now();
        CURLcode res = SendRawGCodeWithResponse(upper, resp);
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (res == CURLE_OK) {
            if (!resp.empty()) {
                try {
                    if (resp.front() == '{' && resp.back() == '}') {
                        auto j = json::parse(resp);
                        std::cout << GetTimeStamp() << "Response: ";
                        if (j.contains("message") && j["message"].is_string()) std::cout << j["message"];
                        else if (j.contains("status") && j["status"].is_boolean()) std::cout << (j["status"].get<bool>() ? "OK" : "Failed");
                        else std::cout << resp;
                        std::cout << std::endl;
                    }
                    else {
                        std::cout << GetTimeStamp() << "Response: " << resp << std::endl;
                    }
                }
                catch (const json::parse_error& e) {
                    std::cout << GetTimeStamp() << "Response (invalid JSON): " << resp << " - Parse error: " << e.what() << std::endl;
                }
                catch (const std::exception& e) {
                    std::cout << GetTimeStamp() << "Response: " << resp << " (Error: " << e.what() << ")" << std::endl;
                }
                catch (...) {
                    std::cout << GetTimeStamp() << "Response: " << resp << " (Unknown error)" << std::endl;
                }
            }
            else {
                std::cout << GetTimeStamp() << "Response: [OK]" << std::endl;
            }
            if (dur > 100) std::cout << GetTimeStamp() << "Execution time: " << dur << " ms" << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[FAIL] " << curl_easy_strerror(res) << " (Duration: " << dur << " ms)" << std::endl;
            if (!resp.empty()) std::cout << GetTimeStamp() << "Error details: " << resp << std::endl;
        }
        std::cout << std::endl;
    }
}

void SendCustomGCodeMenu(bool fromShortcut) {
    CreateMacrosFolder();
    bool inMenu = true;
    bool needRedraw = true;

    while (inMenu && IsRunning()) {
        if (needRedraw) {
            ClearAndDrawHeader(false);
            ClearConsole();
            PrintHeader("SNAPMAKER G-CODE MACROS");
            std::cout << "Create .txt macro files in the macros folder. Executes the G-code sequences line-by-line." << std::endl;
            PrintHeader("");
            std::cout << std::endl;

            auto macros = LoadMacros();
            if (!macros.empty()) {
                std::cout << "Available Macros:" << std::endl;
                for (auto& m : macros)
                    std::cout << "  " << m.first << ". " << fs::path(m.second).filename().string() << std::endl;
                std::cout << std::endl;
            }
            else {
                std::cout << "No macros found in 'macros' folder." << std::endl << std::endl;
            }

            std::cout << "Options:" << std::endl;
            if (!macros.empty()) {
                std::string prefix = "  [1-" + std::to_string(macros.size()) + "]";
                std::cout << std::left << std::setw(10) << prefix << "- Run corresponding macro" << std::endl;
            }
            std::cout << "  [M]     - Send manual G-code" << std::endl;
            std::cout << "  [ENTER] - Refresh macro list" << std::endl;
            std::cout << "  [ESC]   - Return to main menu" << std::endl << std::endl;
            std::cout << "Enter choice: " << std::flush;
            needRedraw = false;
        }

        if (_kbhit()) {
            std::string input;
            while (true) {
                int ch = _getch();
                if (ch == 'M' || ch == 'm') {
                    input = static_cast<char>(ch);
                    std::cout << std::endl;
                    break;
                }
                if (ch == ENTER_ASCII) {
                    std::cout << std::endl;
                    break;
                }
                if (ch == ESC_ASCII) {
                    std::cout << std::endl;
                    inMenu = false;
                    break;
                }
                if (ch == 8) {
                    if (!input.empty()) {
                        input.pop_back();
                        std::cout << "\b \b";
                    }
                    continue;
                }
                if (ch == ARROW_PREFIX) {
                    (void)_getch();
                    continue;
                }
                if (isprint(ch)) {
                    input += static_cast<char>(ch);
                    std::cout << static_cast<char>(ch);
                }
            }

            if (!inMenu) {
                break;
            }

            input.erase(0, input.find_first_not_of(" \t\r\n"));
            input.erase(input.find_last_not_of(" \t\r\n") + 1);

            if (input.empty()) {
                needRedraw = true;
                continue;
            }

            if (input == "M" || input == "m") {
                ClearConsole();
                PrintHeader("SNAPMAKER MANUAL G-CODE");
                SendManualGCode();
                needRedraw = true;
                continue;
            }

            try {
                int choice = std::stoi(input);
                auto macros = LoadMacros();
                bool found = false;
                for (auto& m : macros) {
                    if (m.first == choice) {
                        ClearConsole();
                        PrintHeader("SNAPMAKER EXECUTING MACRO");
                        std::cout << std::endl;
                        std::cout << GetTimeStamp() << "Selected: " << fs::path(m.second).filename().string() << std::endl;
                        SendMacroFile(m.second);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::cout << GetTimeStamp() << "[ERR] Invalid macro number." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                }
                std::cout << std::endl << "Press any key to continue..." << std::endl;
                (void)_getch();
                needRedraw = true;
            }
            catch (const std::invalid_argument& e) {
                std::cout << GetTimeStamp() << "[INFO] Invalid macro number: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                needRedraw = true;
            }
            catch (const std::out_of_range& e) {
                std::cout << GetTimeStamp() << "[INFO] Macro number out of range: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                needRedraw = true;
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[INFO] Macro error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                needRedraw = true;
            }
            catch (...) {
                std::cout << GetTimeStamp() << "[INFO] Unrecognized input (unknown error)" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                needRedraw = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }
    if (!fromShortcut) {
        ClearAndDrawHeader(true);
    }
}

std::vector<std::pair<std::string, std::string>> ExecuteMacroWithResponse(const std::string& filepath) {
    std::vector<std::pair<std::string, std::string>> results;
    std::ifstream file(filepath);
    if (!file.is_open()) return results;

    std::string line;
    while (std::getline(file, line)) {
        // Trim and skip comments/empty lines
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty() || line[0] == ';' || line[0] == '(') continue;

        // Remove inline comment
        size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;

        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        std::string response;
        CURLcode res = SendRawGCodeWithResponse(upper, response);
        if (res == CURLE_OK) {
            results.push_back({ upper, response.empty() ? "OK" : response });
        }
        else {
            results.push_back({ upper, "FAIL: " + std::string(curl_easy_strerror(res)) });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(GCODE_DELAY_MS));
    }
    return results;
}