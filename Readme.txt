================================================================================
Snapmaker LightBurn Host - Readme
================================================================================

Snapmaker LightBurn Host is a Windows application that creates a virtual camera
feed from your Snapmaker 2.0 A250/A350 10W laser for use with LightBurn laser
software. It allows you to capture images, control your machine, upload files,
measure material thickness, and start print jobs automatically.

================================================================================

FEATURES
Virtual Camera for LightBurn
- Streams Snapmaker camera via Softcam
- Works with LightBurn's camera system (requires Default Capture System)

Image Capture & Adjustment
- Capture images at a defined base position
- Fine-tune alignment with pixel-level adjustments (+-96 pixels)
- Toggle adjustments on/off
- Persistent settings via config.json

Machine Control
- Execute G-code directly via Snapmaker HTTP API (port 8080)
- Automatic toolhead detection (Laser / 3D Printer / CNC)
- Smart setup routines:
Laser: G28 -> G90 -> G54
3DP/CNC: G28
- Built-in retry and timeout handling

Material Thickness Measurement
- Automatic probing via API
- Two modes:
Default: uses base position coordinates
Custom: user-defined X/Y coordinates (e.g., front-left of the bed)
- Settings saved between sessions

Print Monitoring (Console)
- Real-time progress display (percentage, elapsed/remaining time, line count)
- Interactive controls:
[P] Pause, [R] Resume, [S] Stop, [O] Toggle Polling, [C] Start/Stop Camera,
[K] Toggle Auto Power-Off, [B] Toggle Beep, [E] Toggle Email,
[ESC]/[X] Exit, [F] Enclosure Fan (if present), [L] Enclosure LED (if present)
- Automatic detection of print completion
- Works with /monitor command-line shortcut and during auto-start prints
- When polling is toggled off, the display shows "POLL PAUSED" and stops
  refreshing the machine status (the print continues normally)
- During pause (door open, filament out) warnings are shown
- Auto-monitoring can be enabled/disabled globally (see CONFIGURATION section)

Beep Notification
- Configurable beep sound when a print completes
- Can be toggled ON/OFF from print monitoring with [B] key
- Setting persists across program restarts via config.json
- Uses Windows default system beep

Email Notifications
- Send email alerts when a print completes
- Configure via /emailconfig command or main menu
- Supports SMTP (Gmail App Password recommended)
- Settings: SMTP server, port, sender email, recipient email, App Password
- Can be toggled ON/OFF during print monitoring with [E] key
- Test email functionality included
- Settings persist via config.json

RTSP Camera Support
- Launch RTSP camera (VLC) in detached mode (stays open after exit)
- Auto-launch camera when print starts (configurable)
- Configure via interactive menu (press [V] in main menu) or /camerasettings shortcut
- Requires VLC media player installed

Kasa Smart Plug Control (TP-Link HS100/HS110/HS300/KP115)
- Turn plug on/off directly from the application
- Auto power-on: if machine is offline at startup or during file upload,
the plug is turned on automatically
- Auto power-off: after a print finishes, the plug can be turned off
  automatically (configurable)
- Toggle auto power-off during print monitoring with [K]
- Interactive menu (press [K]) now includes separate toggles for auto power-on [A]
  and auto power-off [P]
- Requires hs100.exe (included in the release package)

Enclosure Fan Control
- Set fan speed from command line: /enclosurefan off|half|full|0-100
- Sends M1010 S4 command to the machine
- Also available during print monitoring (press F) if enclosure detected

Enclosure LED Control
- Set LED brightness from command line: /enclosureled off|half|full|0-100
- Sends enclosure LED command to the machine
- Also available during print monitoring (press L) if enclosure detected

File Action (Recycle Bin vs Permanent Delete)
- Configurable via config.json "fileAction" setting
- "recycle" (default) - Processed files are moved to Windows Recycle Bin (safe)
- "delete" - Processed files are permanently deleted (dangerous, no recovery)
- Can be set via command line: /fileaction recycle or /fileaction delete
- Applies to:
  * Upload folder monitoring
  * HTTP upload server
  * ProcessFileUpload (temp files and source files)
- Default is Recycle Bin for safety

System Tray (Background Service)
- Right-click tray icon for quick access:
   Open HTTP page in browser
   Start/Stop/Restart HTTP Server
   Open Upload folder
   Process Upload Folder Now (manually start processing)
   Launch Print Monitor (Console)
   Launch Kasa
   Launch Camera
   Launch LightBurn
   Launch OrcaSlicer (if installed)
   Launch Snapmaker Luban (if installed)
   Launch UltiMaker Cura (if installed)
   Web Upload (Browser)
   Web Monitor (Browser)
   Web Config (Browser)
   Web Control (Browser)
   Web Capture (Browser)
   Web Thickness (Browser)
   Web G-code (Browser)
   Web XYZ Laser Calibration (Browser)
   Web About (Browser)
   Exit
- Runs in background even when main console is closed

Upload Folder Monitoring
- Place .gcode, .nc, .cnc files in the "Upload" folder next to the executable
- Files are automatically queued and processed when printer is idle
- If auto-start print is ON and the printer was busy when file arrived, a
  confirmation dialog appears (Yes=start, No=delete, Cancel=delete all & stop)
- After successful processing, the source file is either moved to Recycle Bin
  or permanently deleted based on fileAction setting
- A Readme.txt file is created in the folder explaining the behaviour
- Files older than 10 minutes are removed on program start/stop (except Readme.txt)

Temporary Batch Processing Folder
- A temporary folder is created inside the Upload directory to hold files
  during batch processing. Files placed there are deleted/moved to Recycle Bin
  automatically after successful upload or print start. Do not store permanent
  files there.
