// shortcuts.cpp - Command-line parsing and execution
#include "host.h"

// ---------- Shortcut Functions ----------
ShortcutParams ParseShortcutWithParams(int argc, LPWSTR* argv) {
    ShortcutParams result;

    for (int i = 1; i < argc; ++i) {
        std::wstring warg(argv[i]);
        std::string arg = WStringToString(warg);
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "/help" || arg == "-help" || arg == "--help" || arg == "/?" || arg == "-?") {
            result.action = ShortcutAction::HELP;
            return result;
        }

        if (arg == "/home" || arg == "-home") { result.action = ShortcutAction::HOME; return result; }
        if (arg == "/lasersetup" || arg == "-lasersetup") { result.action = ShortcutAction::LASERSETUP; return result; }
        if (arg == "/printersetup" || arg == "-printersetup") { result.action = ShortcutAction::PRINTERSETUP; return result; }
        if (arg == "/status" || arg == "-status") { result.action = ShortcutAction::STATUS; return result; }
        if (arg == "/kasa" || arg == "-kasa") { result.action = ShortcutAction::KASA; return result; }
        if (arg == "/kasaon" || arg == "-kasaon") { result.action = ShortcutAction::KASA_ON; return result; }
        if (arg == "/kasaoff" || arg == "-kasaoff") { result.action = ShortcutAction::KASA_OFF; return result; }
        if (arg == "/macros" || arg == "-macros") { result.action = ShortcutAction::MACROS; return result; }
        if (arg == "/gcode" || arg == "-gcode") { result.action = ShortcutAction::GCODE; return result; }
        if (arg == "/thickness" || arg == "-thickness") { result.action = ShortcutAction::THICKNESS; return result; }
        if (arg == "/capture" || arg == "-capture") { result.action = ShortcutAction::CAPTURE; return result; }
        if (arg == "/autocapture" || arg == "-autocapture" || arg == "/autorefresh" || arg == "-autorefresh") {
            result.action = ShortcutAction::AUTOCAPTURE;
            return result;
        }
        if (arg == "/resetconfig" || arg == "-resetconfig") { result.action = ShortcutAction::RESETCONFIG; return result; }
        if (arg == "/resettoken" || arg == "-resettoken") { result.action = ShortcutAction::RESETTOKEN; return result; }
        if (arg == "/softcamcheck" || arg == "-softcamcheck") { result.action = ShortcutAction::SOFTCAMCHECK; return result; }
        if (arg == "/lightburn" || arg == "-lightburn") { result.action = ShortcutAction::LIGHTBURN; return result; }
        if (arg == "/lightburncamera" || arg == "-lightburncamera") { result.action = ShortcutAction::LIGHTBURNCAMERA; return result; }
        if (arg == "/about" || arg == "-about") { result.action = ShortcutAction::ABOUT; return result; }
        if (arg == "/stop" || arg == "-stop") { result.action = ShortcutAction::STOP; return result; }
        if (arg == "/pause" || arg == "-pause") { result.action = ShortcutAction::PAUSE; return result; }
        if (arg == "/resume" || arg == "-resume") { result.action = ShortcutAction::RESUME; return result; }
        if (arg == "/monitor" || arg == "-monitor") { result.action = ShortcutAction::MONITOR; return result; }
        if (arg == "/camera" || arg == "-camera") { result.action = ShortcutAction::CAMERA; return result; }
        if (arg == "/camerasettings" || arg == "-camerasettings") { result.action = ShortcutAction::CAMERASETTINGS; return result; }

        // Beep shortcuts
        if (arg == "/beepon" || arg == "-beepon") {
            result.action = ShortcutAction::BEEP_ON;
            return result;
        }
        if (arg == "/beepoff" || arg == "-beepoff") {
            result.action = ShortcutAction::BEEP_OFF;
            return result;
        }

        // Email shortcuts
        if (arg == "/emailconfig" || arg == "-emailconfig") {
            result.action = ShortcutAction::EMAILCONFIG;
            return result;
        }
        if (arg == "/emailon" || arg == "-emailon") {
            result.action = ShortcutAction::EMAIL_ON;
            return result;
        }
        if (arg == "/emailoff" || arg == "-emailoff") {
            result.action = ShortcutAction::EMAIL_OFF;
            return result;
        }

        // File Action shortcut (recycle bin vs permanent delete)
        if (arg == "/fileaction" || arg == "-fileaction") {
            if (i + 1 < argc) {
                std::wstring nextArg(argv[i + 1]);
                std::string next = WStringToString(nextArg);
                std::transform(next.begin(), next.end(), next.begin(), ::tolower);

                if (next == "recycle" || next == "recyclebin") {
                    result.action = ShortcutAction::SETFILEACTION;
                    result.parameter = "recycle";
                    result.hasParameter = true;
                    i++;
                    return result;
                }
                else if (next == "delete" || next == "permanent") {
                    result.action = ShortcutAction::SETFILEACTION;
                    result.parameter = "delete";
                    result.hasParameter = true;
                    i++;
                    return result;
                }
                else {
                    result.action = ShortcutAction::SETFILEACTION;
                    result.parameter = "";
                    result.hasParameter = false;
                    return result;
                }
            }
        }

        if ((arg == "/enclosurefan" || arg == "-enclosurefan") && i + 1 < argc) {
            std::wstring nextArg(argv[i + 1]);
            std::string next = WStringToString(nextArg);
            std::transform(next.begin(), next.end(), next.begin(), ::tolower);

            result.action = ShortcutAction::ENCLOSUREFAN;
            if (next == "off") {
                result.parameter = "0";
            }
            else {
                result.parameter = next;
            }
            result.hasParameter = true;
            return result;
        }

        if ((arg == "/enclosureled" || arg == "-enclosureled") && i + 1 < argc) {
            std::wstring nextArg(argv[i + 1]);
            std::string next = WStringToString(nextArg);
            std::transform(next.begin(), next.end(), next.begin(), ::tolower);

            result.action = ShortcutAction::ENCLOSURELED;
            if (next == "off") {
                result.parameter = "0";
            }
            else {
                result.parameter = next;
            }
            result.hasParameter = true;
            return result;
        }

        if ((arg == "/kasa" || arg == "-kasa") && i + 1 < argc) {
            std::wstring nextArg(argv[i + 1]);
            std::string next = WStringToString(nextArg);
            std::transform(next.begin(), next.end(), next.begin(), ::tolower);

            if (next == "on") {
                result.action = ShortcutAction::KASA_ON;
                return result;
            }
            else if (next == "off") {
                result.action = ShortcutAction::KASA_OFF;
                return result;
            }
            else {
                result.action = ShortcutAction::KASA;
                return result;
            }
        }

        if ((arg == "/upload" || arg == "-upload") && i + 1 < argc) {
            result.action = ShortcutAction::UPLOAD;
            result.filename = WStringToString(argv[i + 1]);
            result.hasFilename = true;
            return result;
        }

        if ((arg == "/print" || arg == "-print") && i + 1 < argc) {
            result.action = ShortcutAction::PRINT;
            result.filename = WStringToString(argv[i + 1]);
            result.hasFilename = true;
            return result;
        }

        if (arg == "/kasapoweronon") {
            result.action = ShortcutAction::SETKASAAUTO;
            result.autoPowerOn = true;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/kasapoweronoff") {
            result.action = ShortcutAction::SETKASAAUTO;
            result.autoPowerOn = false;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/kasapoweroffon") {
            result.action = ShortcutAction::SETKASAAUTOOFF;
            result.autoPowerOn = true;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/kasapoweroffoff") {
            result.action = ShortcutAction::SETKASAAUTOOFF;
            result.autoPowerOn = false;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/autostarton" || arg == "-autostarton") {
            result.action = ShortcutAction::SETAUTOSTART;
            result.autoPowerOn = true;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/autostartoff" || arg == "-autostartoff") {
            result.action = ShortcutAction::SETAUTOSTART;
            result.autoPowerOn = false;
            result.hasAutoPowerOn = true;
            return result;
        }

        if (arg == "/automonitoron" || arg == "-automonitoron") {
            result.action = ShortcutAction::AUTOMONITOR_ON;
            return result;
        }
        if (arg == "/automonitoroff" || arg == "-automonitoroff") {
            result.action = ShortcutAction::AUTOMONITOR_OFF;
            return result;
        }

        if ((arg == "/kasaip" || arg == "-kasaip") && i + 1 < argc) {
            result.action = ShortcutAction::SETKASAIP;
            result.kasaIp = WStringToString(argv[i + 1]);
            return result;
        }

        if (arg == "/serverlocalhost" || arg == "-serverlocalhost") {
            result.action = ShortcutAction::SERVER_BIND_LOCALHOST;
            return result;
        }
        if (arg == "/serverglobal" || arg == "-serverglobal") {
            result.action = ShortcutAction::SERVER_BIND_GLOBAL;
            return result;
        }

        if (arg == "/webauth" || arg == "-webauth") {
            if (i + 1 < argc) {
                std::wstring nextArg(argv[i + 1]);
                std::string next = WStringToString(nextArg);
                std::transform(next.begin(), next.end(), next.begin(), ::tolower);

                if (next == "on") {
                    result.action = ShortcutAction::SETWEBAUTH;
                    result.autoPowerOn = true;  // reuse as boolean flag
                    result.hasAutoPowerOn = true;
                    i++;
                    return result;
                }
                else if (next == "off") {
                    result.action = ShortcutAction::SETWEBAUTH;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                    i++;
                    return result;
                }
            }
        }
        if ((arg == "/webuser" || arg == "-webuser") && i + 1 < argc) {
            result.action = ShortcutAction::SETWEBUSER;
            result.parameter = WStringToString(argv[i + 1]);
            result.hasParameter = true;
            i++;
            return result;
        }
        if ((arg == "/webpassword" || arg == "-webpassword") && i + 1 < argc) {
            result.action = ShortcutAction::SETWEBPASSWORD;
            result.parameter = WStringToString(argv[i + 1]);
            result.hasParameter = true;
            i++;
            return result;
        }

        if ((arg == "/connect" || arg == "-connect") && i + 1 < argc) {
            result.action = ShortcutAction::CONNECT;
            result.ipAddress = WStringToString(argv[i + 1]);
            for (int j = i + 2; j < argc; ++j) {
                std::wstring nextArg(argv[j]);
                std::string next = WStringToString(nextArg);
                std::transform(next.begin(), next.end(), next.begin(), ::tolower);

                if ((next == "/token" || next == "-token") && j + 1 < argc) {
                    result.token = WStringToString(argv[j + 1]);
                    j++;
                }
                else if ((next == "/kasaip" || next == "-kasaip") && j + 1 < argc) {
                    result.kasaIp = WStringToString(argv[j + 1]);
                    j++;
                }
                else if (next == "/kasapoweron") {
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/kasapoweroff") {
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/kasapoweroffon") {
                    result.action = ShortcutAction::SETKASAAUTOOFF;
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/kasapoweroffoff") {
                    result.action = ShortcutAction::SETKASAAUTOOFF;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/autostarton") {
                    result.action = ShortcutAction::SETAUTOSTART;
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/autostartoff") {
                    result.action = ShortcutAction::SETAUTOSTART;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/automonitoron") {
                    result.action = ShortcutAction::AUTOMONITOR_ON;
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/automonitoroff") {
                    result.action = ShortcutAction::AUTOMONITOR_OFF;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/beepon") {
                    result.action = ShortcutAction::BEEP_ON;
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/beepoff") {
                    result.action = ShortcutAction::BEEP_OFF;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/emailon") {
                    result.action = ShortcutAction::EMAIL_ON;
                    result.autoPowerOn = true;
                    result.hasAutoPowerOn = true;
                }
                else if (next == "/emailoff") {
                    result.action = ShortcutAction::EMAIL_OFF;
                    result.autoPowerOn = false;
                    result.hasAutoPowerOn = true;
                }
                else if ((next == "/fileaction" || next == "-fileaction") && j + 1 < argc) {
                    std::wstring faArg(argv[j + 1]);
                    std::string fa = WStringToString(faArg);
                    std::transform(fa.begin(), fa.end(), fa.begin(), ::tolower);
                    if (fa == "recycle" || fa == "recyclebin") {
                        result.action = ShortcutAction::SETFILEACTION;
                        result.parameter = "recycle";
                        result.hasParameter = true;
                    }
                    else if (fa == "delete" || fa == "permanent") {
                        result.action = ShortcutAction::SETFILEACTION;
                        result.parameter = "delete";
                        result.hasParameter = true;
                    }
                    j++;
                }
            }
            return result;
        }

        if (arg == "/tray" || arg == "-tray") {
            result.action = ShortcutAction::TRAY;
            return result;
        }

        if (arg == "/server" || arg == "-server") {
            result.action = ShortcutAction::UPLOADSERVER_START;
            return result;
        }

        if ((arg == "/token" || arg == "-token") && i + 1 < argc) {
            result.action = ShortcutAction::SETTOKEN;
            result.token = WStringToString(argv[i + 1]);
            return result;
        }
    }

    return result;
}

