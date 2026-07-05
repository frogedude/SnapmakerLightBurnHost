// display_about.cpp - Display README/documentation
#include "host.h"

void DisplayAbout() {
    try {
        wchar_t exePathW[MAX_PATH];
        if (GetModuleFileNameW(NULL, exePathW, MAX_PATH) == 0) {
            throw std::runtime_error("Failed to get executable path");
        }

        fs::path exeDir = fs::path(exePathW).parent_path();
        fs::path readmePath = exeDir / "README.txt";
        if (!fs::exists(readmePath)) {
            readmePath = exeDir / "README.md";
        }

        // Check if path is too long
        std::string pathStr = readmePath.string();
        if (pathStr.length() > 260) {
            std::cout << GetTimeStamp() << "[ERR] README.txt path too long: " << pathStr.length() << " chars" << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Expected location: " << pathStr << std::endl;
            return;
        }

        // Check if README.txt exists
        if (!fs::exists(readmePath)) {
            std::cout << GetTimeStamp() << "[ERR] README.txt not found in application directory." << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Expected location: " << pathStr << std::endl;
            return;
        }

        if (!fs::is_regular_file(readmePath)) {
            std::cout << GetTimeStamp() << "[ERR] README.txt exists but is not a regular file." << std::endl;
            return;
        }

        std::ifstream file(readmePath);
        if (!file.is_open()) {
            std::cout << GetTimeStamp() << "[ERR] Could not open README file." << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.find("# ") == 0) {
                std::cout << "\n" << line.substr(2) << "\n";
                std::cout << std::string(line.length() - 2, '=') << "\n";
            }
            else if (line.find("## ") == 0) {
                std::cout << "\n" << line.substr(3) << "\n";
                std::cout << std::string(line.length() - 3, '-') << "\n";
            }
            else {
                std::cout << line << "\n";
            }
        }
    }
    catch (const std::bad_alloc& e) {
        std::cerr << GetTimeStamp() << "[ERR] Out of memory in DisplayAbout: " << e.what() << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Unable to display about information." << std::endl;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << GetTimeStamp() << "[ERR] Filesystem error in DisplayAbout: " << e.what() << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Unable to access README.txt" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] Error in DisplayAbout: " << e.what() << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Unable to display about information." << std::endl;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] Unknown error in DisplayAbout" << std::endl;
        std::cout << GetTimeStamp() << "[ERR] Unable to display about information." << std::endl;
    }
}