// file_utils.cpp - File validation, CheckCommandLineFiles
#include "host.h"

bool IsSupportedFile(const std::string& filename) {
    try {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        for (const auto& e : SUPPORTED_EXTENSIONS) {
            if (ext == e) return true;
        }
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] IsSupportedFile: " << e.what() << std::endl;
        return false;
    }
}

bool IsMacroFile(const std::string& filename) {
    try {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        for (const auto& e : MACRO_EXTENSIONS) {
            if (ext == e) return true;
        }
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] IsMacroFile: " << e.what() << std::endl;
        return false;
    }
}

bool IsFirmwareFile(const std::string& filename) {
    try {
        fs::path p(filename);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ext == ".bin";
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] IsFirmwareFile: " << e.what() << std::endl;
        return false;
    }
}

void CheckCommandLineFiles() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    for (int i = 1; i < argc; ++i) {
        std::wstring warg(argv[i]);
        std::string filename = WStringToString(warg);
        filename = CleanFilePath(filename);

        if (filename.size() > 0 && (filename[0] == '/' || filename[0] == '-')) {
            continue;
        }

        try {
            if (!fs::exists(filename) || !fs::is_regular_file(filename))
                continue;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << GetTimeStamp() << "[WARN] Cannot access file " << filename << ": " << e.what() << std::endl;
            continue;
        }

        if (IsFirmwareFile(filename)) {
            std::cout << GetTimeStamp() << "[WARN] Firmware file detected: " << fs::path(filename).filename().string() << std::endl;
            std::cout << GetTimeStamp() << "[INFO] Will prompt for confirmation before upload.\n";
        }

        if (!IsSupportedFile(filename)) {
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            if (hIn != INVALID_HANDLE_VALUE) {
                FlushConsoleInputBuffer(hIn);
            }
            ClearConsole();
            PrintHeader("SNAPMAKER FILE UPLOAD");
            std::cout << "Unsupported File Type" << std::endl;
            PrintHeader("");
            std::cout << std::endl;
            std::cout << "File: " << fs::path(filename).filename().string() << std::endl;
            std::cout << "Extension: " << fs::path(filename).extension().string() << std::endl;
            std::cout << std::endl;
            std::cout << "Supported extensions: ";
            for (size_t j = 0; j < SUPPORTED_EXTENSIONS.size(); ++j) {
                std::cout << SUPPORTED_EXTENSIONS[j];
                if (j < SUPPORTED_EXTENSIONS.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl << std::endl << "Please use a supported file type." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(ERROR_DISPLAY_SECONDS));
            LocalFree(argv);
            exit(1);
        }

        bool duplicate = false;
        for (size_t j = 0; j < HostContext::instance().droppedFileCount(); ++j) {
            if (HostContext::instance().peekDroppedFile(j) == filename) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            HostContext::instance().addDroppedFile(filename);
            std::cout << GetTimeStamp() << "[OK] Valid file detected: "
                << fs::path(filename).filename().string() << std::endl;
        }
        else {
            std::cout << GetTimeStamp() << "[INFO] Duplicate file ignored: "
                << fs::path(filename).filename().string() << std::endl;
        }
    }
    LocalFree(argv);
}