- Any files older than 10 minutes are cleaned up automatically.

Drag & Drop Upload
- Drop .gcode, .nc, .cnc files onto SnapmakerLightBurnHost.exe
- Upload progress bar
- Two modes (controlled by config setting "autoStartPrint" and SHIFT key):
Auto-start: uploads and starts the job
Upload-only: uploads file only, start manually on the machine (default)
- Module compatibility:
Laser: .nc (upload-only/auto-start), .gcode (auto-start only)
3DP: .gcode (both modes)
CNC: .cnc (upload-only/auto-start), .gcode (auto-start only)

Auto-Start Print Toggle
- Press [P] in the main menu to enable/disable auto-starting of print jobs
- When ON (default), dropped files automatically start after upload
- When OFF, dropped files are only uploaded (no auto-start)

Auto-Monitor Print Toggle
- Controls whether the console monitor window automatically appears after an
  auto-started print.
- Default: ON (monitor starts)
- Can be changed via command line: /automonitoron (enable) or /automonitoroff (disable)
- Setting persists in config.json
- At print start, press SHIFT within 3 seconds to override the current setting

HTTP Upload Server (OctoPrint-like API)
- Start with: /server (minimizes console, runs in background)
- Accepts multipart POST to /api/files/local with "file" and "print" fields
- "print=true" starts the job, "print=false" only uploads
- Uploaded files are renamed with a "snapmaker_" prefix (e.g., snapmaker_model.gcode)
- Serves simple status page on http://localhost:8081
- Also serves user-friendly web pages:
  /upload      - drag & drop file upload with print option
  /monitor     - real-time print monitor (progress, temperatures, toolhead)
  /config      - JSON configuration editor with Softcam/LightBurn checks
  /control     - machine control panel (home, pause, resume, stop, enclosure, Kasa, camera)
  /capture     - one-click image capture from laser camera
  /thickness   - measure material thickness via web interface
  /gcode       - web-based G-code console
  /xyzcal      - laser calibration wizard (Z-height and XY offset)
  /about       - displays README content
  /mobile      - mobile-friendly page for smartphones (see MOBILE WEB PAGE section)
- Can be used with OrcaSlicer (custom G-code upload script)
- OrcaSlicer:
- Physical Printer
    Host Type: Octo/Klipper
    Print Agent: Snapmaker
    Hostname, IP or URL: localhost:8081 or 127.0.0.1:8081
    Test connection
- Cura:
   *Install OctoPrint Connection plugin from Martketplace (top right of Cura)
    Setting > Printer > Manage Printers > Select Printer > Connect OctoPrint > Add
    Instance Name: Snapmaker (anything)
    IP Address or Hostname: localhost:8081 or 127.0.0.1:8081
    Port: 8081
    Path: /
   *OK > API Key: Snapmaker (anything) > Check Start print job after uploading to upload+print or 
    uncheck to just upload    

- Command Line Example usage:
    curl -X POST -F "file=@model.gcode" http://localhost:8081/api/files/local
    curl -X POST -F "file=@model.gcode" -F "print=true" http://localhost:8081/api/files/local

Web Upload Page (/upload)
- Full-featured HTML page with drag & drop support
- Supported: .gcode, .nc, .cnc, .bin (firmware)
- Checkbox for "Upload & Print" (auto-start after upload)
- Progress bar and status messages
- .bin files cannot be auto-started (checkbox disabled)

Web Monitor Page (/monitor)
- Real-time print monitoring in your browser
- Shows tool head, current file, progress bar, elapsed/remaining time, line progress
- Displays toolhead-specific data (laser power, CNC spindle speed, 3DP temperatures)
- Module status (enclosure, rotary, door, E-stop, air purifier)
- Auto-refresh toggle (5 seconds interval, on by default)
- "Refresh Now" button for manual update
- "Launch Console Monitor" button to open the native console monitor

Web Config Page (/config)
- Edit config.json directly in your browser
- Tree, code, and form editing modes
- Save configuration immediately (persists to disk)
- Includes buttons to check Softcam installation and LightBurn camera setting

Web Control Page (/control)
- Machine control buttons: Home, Reprint, Pause, Resume, Stop
- Enclosure fan and LED control (if enclosure present)
- Kasa smart plug on/off with status display
- RTSP camera start button (if configured)

Web Capture Page (/capture)
- One-click image capture from Snapmaker's laser camera
- Captured image appears in both the web browser and LightBurn.
  Note: If the image does not appear, select "None" followed by
  "DirectShow Softcam" in LightBurn's camera control source drop down.
- When the page loads, the virtual camera (Softcam) is initialized
  with a test pattern. Image captures automatically update the feed.
- Additional URL endpoints for embedding in LightBurn or other software:
  - /api/capture               -> triggers a new capture, returns JPEG
  - /api/capture/latest        -> returns the most recently captured
                                  image without a new capture
  Use these URLs directly in LightBurn's network camera source.

Web Thickness Page (/thickness)
- Measure material thickness via web interface
- Shows result in millimeters

Web G-code Console (/gcode)
- Send raw G-code commands from browser
- Command history with click-to-reuse
- Quick command buttons (G28, G90, G91, G54, M5)

Web XYZ Calibration Page (/xyzcal)
- Interactive wizard for laser Z-height and XY offset calibration
- Step-by-step guided process:
  1. Select laser type (10W or 20W/40W/IR)
  2. Home machine and capture static buffer position
  3. Capture Z focus height (or get focal length from machine)
  4. Set origin position (XY offset) by positioning laser and capturing
  5. Fine-tune with 10x10mm square test (enter measured distances)
  6. Copy generated Start G-code directly to LightBurn