void DisplayHelp() {
    std::cout << "\n";
    PrintHeader("");
    std::cout << "              SNAPMAKER LIGHTBURN HOST - COMMAND LINE SHORTCUTS\n";
    PrintHeader("");
    std::cout << "\n";
    std::cout << "QUICK ACTIONS:\n";
    std::cout << "  /home               - Home the machine (G28 only)\n";
    std::cout << "  /lasersetup         - Complete laser setup (G28 + G90 + G54)\n";
    std::cout << "  /printersetup       - Printer/CNC setup (G28 + G90)\n";
    std::cout << "  /thickness          - Measure material thickness\n";
    std::cout << "  /capture            - Capture one image and show in camera feed\n";
    std::cout << "  /autocapture        - Auto-refresh capture (captures every second, ESC to stop)\n";
    std::cout << "  /enclosurefan       - Set enclosure fan speed (off, half, full, or 0-100)\n";
    std::cout << "  /enclosureled       - Set enclosure LED brightness (off, half, full, or 0-100)\n";
    std::cout << "  /lightburn          - Launch LightBurn application\n";
    std::cout << "  /camera             - Launch RTSP camera (VLC) in detached mode (stays open after exit)\n";
    std::cout << "  /camerasettings     - Configure RTSP camera (URL, auto-launch)\n";
    std::cout << "\n";
    std::cout << "FILE ACTION:\n";
    std::cout << "  /fileaction recycle - Move processed files to Recycle Bin (default, safe)\n";
    std::cout << "  /fileaction delete  - Permanently delete processed files (no recovery)\n";
    std::cout << "\n";
    std::cout << "NOTIFICATIONS:\n";
    std::cout << "  /beepon             - Enable sound alerts (print completion, filament runout, etc.)\n";
    std::cout << "  /beepoff            - Disable sound alerts\n";
    std::cout << "  /emailconfig        - Configure email settings (SMTP, recipient, app password)\n";
    std::cout << "  /emailon            - Enable email alerts for all events\n";
    std::cout << "  /emailoff           - Disable email alerts\n";
    std::cout << "\n";
    std::cout << "SYSTEM CHECKS:\n";
    std::cout << "  /softcamcheck       - Check if Softcam is properly installed\n";
    std::cout << "  /lightburncamera    - Check LightBurn camera settings\n";
    std::cout << "\n";
    std::cout << "INTERACTIVE PAGES:\n";
    std::cout << "  /status             - Show machine status page\n";
    std::cout << "  /kasa               - Kasa smart plug control menu\n";
    std::cout << "  /macros             - G-code macros menu\n";
    std::cout << "  /gcode              - Manual G-code entry\n";
    std::cout << "\n";
    std::cout << "FILE OPERATIONS:\n";
    std::cout << "  /upload <file>      - Upload file to Snapmaker (no auto-start)\n";
    std::cout << "  /print <file>       - Upload and auto-start print\n";
    std::cout << "  /stop               - Stop current print\n";
    std::cout << "  /pause              - Pause current print\n";
    std::cout << "  /resume             - Resume paused print\n";
    std::cout << "  /monitor            - Monitor print progress with live updates\n";
    std::cout << "\n";
    std::cout << "CONFIGURATION:\n";
    std::cout << "  /resetconfig        - Reset all configuration to defaults\n";
    std::cout << "  /resettoken         - Clear existing token and request a new one\n";
    std::cout << "  /connect <ip>       - Set Snapmaker IP (will prompt for token)\n";
    std::cout << "  /token <token>      - Set Snapmaker token only\n";
    std::cout << "  /kasaip <ip>        - Set Kasa smart plug IP\n";
    std::cout << "  /kasapoweronon      - Enable Kasa auto power-on when machine is offline\n";
    std::cout << "  /kasapoweronoff     - Disable Kasa auto power-on\n";
    std::cout << "  /kasapoweroffon     - Enable Kasa auto power-off after print finishes\n";
    std::cout << "  /kasapoweroffoff    - Disable Kasa auto power-off\n";
    std::cout << "  /serverlocalhost    - HTTP server binds to localhost (127.0.0.1) only\n";
    std::cout << "  /serverglobal       - HTTP server binds to all network interfaces (0.0.0.0)\n";
    std::cout << "  /webauth on/off     - Enable/disable HTTP authentication for remote clients (recommended on)\n";
    std::cout << "  /webuser <username> - Set HTTP basic auth username\n";
    std::cout << "  /webpassword <pass> - Set HTTP basic auth password\n";
    std::cout << "  /autostarton        - Enable auto-start print after file upload\n";
    std::cout << "  /autostartoff       - Disable auto-start print (upload only)\n";
    std::cout << "  /automonitoron      - Enable auto-monitor after auto-started print\n";
    std::cout << "  /automonitoroff     - Disable auto-monitor after auto-started print\n";
    std::cout << "  /fileaction recycle - Move processed files to Recycle Bin (safe)\n";
    std::cout << "  /fileaction delete  - Permanently delete processed files\n";
    std::cout << "\n";
    std::cout << "KASA DIRECT CONTROL:\n";
    std::cout << "  /kasaon             - Turn Kasa smart plug ON immediately\n";
    std::cout << "  /kasaoff            - Turn Kasa smart plug OFF immediately\n";
    std::cout << "\n";
    std::cout << "HTTP UPLOAD SERVER:\n";
    std::cout << "  /server             - Start HTTP server for receiving files from slicers\n";
    std::cout << "\n";
    std::cout << "SYSTEM TRAY:\n";
    std::cout << "  /tray               - Run in system tray mode (no console window)\n";
    std::cout << "\n";
    std::cout << "DOCUMENTATION:\n";
    std::cout << "  /about              - Open README.txt documentation\n";
    std::cout << "\n";
    std::cout << "COMBINED CONFIGURATION:\n";
    std::cout << "  /connect <ip> /token <token> /kasaip <ip> /kasapoweron /kasapoweroffon /autostarton /beepon /emailon /fileaction recycle\n";
    std::cout << "      - Set all configuration at once\n";
    std::cout << "\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  SnapmakerLightBurnHost.exe /home\n";
    std::cout << "  SnapmakerLightBurnHost.exe /enclosurefan half\n";
    std::cout << "  SnapmakerLightBurnHost.exe /enclosureled 75\n";
    std::cout << "  SnapmakerLightBurnHost.exe /upload model.gcode\n";
    std::cout << "  SnapmakerLightBurnHost.exe /camera\n";
    std::cout << "  SnapmakerLightBurnHost.exe /emailconfig\n";
    std::cout << "  SnapmakerLightBurnHost.exe /fileaction delete\n";
    std::cout << "  SnapmakerLightBurnHost.exe /connect 192.168.1.100 /token abc123 /kasaip 192.168.1.200 /kasapoweron /kasapoweroffon /autostarton /beepon /emailon /fileaction recycle\n";
    std::cout << "\n";
    PrintHeader("");
}