- Automatically updates G92 offsets in sample G-code files
- Follows same logic as the console status page calibration tools

Web About Page (/about)
- Displays README.txt content in a styled web page
- Features Snapmaker-themed SVG logo with question mark and white S
- Navigation links to Home, Upload, Monitor, and Config pages
- Same theme as other web interfaces

Mobile Web Page (/mobile)
A mobile-friendly interface designed for smartphones and small screens.
- Displays machine status (Idle/Running/Paused/Offline) with color-coded badge
- Shows current tool head and print file name
- Progress bar with percentage and time estimates
- Live temperatures (nozzle 1, nozzle 2 if present, bed)
- Module details (filament status, laser power, CNC spindle speed)
- Control buttons: Pause, Resume, Stop
- Kasa Smart Plug card (if configured in config.json) with ON/OFF status and buttons
- RTSP Camera button (opens the RTSP URL in a new tab/window)
- All data is polled every 2 seconds automatically

Access at: http://localhost:8081/mobile
To access this page from a smartphone, you need to:
- Set "bindLocalhost": false in config.json.
- Add a Windows Firewall inbound rule for TCP port 8081.
- Use your PC's local IP address instead of localhost.

IMPORTANT: This makes the server reachable to any device on your local network.
Do not forward port 8081 to the internet. Use a VPN if you need access from
outside your home network.

G-code Macros
- Create .txt macro files in the macros folder
- Executes G-code sequences line-by-line
- Manual G-code input supported (lowercase is automatically converted to uppercase)
- Manual G-code shows responses and supports:
UP/DOWN arrows: navigate command history
LEFT/RIGHT arrows: move cursor
H: show history, C: clear history
CLS or CLEAR: clear the screen
!n, !!: recall previous commands
- Sample macro files included

Real-Time Machine Status
- Status: Idle / Running / Paused / Offline / Error
- Position (X/Y/Z)
- Toolhead-specific data (temperatures, power, spindle speed)
- Module info (enclosure, rotary, E-stop, air purifier, door state)

Toolhead data:
- Laser: Power, focal length, camera status, module detection
- 3DP: Nozzle/bed temperatures, filament status, print status
- CNC: Spindle speed, work speed

Laser Calibration Tools (visible when laser module is installed):
[Z] - Z-Height Calculations (capture Static Buffer Z and Z Focus Height)
[O] - XY Offset Calibration (four methods: auto-calibrate, fine-tune,
      enter custom coordinates, or reset to defaults)
[F] - Manage G-code Files (open folder, open in Notepad, delete files,
      list all files with timestamps)
[B] - Backup Management (open, restore from or delete old backups)
[G] - Open Snapmaker Laser + LightBurn Guide in your browser

Modules: Enclosure, Rotary, E-stop, Air purifier, Door state

Auto Refresh Mode
Press [A] in the main menu.
- Refresh interval: 1 second auto-capture, [ENTER] for immediate capture
- Continuous capture loop
- Adjustment status is displayed in real-time
- Press [O] to open the latest captured image
- Press [S] to stop auto-refresh and return to manual mode

LightBurn Integration
- Auto-detect LightBurn install from registry
- Launch LightBurn from app
- Validate camera settings in prefs.ini

================================================================================

SYSTEM REQUIREMENTS
- Windows 10 x64 / 11
- Snapmaker 2.0 (A250 / A350)
- LightBurn v1.7.08 x64 or earlier (for Softcam camera capture compatibility)
- LightBurn v2.1 x64 or later (for URL camera capture compatibility)
- Softcam virtual camera driver x64 installed (for camera capture)
- Network access to Snapmaker (port 8080)
- hs100.exe (included) - required for Kasa smart plug integration
- VLC - required for RTSP camera
- Internet connection (recommended) - required for email notifications,
  QR code generation, and accessing external dependencies.

================================================================================

INSTALLATION / UNINSTALL
Place Snapmaker Light Burn Host folder anywhere. Run the
SnapmakerLightBurnHost.exe application.

On first run it will try to use Luban settings; if not found, the app
will run setup. A configuration file named config.json will be generated
in the Snapmaker Light Burn Host directory.

Configuration settings are automatically backed up to config.json.old
the next time the application closes.

Optional context menu and shortcuts:
See SnapmakerLightBurnHost-installer and run
"Snapmaker Light Burn Host Installer.bat" as administrator to add/install:

Snapmaker Light Burn Host to context menus for .gcode, .nc and .cnc

Download and install Microsoft Visual C++ 2015-2022 Redistributable if
missing. See "DEPENDENCIES" at the bottom of this readme for the direct
link.

Optional:
Install Softcam (same as standalone installer)

Download and install VLC media player (for RTSP camera support). See
"DEPENDENCIES" at the bottom of this readme for the direct link.

Desktop shortcuts for the host, Kasa Smart Plug Control, Snapmaker Camera,
Snapmaker Monitor, Snapmaker Upload, and Snapmaker Tray are created by default
when you choose to create shortcuts.

Run "Snapmaker Light Burn Host Uninstaller.bat" as administrator to
remove/uninstall them.
(Explorer will restart when running these scripts.)

Install Softcam virtual camera driver (standalone installer):
See softcam-installer and run RegisterSoftcam.bat as administrator.
Run UnregisterSoftcam.bat as administrator to uninstall.

================================================================================

LIGHTBURN SETUP
Open LightBurn.
Go to Devices -> Add Device -> Camera.
Navigate to Settings -> Camera.
Set Camera Capture System = Default Capture System.
(Required for compatibility with the virtual camera.)

================================================================================

CONTROLS
Snapmaker Controls:
[ENTER] - Capture image (moves to base position)
[SPACE] - Measure material thickness
[Y] - Toggle thickness mode (Default / Custom)
[F] - Edit custom thickness coordinates
[M] - Machine setup: Home (G28) + Absolute Positioning (G90) +
Work Coordinates (G54)
[S] - Status page (Z: laser Z-Height calculations, O: XY Calibration,
F: Manage G-code Files, B: Backup Management, G: Open Laser Guide)
[G] - G-code macros (select macro or manual entry)
[A] - Auto-refresh capture mode (1 sec interval)
[U] - Update base position coordinates
[B] - Reset base position to defaults
[P] - Toggle Auto-Start Print (ON/OFF) - see Drag & Drop section

Image Adjustment:
[LEFT] - Adjust image left (-1 pixel)
[RIGHT] - Adjust image right (+1 pixel)
[UP] - Adjust image up (-1 pixel)
[DOWN] - Adjust image down (+1 pixel)
[R] - Reset adjustment to zero
[T] - Toggle adjustment ON/OFF

General:
[L] - Launch LightBurn
[K] - Kasa smart plug control (turn on/off, set auto power-on, auto power-off, change IP)
[V] - RTSP Camera Menu (start/stop camera, configure auto-launch, set URL)
[O] - Open latest captured image in default viewer
[C] - Clear screen / redisplay menu
[ESC] / [X] - Exit

================================================================================

COMMAND LINE SHORTCUTS
The program supports command line arguments for automation and quick access.
The following shortcuts are available:

QUICK ACTIONS:
  /home               - Home the machine (G28 only)
  /lasersetup         - Complete laser setup (G28 + G90 + G54)
  /printersetup       - Printer/CNC setup (G28 + G90)
  /thickness          - Measure material thickness
  /capture            - Capture one image and show in camera feed
  /autocapture        - Auto-refresh capture (captures every second, ESC to stop)
  /enclosurefan       - Set enclosure fan speed (off, half, full, or 0-100)
  /enclosureled       - Set enclosure LED brightness (off, half, full, or 0-100)
  /lightburn          - Launch LightBurn application
  /camera             - Launch RTSP camera (VLC) in detached mode (stays open after exit)
  /camerasettings     - Configure RTSP camera (URL, auto-launch)

NOTIFICATIONS:
  /beepon             - Enable beep sound on print completion
  /beepoff            - Disable beep sound on print completion
  /emailconfig        - Configure email settings (SMTP, recipient, app password)
  /emailon            - Enable email notifications on print completion
  /emailoff           - Disable email notifications on print completion

SYSTEM CHECKS:
  /softcamcheck       - Check if Softcam is properly installed
  /lightburncamera    - Check LightBurn camera settings

INTERACTIVE PAGES:
  /status             - Show machine status page
  /kasa               - Kasa smart plug control menu
  /macros             - G-code macros menu
  /gcode              - Manual G-code entry

FILE OPERATIONS:
  /upload <file>      - Upload file to Snapmaker (no auto-start)
  /print <file>       - Upload and auto-start print
  /stop               - Stop current print
  /pause              - Pause current print
  /resume             - Resume paused print
  /monitor            - Monitor print progress with live updates (console)

CONFIGURATION:
  /resetconfig        - Reset all configuration to defaults
  /resettoken         - Clear existing token and request a new one
  /connect <ip>       - Set Snapmaker IP (will prompt for token)
  /token <token>      - Set Snapmaker token only
  /kasaip <ip>        - Set Kasa smart plug IP
  /kasapoweronon      - Enable Kasa auto power-on when machine is offline
  /kasapoweronoff     - Disable Kasa auto power-on
  /kasapoweroffon     - Enable Kasa auto power-off after print finishes
  /kasapoweroffoff    - Disable Kasa auto power-off
  /serverlocalhost    - HTTP server binds to localhost (127.0.0.1) only
  /serverglobal       - HTTP server binds to all network interfaces (0.0.0.0)
  /webauth on/off     - Enable/disable HTTP authentication for remote clients (recommended on)
  /webuser <username> - Set HTTP basic auth username
  /webpassword <pass> - Set HTTP basic auth password
  /autostarton        - Enable auto-start print after file upload
  /autostartoff       - Disable auto-start print (upload only)
  /automonitoron      - Enable auto-monitor after auto-started print
  /automonitoroff     - Disable auto-monitor after auto-started print
  /fileaction recycle - Move processed files to Recycle Bin (safe, default)
  /fileaction delete  - Permanently delete processed files (dangerous)

KASA DIRECT CONTROL:
  /kasaon             - Turn Kasa smart plug ON immediately
  /kasaoff            - Turn Kasa smart plug OFF immediately

HTTP UPLOAD SERVER:
  /server             - Start HTTP server (minimizes to tray, port 8081)

SYSTEM TRAY:
  /tray               - Run in system tray mode (no console window)

DOCUMENTATION:
  /about              - Open README.txt documentation

COMBINED CONFIGURATION:
  /connect <ip> /token <token> /kasaip <ip> /kasapoweron /kasapoweroffon /autostarton /beepon /emailon /automonitoron /fileaction recycle

EXAMPLES:
  SnapmakerLightBurnHost.exe /home
  SnapmakerLightBurnHost.exe /enclosurefan half
  SnapmakerLightBurnHost.exe /enclosurefan 75
  SnapmakerLightBurnHost.exe /upload model.gcode
  SnapmakerLightBurnHost.exe /camera
  SnapmakerLightBurnHost.exe /emailconfig
  SnapmakerLightBurnHost.exe /automonitoroff
  SnapmakerLightBurnHost.exe /fileaction delete
  SnapmakerLightBurnHost.exe /connect 192.168.1.100 /token abc123 /kasaip 192.168.1.200 /kasapoweron /kasapoweroffon /autostarton /beepon /emailon /automonitoron /fileaction recycle
  SnapmakerLightBurnHost.exe /serverlocalhost
  SnapmakerLightBurnHost.exe /serverglobal