void ExecuteShortcut(const ShortcutParams& params) {
    try {
        if (params.action == ShortcutAction::HELP) {
            DisplayHelp();
            std::cout << "\nPress any key to exit...\n";
            (void)_getch();
            return;
        }

        bool needsMachine = (params.action != ShortcutAction::RESETCONFIG &&
            params.action != ShortcutAction::RESETTOKEN &&
            params.action != ShortcutAction::SETKASAIP &&
            params.action != ShortcutAction::SETKASAAUTO &&
            params.action != ShortcutAction::SETAUTOSTART &&
            params.action != ShortcutAction::SETFILEACTION &&
            params.action != ShortcutAction::KASA_ON &&
            params.action != ShortcutAction::KASA_OFF &&
            params.action != ShortcutAction::UPLOAD &&
            params.action != ShortcutAction::PRINT &&
            params.action != ShortcutAction::SOFTCAMCHECK &&
            params.action != ShortcutAction::LIGHTBURN &&
            params.action != ShortcutAction::LIGHTBURNCAMERA &&
            params.action != ShortcutAction::BEEP_ON &&
            params.action != ShortcutAction::BEEP_OFF &&
            params.action != ShortcutAction::EMAILCONFIG &&
            params.action != ShortcutAction::EMAIL_ON &&
            params.action != ShortcutAction::EMAIL_OFF &&
            params.action != ShortcutAction::ENCLOSURELED &&
            params.action != ShortcutAction::HELP &&
            params.action != ShortcutAction::SERVER_BIND_LOCALHOST &&
            params.action != ShortcutAction::SERVER_BIND_GLOBAL);

        if (needsMachine) {
            UserConfig cfg = HostContext::instance().snapshotConfig();
            if (cfg.ipAddress.empty() || cfg.token.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No valid configuration found.\n";
                std::cout << "Please run the program normally first to set up connection,\n";
                std::cout << "or use /connect to set IP and token.\n";
                std::cout << "\nPress any key to exit...\n";
                (void)_getch();
                return;
            }
        }

        switch (params.action) {
        case ShortcutAction::HOME:
        {
            PrintHeader("HOMING MACHINE");
            HomeMachine();
        }
        break;

        case ShortcutAction::LASERSETUP:
        {
            PrintHeader("LASER SETUP");
            PerformLaserSetup();
        }
        break;

        case ShortcutAction::PRINTERSETUP:
        {
            PrintHeader("PRINTER SETUP");
            PerformPrintSetup();
        }
        break;

        case ShortcutAction::STATUS:
        {
            ClearConsole();
            DisplayMachineStatusPage(true);
        }
        break;

        case ShortcutAction::KASA:
        {
            ClearConsole();
            KasaModeGuard guard;
            KasaInteractiveMode(true);
        }
        break;

        case ShortcutAction::KASA_ON:
        {
            PrintHeader("KASA PLUG - TURN ON");

            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.kasa.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No Kasa IP address configured.\n";
                std::cout << "Please set Kasa IP first using: /kasaip <ip>\n";
                break;
            }

            if (!IsValidIPAddress(cfg.kasa.ipAddress)) {
                std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address: " << cfg.kasa.ipAddress << "\n";
                std::cout << "Please update using: /kasaip <ip>\n";
                break;
            }

            std::cout << GetTimeStamp() << "Kasa IP: " << cfg.kasa.ipAddress << "\n";
            std::cout << GetTimeStamp() << "Turning plug ON...\n";

            if (SetKasaPlugState(cfg.kasa.ipAddress, true)) {
                std::cout << GetTimeStamp() << "[OK] Plug turned ON successfully!\n";

                std::this_thread::sleep_for(std::chrono::milliseconds(KASA_WAIT_FOR_POWER_SWITCH_MILLISECONDS));
                bool isOn = false;
                if (CheckKasaPlugStatus(cfg.kasa.ipAddress, isOn)) {
                    std::cout << GetTimeStamp() << "[OK] Verified: Plug is " << (isOn ? "ON" : "OFF") << "\n";
                }
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Failed to turn on plug.\n";
                std::cout << "Check that hs100.exe is in the correct location and the IP is correct.\n";
            }
        }
        break;

        case ShortcutAction::KASA_OFF:
        {
            PrintHeader("KASA PLUG - TURN OFF");

            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.kasa.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No Kasa IP address configured.\n";
                std::cout << "Please set Kasa IP first using: /kasaip <ip>\n";
                break;
            }

            if (!IsValidIPAddress(cfg.kasa.ipAddress)) {
                std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP address: " << cfg.kasa.ipAddress << "\n";
                std::cout << "Please update using: /kasaip <ip>\n";
                break;
            }

            std::cout << GetTimeStamp() << "Kasa IP: " << cfg.kasa.ipAddress << "\n";
            std::cout << GetTimeStamp() << "Turning plug OFF...\n";

            if (SetKasaPlugState(cfg.kasa.ipAddress, false)) {
                std::cout << GetTimeStamp() << "[OK] Plug turned OFF successfully!\n";

                std::this_thread::sleep_for(std::chrono::milliseconds(KASA_WAIT_FOR_POWER_SWITCH_MILLISECONDS));
                bool isOn = false;
                if (CheckKasaPlugStatus(cfg.kasa.ipAddress, isOn)) {
                    std::cout << GetTimeStamp() << "[OK] Verified: Plug is " << (isOn ? "ON" : "OFF") << "\n";
                }
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Failed to turn off plug.\n";
                std::cout << "Check that hs100.exe is in the correct location and the IP is correct.\n";
            }
        }
        break;

        case ShortcutAction::MACROS:
        {
            ClearConsole();
            SendCustomGCodeMenu(true);
        }
        break;

        case ShortcutAction::GCODE:
        {
            ClearConsole();
            PrintHeader("MANUAL G-CODE");
            SendManualGCode();
        }
        break;

        case ShortcutAction::THICKNESS:
        {
            PrintHeader("MATERIAL THICKNESS");

            if (!CheckMachineHomedAndWarn() || !CheckLaserInstalledAndWarn()) {
                break;
            }

            double dummyThickness = 0.0;
            GetMaterialThicknessFromSnapmaker(dummyThickness);
        }
        break;

        case ShortcutAction::CAPTURE:
        {
            PrintHeader("CAPTURE IMAGE");

            if (!CheckMachineHomedAndWarn() || !CheckLaserInstalledAndWarn()) {
                break;
            }

            CameraSession cam(CAMERA_WIDTH, CAMERA_HEIGHT, DEFAULT_FPS);
            if (!cam) {
                std::cout << GetTimeStamp() << "[ERR] Failed to initialize camera.\n";
                break;
            }

            ImageBuffer testImg = CreateEnhancedTestPattern(CAMERA_WIDTH, CAMERA_HEIGHT);
            if (testImg) {
                cam.sendFrame(testImg.get());
            }

            ImageBuffer capturedImg;
            int w = CAMERA_WIDTH, h = CAMERA_HEIGHT;
            if (!CaptureAndUpdateImage("latest.jpg", capturedImg, w, h, false)) {
                std::cout << GetTimeStamp() << "[ERR] Image capture failed.\n";
                break;
            }

            std::cout << GetTimeStamp() << "[OK] Image captured and saved as 'latest.jpg'\n";

            if (capturedImg) {
                UserConfig cfg = HostContext::instance().snapshotConfig();
                if (cfg.adjustment.useAdjustment && (cfg.adjustment.pixelsX != 0 || cfg.adjustment.pixelsY != 0)) {
                    ImageBuffer adjusted = ApplyImageAdjustment(capturedImg.get(), w, h, 3,
                        cfg.adjustment.pixelsX, cfg.adjustment.pixelsY);
                    if (adjusted) {
                        cam.sendFrame(adjusted.get());
                    }
                    else {
                        cam.sendFrame(capturedImg.get());
                    }
                }
                else {
                    cam.sendFrame(capturedImg.get());
                }
            }

            std::cout << GetTimeStamp() << "[INFO] Press any key to stop feed and exit...\n";

            auto lastFrameTime = std::chrono::steady_clock::now();
            bool exitCapture = false;

            while (!exitCapture && IsRunning()) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();

                if (elapsedMs >= (1000 / DEFAULT_FPS)) {
                    if (capturedImg) {
                        UserConfig cfg = HostContext::instance().snapshotConfig();
                        if (cfg.adjustment.useAdjustment && (cfg.adjustment.pixelsX != 0 || cfg.adjustment.pixelsY != 0)) {
                            ImageBuffer adjusted = ApplyImageAdjustment(capturedImg.get(), w, h, 3,
                                cfg.adjustment.pixelsX, cfg.adjustment.pixelsY);
                            if (adjusted) cam.sendFrame(adjusted.get());
                        }
                        else {
                            cam.sendFrame(capturedImg.get());
                        }
                    }
                    lastFrameTime = now;
                }

                if (_kbhit()) {
                    (void)_getch();
                    exitCapture = true;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
            }

            std::cout << GetTimeStamp() << "[INFO] Feed stopped. Exiting...\n";
        }
        break;

        case ShortcutAction::AUTOCAPTURE:
        {
            PrintHeader("AUTO REFRESH CAPTURE");

            if (!CheckMachineHomedAndWarn() || !CheckLaserInstalledAndWarn()) {
                break;
            }

            std::cout << GetTimeStamp() << "[INFO] Auto-refresh capture mode started.\n";
            std::cout << GetTimeStamp() << "[INFO] Capturing and updating camera feed every " << AUTO_REFRESH_INTERVAL_SECONDS << " second(s).\n";
            std::cout << GetTimeStamp() << "[INFO] Press ESC to stop and exit.\n\n";

            CameraSession cam(CAMERA_WIDTH, CAMERA_HEIGHT, DEFAULT_FPS);
            if (!cam) {
                std::cout << GetTimeStamp() << "[ERR] Failed to initialize camera.\n";
                break;
            }

            ImageBuffer currentImg = CreateEnhancedTestPattern(CAMERA_WIDTH, CAMERA_HEIGHT);
            if (currentImg) {
                cam.sendFrame(currentImg.get());
            }

            int w = CAMERA_WIDTH, h = CAMERA_HEIGHT;
            int channels = 3;
            auto lastCapture = std::chrono::steady_clock::now();
            auto lastFrame = std::chrono::steady_clock::now();
            bool running = true;
            int captureCount = 0;

            while (running && IsRunning()) {
                auto now = std::chrono::steady_clock::now();

                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame).count() >= (1000 / DEFAULT_FPS)) {
                    if (currentImg) {
                        UserConfig cfg = HostContext::instance().snapshotConfig();
                        if (cfg.adjustment.useAdjustment && (cfg.adjustment.pixelsX != 0 || cfg.adjustment.pixelsY != 0)) {
                            ImageBuffer adjusted = ApplyImageAdjustment(currentImg.get(), w, h, channels,
                                cfg.adjustment.pixelsX, cfg.adjustment.pixelsY);
                            if (adjusted) cam.sendFrame(adjusted.get());
                        }
                        else {
                            cam.sendFrame(currentImg.get());
                        }
                    }
                    lastFrame = now;
                }

                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCapture).count() >= AUTO_REFRESH_INTERVAL_SECONDS) {
                    captureCount++;
                    std::cout << GetTimeStamp() << "[CAPTURE " << captureCount << "] ";
                    std::cout.flush();

                    if (CaptureAndUpdateImage("latest.jpg", currentImg, w, h, false)) {
                        channels = 3;
                        std::cout << "[OK] Image updated.\n";
                    }
                    else {
                        std::cout << "[FAIL] Capture failed.\n";
                    }
                    lastCapture = now;
                }

                if (_kbhit()) {
                    int key = _getch();
                    if (key == ESC_ASCII) {
                        running = false;
                        std::cout << GetTimeStamp() << "[INFO] Stopped by user.\n";
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
            }

            std::cout << GetTimeStamp() << "[INFO] Total captures: " << captureCount << "\n";
            std::cout << GetTimeStamp() << "[INFO] Auto-refresh capture mode ended.\n";
        }
        break;

        case ShortcutAction::ENCLOSUREFAN:
        {
            PrintHeader("ENCLOSURE FAN CONTROL");

            if (!params.hasParameter || params.parameter.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No speed specified.\n";
                std::cout << "Usage: /enclosurefan <speed>\n";
                std::cout << "  /enclosurefan off   - Turn OFF\n";
                std::cout << "  /enclosurefan half  - Half speed (50%)\n";
                std::cout << "  /enclosurefan full  - Full speed (100%)\n";
                std::cout << "  /enclosurefan 0-100 - Set specific percentage\n";
                break;
            }

            MachineStatusData status = GetMachineStatus(false);
            if (IsMachineOffline(status)) {
                std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot control enclosure fan.\n";
                break;
            }

            if (!status.enclosure) {
                std::cout << GetTimeStamp() << "[WARN] Enclosure module not detected.\n";
                std::cout << GetTimeStamp() << "The command will still be sent, but may not work.\n";
            }

            auto [success, msg] = SetEnclosureFanSpeed(params.parameter);
            if (success) {
                std::cout << GetTimeStamp() << "[OK] " << msg << "\n";
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] " << msg << "\n";
            }
            break;
        }

        case ShortcutAction::ENCLOSURELED:
        {
            PrintHeader("ENCLOSURE LED CONTROL");

            if (!params.hasParameter || params.parameter.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No brightness specified.\n";
                std::cout << "Usage: /enclosureled <brightness>\n";
                std::cout << "  /enclosureled off   - Turn OFF\n";
                std::cout << "  /enclosureled half  - Half brightness (50%)\n";
                std::cout << "  /enclosureled full  - Full brightness (100%)\n";
                std::cout << "  /enclosureled 0-100 - Set specific percentage\n";
                break;
            }

            MachineStatusData status = GetMachineStatus(false);
            if (IsMachineOffline(status)) {
                std::cout << GetTimeStamp() << "[ERR] Machine is offline. Cannot control enclosure LED.\n";
                break;
            }

            if (!status.enclosure) {
                std::cout << GetTimeStamp() << "[WARN] Enclosure module not detected.\n";
                std::cout << GetTimeStamp() << "The command will still be sent, but may not work.\n";
            }

            auto [success, msg] = SetEnclosureLed(params.parameter);
            if (success) {
                std::cout << GetTimeStamp() << "[OK] " << msg << "\n";
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] " << msg << "\n";
            }
            break;
        }

        case ShortcutAction::SOFTCAMCHECK:
        {
            PrintHeader("SOFTCAM CHECK");

            if (CheckSoftcamRegistry()) {
                std::cout << GetTimeStamp() << "[OK] Softcam is properly installed.\n";
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Softcam is NOT installed!\n";
            }
        }
        break;

        case ShortcutAction::LIGHTBURN:
        {
            PrintHeader("LIGHTBURN");
            LaunchLightBurn();
        }
        break;

        case ShortcutAction::LIGHTBURNCAMERA:
        {
            PrintHeader("LIGHTBURN CAMERA SETTINGS");
            CheckLightBurnCameraSetting();
        }
        break;

        case ShortcutAction::BEEP_ON:
        {
            PrintHeader("SOUND ALERTS - ENABLE");

            HostContext::instance().updateConfig([](UserConfig& c) {
                c.enableSoundAlerts = true;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] Sound alerts ENABLED.\n";
            std::cout << GetTimeStamp() << "[INFO] Sounds will play for print completion, filament runout, and other alerts.\n";
            break;
        }

        case ShortcutAction::BEEP_OFF:
        {
            PrintHeader("SOUND ALERTS - DISABLE");

            HostContext::instance().updateConfig([](UserConfig& c) {
                c.enableSoundAlerts = false;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] Sound alerts DISABLED.\n";
            break;
        }

        case ShortcutAction::EMAILCONFIG:
        {
            ConfigureEmailSettings();
            break;
        }

        case ShortcutAction::EMAIL_ON:
        {
            PrintHeader("EMAIL NOTIFICATIONS - ENABLE");

            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.emailRecipient.empty() || cfg.emailSender.empty() || cfg.emailAppPassword.empty()) {
                std::cout << GetTimeStamp() << "[ERR] Email not fully configured.\n";
                std::cout << GetTimeStamp() << "[INFO] Please run /emailconfig first to set up email settings.\n";
                break;
            }

            HostContext::instance().updateConfig([](UserConfig& c) {
                c.enableEmailAlerts = true;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] Email notifications ENABLED.\n";
            std::cout << GetTimeStamp() << "[INFO] Emails will be sent for print completion, filament runout, and other alerts.\n";
            break;
        }

        case ShortcutAction::EMAIL_OFF:
        {
            PrintHeader("EMAIL NOTIFICATIONS - DISABLE");

            HostContext::instance().updateConfig([](UserConfig& c) {
                c.enableEmailAlerts = false;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] Email notifications DISABLED.\n";
            break;
        }

        case ShortcutAction::SETFILEACTION:
        {
            PrintHeader("FILE ACTION SETTING");

            UserConfig cfg = HostContext::instance().snapshotConfig();
            std::string currentAction = cfg.fileAction.empty() ? "recycle" : cfg.fileAction;

            std::cout << GetTimeStamp() << "Current file action: ";
            if (currentAction == "recycle") {
                std::cout << "Move to Recycle Bin (SAFE - files can be recovered)" << std::endl;
            }
            else {
                std::cout << "Permanently Delete (DANGEROUS - files cannot be recovered)" << std::endl;
            }

            if (!params.hasParameter || params.parameter.empty()) {
                std::cout << GetTimeStamp() << "[INFO] No action specified. Usage: /fileaction recycle|delete\n";
                std::cout << "  /fileaction recycle - Move processed files to Recycle Bin (safe, default)\n";
                std::cout << "  /fileaction delete  - Permanently delete processed files (dangerous)\n";
                break;
            }

            std::string newAction = params.parameter;
            std::cout << GetTimeStamp() << "Setting file action to: ";
            if (newAction == "recycle") {
                std::cout << "Recycle Bin (SAFE - files can be recovered)" << std::endl;
            }
            else {
                std::cout << "Permanent Delete (DANGEROUS - files cannot be recovered)" << std::endl;
            }

            HostContext::instance().updateConfig([newAction](UserConfig& c) {
                c.fileAction = newAction;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] File action setting saved.\n";

            if (newAction == "recycle") {
                std::cout << "\n[INFO] When set to Recycle Bin:\n";
                std::cout << "  - Processed files are moved to the Windows Recycle Bin\n";
                std::cout << "  - Files can be restored if deleted by mistake\n";
                std::cout << "  - This is the SAFE default setting\n";
            }
            else {
                std::cout << "\n[WARNING] When set to Permanent Delete:\n";
                std::cout << "  - Processed files are PERMANENTLY DELETED\n";
                std::cout << "  - Files CANNOT BE RECOVERED\n";
                std::cout << "  - Use this option only if you are sure\n";
            }
            break;
        }

        case ShortcutAction::RESETCONFIG:
        {
            PrintHeader("RESET CONFIGURATION");
            fs::path configPath = HostContext::instance().getConfigPath();

            if (fs::exists(configPath)) {
                fs::path backupPath = configPath;
                backupPath.replace_extension(".json.old");
                try {
                    fs::copy(configPath, backupPath, fs::copy_options::overwrite_existing);
                    std::cout << GetTimeStamp() << "[OK] Backup created: " << backupPath.filename().string() << "\n";

                    if (fs::remove(configPath)) {
                        std::cout << GetTimeStamp() << "[OK] Configuration reset to defaults.\n";
                        std::cout << GetTimeStamp() << "[INFO] Please run the program normally to reconfigure.\n";
                    }
                    else {
                        std::cout << GetTimeStamp() << "[ERR] Failed to delete config file.\n";
                    }
                }
                catch (const std::exception& e) {
                    std::cout << GetTimeStamp() << "[ERR] Failed to backup config: " << e.what() << "\n";
                }
            }
            else {
                std::cout << GetTimeStamp() << "[INFO] No config file found to reset.\n";
            }

            HostContext::instance().updateConfig([](UserConfig& cfg) {
                cfg = UserConfig();
                return true;
                });
        }
        break;

        case ShortcutAction::RESETTOKEN:
        {
            PrintHeader("RESET TOKEN");

            UserConfig fileConfig;
            fs::path configPath = HostContext::instance().getConfigPath();
            bool hasFileConfig = LoadConfig(configPath.string(), fileConfig);

            UserConfig lubanConfig;
            bool hasLubanConfig = LoadLuban(lubanConfig);

            UserConfig cfg = HostContext::instance().snapshotConfig();

            if (cfg.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "[ERR] No Snapmaker IP address configured.\n";
                std::cout << "Please set IP first using: /connect <ip>\n";
                break;
            }

            std::cout << GetTimeStamp() << "Current Snapmaker IP: " << cfg.ipAddress << "\n";

            std::cout << "\nToken Sources:\n";
            bool hasFileToken = (hasFileConfig && !fileConfig.token.empty() && IsValidIPAddress(fileConfig.ipAddress));
            if (hasFileToken) {
                std::cout << "  " << CONFIG_FILE << " token: " << fileConfig.token << "\n";
            }
            else {
                std::cout << "  " << CONFIG_FILE << " token: [NOT SET or INVALID]\n";
            }

            bool hasLubanToken = (hasLubanConfig && !lubanConfig.token.empty());
            if (hasLubanToken) {
                std::cout << "  Luban token:       " << lubanConfig.token << "\n";
            }
            else {
                std::cout << "  Luban token:       [NOT FOUND]\n";
            }

            std::cout << "  In-memory token:   " << (cfg.token.empty() ? "[NOT SET]" : cfg.token) << "\n";
            std::cout << GetTimeStamp() << "Checking if machine is reachable at " << cfg.ipAddress << "...\n";
            std::string testUrl = BuildConnectUrl(cfg.ipAddress);
            std::string testResponse;
            auto [testRes, testCode] = PerformHttpGetWithCode(testUrl, testResponse, 5);

            bool machineWasOffline = false;
            bool hasWorkingToken = false;
            std::string workingTokenSource;
            std::string workingTokenValue;

            if (testRes != CURLE_OK) {
                machineWasOffline = true;
                std::cout << GetTimeStamp() << "[WARN] Machine not reachable at " << cfg.ipAddress << "\n";
                std::cout << "Please check:\n";
                std::cout << "  - Machine is powered on\n";
                std::cout << "  - Network connection\n";
                std::cout << "  - IP address is correct\n";

                if (cfg.kasa.autoPowerOn && !cfg.kasa.ipAddress.empty()) {
                    std::cout << GetTimeStamp() << "[INFO] Attempting auto power-on via Kasa...\n";
                    if (AutoPowerOnKasaIfNeeded()) {
                        std::cout << GetTimeStamp() << "[OK] Machine powered on. Waiting for API to be ready...\n";
                        std::this_thread::sleep_for(std::chrono::seconds(10));

                        auto [retryRes, retryCode] = PerformHttpGetWithCode(testUrl, testResponse, 10);
                        if (retryRes != CURLE_OK) {
                            std::cout << GetTimeStamp() << "[ERR] Still cannot reach machine API.\n";
                            std::cout << GetTimeStamp() << "[INFO] The machine may still be booting. Please wait and try again.\n";
                            break;
                        }
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }

            std::cout << GetTimeStamp() << "[OK] Machine is reachable.\n";

            if (hasFileToken) {
                HostContext::instance().updateConfig([fileConfig](UserConfig& c) {
                    c.token = fileConfig.token;
                    return true;
                    });

                MachineStatusData status = GetMachineStatus(false);
                if (!IsMachineOffline(status)) {
                    hasWorkingToken = true;
                    workingTokenSource = CONFIG_FILE;
                    workingTokenValue = fileConfig.token;
                    std::cout << GetTimeStamp() << "[OK] Token from " << CONFIG_FILE << " is working!\n";
                    std::cout << GetTimeStamp() << "  Status: " << status.getStatusDisplay() << "\n";
                    std::cout << GetTimeStamp() << "  Tool head: " << status.getToolHeadDisplay() << "\n";
                }

                HostContext::instance().updateConfig([cfg](UserConfig& c) {
                    c = cfg;
                    return true;
                    });
            }

            bool lubanTokenValid = false;
            if (!hasWorkingToken && hasLubanToken) {
                HostContext::instance().updateConfig([lubanConfig](UserConfig& c) {
                    c.token = lubanConfig.token;
                    return true;
                    });

                MachineStatusData status = GetMachineStatus(false);
                if (!IsMachineOffline(status)) {
                    lubanTokenValid = true;
                    std::cout << GetTimeStamp() << "[OK] Token from Luban is working!\n";
                    std::cout << GetTimeStamp() << "  Status: " << status.getStatusDisplay() << "\n";
                    std::cout << GetTimeStamp() << "  Tool head: " << status.getToolHeadDisplay() << "\n";
                }

                HostContext::instance().updateConfig([cfg](UserConfig& c) {
                    c = cfg;
                    return true;
                    });
            }

            if (lubanTokenValid && !hasFileToken) {
                std::cout << GetTimeStamp() << "[INFO] Luban has a valid token but " << CONFIG_FILE << " does not.\n";
                std::cout << "Do you want to save the Luban token to " << CONFIG_FILE << "? (Y/n): ";

                int ch = _getch();
                std::cout << std::endl;
                if (ch != 'n' && ch != 'N') {
                    try {
                        std::ifstream inFile(configPath.string());
                        json j;
                        if (inFile.is_open()) {
                            j = json::parse(inFile);
                            inFile.close();
                        }

                        j["token"] = lubanConfig.token;
                        j["ipAddress"] = cfg.ipAddress;

                        std::ofstream outFile(configPath.string());
                        if (outFile.is_open()) {
                            outFile << std::setw(4) << j << std::endl;
                            outFile.close();
                            std::cout << GetTimeStamp() << "[OK] Luban token saved to " << CONFIG_FILE << "!\n";

                            HostContext::instance().updateConfig([lubanConfig](UserConfig& c) {
                                c.token = lubanConfig.token;
                                return true;
                                });
                        }
                        else {
                            std::cout << GetTimeStamp() << "[ERR] Could not save token to " << CONFIG_FILE << ".\n";
                        }
                    }
                    catch (const std::exception& e) {
                        std::cout << GetTimeStamp() << "[ERR] Failed to save token: " << e.what() << "\n";
                    }
                    break;
                }
                else {
                    std::cout << GetTimeStamp() << "[INFO] Skipped saving Luban token to " << CONFIG_FILE << ".\n";
                }
            }

            if (hasWorkingToken && !machineWasOffline) {
                std::cout << GetTimeStamp() << "[INFO] A valid token is already working from: " << workingTokenSource << ".\n";
                std::cout << "Do you still want to request a new token? (y/N): ";

                int ch = _getch();
                std::cout << std::endl;
                if (ch != 'y' && ch != 'Y') {
                    std::cout << GetTimeStamp() << "[INFO] Token reset cancelled. Existing token kept.\n";
                    break;
                }
            }
            else if (!hasWorkingToken && hasFileToken) {
                std::cout << GetTimeStamp() << "[WARN] Token in " << CONFIG_FILE << " is NOT working.\n";
                std::cout << "Do you want to clear it and request a new token? (Y/n): ";

                int ch = _getch();
                std::cout << std::endl;
                if (ch == 'n' || ch == 'N') {
                    std::cout << GetTimeStamp() << "[INFO] Token reset cancelled.\n";
                    break;
                }
            }
            else if (!hasWorkingToken && !hasFileToken && !lubanTokenValid) {
                std::cout << GetTimeStamp() << "[INFO] No working token found.\n";
                std::cout << "Do you want to request a new token? (Y/n): ";

                int ch = _getch();
                std::cout << std::endl;
                if (ch == 'n' || ch == 'N') {
                    std::cout << GetTimeStamp() << "[INFO] Token request cancelled.\n";
                    break;
                }
            }

            std::cout << GetTimeStamp() << "Clearing token from " << CONFIG_FILE << "...\n";

            if (fs::exists(configPath)) {
                try {
                    std::ifstream inFile(configPath.string());
                    json j;
                    if (inFile.is_open()) {
                        j = json::parse(inFile);
                        inFile.close();
                    }

                    if (j.contains("token")) {
                        j.erase("token");
                    }
                    j["token"] = "";

                    std::ofstream outFile(configPath.string());
                    if (outFile.is_open()) {
                        outFile << std::setw(4) << j << std::endl;
                        outFile.close();
                        std::cout << GetTimeStamp() << "[OK] Token cleared from " << CONFIG_FILE << ".\n";
                    }
                    else {
                        std::cout << GetTimeStamp() << "[ERR] Could not write to " << CONFIG_FILE << ".\n";
                    }
                }
                catch (const std::exception& e) {
                    std::cout << GetTimeStamp() << "[ERR] Failed to update " << CONFIG_FILE << ": " << e.what() << "\n";
                }
            }

            HostContext::instance().updateConfig([](UserConfig& c) {
                c.token.clear();
                return true;
                });

            std::cout << "\n" << GetTimeStamp() << "[ACTION REQUIRED] Please tap 'Yes' on the Snapmaker touchscreen.\n";
            std::cout << GetTimeStamp() << "Requesting new token (timeout: " << CONFIRMATION_TIMEOUT_SECONDS << " seconds)...\n";

            std::this_thread::sleep_for(std::chrono::seconds(2));

            std::string connectUrl = BuildConnectUrl(cfg.ipAddress);
            std::string response;
            auto [res, code] = PerformHttpPostWithCode(connectUrl, "", response, 10);

            if (res != CURLE_OK || code != 200 || response.empty()) {
                std::cout << GetTimeStamp() << "[ERR] Failed to initiate connection request.\n";
                std::cout << GetTimeStamp() << "[INFO] Make sure you tapped 'Yes' on the touchscreen.\n";
                std::cout << GetTimeStamp() << "[INFO] You can try again with: /resettoken\n";
                break;
            }

            std::string newToken;
            try {
                auto data = json::parse(response);
                if (data.contains("token") && data["token"].is_string() && !data["token"].empty()) {
                    newToken = data["token"];
                    std::cout << GetTimeStamp() << "[OK] Connection initiated. Waiting for confirmation...\n";
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] No token in response.\n";
                    break;
                }
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[ERR] Failed to parse response: " << e.what() << "\n";
                break;
            }

            bool confirmed = false;
            auto startTime = std::chrono::steady_clock::now();
            int spinnerCount = 0;
            const char spinner[] = { '|', '/', '-', '\\' };

            while (!confirmed && IsRunning()) {
                auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count();

                if (elapsedSeconds >= CONFIRMATION_TIMEOUT_SECONDS) {
                    std::cout << "\n" << GetTimeStamp() << "[TIMEOUT] No confirmation within "
                        << CONFIRMATION_TIMEOUT_SECONDS << " seconds.\n";
                    std::cout << GetTimeStamp() << "[INFO] Make sure you tap 'Yes' on the Snapmaker touchscreen.\n";
                    break;
                }

                std::cout << "\r" << GetTimeStamp() << "Waiting for confirmation... Click 'Yes' on the touch controller. "
                    << spinner[spinnerCount % 4] << " (" << elapsedSeconds << "/"
                    << CONFIRMATION_TIMEOUT_SECONDS << "s)   (ESC to cancel)   " << std::flush;
                spinnerCount++;

                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == ESC_ASCII) {
                        std::cout << "\n" << GetTimeStamp() << "[INFO] Cancelled by user.\n";
                        return;
                    }
                }

                std::string statusUrl = BuildStatusUrl(cfg.ipAddress, newToken);
                std::string statusResponse;
                auto [statusRes, statusCode] = PerformHttpGetWithCode(statusUrl, statusResponse, 3);

                if (statusRes == CURLE_OK && statusCode == 200) {
                    confirmed = true;
                    std::cout << "\n" << GetTimeStamp() << "[OK] Connection confirmed!\n";
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            if (!confirmed) {
                std::cout << GetTimeStamp() << "[ERR] Could not confirm connection.\n";
                break;
            }

            try {
                std::ifstream inFile(configPath.string());
                json j;
                if (inFile.is_open()) {
                    j = json::parse(inFile);
                    inFile.close();
                }

                j["token"] = newToken;

                std::ofstream outFile(configPath.string());
                if (outFile.is_open()) {
                    outFile << std::setw(4) << j << std::endl;
                    outFile.close();
                    std::cout << GetTimeStamp() << "[OK] New token saved to " << CONFIG_FILE << "!\n";
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] Could not save token to " << CONFIG_FILE << ".\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[ERR] Failed to save token: " << e.what() << "\n";
            }

            HostContext::instance().updateConfig([newToken](UserConfig& c) {
                c.token = newToken;
                return true;
                });

            std::cout << GetTimeStamp() << "Token: " << newToken << "\n";

            std::cout << GetTimeStamp() << "Verifying new token...\n";
            MachineStatusData status = GetMachineStatus(false);
            if (!IsMachineOffline(status)) {
                std::cout << GetTimeStamp() << "[OK] Token is valid!\n";
                std::cout << "  Machine Status: " << status.getStatusDisplay() << "\n";
                std::cout << "  Tool Head: " << status.getToolHeadDisplay() << "\n";
            }
            else {
                std::cout << GetTimeStamp() << "[WARN] Token verification failed. Please try again.\n";
            }
        }
        break;

        case ShortcutAction::SERVER_BIND_LOCALHOST:
        {
            PrintHeader("SERVER BIND - LOCALHOST ONLY");
            HostContext::instance().updateConfig([](UserConfig& c) {
                c.bindLocalhost = true;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Server will now bind to localhost (127.0.0.1) only.\n";
            std::cout << GetTimeStamp() << "[INFO] This setting takes effect the next time you start the server with /server.\n";
            break;
        }
        case ShortcutAction::SERVER_BIND_GLOBAL:
        {
            PrintHeader("SERVER BIND - ALL INTERFACES");
            HostContext::instance().updateConfig([](UserConfig& c) {
                c.bindLocalhost = false;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Server will now bind to all network interfaces (0.0.0.0).\n";
            std::cout << GetTimeStamp() << "[INFO] This setting takes effect the next time you start the server with /server.\n";
            break;
        }

        case ShortcutAction::SETWEBAUTH: {
            PrintHeader("WEB AUTHENTICATION");
            bool newValue = params.autoPowerOn;
            HostContext::instance().updateConfig([newValue](UserConfig& c) {
                c.webAuthEnabled = newValue;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Web authentication " << (newValue ? "ENABLED" : "DISABLED") << std::endl;
            if (!newValue) {
                std::cout << GetTimeStamp() << "[WARNING] Disabling authentication is dangerous!" << std::endl;
                std::cout << GetTimeStamp() << "[WARNING] Anyone on your network can control your machine." << std::endl;
            }
            break;
        }
        case ShortcutAction::SETWEBUSER: {
            PrintHeader("WEB USERNAME");
            std::string newUser = params.parameter;
            HostContext::instance().updateConfig([newUser](UserConfig& c) {
                c.webUser = newUser;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Web username set to: " << newUser << std::endl;
            break;
        }
        case ShortcutAction::SETWEBPASSWORD: {
            PrintHeader("WEB PASSWORD");
            std::string newPass = params.parameter;
            HostContext::instance().updateConfig([newPass](UserConfig& c) {
                c.webPassword = newPass;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Web password updated." << std::endl;
            break;
        }

        case ShortcutAction::SETAUTOSTART:
        {
            PrintHeader("AUTO-START PRINT SETTING");
            UserConfig cfg = HostContext::instance().snapshotConfig();

            std::cout << GetTimeStamp() << "Current Auto-Start Print setting: "
                << (cfg.autoStartPrint ? "ON" : "OFF") << "\n";

            bool newValue = params.autoPowerOn;
            std::cout << GetTimeStamp() << "Setting Auto-Start Print to: "
                << (newValue ? "ON" : "OFF") << "\n";

            HostContext::instance().updateConfig([newValue](UserConfig& c) {
                c.autoStartPrint = newValue;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());

            std::cout << GetTimeStamp() << "[OK] Auto-Start Print setting saved.\n";

            if (newValue) {
                std::cout << "\n[INFO] When Auto-Start is ON:\n";
                std::cout << "  - Dragging a file onto the .exe will AUTO-START printing\n";
                std::cout << "  - Press Shift within 3 seconds to override to UPLOAD-ONLY\n";
            }
            else {
                std::cout << "\n[INFO] When Auto-Start is OFF:\n";
                std::cout << "  - Dragging a file onto the .exe will UPLOAD-ONLY\n";
                std::cout << "  - Press Shift within 3 seconds to override to AUTO-START\n";
            }
            std::cout << "\n" << GetTimeStamp() << "You can also toggle this in the main UI with the 'P' key.\n";
        }
        break;

        case ShortcutAction::AUTOMONITOR_ON:
        {
            PrintHeader("AUTO-MONITOR PRINT SETTING - ENABLE");
            HostContext::instance().updateConfig([](UserConfig& c) {
                c.autoMonitorPrint = true;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Auto-monitor print ENABLED. The console monitor window will appear after auto-started prints.\n";
            std::cout << GetTimeStamp() << "[INFO] Press SHIFT within 3 seconds at print start to skip monitoring.\n";
            break;
        }

        case ShortcutAction::AUTOMONITOR_OFF:
        {
            PrintHeader("AUTO-MONITOR PRINT SETTING - DISABLE");
            HostContext::instance().updateConfig([](UserConfig& c) {
                c.autoMonitorPrint = false;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Auto-monitor print DISABLED. The console monitor window will NOT appear after auto-started prints.\n";
            std::cout << GetTimeStamp() << "[INFO] Press SHIFT within 3 seconds at print start to override and start monitoring anyway.\n";
            break;
        }

        case ShortcutAction::UPLOAD:
        {
            if (!params.hasFilename) {
                std::cout << GetTimeStamp() << "[ERR] No filename specified.\n";
                std::cout << "Usage: /upload <filename>\n";
                break;
            }
            ProcessFileWithMode(params.filename, FILEOP_UPLOAD_ONLY, false);
        }
        break;

        case ShortcutAction::PRINT:
        {
            if (!params.hasFilename) {
                std::cout << GetTimeStamp() << "[ERR] No filename specified.\n";
                std::cout << "Usage: /print <filename>\n";
                break;
            }

            if (IsFirmwareFile(params.filename)) {
                std::cout << GetTimeStamp() << "[ERR] Firmware file cannot be auto-started: "
                    << fs::path(params.filename).filename().string() << "\n";
                std::cout << GetTimeStamp() << "[INFO] Please use /upload for firmware updates.\n";
                break;
            }

            ProcessFileWithMode(params.filename, FILEOP_AUTO_START, false);
        }
        break;

        case ShortcutAction::CONNECT:
        case ShortcutAction::SETTOKEN:
        case ShortcutAction::SETKASAIP:
        case ShortcutAction::SETKASAAUTO:
        {
            PrintHeader("CONFIGURATION UPDATE");

            UserConfig cfg = HostContext::instance().snapshotConfig();
            bool needsSave = false;

            std::cout << "\nCurrent Configuration:\n";
            std::cout << "  Snapmaker IP: " << (cfg.ipAddress.empty() ? "[NOT SET]" : cfg.ipAddress) << "\n";
            std::cout << "  Snapmaker Token: " << (cfg.token.empty() ? "[NOT SET]" : cfg.token) << "\n";
            std::cout << "  Kasa IP: " << (cfg.kasa.ipAddress.empty() ? "[NOT SET]" : cfg.kasa.ipAddress) << "\n";
            std::cout << "  Kasa Auto Power-On: " << (cfg.kasa.autoPowerOn ? "ON" : "OFF") << "\n";
            std::cout << "  File Action: " << (cfg.fileAction.empty() ? "recycle" : cfg.fileAction) << "\n";
            std::cout << "  Sound Alerts: " << (cfg.enableSoundAlerts ? "ON" : "OFF") << "\n";
            std::cout << "  Email Alerts: " << (cfg.enableEmailAlerts ? "ON" : "OFF") << "\n";
            std::cout << "\n";

            if (!params.ipAddress.empty()) {
                if (IsValidIPAddress(params.ipAddress)) {
                    std::cout << GetTimeStamp() << "Setting Snapmaker IP: " << params.ipAddress << "\n";
                    cfg.ipAddress = params.ipAddress;
                    needsSave = true;
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] Invalid Snapmaker IP: " << params.ipAddress << "\n";
                    break;
                }
            }

            if (!params.token.empty()) {
                std::cout << GetTimeStamp() << "Setting Snapmaker Token: " << params.token << "\n";
                cfg.token = params.token;
                needsSave = true;
            }

            if (!params.kasaIp.empty()) {
                if (IsValidIPAddress(params.kasaIp)) {
                    std::cout << GetTimeStamp() << "Setting Kasa IP: " << params.kasaIp << "\n";
                    cfg.kasa.ipAddress = params.kasaIp;
                    needsSave = true;

                    std::cout << GetTimeStamp() << "Testing Kasa connection...\n";
                    bool isOn = false;
                    if (CheckKasaPlugStatus(params.kasaIp, isOn)) {
                        std::cout << GetTimeStamp() << "[OK] Kasa plug reachable! Current state: " << (isOn ? "ON" : "OFF") << "\n";
                    }
                    else {
                        std::cout << GetTimeStamp() << "[WARN] Could not reach Kasa plug. Check IP and network.\n";
                    }
                }
                else {
                    std::cout << GetTimeStamp() << "[ERR] Invalid Kasa IP: " << params.kasaIp << "\n";
                    break;
                }
            }

            if (params.hasAutoPowerOn) {
                cfg.kasa.autoPowerOn = params.autoPowerOn;
                std::cout << GetTimeStamp() << "Setting Kasa Auto Power-On: " << (cfg.kasa.autoPowerOn ? "ON" : "OFF") << "\n";
                needsSave = true;
            }

            if (params.hasParameter && !params.parameter.empty()) {
                cfg.fileAction = params.parameter;
                std::cout << GetTimeStamp() << "Setting File Action: " << cfg.fileAction << "\n";
                needsSave = true;
            }

            if (!needsSave) {
                std::cout << GetTimeStamp() << "[INFO] No changes specified.\n";
                break;
            }

            HostContext::instance().updateConfig([cfg](UserConfig& c) {
                c = cfg;
                return true;
                });
            SaveConfig(HostContext::instance().getConfigPath().string());
            std::cout << GetTimeStamp() << "[OK] Configuration saved.\n";

            if (!cfg.ipAddress.empty()) {
                std::cout << GetTimeStamp() << "Testing Snapmaker connection...\n";

                std::string testUrl = BuildConnectUrl(cfg.ipAddress);
                std::string testResponse;
                auto [testRes, testCode] = PerformHttpGetWithCode(testUrl, testResponse, 5);

                if (testRes != CURLE_OK) {
                    std::cout << GetTimeStamp() << "[WARN] Snapmaker not reachable at " << cfg.ipAddress << "\n";
                    std::cout << "   Check: Power, network, and IP address\n";
                }
                else {
                    std::cout << GetTimeStamp() << "[OK] Snapmaker is reachable!\n";

                    if (!cfg.token.empty()) {
                        MachineStatusData status = GetMachineStatus(false);
                        if (!IsMachineOffline(status)) {
                            std::cout << GetTimeStamp() << "[OK] Token is valid!\n";
                            std::cout << "   Status: " << status.getStatusDisplay() << "\n";
                            std::cout << "   Tool head: " << status.getToolHeadDisplay() << "\n";
                        }
                        else {
                            std::cout << GetTimeStamp() << "[WARN] Token may be invalid or expired.\n";
                            std::cout << "   Run without token to get a new one.\n";
                        }
                    }
                    else {
                        std::cout << GetTimeStamp() << "[INFO] No token set.\n";
                        std::cout << "   Would you like to request a token now? (y/N): ";

                        if (_kbhit()) {
                            char ch = _getch();
                            if (ch == 'y' || ch == 'Y') {
                                std::cout << "y\n";
                                if (ConnectionSetup(cfg.ipAddress, false)) {
                                    std::cout << GetTimeStamp() << "[OK] Token obtained and saved!\n";
                                }
                                else {
                                    std::cout << GetTimeStamp() << "[ERR] Failed to get token.\n";
                                }
                            }
                            else {
                                std::cout << "N\n";
                            }
                        }
                        else {
                            std::cout << "N\n";
                        }
                    }
                }
            }

            cfg = HostContext::instance().snapshotConfig();
            std::cout << "\n" << GetTimeStamp() << "Final Configuration:\n";
            std::cout << "  Snapmaker: " << cfg.ipAddress << " (Token: "
                << (cfg.token.empty() ? "MISSING" : "SET") << ")\n";
            std::cout << "  Kasa: " << (cfg.kasa.ipAddress.empty() ? "Not configured" : cfg.kasa.ipAddress)
                << " (Auto Power-On: " << (cfg.kasa.autoPowerOn ? "ON" : "OFF") << ")\n";
            std::cout << "  File Action: " << (cfg.fileAction.empty() ? "recycle" : cfg.fileAction) << "\n";
            std::cout << "  Sound Alerts: " << (cfg.enableSoundAlerts ? "ON" : "OFF") << "\n";
            std::cout << "  Email Alerts: " << (cfg.enableEmailAlerts ? "ON" : "OFF") << "\n";
        }
        break;

        case ShortcutAction::ABOUT:
        {
            try {
                ClearConsole();
                PrintHeader("ABOUT");
                std::cout << std::endl;
                DisplayAbout();
            }
            catch (const std::exception& e) {
                std::cout << GetTimeStamp() << "[ERR] Failed to display about information: " << e.what() << std::endl;
            }
            catch (...) {
                std::cout << GetTimeStamp() << "[ERR] Unknown error displaying about information" << std::endl;
            }
        }
        break;

        case ShortcutAction::STOP:
        {
            PrintHeader("STOP PRINT");
            StopPrint();
        }
        break;

        case ShortcutAction::PAUSE:
        {
            PrintHeader("PAUSE PRINT");
            PausePrint();
        }
        break;

        case ShortcutAction::RESUME:
        {
            PrintHeader("RESUME PRINT");
            ResumePrint();
        }
        break;

        case ShortcutAction::MONITOR:
        {
            ClearConsole();
            PrintHeader("PRINT MONITOR");
            std::cout << GetTimeStamp() << "Checking for active print job..." << std::endl;

            MachineStatusData status = GetMachineStatus(false);
            if (status.getMachineStatus() != STATUS_RUNNING && status.getMachineStatus() != STATUS_PAUSED) {
                std::cout << GetTimeStamp() << "[INFO] No active print job found." << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Start a print first using /print <file> or touchscreen." << std::endl;
                std::cout << std::endl << "Press any key to exit..." << std::endl;
                (void)_getch();
                break;
            }
            else {
                std::cout << GetTimeStamp() << "Active print detected. Starting monitor..." << std::flush;
                ClearConsole();
                MonitorPrintWithProgress(false);
            }
        }
        break;

        case ShortcutAction::CAMERA:
        {
            PrintHeader("RTSP CAMERA CONTROL");
            UserConfig cfg = HostContext::instance().snapshotConfig();
            if (!IsRtspCameraConfigured(cfg.rtspCamera)) {
                std::cout << GetTimeStamp() << "Camera not configured (missing RTSP URL or VLC). Use /camerasettings to set it up.\n";
                break;
            }
            if (!CheckVlcInstalled()) {
                std::cout << GetTimeStamp() << "VLC not found. Install VLC to use the camera feature.\n";
                break;
            }
            if (HostContext::instance().startCameraDetached(cfg.rtspCamera.rtspUrl))
                std::cout << GetTimeStamp() << "Camera launched. VLC will remain open after this program exits.\n";
            else
                std::cout << GetTimeStamp() << "Failed to launch camera.\n";
            break;
        }

        case ShortcutAction::CAMERASETTINGS:
        {
            ConfigureRtspCamera();
            break;
        }

        case ShortcutAction::UPLOADSERVER_START:
        {
            ClearConsole();
            PrintHeader("HTTP UPLOAD SERVER");
            if (HostContext::instance().startUploadServer()) {
                std::cout << GetTimeStamp() << "[OK] HTTP upload server started on port 8081" << std::endl;
                std::cout << GetTimeStamp() << "[INFO] Upload files via: curl -X POST -F \"file=@model.gcode\" http://localhost:8081/api/files/local" << std::endl;
 
                // Minimize the console window to the taskbar
                HWND hConsole = GetConsoleWindow();
                if (hConsole) {
                    ShowWindow(hConsole, SW_MINIMIZE);   // or SW_HIDE to hide completely
                }
            }
            else {
                std::cout << GetTimeStamp() << "[ERR] Could not start server (port 8081 may be in use)" << std::endl;
            }
            break;
        }

        default:
            std::cout << GetTimeStamp() << "[ERR] Unknown shortcut action.\n";
            DisplayHelp();
            break;
        }

        bool isInteractive = (params.action == ShortcutAction::HOME ||
            params.action == ShortcutAction::LASERSETUP ||
            params.action == ShortcutAction::PRINTERSETUP ||
            params.action == ShortcutAction::STATUS ||
            params.action == ShortcutAction::KASA ||
            params.action == ShortcutAction::KASA_ON ||
            params.action == ShortcutAction::KASA_OFF ||
            params.action == ShortcutAction::MACROS ||
            params.action == ShortcutAction::GCODE ||
            params.action == ShortcutAction::THICKNESS ||
            params.action == ShortcutAction::CAPTURE ||
            params.action == ShortcutAction::RESETCONFIG ||
            params.action == ShortcutAction::RESETTOKEN ||
            params.action == ShortcutAction::UPLOAD ||
            params.action == ShortcutAction::PRINT ||
            params.action == ShortcutAction::CONNECT ||
            params.action == ShortcutAction::SETTOKEN ||
            params.action == ShortcutAction::SETKASAIP ||
            params.action == ShortcutAction::SETKASAAUTO ||
            params.action == ShortcutAction::SETAUTOSTART ||
            params.action == ShortcutAction::AUTOMONITOR_ON ||
            params.action == ShortcutAction::AUTOMONITOR_OFF ||
            params.action == ShortcutAction::SETFILEACTION ||
            params.action == ShortcutAction::FULLCONFIGURE ||
            params.action == ShortcutAction::ENCLOSUREFAN ||
            params.action == ShortcutAction::ENCLOSURELED ||
            params.action == ShortcutAction::MONITOR ||
            params.action == ShortcutAction::SOFTCAMCHECK ||
            params.action == ShortcutAction::LIGHTBURN ||
            params.action == ShortcutAction::LIGHTBURNCAMERA ||
            params.action == ShortcutAction::STOP ||
            params.action == ShortcutAction::PAUSE ||
            params.action == ShortcutAction::RESUME ||
            params.action == ShortcutAction::CAMERA ||
            params.action == ShortcutAction::CAMERASETTINGS ||
            params.action == ShortcutAction::BEEP_ON ||
            params.action == ShortcutAction::BEEP_OFF ||
            params.action == ShortcutAction::EMAILCONFIG ||
            params.action == ShortcutAction::EMAIL_ON ||
            params.action == ShortcutAction::EMAIL_OFF ||
            params.action == ShortcutAction::SERVER_BIND_LOCALHOST ||
            params.action == ShortcutAction::SERVER_BIND_GLOBAL ||
            params.action == ShortcutAction::SETWEBAUTH ||
            params.action == ShortcutAction::SETWEBUSER ||
            params.action == ShortcutAction::SETWEBPASSWORD);

        if (!isInteractive && params.action != ShortcutAction::HELP) {
            std::cout << "\nPress any key to exit...\n";
            (void)_getch();
        }
    }
    catch (const std::exception& e) {
        std::cout << GetTimeStamp() << "[ERR] Shortcut execution error: " << e.what() << std::endl;
        std::cout << "\nPress any key to exit...\n";
        (void)_getch();
    }
    catch (...) {
        std::cout << GetTimeStamp() << "[ERR] Unknown error in shortcut execution" << std::endl;
        std::cout << "\nPress any key to exit...\n";
        (void)_getch();
    }
}