================================================================================

PRINT MONITOR (CONSOLE)
When a print job is started via auto-start (drag & drop with auto-start enabled,
or /print shortcut), the program by default automatically enters monitoring mode.
This behaviour can be changed using the /automonitoron/off shortcuts or by editing
the "autoMonitorPrint" field in config.json.

You can also monitor an existing print job from the command line:
SnapmakerLightBurnHost.exe /monitor

Monitoring Features:
- Real-time progress bar: shows percentage complete
- Elapsed time and estimated remaining time
- Current line / total lines
- Interactive controls:
  [P] Pause the print
  [R] Resume a paused print
  [S] Stop the print (with confirmation and homing)
  [O] Toggle polling – pauses live status updates (the print continues)
  [C] Start/Stop the RTSP camera (if configured)
  [K] Toggle auto power-off after print finishes (persistent setting)
  [B] Toggle beep notification on print completion (persistent setting)
  [E] Toggle email notification on print completion (persistent setting)
  [F] Cycle enclosure fan speed (OFF -> HALF -> FULL) if enclosure present
  [L] Cycle enclosure LED brightness (OFF -> HALF -> FULL) if enclosure present
  [ESC] or [X] Exit monitoring (does not affect the print)
- When polling is toggled off, the display shows a "POLL PAUSED" indicator and
  stops refreshing the machine status until polling is resumed. This frees up the
  Snapmaker touchscreen for normal operation.
- If the print is paused (e.g., due to an open door or filament runout), the
  monitor will show warnings ("DOOR OPEN" or "FILAMENT OUT") on the status line.
- After print completion, if auto power-off is enabled, the Kasa plug will be
  turned off after a short wait (press SHIFT to cancel).
- A beep sounds when the print completes if enabled (toggle with [B]).
- An email notification is sent when the print completes if enabled (toggle with
  [E] during monitoring). Email settings must be configured first using the
  /emailconfig command-line shortcut.

================================================================================

WEB PAGES
The HTTP server (port 8081) serves several interactive web pages:

- http://localhost:8081/           - Simple status page with last uploaded file and monitor link
- http://localhost:8081/upload     - Drag & drop file upload with print option
- http://localhost:8081/monitor    - Real-time print monitor (auto-refresh)
- http://localhost:8081/config     - JSON configuration editor with Softcam/LightBurn checks
- http://localhost:8081/control    - Machine control panel (home, pause, resume, stop, enclosure, Kasa, camera)
- http://localhost:8081/capture    - One-click image capture from laser camera
- http://localhost:8081/thickness  - Measure material thickness
- http://localhost:8081/gcode      - Web-based G-code console with history
- http://localhost:8081/xyzcal     - Interactive laser calibration wizard (Z-height and XY offset)
- http://localhost:8081/about      - README documentation in styled HTML
- http://localhost:8081/mobile     - Mobile-friendly interface (see MOBILE WEB PAGE section below)

All pages feature a themed design and are accessible from the system tray
right-click menu.

================================================================================

MOBILE WEB PAGE (/mobile)
A mobile-friendly interface designed for smartphones and small screens.

Features:
- Machine status: displays Idle/Running/Paused/Offline with a color-coded badge
- Current tool head and print file name (if any)
- Progress bar showing percentage complete
- Elapsed and remaining time estimates (if available)
- Live temperatures: Nozzle 1, Nozzle 2 (if present), and bed
- Module details:
  - For 3D printer: filament status (Out/Loaded)
  - For Laser: power percentage, focal length, work speed
  - For CNC: spindle speed, work speed
- Control buttons: Pause, Resume, Stop (only enabled when appropriate)
- Kasa Smart Plug card: appears if a Kasa IP is configured in config.json
  - Displays current ON/OFF state
  - "Turn On" and "Turn Off" buttons
  - Updates every 2 seconds
- RTSP Camera button: opens the configured RTSP URL in a new tab/window (requires VLC)
- All data is polled every 2 seconds automatically.

Access the mobile page at: http://your-pc-ip:8081/mobile

================================================================================

STATUS PAGE: LASER SETUP & CALIBRATION
The Status Page provides tools for laser users, including Z-Height calculations,
XY offset calibration, file management, and backup management.

Z-Height Calculations (Laser only):
- Static Buffer Z (homed position)
- Laser Focal Length (directly from machine)
- Z Focus Height (captured with focus bar on the bed)
- LightBurn Z Focal values:
  10W Laser: Static Buffer Z - Laser Focal Length
  20W/40W/IR Laser: Static Buffer Z - Z Focus Height
- Sample G-code files are generated in the 'G-code' folder.
- Press [F] on the status page to manage G-code files.

XY Offset Calibration (Laser only):
This feature calibrates the X/Y origin offset between LightBurn's 0,0
and the actual laser position.

Prerequisites:
- Z-Height calculations must be completed at least once to generate the
  sample Start G-code files.
- In LightBurn, set the appropriate Start G-code file
  (e.g., "Start G-code Sample - 10W Laser.txt") and End G-code file
  ("End G-code Sample - All Lasers.txt") in your device settings (GCode tab).

Calibration steps:
1. In LightBurn, create a new project.
2. Draw a 10mm x 10mm square.
3. Set its position to X=20, Y=20 mm.
4. Assign a line engrave layer for your laser.
5. Run the job on your Snapmaker.
6. Using a caliper or ruler, measure the distance from the left edge of
   the material to the left side of the engraved square. Enter this as
   the measured X distance.
7. Measure the distance from the front edge of the material to the
   bottom side of the engraved square. Enter this as the measured Y distance.
8. The program calculates the correct G92 X/Y offsets and updates your
   Start G-code file automatically.
9. Apply the new offsets in your LightBurn device settings (GCode tab).
10. Run the test job again to verify alignment. Repeat the X/Y steps if further
    adjustment is required.

Coordinate system (front-left corner origin):
- +X moves the laser right
- -X moves the laser left
- +Y moves the laser down (away from front edge)
- -Y moves the laser up (toward front edge)

Options on status page:
- [Z] - Run Z-Height Calculations.
- [O] - XY Offset Calibration (choose laser type: 10W or 20W/40W/IR).
- [F] - Manage G-code Files (open folder, open in Notepad, delete files).
- [B] - Backup Management (open, restore files or delete old backups).
- [G] - Open LightBurn Laser Guide in browser.
- [ENTER] - Refresh machine status display.
- [M] - Return to main menu.
- [ESC] - Exit.

The sample files contain a G92 command that sets the coordinate system origin
to the front-left corner of the bed. After calibration, LightBurn should align
correctly.

                   Bed
                    |
                    v
 +-------------------------------------------+
 |                                           |
 |                                           |
 |                                  *<-------|---- Position laser here (Z)
 |                                           |     Anywhere on the bed is ok
 |                                           |
 |             +----------+                  |
 |             |          |                  |
 |             |   10mm   |                  |
 |             |          |                  |
 |             +----------+                  |
 |             ^                             |
 |             |                             |
 |     measured to X = 20mm (aligned)        |
 |     measured to Y = 20mm (aligned)        |
 |/measured from here                        |
 +-------------------------------------------+
*<--- Position laser here (X/Y)
 ^
 `--- Front left corner of bed (0,0)

X = 22 - 20 = +2   (square is 2mm too far right)
Y = 19 - 20 = -1   (square is 1mm too close to front)

If current G92 X = -270, Y = 330:
    X = -270 + 2 = -268  (move origin 2mm left to compensate)
    Y = 330 + (-1) = 329 (move origin 1mm forward to compensate)

Consult the comprehensive 1064nm IR+20/40W + Lightburn Guide available at the
following URL:
https://forum.snapmaker.com/t/1064nm-ir-20-40w-lightburn-guide/35954

Manage G-code Files:
Press [F] on the status page to:
- Open G-code Folder in Explorer.
- Open a G-code file in Notepad (select by number).
- Delete a G-code file (select by number).
- List all G-code files with timestamps.

Backup Management:
When you update G-code files via Z-Height or XY calibration, backups are
automatically created with timestamps. Press [B] on the status page to:
- Open specific backup.
- Restore from latest or specific backup.
- Delete old backup files.
- List available backups for each laser type.

================================================================================

RTSP CAMERA CONFIGURATION
You can set up an RTSP camera (e.g., IP camera, USB camera via RTSP server) to
monitor your machine remotely during prints.

Configuration:
- Press [V] in the main menu to open the RTSP Camera Menu, or use the
  /camerasettings command-line shortcut.
- From the menu you can:
  * Toggle Auto-Launch on print start
  * Start the camera in attached mode (closes when program exits)
  * Stop the camera in attached mode (terminates the camera window)
  * Start the camera in detached mode (stays open after exit)
  * Configure RTSP URL and settings
- To configure the RTSP URL, choose the "Configure" option and enter:
  * Full RTSP URL directly, or
  * Individual components (IP, port, path, username, password)
- Example URL: rtsp://username:password@192.168.1.50:554/stream1

Usage:
- During print monitoring, press [C] to start or stop the camera.
- From command line: /camera launches VLC in detached mode (VLC stays open
  after the program exits).
- Requires VLC media player installed (https://www.videolan.org/vlc/).

================================================================================

KASA SMART PLUG CONTROL (Auto Power-Off)
In addition to auto power-on, the application now supports auto power-off after
a print job finishes. This setting is stored in config.json and can be toggled
during print monitoring by pressing [K] or from the interactive menu.

Interactive Mode (press [K] in main menu):
- Turn the plug ON or OFF
- Toggle auto power-on (offline detection) - [A] key
- Toggle auto power-off (post-print shutdown) - [P] key
- Change the plug's IP address - [C] key
- Refresh plug status - [Enter]

Auto Power-Off:
- When enabled, after a print completes successfully, the program will wait
  for the machine to become idle (up to 60 seconds) and then turn off the
  Kasa plug.
- Press SHIFT during the 30-second countdown to cancel auto power-off.
- The setting persists across program restarts.

================================================================================

MANUAL G-CODE (within Macros menu)
Press [G] -> [M] to enter the manual G-code console.

Special commands:
- CLS or CLEAR - Clear the screen (keep the manual G-code header)
- H - Show command history
- C - Clear history
- !n - Recall command number n (e.g., !2)
- !! - Recall the last command
- EXIT / QUIT / Q - Return to the macro menu

Arrow keys:
- UP / DOWN - Navigate history
- LEFT / RIGHT - Move cursor inside the current line
- Backspace - Delete character before cursor

All commands are converted to uppercase before being sent to the machine.
The application shows the response (if any) and the execution time.

================================================================================

CONFIGURATION
Stored in config.json (new categorized format):

{
    "machine": {
        "ipAddress": "your-snapmaker-ip",
        "token": "your-snapmaker-token",
        "model": 0,
        "autoStartPrint": false,
        "autoMonitorPrint": true,
        "fileAction": "recycle",
        "bindLocalhost": true
    },
    "basePosition": {
        "x": 232.0,
        "y": 178.0,
        "z": 290.0
    },
    "image": {
        "adjustment": {
            "pixelsX": 0,
            "pixelsY": 0,
            "useAdjustment": false
        }
    },
    "material": {
        "thickness": {
            "mode": 0,
            "customX": 99.0,
            "customY": 22.0
        }
    },
    "kasa": {
        "ipAddress": "your-kasa-ip",
        "autoPowerOn": false,
        "autoPowerOff": true
    },
    "camera": {
        "rtsp": {
            "autoLaunch": true,
            "rtspUrl": "rtsp://username:password@your-camera-ip:port/path"
        }
    },
    "notifications": {
        "soundAlerts": true,
        "emailAlerts": false,
        "email": {
            "recipient": "recipient@example.com",
            "sender": "sender@gmail.com",
            "appPassword": "abcd efgh ijkl mnop",
            "smtpServer": "smtp.gmail.com",
            "smtpPort": 587
        }
    },
    "web": {
        "authEnabled": true,
        "password": "Snapmaker_Bridge",
        "user": "admin"
    }
}

Model values:
0 = A350
1 = A250

autoStartPrint:
true -> drag & drop files will auto-start the print
false -> drag & drop files will only upload (default)

autoMonitorPrint:
true -> automatically start the console monitor after an auto-started print
false -> do not start the monitor (user can still press SHIFT to override)

fileAction:
"recycle" -> move processed files to Windows Recycle Bin (safe, default)
"delete" -> permanently delete processed files (dangerous, no recovery)

bindLocalhost:
true -> HTTP server binds to 127.0.0.1 only (localhost)
false -> HTTP server binds to 0.0.0.0 (all interfaces, allowing remote access)
Default: true. Set to false only if you understand the security implications.

kasa.autoPowerOff:
true -> after a print finishes, turn off the Kasa plug automatically
false -> do not turn off

notifications.soundAlerts:
true -> play a beep when print finishes
false -> no beep

notifications.emailAlerts:
true -> send email notification when print finishes
false -> no email

notifications.email.smtpServer / smtpPort:
SMTP server settings (default: smtp.gmail.com:587)

notifications.email.sender:
Sender email address (must be authorized for the SMTP server)

notifications.email.recipient:
Recipient email address for notifications

notifications.email.appPassword:
App Password (for Gmail) or SMTP password. For Gmail, generate an App Password
at Google Account -> Security -> App passwords. Your regular Gmail password
will NOT work.

rtspCamera.autoLaunch:
true -> automatically launch RTSP camera when a print starts
false -> do not auto-launch

rtspCamera.rtspUrl:
RTSP URL of the camera (e.g., rtsp://username:password@192.168.1.50:554/stream0)

web:
Username and password for HTTP Bridge.

TIP: To prevent your Snapmaker or Kasa plug from getting a new IP address after
a power cycle or router reboot, set a DHCP reservation (static lease) for each
device in your router's settings. This ensures the IP addresses in config.json
remain valid.

================================================================================

FIREWALL CONFIGURATION FOR REMOTE ACCESS
To access the mobile page (and any other web interface) from another device on
your local network (e.g., smartphone, tablet), you need to:

1. Set "bindLocalhost": false in config.json (under "machine").
   This tells the server to listen on all network interfaces (0.0.0.0).
   Restart the application after making this change.

2. Create a Windows Firewall inbound rule to allow TCP traffic on port 8081.
   Use Add Firewall Rule.bat in SnapmakerLightBurnHost-installer to add the rule
   and Remove Firewall Rule.bat to remove the rule.

3. Access the mobile page from your device using the host PC's IP address:
   http://your-pc-ip:8081/mobile

   To find your PC's IP address, right-click the tray icon and select
   "Server status" - the IP address(es) will be displayed along with the
   full LAN access URL. QR codes for the mobile URLs will be opened in
   your browser. Scan the QR code with your mobile phone to open the page.

================================================================================

STARTUP BEHAVIOR
On startup, the application checks machine status and performs appropriate setup:

Machine Online:
- Checks if the machine is idle or busy
- Detects tool head (Laser / 3DP / CNC)
- Checks homing status

If NOT HOMED:
Laser: offers 3-second SHIFT bypass for full setup (G28, G90, G54)
3DP/CNC: offers 3-second SHIFT bypass for homing (G28)
Unknown: auto-detects and performs appropriate setup

If HOMED:
Laser: offers 3-second SHIFT bypass for additional setup (G90, G54)
3DP/CNC: no additional setup

Machine Offline:
- If auto power-on is enabled and a Kasa plug is configured, the plug is
  turned on automatically and the program waits for the machine to boot.
- Otherwise, a warning is displayed with troubleshooting steps.
- Automatic setup is skipped.

================================================================================

MATERIAL THICKNESS MEASUREMENT
Two modes:

Default Mode (center of bed):
Uses the base position coordinates (X,Y from your model settings).
Ideal for consistent measurements at the same location every time.

Custom Mode:
Uses user-defined X and Y coordinates.
Press [F] in the main menu to open the custom coordinates menu with options:
- Enter new custom coordinates
- Reset to default custom coordinates (X=99.0, Y=22.0) - front left of bed
- Return to main menu

Toggle Modes:
Press [Y] to switch between Default and Custom.
The current mode and coordinates are shown in the header.

Measure:
Press [SPACE] to start thickness measurement.
The machine moves to the specified position and measures thickness.
Results are displayed in the activity log.

================================================================================

MACHINE SETUP
Press [M] to perform machine setup. The setup varies by tool head:

Laser Setup (Full):
G28 - Home all axes
G90 - Set absolute positioning
G54 - Switch to work coordinates

3DP / CNC Setup (Basic):
G28 - Home all axes

This ensures the machine is properly configured before starting jobs.

================================================================================

TROUBLESHOOTING
Camera Not Working:
- Check that Softcam is installed
- Verify Device Manager -> Cameras
- Ensure LightBurn uses "Default Capture System"
- Only one instance of Snapmaker LightBurn Host can run at a time

Connection Issues:
- Verify IP address is correct
- Ensure port 8080 is accessible
- Check that the Snapmaker is online
- Verify the token is valid
- Restart Snapmaker if needed

Token Issues:
- Reconnect with Luban and clear the token in config.json to request a new one
- Use /resettoken to force a new token

Upload Failures:
- Ensure the machine is idle
- Verify the file type is compatible with the installed module
- Check network connectivity
- Make sure the machine is ready (correct module, filament loaded, etc.)

Image Alignment Issues:
- Use arrow keys for adjustment (maximum offset: +-96 pixels)

Kasa Plug Not Working:
- Ensure hs100.exe is in the hs100 subfolder next to the executable
- Verify the plug IP address is correct and reachable

Manual G-code screen becomes cluttered:
- Use "CLS" or "CLEAR" to clear the screen while staying in manual mode

Laser XY Offset Calibration gives unexpected results:
- Ensure your material is fixed in place (does not move between measurement
  and calibration).
- Verify that the square in LightBurn was exactly 10x10mm at position X=20, Y=20.
- Make sure the correct Start G-code file (10W vs 20W/40W/IR) is selected
  in LightBurn device settings.

RTSP Camera not working:
- Verify VLC is installed (default location)
- Check the RTSP URL is correct and reachable
- Ensure the camera stream is active (test with VLC separately)

Email Notifications not working:
- Verify email is enabled in config or via /emailon
- Ensure SMTP settings are correct (server, port)
- For Gmail, use an App Password (not your regular password)
- Check that sender and recipient emails are entered correctly
- Run /emailconfig to reconfigure and send a test email

Upload Folder not processing:
- Check that the "Upload" folder exists next to the executable
- Ensure the printer is online and connected
- The folder is monitored even when the main console is closed (tray mode)

HTTP Server returns "Not Found":
- Use the correct endpoint: /api/files/local (not /upload)
- Example: curl -X POST -F "file=@model.gcode" http://localhost:8081/api/files/local

Web Monitor not refreshing:
- Auto-refresh is on by default. If it stops, toggle the switch off and on.
- Or click "Refresh Now" for a manual update.

Web About page not showing:
- Ensure README.txt exists in the application directory
- Access at http://localhost:8081/about

Web XYZ Calibration page not working:
- Ensure the laser module is installed and the machine is connected
- Follow the step-by-step wizard; it requires the sample G-code files to exist
  (run console Z-calibration first to generate them if missing)

Security warning: The web interface is not encrypted and has basic authentication.
Do not expose it directly to the internet. If you need remote access, use a VPN.

================================================================================

CONSOLE TIPS
- To select text: hold Shift while clicking and dragging with the mouse
- To copy selected text: press Enter or Ctrl+C
- To paste text (e.g., IP addresses or G-code commands): hold Shift and
  right-click, or use Ctrl+V

================================================================================

SUPPORT
For issues or questions:
- Check Snapmaker or LightBurn community forums
- Consult LightBurn documentation for camera setup
- Verify your Snapmaker firmware is up to date
- Restart Snapmaker or your computer if needed
- Ensure your token is valid
- If you have issues closing Snapmaker Tray or uninstalling Softcam, you may 
  need to close your browser(s).

================================================================================

IMPORTANT NOTES
- Designed for Snapmaker 2.0 A250/A350.
- Using Snapmaker LightBurn Host and OctoPrint simultaneously is not recommended.
- Always ensure your workspace is clear before homing or starting prints.
- The application will move the machine head and the bed.

Safety:
- Laser: always wear appropriate laser safety glasses, use an enclosure,
  and ventilate fumes.
- 3D Printing: always ventilate your machine when printing.
- CNC: always wear appropriate safety goggles or use an enclosure.

SECURITY:
- The HTTP server is not encrypted and only has basic authentication.
- It is intended for local use only.
- Do not expose it to the internet.
- If you need remote access, use a VPN or an SSH tunnel.

================================================================================

DISCLAIMER
Use at your own risk. This software is provided as-is for use with Snapmaker 2.0
series machines and LightBurn software. The author(s) is/are not responsible for
any damage to equipment or injury resulting from use of this software. Always
follow safety procedures and supervise machine operations.

================================================================================

DEPENDENCIES
- https://release.lightburnsoftware.com/LightBurn/Release: LightBurn pre-1.7.08 or post-v2.1
- https://github.com/tshino/softcam: virtual DirectShow camera.
- https://github.com/nlohmann/json: JSON parser.
- https://github.com/nothings/stb: stb_image.h, stb_image_resize.h
- libcurl.dll, zlib1.dll: curl
- https://github.com/frogedude/hs100: hs100 for Kasa integration.
- https://github.com/microsoft/vcpkg: curl:x64-windows
- https://aka.ms/vs/17/release/vc_redist.x64.exe: VC++ Redistributable 2015-2022
- https://www.videolan.org/vlc: VLC media player (for RTSP camera support)
- https://www.jsdelivr.com/package/npm/jsoneditor: (A web-based tool to view, edit, format, and validate JSON)
- https://goqr.me/api: (QR generator)

================================================================================

ACKNOWLEDGMENTS
Special thanks to Maycuz, Skreelink, Slynold, and theevl for their contributions.
Based on: https://github.com/Maycuz/SnapmakerLightBurnHost

================================================================================