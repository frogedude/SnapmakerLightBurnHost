// upload_pages.cpp
#include "upload_server.hpp"

void ServeXYZCalibrationPage(SOCKET clientSocket) {
    std::string html;

    // ---------- Part 1: HTML head + opening ----------
    html += R"(<!DOCTYPE html>
<html>
<head>
    <title>Snapmaker Laser Calibration Wizard</title>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        *{margin:0;padding:0;box-sizing:border-box}
        body{font-family:'Segoe UI',Tahoma,Helvetica,sans-serif;background:linear-gradient(135deg,#1e2a3a 0%,#0f1a24 100%);min-height:100vh;padding:20px}
        .container{max-width:1100px;margin:0 auto}
        .card{background:#fff;border-radius:20px;box-shadow:0 10px 30px rgba(0,0,0,0.3);overflow:hidden}
        .header{background:#0a1c2f;padding:20px;color:#fff;border-bottom:3px solid #3498db;display:flex;align-items:center;gap:20px}
        .header svg{flex-shrink:0}
        .header h1{font-size:1.8rem;margin-bottom:5px}
        .header p{color:#8aa9c9;font-size:0.85rem;margin-top:5px}
        .header-text{flex:1}
        .content{padding:25px}
        .step{background:#f8fafc;border-radius:15px;padding:20px;margin-bottom:20px;border-left:5px solid #3498db}
        .step.completed{border-left-color:#2ecc71;background:#f0fff4}
        .step h3{color:#0a1c2f;margin-bottom:10px;display:flex;align-items:center;gap:10px}
        .step-number{background:#3498db;color:#fff;width:28px;height:28px;border-radius:50%;display:inline-flex;align-items:center;justify-content:center;font-size:0.9rem;font-weight:bold}
        .step.completed .step-number{background:#2ecc71}
        .info-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #e2edf4}
        .info-row:last-child{border-bottom:none}
        .label{font-weight:600;color:#1e4663}
        .value{font-family:monospace;font-size:1.2rem;font-weight:bold;color:#2ecc71}
        button{background:#0a1c2f;color:#fff;border:none;padding:8px 20px;border-radius:25px;cursor:pointer;font-size:0.9rem;margin-top:10px;margin-right:10px}
        button:hover{background:#1e4a76}
        button.primary{background:#2ecc71}
        button.primary:hover{background:#27ae60}
        .result-box{background:#0a1c2f;color:#2ecc71;padding:15px;border-radius:10px;font-family:monospace;font-size:1.1rem;text-align:center;margin-top:15px}
        .status{margin-top:15px;padding:10px;border-radius:8px;display:none}
        .status.success{background:#d4edda;color:#155724;display:block}
        .status.error{background:#f8d7da;color:#721c24;display:block}
        .status.info{background:#d1ecf1;color:#0c5460;display:block}
        .progress-bar{height:4px;background:#e2edf4;border-radius:2px;margin-bottom:20px}
        .progress-fill{height:100%;background:#2ecc71;border-radius:2px;width:0%;transition:width 0.3s}
        .measure-input{background:#eef2f7;padding:12px;border-radius:10px;margin-top:10px}
        input{padding:6px 10px;border:2px solid #ddd;border-radius:8px;width:100px;text-align:center}
        .coordinate-input{display:flex;gap:15px;align-items:center;flex-wrap:wrap;margin-top:10px}
        .warning{color:#e74c3c;font-size:0.8rem;margin-top:10px;padding:8px;background:#fef2f2;border-radius:8px}
        .note{color:#3498db;font-size:0.8rem;margin-top:10px;padding:8px;background:#e8f4fd;border-radius:8px}
        .gcode-block{background:#2d2d2d;color:#f8f8f2;padding:15px;border-radius:8px;font-family:monospace;font-size:0.9rem;margin-top:10px;overflow-x:auto;white-space:pre-wrap}
        .radio-group{display:flex;gap:20px;margin:15px 0;flex-wrap:wrap}
        .radio-group label{display:flex;align-items:center;gap:8px;cursor:pointer}
        .gcode-wrapper{background:#2d2d2d;border-radius:10px;margin:10px 0;overflow:hidden}
        .gcode-header{background:#1e1e1e;padding:8px 12px;color:#aaa;font-size:0.8rem;border-bottom:1px solid #444}
    </style>
</head>
<body>
<div class='container'>
    <div class='card'>
        <div class='header'>
            <svg width='64' height='64' viewBox='0 0 256 256' version='1.1' xmlns='http://www.w3.org/2000/svg'>
                <rect width='256' height='256' rx='48' fill='#000000'/>
                <g transform='translate(128,128)'>
                    <circle r='80' fill='none' stroke='#E74C3C' stroke-width='4' stroke-dasharray='8 8'/>
                    <circle r='40' fill='#3498DB'/>
                    <circle r='32' fill='#2980B9'/>
                    <circle r='24' fill='DarkBlue'/>
                    <line x1='-60' y1='0' x2='-45' y2='0' stroke='#E74C3C' stroke-width='6'/>
                    <line x1='45' y1='0' x2='60' y2='0' stroke='#E74C3C' stroke-width='6'/>
                    <line x1='0' y1='-60' x2='0' y2='-45' stroke='#E74C3C' stroke-width='6'/>
                    <line x1='0' y1='45' x2='0' y2='60' stroke='#E74C3C' stroke-width='6'/>
                    <text x='-16' y='17' font-family='Arial' font-size='48' font-weight='bold' fill='white'>S</text>
                </g>
                <rect x='20' y='20' width='216' height='216' rx='32' fill='none' stroke='white' stroke-width='2' stroke-opacity='0.3'/>
            </svg>
            <div class='header-text'>
                <h1>Laser Calibration Wizard</h1>
                <p>Follow steps in order - Z-Height and XY Offset Calibration based on official guide</p>
            </div>
        </div>
        <div class='content'>
            <div class='progress-bar'><div class='progress-fill' id='progressFill'></div></div>
            <div id='statusMsg' class='status'></div>
)";

    // ---------- Part 2: Step 0 (Laser Type) with confirm button ----------
    html += R"(
            <!-- Step 0: Laser Type -->
            <div class='step' id='step0'>
                <h3><span class='step-number'>1</span> Select Your Laser Type (changing will reset calibration progress)</h3>
                <div class='radio-group'>
                    <label><input type='radio' name='laserType' value='10w' checked> 10W Laser (uses focal length from machine)</label>
                    <label><input type='radio' name='laserType' value='20w'> 20W/40W/IR Laser (uses focus bar height)</label>
                </div>
                <button id='confirmLaserBtn' class='primary'>Confirm Laser Type</button>
            </div>
)";

    // ---------- Part 3: Step 1 (Home & Capture Static) ----------
    html += R"(
            <!-- Step 1: Home & Capture Static Buffer -->
            <div class='step' id='step1'>
                <h3><span class='step-number'>2</span> Home Machine & Capture Static Position</h3>
                <p>Home all axes (G28) then switch to machine coordinates (G53) to get static buffer position</p>
                <button id='homeBtn' class='primary'>Home & Capture Static Position</button>
                <div class='info-row' style='margin-top:10px'><span class='label'>Static Buffer X:</span><span id='staticX' class='value'>--</span></div>
                <div class='info-row'><span class='label'>Static Buffer Y:</span><span id='staticY' class='value'>--</span></div>
                <div class='info-row'><span class='label'>Static Buffer Z:</span><span id='staticZ' class='value'>--</span></div>
            </div>
)";

    // ---------- Part 4: Step 2 (Z Focus Height) ----------
    html += R"(
            <!-- Step 2: Z Focus Height -->
            <div class='step' id='step2'>
                <h3><span class='step-number'>3</span> Capture Z Focus Height</h3>
                <div id='focusInstruction10w' style='display:block'>
                    <p>10W Laser: Get focal length directly from the machine</p>
                    <button id='getFocalBtn'>Get Laser Focal Length from Machine</button>
                    <div class='info-row'><span class='label'>Laser Focal Length:</span><span id='focalLength' class='value'>--</span></div>
                </div>
                <div id='focusInstruction20w' style='display:none'>
                    <p>20W/40W/IR Laser: Place the focus bar directly on the bed, then capture Z position</p>
                    <button id='captureFocusBtn'>Capture Focus Z</button>
                    <div class='info-row'><span class='label'>Focus Z Height:</span><span id='focusZ' class='value'>--</span></div>
                </div>
                <div class='info-row'><span class='label'>Z Offset for LightBurn (G92 Z value):</span><span id='zOffset' class='value'>--</span></div>
            </div>
)";

    // ---------- Part 5: Step 3 (Origin + new buttons) ----------
    html += R"(
            <!-- Step 3: XY Offset Capture (enhanced with focus move and home) -->
            <div class='step' id='step3'>
                <h3><span class='step-number'>4</span> Set Origin Position (XY Offset)</h3>
                <button id='moveX0Y0Btn'>Move to X0 Y0 (Front-Left Corner)</button>
                <button id='moveToFocusZBtn' style='background:#3498db;'>Move to Z Focus Height</button>
                <button id='homeMachineBtn' style='background:#e67e22;'>Home Machine (G28)</button>
                <div class='note' style='margin-top:10px'>
                    <strong>Z Focus Height:</strong> 
                    <span id='focusZValueDisplay'>--</span> mm (determined by laser type).<br>
                    Use the Snapmaker touchscreen to fine-tune the laser head position.<br>
                    Move it slightly off the front-left corner of the bed.<br>
                    Once positioned, click the button below to capture this location as the origin.
                </div>
                <button id='captureXYBtn' style='background:#3498db;'>Capture Current Position as Origin</button>
                <div class='info-row'><span class='label'>Captured X Position:</span><span id='capturedX' class='value'>--</span></div>
                <div class='info-row'><span class='label'>Captured Y Position:</span><span id='capturedY' class='value'>--</span></div>
                <div class='info-row'><span class='label'>Current G92 X Offset:</span><span id='currentXOffset' class='value'>0.0</span></div>
                <div class='info-row'><span class='label'>Current G92 Y Offset:</span><span id='currentYOffset' class='value'>0.0</span></div>
            </div>
)";

    // ---------- Part 6: Step 4 (Start G-code) ----------
    html += R"(
            <!-- Step 4: Start G-code Script -->
            <div class='step' id='step4'>
                <h3><span class='step-number'>5</span> Your LightBurn Start G-code Script</h3>
                <p>Copy this entire block into LightBurn Device Settings (GCode tab)</p>
                <div class='gcode-wrapper'>
                    <div class='gcode-header'>Start G-code (copy this exactly)</div>
                    <div class='gcode-block' id='gcodeResult'>
M106 P0 S255
M1010 S3 P100
M1010 S4 P100
G28
G90
G54
G92 X<span id='finalX'>0</span> Y<span id='finalY'>0</span> Z<span id='finalZ'>0</span>
M3 S0
                    </div>
                </div>
                <button id='copyBtn' class='primary'>Copy Start G-code</button>
                <div class='warning'>
                    <strong>Note:</strong> The Y-axis may exhibit up to approximately 0.5 mm of drift due to fast homing. Run the square test multiple times to verify alignment.
                </div>
            </div>
)";

    // ---------- Part 7: Step 5 (End G-code) ----------
    html += R"(
            <!-- Step 5: End G-code (Lift after Job) -->
            <div class='step' id='step5'>
                <h3><span class='step-number'>6</span> End G-code - Automatic Z Lift</h3>
                <p>After the job finishes, the laser will turn off, lift to a safe height, and home.</p>
                <div class='gcode-wrapper'>
                    <div class='gcode-header'>End G-code (copy into LightBurn Device Settings)</div>
                    <div class='gcode-block' id='endGcodeResult'>
M5
G0 Z<span id='endZLift'>0.0</span> F6000
G28
                    </div>
                </div>
                <button id='copyEndBtn' class='primary'>Copy End G-code</button>
                <div class='note'>
                    This ensures the head moves safely above the workpiece and then homes.
                </div>
            </div>
)";

    // ---------- Part 8: Step 6 (Fine-Tune) ----------
    html += R"(
            <!-- Step 6: Fine-Tune -->
            <div class='step' id='step6'>
                <h3><span class='step-number'>7</span> Fine-Tune with 10x10mm Square Test</h3>
                <p>In LightBurn, create a 10x10mm square at X=20, Y=20. Engrave and measure the actual position.</p>
                <div class='measure-input'>
                    <div class='coordinate-input'>
                        <label>Measured X (from left edge, mm):</label>
                        <input type='number' id='measuredX' step='0.1' value='20.0'>
                        <label>Measured Y (from front edge, mm):</label>
                        <input type='number' id='measuredY' step='0.1' value='20.0'>
                    </div>
                    <button id='fineTuneBtn' style='background:#3498db;'>Apply Fine-Tune Correction</button>
                    <div class='note'>
                        Previous offset adjustments can be reviewed in the browser's developer console (F12).<br>
                        After applying a correction, run the test again, measure the new position, and enter the new values here.
                    </div>
                </div>
                <div class='result-box' id='fineTuneResult' style='display:none;'>
                    <strong>Correction Applied:</strong><br>
                    X Adjustment: <span id='deltaX'>0</span> mm<br>
                    Y Adjustment: <span id='deltaY'>0</span> mm
                </div>
            </div>
)";

    // ---------- Part 9: Guide link and closing tags ----------
    html += R"(
            <div class='note' style='margin-top:20px; text-align:center;'>
                For complete setup instructions, refer to: 
                <a href='https://forum.snapmaker.com/t/1064nm-ir-20-40w-lightburn-guide/35954' target='_blank'>
                    1064nm IR+20/40W + Lightburn Guide
                </a>
            </div>
        </div>
    </div>
</div>
)";

    // ---------- Part 10: JavaScript (first half) ----------
    html += R"(<script>
    let laserType = '10w';
    let staticX = 0, staticY = 0, staticZ = 0;
    let capturedX = 0, capturedY = 0;
    let currentXOffset = 0, currentYOffset = 0;
    let focalLength = 0, focusZ = 0, zOffset = 0;
    let stepCompleted = [false, false, false, false, false, false, false];

    function updateProgress() {
        let completedCount = stepCompleted.filter(v => v === true).length;
        let fill = document.getElementById('progressFill');
        if (fill) fill.style.width = (completedCount / 7 * 100) + '%';
    }

    function markStep(n) {
        if (n < 0 || n > 6) return;
        stepCompleted[n] = true;
        let stepElement = document.getElementById('step' + n);
        if (stepElement && !stepElement.classList.contains('completed')) {
            stepElement.classList.add('completed');
        }
        updateProgress();
    }

    function showMessage(msg, type) {
        let msgDiv = document.getElementById('statusMsg');
        if (!msgDiv) return;
        msgDiv.textContent = msg;
        msgDiv.className = 'status ' + type;
        msgDiv.style.display = 'block';
        setTimeout(() => { msgDiv.style.display = 'none'; }, 5000);
    }

    async function getStatus() {
        try {
            let response = await fetch('/api/machine/status');
            if (!response.ok) return null;
            return await response.json();
        } catch(e) {
            console.error('getStatus error:', e);
            return null;
        }
    }

    async function getFocalLength() {
        try {
            let response = await fetch('/api/laser/focal');
            if (!response.ok) return null;
            return await response.json();
        } catch(e) {
            console.error('getFocalLength error:', e);
            return null;
        }
    }

    async function sendGcode(gcode) {
        try {
            let response = await fetch('/api/gcode/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ gcode: gcode })
            });
            return response.ok;
        } catch(e) {
            console.error('sendGcode error:', e);
            return false;
        }
    }

    function updateStartGcodeDisplay() {
        let finalXSpan = document.getElementById('finalX');
        let finalYSpan = document.getElementById('finalY');
        let finalZSpan = document.getElementById('finalZ');
        if (finalXSpan) finalXSpan.innerHTML = currentXOffset.toFixed(1);
        if (finalYSpan) finalYSpan.innerHTML = currentYOffset.toFixed(1);
        if (finalZSpan) finalZSpan.innerHTML = zOffset.toFixed(1);
    }

    function updateEndGcodeDisplay() {
        let endZLiftSpan = document.getElementById('endZLift');
        if (endZLiftSpan) {
            if (staticZ !== 0) {
                let endZ = staticZ - 1.0;
                endZ = Math.round(endZ * 10) / 10;
                endZLiftSpan.innerHTML = endZ.toFixed(1);
                if (!stepCompleted[5]) markStep(5);
            } else {
                endZLiftSpan.innerHTML = '--';
            }
        }
    }

    function getFocusZValue() {
        if (laserType === '10w') {
            if (focalLength > 0) return focalLength;
            else return null;
        } else {
            if (focusZ !== 0) return focusZ;
            else return null;
        }
    }

    function updateFocusZDisplay() {
        let focusVal = getFocusZValue();
        let displaySpan = document.getElementById('focusZValueDisplay');
        if (displaySpan) {
            if (focusVal !== null) {
                displaySpan.innerHTML = focusVal.toFixed(1);
                displaySpan.style.color = '#2ecc71';
            } else {
                displaySpan.innerHTML = 'not yet captured';
                displaySpan.style.color = '#e74c3c';
            }
        }
        let moveFocusBtn = document.getElementById('moveToFocusZBtn');
        if (moveFocusBtn) {
            moveFocusBtn.disabled = (focusVal === null);
        }
    }

    // Function to reset all calibration state (called when laser type changes or confirm resets)
    function resetCalibrationState() {
        staticX = 0; staticY = 0; staticZ = 0;
        capturedX = 0; capturedY = 0;
        currentXOffset = 0; currentYOffset = 0;
        focalLength = 0; focusZ = 0; zOffset = 0;
        // Reset step completion flags (0..6)
        stepCompleted = [false, false, false, false, false, false, false];
        // Clear UI elements
        let ids = ['staticX', 'staticY', 'staticZ', 'focalLength', 'focusZ', 'zOffset',
                   'capturedX', 'capturedY', 'currentXOffset', 'currentYOffset',
                   'finalX', 'finalY', 'finalZ', 'endZLift', 'focusZValueDisplay'];
        for (let id of ids) {
            let el = document.getElementById(id);
            if (el) {
                if (id === 'currentXOffset' || id === 'currentYOffset' || id === 'finalX' || id === 'finalY' || id === 'finalZ')
                    el.innerHTML = '0.0';
                else if (id === 'endZLift')
                    el.innerHTML = '--';
                else if (id === 'focusZValueDisplay')
                    el.innerHTML = 'not yet captured';
                else
                    el.innerHTML = '--';
            }
        }
        let resultBox = document.getElementById('fineTuneResult');
        if (resultBox) resultBox.style.display = 'none';
        // Remove 'completed' class from all steps
        for (let i = 0; i <= 6; i++) {
            let stepDiv = document.getElementById('step' + i);
            if (stepDiv) stepDiv.classList.remove('completed');
        }
        updateProgress();
        console.log('Calibration state reset.');
    }

    // Laser type radio change with confirmation and auto-reset
    let radios = document.querySelectorAll('input[name=laserType]');
    let previousLaserType = '10w';
    function updateLaserTypeUI() {
        let selected = document.querySelector('input[name=laserType]:checked');
        if (selected) {
            let newType = selected.value;
            if (newType === previousLaserType) return;
            // Check if any calibration progress exists (step 1..6 completed)
            let hasProgress = false;
            for (let i = 1; i <= 6; i++) {
                if (stepCompleted[i]) { hasProgress = true; break; }
            }
            if (hasProgress) {
                let confirmReset = confirm('Changing laser type will reset all calibration progress. Continue?');
                if (!confirmReset) {
                    // Revert radio button to previous selection
                    document.querySelector(`input[name='laserType'][value='${previousLaserType}']`).checked = true;
                    return;
                }
                // Reset everything
                resetCalibrationState();
                // Step 0 is no longer completed
                stepCompleted[0] = false;
            }
            previousLaserType = newType;
            laserType = newType;
            // Update UI for focus instructions
            let div10 = document.getElementById('focusInstruction10w');
            let div20 = document.getElementById('focusInstruction20w');
            if (laserType === '10w') {
                if (div10) div10.style.display = 'block';
                if (div20) div20.style.display = 'none';
            } else {
                if (div10) div10.style.display = 'none';
                if (div20) div20.style.display = 'block';
            }
            updateFocusZDisplay();
            showMessage('Laser type changed to ' + (laserType === '10w' ? '10W' : '20W/40W/IR') + '. Click "Confirm Laser Type" to proceed.', 'info');
        }
    }
    if (radios.length) {
        radios.forEach(radio => radio.addEventListener('change', updateLaserTypeUI));
    }
    updateLaserTypeUI();

    // Confirm Laser Type button
    let confirmBtn = document.getElementById('confirmLaserBtn');
    if (confirmBtn) {
        confirmBtn.onclick = function() {
            // If any progress exists (step 1..6), ask for confirmation to reset
            let hasProgress = false;
            for (let i = 1; i <= 6; i++) {
                if (stepCompleted[i]) { hasProgress = true; break; }
            }
            if (hasProgress) {
                let confirmReset = confirm('Confirming laser type will reset all previous calibration progress. Continue?');
                if (!confirmReset) return;
                resetCalibrationState();
            }
            markStep(0);
            showMessage('Laser type confirmed: ' + (laserType === '10w' ? '10W' : '20W/40W/IR'), 'success');
        };
    }
)";

    // ---------- Part 11: JavaScript (second half) with safe homing and debug ----------
    html += R"(
    let homeBtn = document.getElementById('homeBtn');
    if (homeBtn) {
        homeBtn.onclick = async function() {
            showMessage('Homing machine (G28)...', 'info');
            let btn = homeBtn;
            btn.disabled = true;
            try {
                if (await sendGcode('G28')) {
                    showMessage('Homed! Switching to machine coordinates (G53)...', 'info');
                    await new Promise(r => setTimeout(r, 2000));
                    if (await sendGcode('G53')) {
                        await new Promise(r => setTimeout(r, 1000));
                        let status = await getStatus();
                        if (status && status.online && typeof status.x === 'number') {
                            staticX = status.x;
                            staticY = status.y;
                            staticZ = status.z;
                            let sx = document.getElementById('staticX');
                            let sy = document.getElementById('staticY');
                            let sz = document.getElementById('staticZ');
                            if (sx) sx.innerHTML = staticX.toFixed(1) + ' mm';
                            if (sy) sy.innerHTML = staticY.toFixed(1) + ' mm';
                            if (sz) sz.innerHTML = staticZ.toFixed(1) + ' mm';
                            showMessage('Static buffer position captured!', 'success');
                            markStep(1);
                            updateStartGcodeDisplay();
                            updateEndGcodeDisplay();
                            updateFocusZDisplay();
                        } else {
                            showMessage('Failed to get machine position', 'error');
                        }
                    } else {
                        showMessage('Failed to switch to machine coordinates (G53)', 'error');
                    }
                } else {
                    //showMessage('Homing (G28) failed', 'error');
                }
            } catch(e) {
                showMessage('Error: ' + e.message, 'error');
            }
            btn.disabled = false;
        };
    }

    let getFocalBtn = document.getElementById('getFocalBtn');
    if (getFocalBtn) {
        getFocalBtn.onclick = async function() {
            if (staticZ === 0) {
                showMessage('Please complete Step 2 (Home Machine) first', 'error');
                return;
            }
            showMessage('Getting focal length from machine...', 'info');
            let btn = getFocalBtn;
            btn.disabled = true;
            try {
                let data = await getFocalLength();
                if (data && data.success && data.focalLength > 0) {
                    focalLength = data.focalLength;
                    zOffset = staticZ - focalLength;
                    let flElem = document.getElementById('focalLength');
                    let zOffElem = document.getElementById('zOffset');
                    if (flElem) flElem.innerHTML = focalLength + ' mm';
                    if (zOffElem) zOffElem.innerHTML = zOffset.toFixed(1) + ' mm';
                    updateStartGcodeDisplay();
                    updateFocusZDisplay();
                    showMessage('Focal length: ' + focalLength + ' mm, Z Offset: ' + zOffset.toFixed(1) + ' mm', 'success');
                    markStep(2);
                } else {
                    showMessage('Could not get focal length. Make sure laser is connected.', 'error');
                }
            } catch(e) {
                showMessage('Error: ' + e.message, 'error');
            }
            btn.disabled = false;
        };
    }

    let captureFocusBtn = document.getElementById('captureFocusBtn');
    if (captureFocusBtn) {
        captureFocusBtn.onclick = async function() {
            if (staticZ === 0) {
                showMessage('Please complete Step 2 (Home Machine) first', 'error');
                return;
            }
            showMessage('Place focus bar on bed, then click OK on touchscreen...', 'info');
            if (await sendGcode('G53')) {
                await new Promise(r => setTimeout(r, 1000));
                let status = await getStatus();
                if (status && status.online && typeof status.z === 'number') {
                    focusZ = status.z;
                    zOffset = staticZ - focusZ;
                    let fzElem = document.getElementById('focusZ');
                    let zOffElem = document.getElementById('zOffset');
                    if (fzElem) fzElem.innerHTML = focusZ.toFixed(1) + ' mm';
                    if (zOffElem) zOffElem.innerHTML = zOffset.toFixed(1) + ' mm';
                    updateStartGcodeDisplay();
                    updateFocusZDisplay();
                    showMessage('Focus Z captured: ' + focusZ.toFixed(1) + ' mm, Z Offset: ' + zOffset.toFixed(1) + ' mm', 'success');
                    markStep(2);
                } else {
                    showMessage('Failed to get Z position', 'error');
                }
                await sendGcode('G54');
            } else {
                showMessage('Failed to switch to machine coordinates (G53)', 'error');
            }
        };
    }

    let moveBtn = document.getElementById('moveX0Y0Btn');
    if (moveBtn) {
        moveBtn.onclick = async function() {
            showMessage('Moving to X0 Y0 (front-left corner)...', 'info');
            await sendGcode('G0 X0 Y0');
            showMessage('Moved to X0 Y0. Use touchscreen to fine-tune position.', 'success');
        };
    }

    let moveFocusBtn = document.getElementById('moveToFocusZBtn');
    if (moveFocusBtn) {
        moveFocusBtn.onclick = async function() {
            let focusVal = getFocusZValue();
            if (focusVal === null) {
                showMessage('Focus Z value not available. Please complete Step 3 first.', 'error');
                return;
            }
            showMessage('Moving to Z focus height (' + focusVal.toFixed(1) + ' mm)...', 'info');
            if (await sendGcode('G53')) {
                await new Promise(r => setTimeout(r, 500));
                if (await sendGcode('G0 Z' + focusVal.toFixed(1))) {
                    await new Promise(r => setTimeout(r, 1000));
                    showMessage('Moved to Z ' + focusVal.toFixed(1) + ' mm (machine coordinates).', 'success');
                } else {
                    showMessage('Failed to move Z axis.', 'error');
                }
                await sendGcode('G54');
            } else {
                showMessage('Failed to switch to machine coordinates.', 'error');
            }
        };
    }

    let homeMachineBtn = document.getElementById('homeMachineBtn');
    if (homeMachineBtn) {
        homeMachineBtn.onclick = async function() {
            showMessage('Homing machine (G28)...', 'info');
            if (await sendGcode('G28')) {
                showMessage('Machine homed successfully.', 'success');
            } else {
                showMessage('Homing failed.', 'error');
            }
        };
    }

    let captureXYBtn = document.getElementById('captureXYBtn');
    if (captureXYBtn) {
        captureXYBtn.onclick = async function() {
            if (staticX === 0 && staticY === 0) {
                showMessage('Please complete Step 2 (Home Machine) first', 'error');
                return;
            }
            showMessage('Capturing current position as origin...', 'info');
            if (await sendGcode('G53')) {
                await new Promise(r => setTimeout(r, 1000));
                let status = await getStatus();
                if (status && status.online && typeof status.x === 'number' && typeof status.y === 'number') {
                    capturedX = status.x;
                    capturedY = status.y;
                    currentXOffset = -(capturedX - staticX);
                    currentYOffset = -(capturedY - staticY);
                    currentXOffset = Math.round(currentXOffset * 10) / 10;
                    currentYOffset = Math.round(currentYOffset * 10) / 10;
                    let capX = document.getElementById('capturedX');
                    let capY = document.getElementById('capturedY');
                    let curX = document.getElementById('currentXOffset');
                    let curY = document.getElementById('currentYOffset');
                    if (capX) capX.innerHTML = capturedX.toFixed(1) + ' mm';
                    if (capY) capY.innerHTML = capturedY.toFixed(1) + ' mm';
                    if (curX) curX.innerHTML = currentXOffset.toFixed(1);
                    if (curY) curY.innerHTML = currentYOffset.toFixed(1);
                    updateStartGcodeDisplay();
                    showMessage('Origin captured! G92 X' + currentXOffset.toFixed(1) + ' Y' + currentYOffset.toFixed(1), 'success');
                    markStep(3);
                    markStep(4);
                    markStep(5);
                } else {
                    showMessage('Failed to get machine position', 'error');
                }
                await sendGcode('G54');
            } else {
                showMessage('Failed to switch to machine coordinates (G53)', 'error');
            }
        };
    }

    let fineTuneBtn = document.getElementById('fineTuneBtn');
    if (fineTuneBtn) {
        fineTuneBtn.onclick = async function() {
            if (staticX === 0) {
                showMessage('Please complete Step 2 (Home Machine) first', 'error');
                return;
            }
            if (capturedX === 0 && capturedY === 0 && (currentXOffset===0 && currentYOffset===0)) {
                showMessage('Please set origin first (Step 4)', 'error');
                return;
            }
            let measuredX = parseFloat(document.getElementById('measuredX').value);
            let measuredY = parseFloat(document.getElementById('measuredY').value);
            if (isNaN(measuredX) || isNaN(measuredY)) {
                showMessage('Please enter valid measurements', 'error');
                return;
            }
            console.log('=== Fine-Tune Adjustment ===');
            console.log('Current X offset:', currentXOffset.toFixed(1), 'mm');
            console.log('Current Y offset:', currentYOffset.toFixed(1), 'mm');
            console.log('Measured X:', measuredX.toFixed(1), 'mm (expected 20.0)');
            console.log('Measured Y:', measuredY.toFixed(1), 'mm (expected 20.0)');
            let deltaX = measuredX - 20.0;
            let deltaY = measuredY - 20.0;
            console.log('Delta X:', deltaX.toFixed(1), 'mm');
            console.log('Delta Y:', deltaY.toFixed(1), 'mm');
            currentXOffset = currentXOffset + deltaX;
            currentYOffset = currentYOffset + deltaY;
            currentXOffset = Math.round(currentXOffset * 10) / 10;
            currentYOffset = Math.round(currentYOffset * 10) / 10;
            console.log('New X offset:', currentXOffset.toFixed(1), 'mm');
            console.log('New Y offset:', currentYOffset.toFixed(1), 'mm');
            console.log('==========================');
            let curX = document.getElementById('currentXOffset');
            let curY = document.getElementById('currentYOffset');
            if (curX) curX.innerHTML = currentXOffset.toFixed(1);
            if (curY) curY.innerHTML = currentYOffset.toFixed(1);
            let deltaXspan = document.getElementById('deltaX');
            let deltaYspan = document.getElementById('deltaY');
            if (deltaXspan) deltaXspan.innerHTML = (deltaX > 0 ? '+' : '') + deltaX.toFixed(1);
            if (deltaYspan) deltaYspan.innerHTML = (deltaY > 0 ? '+' : '') + deltaY.toFixed(1);
            let resultBox = document.getElementById('fineTuneResult');
            if (resultBox) resultBox.style.display = 'block';
            updateStartGcodeDisplay();
            let mxInput = document.getElementById('measuredX');
            let myInput = document.getElementById('measuredY');
            if (mxInput) mxInput.value = '20.0';
            if (myInput) myInput.value = '20.0';
            showMessage('Fine-tune applied! New G92 X' + currentXOffset.toFixed(1) + ' Y' + currentYOffset.toFixed(1), 'success');
            markStep(6);
        };
    }

    let copyBtn = document.getElementById('copyBtn');
    if (copyBtn) {
        copyBtn.onclick = function() {
            let gcodeText = 'M106 P0 S255\nM1010 S3 P100\nM1010 S4 P100\nG28\nG90\nG54\nG92 X' +
                            currentXOffset.toFixed(1) + ' Y' + currentYOffset.toFixed(1) + ' Z' +
                            zOffset.toFixed(1) + '\nM3 S0';
            navigator.clipboard.writeText(gcodeText).then(() => {
                showMessage('Start G-code copied to clipboard!', 'success');
            }).catch(() => {
                showMessage('Failed to copy. Press Ctrl+C manually.', 'error');
            });
        };
    }

    let copyEndBtn = document.getElementById('copyEndBtn');
    if (copyEndBtn) {
        copyEndBtn.onclick = function() {
            if (staticZ === 0) {
                showMessage('Please complete Step 2 (Home Machine) first', 'error');
                return;
            }
            let endZ = staticZ - 1.0;
            endZ = Math.round(endZ * 10) / 10;
            let endGcode = 'M5\nG0 Z' + endZ.toFixed(1) + ' F6000\nG28';
            navigator.clipboard.writeText(endGcode).then(() => {
                showMessage('End G-code copied to clipboard!', 'success');
            }).catch(() => {
                showMessage('Failed to copy. Press Ctrl+C manually.', 'error');
            });
        };
    }

    updateStartGcodeDisplay();
    updateEndGcodeDisplay();
    updateFocusZDisplay();
</script>
</body>
</html>)";

    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeUploadPage(SOCKET clientSocket) {
    std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snapmaker Upload</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e2a3a 0%, #0f1a24 100%);
            min-height: 100vh;
            margin: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .card {
            background: #ffffff;
            border-radius: 28px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.3);
            max-width: 700px;
            width: 100%;
            padding: 2rem;
        }
        h1 {
            font-size: 1.8rem;
            color: #0a1c2f;
            margin-top: 0;
            display: flex;
            align-items: center;
            gap: 12px;
        }
        .logo { width: 42px; height: 42px; }
        .dropzone {
            border: 3px dashed #3498db;
            border-radius: 28px;
            padding: 2.5rem;
            text-align: center;
            background: #f8fafc;
            cursor: pointer;
            margin: 1.5rem 0;
        }
        .dropzone.dragover {
            border-color: #2ecc71;
            background: #e8f5e9;
            transform: scale(1.01);
        }
        .file-input-label {
            display: inline-block;
            background: #0a1c2f;
            color: white;
            padding: 10px 24px;
            border-radius: 40px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 12px;
        }
        .file-input-label:hover { background: #1e4a76; }
        input[type="file"] { display: none; }
        .info { font-size: 0.85rem; color: #4a627a; margin-top: 10px; }
        .option {
            background: #f0f4f9;
            padding: 1rem;
            border-radius: 20px;
            margin: 1rem 0;
            display: flex;
            align-items: center;
            gap: 12px;
            flex-wrap: wrap;
        }
        .option label { font-weight: 600; color: #0a1c2f; }
        .option input { width: 20px; height: 20px; cursor: pointer; }
        button {
            background: #2ecc71;
            color: white;
            border: none;
            padding: 12px 28px;
            font-size: 1rem;
            font-weight: bold;
            border-radius: 40px;
            cursor: pointer;
            width: 100%;
        }
        button:hover { background: #27ae60; transform: scale(1.02); }
        .progress-container {
            margin-top: 1.5rem;
            background: #e2e8f0;
            border-radius: 40px;
            height: 12px;
            overflow: hidden;
            display: none;
        }
        .progress-bar {
            width: 0%;
            height: 100%;
            background: #3498db;
            transition: width 0.3s;
        }
        .message {
            margin-top: 1rem;
            padding: 12px;
            border-radius: 16px;
            font-size: 0.9rem;
            display: none;
        }
        .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .filename {
            font-weight: bold;
            word-break: break-all;
            background: #eef2f7;
            padding: 6px 12px;
            border-radius: 40px;
            display: inline-block;
            margin-top: 8px;
        }
        hr { margin: 20px 0; border: none; border-top: 1px solid #cbd5e1; }
        .footer { font-size: 0.7rem; text-align: center; color: #6c7a89; margin-top: 1.5rem; }
    </style>
</head>
<body>
<div class="card">
    <h1>
        <svg class="logo" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
            <rect x="22" y="20" width="32" height="12" rx="3" fill="#000000"></rect>
            <circle cx="28" cy="25" r="2.5" fill="#E74C3C"></circle>
            <circle cx="36" cy="25" r="2.5" fill="#2ECC71"></circle>
            <rect x="14" y="28" width="72" height="58" rx="4" fill="#000000"></rect>
            <rect x="18" y="32" width="64" height="50" rx="3" fill="DarkBlue"></rect>
            <rect x="18" y="32" width="64" height="50" rx="3" fill="none" stroke="#3498DB" stroke-width="2.5"></rect>
            <polygon points="50,33.5 40,46 60,46" fill="#FFFFFF"></polygon>
            <rect x="46" y="41" width="8" height="15" rx="2" fill="#FFFFFF"></rect>
            <circle cx="50" cy="67" r="10" fill="none" stroke="#3498DB" stroke-width="2"></circle>
            <circle cx="50" cy="67" r="8" fill="none" stroke="#2980B9" stroke-width="2"></circle>
            <text x="50" y="72" font-family="Arial, sans-serif" font-size="13" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
            <ellipse cx="32" cy="42" rx="10" ry="3" fill="#FFFFFF" opacity="0.08"></ellipse>
        </svg>
        Snapmaker Upload
    </h1>
    <p>Upload files to your Snapmaker printer.</p>
    <div class="dropzone" id="dropzone">
        <p>Drag and drop your file here</p>
        <p style="font-size:0.9rem;">or</p>
        <label class="file-input-label">
            Browse files
            <input type="file" id="fileInput" accept=".gcode,.nc,.cnc,.bin">
        </label>
        <div id="selectedFile" class="info"></div>
        <div class="info">Supported: <strong>.gcode, .nc, .cnc, .bin</strong></div>
        <div id="moduleHint" class="info" style="margin-top: 8px;"></div>
    </div>
    <div class="option">
        <input type="checkbox" id="printCheckbox">
        <label for="printCheckbox">Upload and Print (auto-start after upload)</label>
        <span style="margin-left:auto; font-size:0.8rem;">Unchecked = upload only</span>
    </div>
    <button id="uploadBtn">Upload File</button>
    <div class="progress-container" id="progressContainer">
        <div class="progress-bar" id="progressBar"></div>
    </div>
    <div id="message" class="message"></div>
    <hr>
    <div class="footer">
    Bridge running on port 8081 | Temporary files stored in %TEMP%\snapmaker_upload\<br>
    </div>
</div>
<script>
    var dropzone = document.getElementById('dropzone');
    var fileInput = document.getElementById('fileInput');
    var selectedFileDiv = document.getElementById('selectedFile');
    var printCheckbox = document.getElementById('printCheckbox');
    var uploadBtn = document.getElementById('uploadBtn');
    var progressContainer = document.getElementById('progressContainer');
    var progressBar = document.getElementById('progressBar');
    var messageDiv = document.getElementById('message');
    var selectedFile = null;
    var currentToolhead = null;

    function showMessage(text, isError) {
        messageDiv.textContent = text;
        messageDiv.className = 'message ' + (isError ? 'error' : 'success');
        messageDiv.style.display = 'block';
        setTimeout(function() {
            if (messageDiv.textContent === text) messageDiv.style.display = 'none';
        }, 5000);
    }

    function updateSelectedFile(file) {
        if (file) {
            selectedFileDiv.innerHTML = '<span class="filename">File: ' + file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)</span>';
            selectedFileDiv.style.display = 'block';
        } else {
            selectedFileDiv.innerHTML = '';
        }
    }

    function clearSelection() {
        selectedFile = null;
        fileInput.value = '';
        updateSelectedFile(null);
        printCheckbox.disabled = false;
        printCheckbox.checked = false;
        progressBar.style.width = '0%';
        progressContainer.style.display = 'none';
        updateAllowedExtensionsMessage(); // update message after clearing
    }

    function updateAllowedExtensionsMessage() {
        var hintDiv = document.getElementById('moduleHint');
        if (!hintDiv || !currentToolhead) return;

        var printMode = printCheckbox.checked;
        var tool = currentToolhead;
        var message = 'Current module: ' + tool;

        if (tool === '3DP' || tool === '3D Printer') {
            message += ' - Allowed extensions: .gcode';
        } else if (tool === 'Laser') {
            if (printMode) {
                message += ' - Allowed extensions: .nc, .gcode (print)';
            } else {
                message += ' - Allowed extensions: .nc (upload only)';
            }
        } else if (tool === 'CNC') {
            if (printMode) {
                message += ' - Allowed extensions: .cnc, .gcode (print)';
            } else {
                message += ' - Allowed extensions: .cnc (upload only)';
            }
        } else {
            message += ' - Could not determine allowed extensions';
        }
        hintDiv.innerHTML = message;
    }

    dropzone.addEventListener('dragover', function(e) {
        e.preventDefault();
        dropzone.classList.add('dragover');
    });
    dropzone.addEventListener('dragleave', function() {
        dropzone.classList.remove('dragover');
    });
    dropzone.addEventListener('drop', function(e) {
        e.preventDefault();
        dropzone.classList.remove('dragover');
        if (e.dataTransfer.files.length) {
            selectedFile = e.dataTransfer.files[0];
            validateAndSetFile(selectedFile);
        }
    });
    fileInput.addEventListener('change', function(e) {
        if (e.target.files.length) {
            selectedFile = e.target.files[0];
            validateAndSetFile(selectedFile);
        }
    });

    function isValidExtension(filename) {
        var ext = filename.split('.').pop().toLowerCase();
        return (ext === 'gcode' || ext === 'nc' || ext === 'cnc' || ext === 'bin');
    }

    function validateAndSetFile(file) {
        var ext = file.name.split('.').pop().toLowerCase();
        var isBin = (ext === 'bin');
        var isValid = isValidExtension(file.name);

        if (!isValid) {
            showMessage('ERROR: Unsupported file type: .' + ext + '. Allowed: .gcode, .nc, .cnc, .bin', true);
            clearSelection();
            return;
        }

        updateSelectedFile(file);
        if (isBin) {
            printCheckbox.checked = false;
            printCheckbox.disabled = true;
            showMessage('Note: .bin (firmware) files cannot be auto-started. Upload only.', false);
        } else {
            printCheckbox.disabled = false;
            showMessage('Ready to upload: ' + file.name, false);
        }
        updateAllowedExtensionsMessage();
    }

    function uploadFile(file, printAfterUpload) {
        var formData = new FormData();
        formData.append('file', file);
        formData.append('print', printAfterUpload ? 'true' : 'false');
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/upload', true);

        xhr.upload.addEventListener('progress', function(e) {
            if (e.lengthComputable) {
                var percent = (e.loaded / e.total) * 100;
                progressBar.style.width = percent + '%';
                progressContainer.style.display = 'block';
            }
        });

        xhr.onload = function() {
            progressContainer.style.display = 'none';
            if (xhr.status === 200) {
                showMessage((printAfterUpload ? 'Print started' : 'Upload complete') + '! ' + xhr.responseText, false);
                clearSelection();
            } else {
                showMessage('Server error (' + xhr.status + '): ' + xhr.responseText, true);
            }
        };

        xhr.onerror = function() {
            progressContainer.style.display = 'none';
            showMessage('Network error - is the bridge running?', true);
        };

        xhr.send(formData);
    }

    uploadBtn.addEventListener('click', function() {
        if (!selectedFile) {
            showMessage('Please select a file first.', true);
            return;
        }
        var printMode = printCheckbox.checked;
        uploadFile(selectedFile, printMode);
    });

    printCheckbox.addEventListener('change', function() {
        updateAllowedExtensionsMessage();
    });

    // Fetch current toolhead and show allowed extensions
    fetch('/api/machine/status')
        .then(res => res.json())
        .then(data => {
            currentToolhead = data.toolhead;
            updateAllowedExtensionsMessage();
        })
        .catch(err => console.warn("Could not detect module", err));
</script>
</body>
</html>)";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeMonitorPage(SOCKET clientSocket) {
    std::string html =
        R"html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snapmaker Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: linear-gradient(135deg, #eef2f7 0%, #d9e2ec 100%);
            font-family: 'Segoe UI', 'Inter', system-ui, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 2rem;
        }
        .card {
            max-width: 850px;
            width: 100%;
            background: #ffffff;
            border-radius: 2rem;
            box-shadow: 0 20px 35px -12px rgba(0,0,0,0.15);
            overflow: hidden;
        }
        .header {
            background: #0a1c2f;
            padding: 1.2rem 2rem;
            display: flex;
            align-items: center;
            gap: 1rem;
            border-bottom: 3px solid #3498db;
        }
        .logo { width: 50px; height: 50px; }
        .logo svg { width: 100%; height: 100%; }
        .title h1 { color: white; font-size: 1.5rem; font-weight: 600; }
        .title p { color: #8aa9c9; font-size: 0.75rem; }
        .content { padding: 1.8rem; }
        .status-grid {
            background: #f8fafc;
            border-radius: 1.2rem;
            padding: 1rem 1.3rem;
            border: 1px solid #e2edf7;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            padding: 0.6rem 0;
            border-bottom: 1px solid #e2edf4;
            flex-wrap: wrap;
            gap: 0.5rem;
        }
        .info-row:last-child { border-bottom: none; }
        .label { font-weight: 600; color: #1e4663; }
        .value {
            font-family: monospace;
            background: #eef3fc;
            padding: 0.2rem 0.7rem;
            border-radius: 20px;
            color: #0a2e4a;
            word-break: break-all;
        }
        .progress-bar-container {
            background: #e0e0e0;
            border-radius: 30px;
            overflow: hidden;
            width: 100%;
            margin: 8px 0;
            height: 24px;
        }
        .progress-fill {
            background: #4caf50;
            width: 0%;
            height: 100%;
            color: white;
            text-align: center;
            line-height: 24px;
            font-size: 0.8rem;
            font-weight: bold;
        }
        .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 0.4rem;
            background: #e6f7e6;
            padding: 0.2rem 0.8rem;
            border-radius: 40px;
            color: #2c6e2c;
            font-weight: 500;
        }
        .status-badge::before { content: "*"; color: #2ecc71; margin-right: 4px; }
        .controls {
            margin-top: 1.2rem;
            display: flex;
            align-items: center;
            justify-content: flex-end;
            gap: 1rem;
            flex-wrap: wrap;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 46px;
            height: 24px;
        }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: 0.2s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: white;
            transition: 0.2s;
            border-radius: 50%;
        }
        input:checked + .slider { background-color: #4caf50; }
        input:checked + .slider:before { transform: translateX(22px); }
        button {
            background: #0a1c2f;
            color: white;
            border: none;
            padding: 0.5rem 1.2rem;
            border-radius: 2rem;
            cursor: pointer;
            font-weight: 500;
            display: inline-flex;
            align-items: center;
            gap: 8px;
        }
        button:hover { background: #1e4a76; }
        .footer {
            margin-top: 1.5rem;
            text-align: center;
            font-size: 0.7rem;
            color: #6c7a89;
            border-top: 1px solid #e2e8f0;
            padding-top: 1rem;
        }
        .grid-2col {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 1rem;
            margin-top: 0.5rem;
        }
        .module-grid {
            display: flex;
            flex-wrap: wrap;
            gap: 0.5rem;
            margin-top: 0.5rem;
        }
        .module-badge {
            background: #eef2f7;
            padding: 0.2rem 0.6rem;
            border-radius: 20px;
            font-size: 0.75rem;
        }
        @media (max-width: 650px) {
            .grid-2col { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
<div class="card">
    <div class="header">
        <div class="logo">
            <svg viewBox="0 0 100 100">
                <rect x="12" y="15" width="76" height="52" rx="6" fill="#000000"/>
                <rect x="16" y="19" width="68" height="44" rx="3" fill="#3498DB"/>
                <rect x="20" y="22" width="60" height="38" rx="2" fill="DarkBlue"/>
                <rect x="20" y="22" width="60" height="38" rx="2" fill="none" stroke="#2980B9" stroke-width="2.5"/>
                <polyline points="22,48 30,48 34,38 38,55 42,42 44,48 56,48 58,42 62,55 66,38 70,48 78,48" fill="none" stroke="#E74C3C" stroke-width="2"/>
                <text x="50" y="47" font-family="Arial" font-size="18" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                <rect x="35" y="67" width="30" height="6" rx="2" fill="#000000"/>
                <rect x="40" y="73" width="20" height="6" rx="2" fill="#000000"/>
                <circle cx="84" cy="68" r="2.5" fill="#2ECC71"/>
                <ellipse cx="32" cy="28" rx="12" ry="4" fill="rgba(255,255,255,0.06)" transform="rotate(-15 32 28)"/>
            </svg>
        </div>
        <div class="title">
            <h1>Snapmaker Monitor</h1>
            <p>Print Monitor & Machine Status</p>
        </div>
    </div>
    <div class="content">
        <div class="status-grid">
            <div class="info-row">
                <span class="label">Tool Head</span>
                <span id="toolHead" class="value">-</span>
            </div>
            <div class="info-row">
                <span class="label">Current Print File</span>
                <span id="currentFile" class="value">-</span>
            </div>
            <div class="info-row">
                <span class="label">Progress</span>
                <div style="flex:1; min-width: 150px;">
                    <div class="progress-bar-container">
                        <div id="progressFill" class="progress-fill">0%</div>
                    </div>
                </div>
            </div>
            <div class="info-row">
                <span class="label">Elapsed / Remaining</span>
                <span id="timeInfo" class="value">0s / -</span>
            </div>
            <div class="info-row">
                <span class="label">Line Progress</span>
                <span id="lineInfo" class="value">-</span>
            </div>
            <div class="info-row">
                <span class="label">Machine Status</span>
                <span id="machineState" class="value">-</span>
            </div>
        </div>

        <div id="toolheadSection" class="status-grid" style="margin-top: 1rem;"></div>
        <div id="modulesSection" class="status-grid" style="margin-top: 1rem;"></div>

        <div class="controls">
            <label for="refreshToggle" style="font-size:0.85rem;">Auto-refresh (2s)</label>
            <label class="switch">
                <input type="checkbox" id="refreshToggle" checked>
                <span class="slider"></span>
            </label>
            <button id="manualRefreshBtn">Refresh Now</button>
            <button id="consoleMonitorBtn">
                <svg width="18" height="18" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg" style="vertical-align: middle;">
                    <rect x="12" y="15" width="76" height="52" rx="6" fill="#FFFFFF"/>
                    <rect x="16" y="19" width="68" height="44" rx="3" fill="#3498DB"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="DarkBlue"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="none" stroke="#2980B9" stroke-width="2.5"/>
                    <polyline points="22,48 30,48 34,38 38,55 42,42 44,48 56,48 58,42 62,55 66,38 70,48 78,48" fill="none" stroke="#E74C3C" stroke-width="2"/>
                    <text x="50" y="47" font-family="Arial" font-size="18" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                    <rect x="35" y="67" width="30" height="6" rx="2" fill="#FFFFFF"/>
                    <rect x="40" y="73" width="20" height="6" rx="2" fill="#FFFFFF"/>
                    <circle cx="84" cy="68" r="2.5" fill="#2ECC71"/>
                </svg>
                Launch Console Monitor
            </button>
        </div>
        <div class="footer">
            Bridge running on port 8081 | Data refreshes when enabled<br>
            <span style="color: #e74c3c;">Do not expose this interface directly to the internet. Use on <strong>private, trusted networks</strong> only. If you need remote access, use a VPN.</span>
        </div>
    </div>
</div>

<script>
    let refreshInterval = null;
    const toggle = document.getElementById('refreshToggle');
    const manualBtn = document.getElementById('manualRefreshBtn');
    const consoleMonitorBtn = document.getElementById('consoleMonitorBtn');
    const currentFileSpan = document.getElementById('currentFile');
    const progressFillDiv = document.getElementById('progressFill');
    const timeInfoSpan = document.getElementById('timeInfo');
    const lineInfoSpan = document.getElementById('lineInfo');
    const machineStateSpan = document.getElementById('machineState');
    const toolHeadSpan = document.getElementById('toolHead');
    const toolheadDiv = document.getElementById('toolheadSection');
    const modulesDiv = document.getElementById('modulesSection');

    function formatSeconds(sec) {
        if (sec <= 0) return '0s';
        let hours = Math.floor(sec / 3600);
        let minutes = Math.floor((sec % 3600) / 60);
        let seconds = sec % 60;
        let parts = [];
        if (hours > 0) parts.push(hours + 'h');
        if (minutes > 0 || hours > 0) parts.push(minutes + 'm');
        parts.push(seconds + 's');
        return parts.join(' ');
    }

    async function fetchStatus() {
        try {
            const jobResp = await fetch('/api/live/job');
            const jobData = await jobResp.json();
            const fileName = jobData.fileName || '-';
            const progressPercent = Math.round(jobData.progress * 100);
            const printTime = jobData.elapsedTime || 0;
            let printTimeLeft = jobData.remainingTime || 0;
            const state = jobData.state || 'Unknown';
            const totalLines = jobData.totalLines || 0;
            const currentLine = jobData.currentLine || 0;

            let elapsedFormatted = formatSeconds(printTime);
            let remainingFormatted = '-';

            if (printTimeLeft > 0) {
                let eta = new Date(Date.now() + printTimeLeft * 1000);
                let now = new Date();
                let timeStr = eta.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' });
                let etaStr;
                if (eta.toDateString() !== now.toDateString()) {
                    let dateStr = eta.toLocaleDateString([], { month: 'short', day: 'numeric' });
                    etaStr = `(ETA ${dateStr} ${timeStr})`;
                } else {
                    etaStr = `(ETA ${timeStr})`;
                }
                remainingFormatted = formatSeconds(printTimeLeft) + ' ' + etaStr;
            } else if (totalLines > 0 && currentLine > 0) {
                let linePercent = Math.min(100, (currentLine * 100) / totalLines);
                remainingFormatted = 'Calculating... (' + linePercent.toFixed(0) + '% complete)';
            }

            currentFileSpan.innerText = fileName;
            progressFillDiv.style.width = progressPercent + '%';
            progressFillDiv.innerText = progressPercent + '%';
            timeInfoSpan.innerText = elapsedFormatted + ' / ' + remainingFormatted;
            lineInfoSpan.innerText = (totalLines > 0) ? currentLine + ' / ' + totalLines : '-';
            machineStateSpan.innerText = state;
)html"
R"html(
            const printerResp = await fetch('/api/live/printer');
            const printerData = await printerResp.json();
            const tool = printerData.toolHead || 'Unknown';
            toolHeadSpan.innerText = tool;
            let html = '';
            const custom = printerData.custom || {};
            if (custom.laser) {
                html += `<div class="info-row"><span class="label">Laser Power</span><span class="value">${custom.laser.power}%</span></div>`;
                html += `<div class="info-row"><span class="label">Focal Length</span><span class="value">${custom.laser.focalLength} mm</span></div>`;
                html += `<div class="info-row"><span class="label">Work Speed</span><span class="value">${custom.laser.workSpeed || 0} mm/min</span></div>`;
                html += `<div class="info-row"><span class="label">Camera</span><span class="value">${custom.laser.camera ? 'Present' : 'Not present'}</span></div>`;
            } else if (custom.cnc) {
                html += `<div class="info-row"><span class="label">Spindle Speed</span><span class="value">${custom.cnc.spindleSpeed} RPM</span></div>`;
                html += `<div class="info-row"><span class="label">Work Speed</span><span class="value">${custom.cnc.workSpeed || 0} mm/min</span></div>`;
            } else if (custom['3dp']) {
                const dp = custom['3dp'];
                html += `<div class="info-row"><span class="label">Nozzle 1 Temp</span><span class="value">${printerData.temperature?.tool0?.actual || 0} / ${printerData.temperature?.tool0?.target || 0} C</span></div>`;
                if (dp.nozzle2) {
                    html += `<div class="info-row"><span class="label">Nozzle 2 Temp</span><span class="value">${dp.nozzle2.actual || 0} / ${dp.nozzle2.target || 0} C</span></div>`;
                }
                html += `<div class="info-row"><span class="label">Bed Temp</span><span class="value">${printerData.temperature?.bed?.actual || 0} / ${printerData.temperature?.bed?.target || 0} C</span></div>`;
                html += `<div class="info-row"><span class="label">Filament</span><span class="value">${dp.filamentOut ? 'Out' : 'Loaded'}</span></div>`;
                html += `<div class="info-row"><span class="label">Print Status</span><span class="value">${dp.printStatus || 'Unknown'}</span></div>`;
            }
            if (html) {
                toolheadDiv.innerHTML = `<div style="margin-bottom:0.5rem; font-weight:600;">Toolhead Specifics</div>` + html;
                toolheadDiv.style.display = 'block';
            } else {
                toolheadDiv.style.display = 'none';
            }

            const modules = custom.modules || {};
            let moduleHtml = `<div style="margin-bottom:0.5rem; font-weight:600;">Modules</div><div class="module-grid">`;
            if (modules.enclosure) moduleHtml += `<span class="module-badge">Enclosure: Yes</span>`;
            if (modules.rotary) moduleHtml += `<span class="module-badge">Rotary: Yes</span>`;
            if (modules.doorOpen !== undefined) moduleHtml += `<span class="module-badge">Door: ${modules.doorOpen ? 'Open' : 'Closed'}</span>`;
            if (modules.emergencyStop) moduleHtml += `<span class="module-badge">E-Stop: Active</span>`;
            if (modules.airPurifier) moduleHtml += `<span class="module-badge">Air Purifier: Yes</span>`;
            if (!modules.enclosure && !modules.rotary && modules.doorOpen === undefined && !modules.emergencyStop && !modules.airPurifier) {
                moduleHtml += `<span class="module-badge">No additional modules detected</span>`;
            }
            moduleHtml += `</div>`;
            modulesDiv.innerHTML = moduleHtml;

        } catch (err) {
            console.warn('Status fetch error:', err);
        }
    }

    function startRefresh() {
        if (refreshInterval) clearInterval(refreshInterval);
        fetchStatus();
        refreshInterval = setInterval(fetchStatus, 2000);
    }

    function stopRefresh() {
        if (refreshInterval) {
            clearInterval(refreshInterval);
            refreshInterval = null;
        }
    }

    toggle.addEventListener('change', (e) => {
        if (e.target.checked) startRefresh();
        else stopRefresh();
    });
    manualBtn.addEventListener('click', () => { fetchStatus(); });
    consoleMonitorBtn.addEventListener('click', () => {
        fetch('/launch_monitor').catch(e => console.warn(e));
    });

    if (toggle.checked) {
        startRefresh();
    } else {
        fetchStatus();
    }
</script>
</body>
</html>)html";

    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeConfigPage(SOCKET clientSocket) {
    std::string html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Snapmaker LightBurn Host Configuration Editor</title>
    <script src="https://cdn.jsdelivr.net/npm/jsoneditor@10.4.3/dist/jsoneditor.min.js"></script>
    <link href="https://cdn.jsdelivr.net/npm/jsoneditor@10.4.3/dist/jsoneditor.min.css" rel="stylesheet">
    <style>
        body { font-family: sans-serif; margin: 20px; background: #f0f2f5; }
        .container { max-width: 1000px; margin: auto; background: white; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 20px; }
        .header { display: flex; align-items: center; gap: 12px; margin-bottom: 20px; }
        .header svg { width: 48px; height: 48px; }
        h1 { margin: 0; }
        .controls { margin-top: 20px; text-align: right; }
        .check-buttons { margin-top: 20px; padding-top: 20px; border-top: 1px solid #e0e0e0; display: flex; gap: 12px; justify-content: flex-start; }
        button { padding: 8px 16px; background: #0a1c2f; color: white; border: none; border-radius: 6px; cursor: pointer; margin-left: 10px; }
        button:hover { background: #1e4a76; }
        .check-btn { background: #3498db; }
        .check-btn:hover { background: #2980b9; }
        .status { margin-top: 10px; padding: 8px; border-radius: 6px; display: none; }
        .status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .status.info { background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }
        #jsoneditor { height: 500px; border: 1px solid #ccc; border-radius: 6px; }
        a { color: #3498db; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
<div class="container">
    <div class="header">
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="48" height="48">
            <rect x="10" y="10" width="80" height="80" rx="8" fill="#000000"/>
            <rect x="16" y="16" width="68" height="68" rx="6" fill="DarkBlue"/>
            <rect x="16" y="16" width="68" height="68" rx="6" fill="none" stroke="#3498DB" stroke-width="2.5"/>
            <rect x="28" y="28" width="44" height="4" rx="2" fill="#2980B9"/>
            <circle cx="60" cy="30" r="6" fill="#3498DB"/>
            <circle cx="60" cy="30" r="4" fill="#2ECC71"/>
            <rect x="28" y="68" width="44" height="4" rx="2" fill="#2980B9"/>
            <circle cx="40" cy="70" r="6" fill="#3498DB"/>
            <circle cx="40" cy="70" r="4" fill="#E74C3C"/>
            <circle cx="50" cy="50" r="14" fill="none" stroke="#3498DB" stroke-width="2.1"/>
            <circle cx="50" cy="50" r="12" fill="none" stroke="#2980B9" stroke-width="2.1"/>
            <text x="50" y="55" font-family="Arial, sans-serif" font-size="15" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
            <ellipse cx="32" cy="28" rx="12" ry="3" fill="rgba(255,255,255,0.06)" transform="rotate(-15 32 28)"/>
        </svg>
        <h1>Snapmaker LightBurn Host Configuration Editor</h1>
    </div>
    <p>Edit config.json directly - changes are applied immediately.</p>
    <div id="jsoneditor"></div>
    <div class="controls">
        <button id="saveBtn">Save Configuration</button>
        <button id="reloadBtn">Reload from Disk</button>
    </div>
    <div class="check-buttons">
        <button id="softcamCheckBtn" class="check-btn">Check Softcam Installation</button>
        <button id="lightburnCheckBtn" class="check-btn">Check LightBurn Camera</button>
    </div>
    <div id="checkStatus" class="status"></div>
<script>
    let editor;
    const statusDiv = document.getElementById('checkStatus');

    function showMessage(text, type) {
        statusDiv.textContent = text;
        statusDiv.className = 'status ' + type;
        statusDiv.style.display = 'block';
        setTimeout(() => { statusDiv.style.display = 'none'; }, 8000);
    }

    async function loadConfig() {
        try {
            const resp = await fetch('/config.json');
            if (!resp.ok) throw new Error('Failed to load config');
            const config = await resp.json();
            if (!editor) {
                const container = document.getElementById('jsoneditor');
                editor = new JSONEditor(container, {
                    modes: ['tree', 'code', 'form'],
                    mode: 'tree'
                });
            }
            editor.set(config);
            showMessage('Configuration loaded.', 'success');
        } catch (err) {
            showMessage('Error loading config: ' + err.message, 'error');
        }
    }

    async function saveConfig() {
        try {
            const updated = editor.get();
            const resp = await fetch('/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(updated)
            });
            const result = await resp.text();
            if (resp.ok) {
                showMessage('Configuration saved successfully!', 'success');
            } else {
                showMessage('Save failed: ' + result, 'error');
            }
        } catch (err) {
            showMessage('Error saving: ' + err.message, 'error');
        }
    }

    async function checkSoftcam() {
        showMessage('Checking Softcam installation...', 'info');
        try {
            const resp = await fetch('/api/check/softcam');
            const data = await resp.json();
            if (data.installed) {
                showMessage('Softcam is properly installed.', 'success');
            } else {
                showMessage('Softcam is NOT installed. Please install softcam for camera capture.', 'error');
            }
        } catch (err) {
            showMessage('Check failed: ' + err.message, 'error');
        }
    }

    async function checkLightburnCamera() {
        showMessage('Checking LightBurn camera setting...', 'info');
        try {
            const resp = await fetch('/api/check/lightburn');
            const data = await resp.json();
            if (data.configured) {
                showMessage('LightBurn camera setting is correct.', 'success');
            } else {
                showMessage('LightBurn camera setting is incorrect. Enable: Edit -> Settings -> Camera -> Use Default Camera Capture System', 'error');
            }
        } catch (err) {
            showMessage('Check failed: ' + err.message, 'error');
        }
    }

    document.getElementById('saveBtn').addEventListener('click', saveConfig);
    document.getElementById('reloadBtn').addEventListener('click', loadConfig);
    document.getElementById('softcamCheckBtn').addEventListener('click', checkSoftcam);
    document.getElementById('lightburnCheckBtn').addEventListener('click', checkLightburnCamera);
    loadConfig();
</script>
</body>
</html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeControlPage(SOCKET clientSocket) {
    std::string html = R"html(
<!DOCTYPE html>
<html>
<head><title>Snapmaker Control Panel</title><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>
body{font-family:Segoe UI,sans-serif;background:#f0f4f9;margin:0;padding:20px}
.container{max-width:1200px;margin:auto}
.card{background:white;border-radius:20px;box-shadow:0 10px 30px rgba(0,0,0,0.1);padding:1.5rem;margin-bottom:20px}
h2{color:#0a1c2f;margin-top:0}
.header{display:flex;align-items:center;gap:16px;margin-bottom:24px}
.header svg{width:52px;height:52px}
.header h1{color:#0a1c2f;margin:0;font-size:1.8rem}
.button-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;margin:15px 0}
button{background:#0a1c2f;color:white;border:none;padding:10px 16px;border-radius:40px;cursor:pointer;font-weight:bold}
button:hover{background:#1e4a76}
button:disabled{background:#6c7a89;cursor:not-allowed}
.reprint-btn{background:#e67e22;}
.reprint-btn:hover{background:#d35400;}
.error{color:#e74c3c;margin-top:10px}
.success{color:#2ecc71}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid #ddd;border-top-color:#3498db;border-radius:50%;animation:spin 0.8s linear infinite;vertical-align:middle;margin-left:8px}
@keyframes spin{to{transform:rotate(360deg)}}
select,input{padding:8px;border-radius:20px;border:1px solid #ccc;margin:0 8px}
.hidden{display:none}
.status{font-size:0.85rem;margin-top:8px}
.info{font-size:0.7rem;color:#6c7a89;margin-top:8px}
</style></head>
<body>
<div class="container">

<div class="header">
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="48" height="48">
    <circle cx="50" cy="50" r="46" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
    <circle cx="50" cy="50" r="32" fill="none" stroke="#3498db" stroke-width="2"/>
    <polygon points="50,12 44,22 48,22 48,34 52,34 52,22 56,22" fill="#2ecc71"/>
    <polygon points="50,88 44,78 48,78 48,66 52,66 52,78 56,78" fill="#e74c3c"/>
    <polygon points="12,50 22,44 22,48 34,48 34,52 22,52 22,56" fill="#e74c3c"/>
    <polygon points="88,50 78,44 78,48 66,48 66,52 78,52 78,56" fill="#2ecc71"/>
    <line x1="50" y1="18" x2="50" y2="82" stroke="#3498db" stroke-width="1.5" opacity="0.5"/>
    <line x1="18" y1="50" x2="82" y2="50" stroke="#3498db" stroke-width="1.5" opacity="0.5"/>
    <text x="50" y="60" font-family="Arial, sans-serif" font-size="28" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
    <ellipse cx="34" cy="34" rx="12" ry="8" fill="rgba(255,255,255,0.06)" transform="rotate(-30 34 34)"/>
  </svg>
  <h1>Snapmaker Control</h1>
</div>

<div class="card"><h2>Machine Control</h2>
<div class="button-grid">
<button id="homeBtn">Home</button>
<button id="reprintBtn" class="reprint-btn">Reprint Last File</button>
<button id="pauseBtn">Pause Print</button>
<button id="resumeBtn">Resume Print</button>
<button id="stopBtn">Stop Print</button>
</div></div>

<div id="enclosureCard" class="card hidden"><h2>Enclosure Control</h2>
<div><label>Fan Speed:</label>
<select id="fanSelect"><option value="0">Off</option><option value="25">25%</option><option value="50">50%</option><option value="75">75%</option><option value="100">100%</option></select>
<button id="setFanBtn">Set Fan</button></div>
<div style="margin-top:12px"><label>LED Brightness:</label>
<select id="ledSelect"><option value="0">Off</option><option value="25">25%</option><option value="50">50%</option><option value="75">75%</option><option value="100">100%</option></select>
<button id="setLedBtn">Set LED</button></div>
</div>

<div id="kasaCard" class="card hidden"><h2>Kasa Smart Plug</h2>
<div class="button-grid">
<button id="kasaOnBtn">Power On</button>
<button id="kasaOffBtn">Power Off</button>
</div>
<div id="kasaStatus" class="status"></div>
</div>

<div id="cameraCard" class="card hidden"><h2>Webcam (RTSP)</h2>
<div class="button-grid">
<button id="cameraStartBtn">Start Camera</button>
</div>
<div id="cameraStatus" class="status"></div>
<div class="info">Note: Close the VLC window manually to stop the camera</div>
</div>

<div id="messageArea" class="error"></div>
</div>
<script>
const homeBtn=document.getElementById('homeBtn'), pauseBtn=document.getElementById('pauseBtn');
const resumeBtn=document.getElementById('resumeBtn'), stopBtn=document.getElementById('stopBtn');
const setFanBtn=document.getElementById('setFanBtn'), setLedBtn=document.getElementById('setLedBtn');
const fanSelect=document.getElementById('fanSelect'), ledSelect=document.getElementById('ledSelect');
const enclosureCard=document.getElementById('enclosureCard');
const kasaCard=document.getElementById('kasaCard');
const kasaOnBtn=document.getElementById('kasaOnBtn');
const kasaOffBtn=document.getElementById('kasaOffBtn');
const kasaStatus=document.getElementById('kasaStatus');
const cameraCard=document.getElementById('cameraCard');
const cameraStartBtn=document.getElementById('cameraStartBtn');
const cameraStatus=document.getElementById('cameraStatus');
const messageDiv=document.getElementById('messageArea');

function showMessage(msg,isError){messageDiv.textContent=msg;messageDiv.className=isError?'error':'success';setTimeout(()=>messageDiv.textContent='',5000);}
function setLoading(btn,loading){btn.disabled=loading;btn.innerHTML=loading?btn.innerHTML+'<span class="spinner"></span>':btn.innerHTML.replace('<span class="spinner"></span>','');}

async function apiCall(url,options,successMsg,errorMsg){
    try{
        const resp=await fetch(url,options);
        const text=await resp.text();
        if(resp.ok){
            if(successMsg) showMessage(successMsg+': '+text,false);
            return {success:true,data:text};
        }else{
            showMessage((errorMsg||'Request failed')+': '+text,true);
            return {success:false,data:text};
        }
    }catch(e){
        showMessage('Network error: '+e.message,true);
        return {success:false,data:e.message};
    }
}

homeBtn.onclick=()=>{setLoading(homeBtn,true);apiCall('/api/home',{method:'POST'},'Home sent').finally(()=>setLoading(homeBtn,false));};
const reprintBtn=document.getElementById('reprintBtn');reprintBtn.onclick=async()=>{setLoading(reprintBtn,true);try{const resp=await fetch('/api/reprint',{method:'POST'});const text=await resp.text();if(resp.ok){showMessage('Reprint started: '+text,false);setTimeout(()=>{fetchStatus();},2000);}else{showMessage('Reprint failed: '+text,true);}}catch(err){showMessage('Network error: '+err.message,true);}finally{setLoading(reprintBtn,false);}};
pauseBtn.onclick=()=>{setLoading(pauseBtn,true);apiCall('/api/pause',{method:'POST'},'Pause sent').finally(()=>setLoading(pauseBtn,false));};
resumeBtn.onclick=()=>{setLoading(resumeBtn,true);apiCall('/api/resume',{method:'POST'},'Resume sent').finally(()=>setLoading(resumeBtn,false));};
stopBtn.onclick=()=>{if(confirm('Stop current print?')){setLoading(stopBtn,true);apiCall('/api/stop',{method:'POST'},'Stop sent').finally(()=>setLoading(stopBtn,false));}};

setFanBtn.onclick=()=>{const speed=fanSelect.value;setLoading(setFanBtn,true);apiCall('/api/enclosure/fan',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({speed:speed})},'Fan set').finally(()=>setLoading(setFanBtn,false));};
setLedBtn.onclick=()=>{const brightness=ledSelect.value;setLoading(setLedBtn,true);apiCall('/api/enclosure/led',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({brightness:brightness})},'LED set').finally(()=>setLoading(setLedBtn,false));};

kasaOnBtn.onclick=async()=>{setLoading(kasaOnBtn,true);const result=await apiCall('/api/kasa/on',{method:'POST'},'Power on');if(result.success) checkKasaStatus();setLoading(kasaOnBtn,false);};
kasaOffBtn.onclick=async()=>{setLoading(kasaOffBtn,true);const result=await apiCall('/api/kasa/off',{method:'POST'},'Power off');if(result.success) checkKasaStatus();setLoading(kasaOffBtn,false);};

cameraStartBtn.onclick=async()=>{setLoading(cameraStartBtn,true);await apiCall('/api/camera/start',{method:'POST'},'Camera started');setLoading(cameraStartBtn,false);};

async function checkKasaStatus(){
    try{
        const resp=await fetch('/api/kasa/status');
        const data=await resp.json();
        if(data.status==='ok'){
            kasaStatus.innerHTML='<span class="success">Plug is '+(data.state?'ON':'OFF')+'</span>';
        }else{
            kasaStatus.innerHTML='<span class="error">'+ (data.message || 'Cannot read status') +'</span>';
        }
    }catch(e){
        kasaStatus.innerHTML='<span class="error">Status error: '+e.message+'</span>';
    }
}

fetch('/config.json')
    .then(resp=>resp.json())
    .then(cfg=>{
        if(cfg.kasa && cfg.kasa.ipAddress && cfg.kasa.ipAddress!==''){
            kasaCard.classList.remove('hidden');
            checkKasaStatus();
            setInterval(checkKasaStatus,30000);
        }
        if(cfg.rtspCamera && cfg.rtspCamera.rtspUrl && cfg.rtspCamera.rtspUrl!==''){
            cameraCard.classList.remove('hidden');
        }else if(cfg.camera && cfg.camera.rtsp && cfg.camera.rtsp.rtspUrl && cfg.camera.rtsp.rtspUrl!==''){
            cameraCard.classList.remove('hidden');
        }
    })
    .catch(err=>{
        console.error('Failed to load config:', err);
    });

fetch('/api/machine/status')
    .then(resp=>resp.json())
    .then(s=>{if(s.enclosure) enclosureCard.classList.remove('hidden');})
    .catch(()=>{});
</script></body></html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeCapturePage(SOCKET clientSocket) {
    FileUploadServer::EnsureHttpCameraFeed();
    std::string html = R"html(
<!DOCTYPE html>
<html>
<head><title>Snapmaker Capture Image</title><meta charset="UTF-8"><style>
body{font-family:Segoe UI,sans-serif;background:#f0f4f9;margin:0;padding:20px}
.container{max-width:800px;margin:auto}
.card{background:white;border-radius:28px;box-shadow:0 10px 30px rgba(0,0,0,0.1);padding:2rem}
.header{display:flex;align-items:center;gap:16px;margin-bottom:24px}
.header svg{width:52px;height:52px}
.header h1{color:#0a1c2f;margin:0;font-size:1.8rem}
button{background:#0a1c2f;color:white;border:none;padding:12px 28px;font-size:1rem;font-weight:bold;border-radius:40px;cursor:pointer;margin-top:10px;margin-right:10px}
button:hover{background:#1e4a76}
button:disabled{background:#6c7a89;cursor:not-allowed}
.setup-btn{background:#e67e22}
.setup-btn:hover{background:#d35400}
.auto-btn{background:#3498db}
.auto-btn:hover{background:#2980b9}
.stop-btn{background:#e74c3c}
.stop-btn:hover{background:#c0392b}
.image-container{text-align:center;margin-top:20px;min-height:200px;background:#f8fafc;border-radius:12px;display:flex;align-items:center;justify-content:center}
img{max-width:100%;max-height:500px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.1);border:1px solid #ddd}
.error{color:#e74c3c;font-weight:bold;margin-top:10px}
.success{color:#27ae60;font-weight:bold}
.spinner{display:inline-block;width:20px;height:20px;border:3px solid #ddd;border-top-color:#3498db;border-radius:50%;animation:spin 1s linear infinite;vertical-align:middle;margin-left:10px}
@keyframes spin{to{transform:rotate(360deg)}}
.note{background:#fff3cd;border-left:4px solid #ffc107;padding:12px;margin:15px 0;border-radius:8px;font-size:0.9rem}
hr{margin:20px 0;border:0;border-top:1px solid #e2edf7}
.button-group{display:flex;flex-wrap:wrap;gap:10px;justify-content:center}
.info{font-size:0.85rem;color:#6c7a89;margin-top:10px}
.placeholder{color:#aaa;font-style:italic}
</style>
</head>
<body>
<div class="container">
<div class="card">
<div class="header">
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="52" height="52">
    <circle cx="50" cy="50" r="46" fill="#000000" stroke="#3498db" stroke-width="2.5"/>
    <rect x="24" y="34" width="52" height="36" rx="6" fill="#3498db"/>
    <circle cx="50" cy="52" r="16" fill="#0a1c2f" stroke="#e74c3c" stroke-width="2"/>
    <circle cx="50" cy="52" r="10" fill="none" stroke="#3498db" stroke-width="1.5"/>
    <text x="50" y="57" font-family="Arial,sans-serif" font-size="14" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
  </svg>
  <h1>Camera Capture</h1>
</div>

<div class="note">
  <strong>IMPORTANT:</strong> Run <strong>Machine Setup</strong> first (home and calibrate) before capturing.
</div>

<button id="setupBtn" class="setup-btn">Machine Setup (Home & Calibrate)</button>
<div id="setupMsg" style="margin-top:8px; font-size:0.85rem;"></div>
<hr>

<div class="button-group">
  <button id="captureBtn">Capture Single Image</button>
  <button id="autoStartBtn" class="auto-btn">Start Auto Capture (2s)</button>
  <button id="autoStopBtn" class="stop-btn" disabled>Stop Auto Capture</button>
</div>

<div id="autoStatus" class="info" style="display:none; text-align:center;">Auto capture running (every 2 seconds). Click Stop to end.</div>

<div class="image-container" id="imageContainer">
  <div class="placeholder">No image captured yet. Click a button above.</div>
</div>
<div id="errorMsg" class="error" style="display:none"></div>

<div class="url-note" style="background:#f8fafc; border:1px solid #e2edf4; border-radius:12px; padding:16px; margin-top:24px;">
    <div style="font-weight:600; margin-bottom:12px; color:#0a1c2f;">[INFO] Image URLs - for use in LightBurn or other software:</div>
    <ul style="margin:0 0 12px 0; padding-left:20px;">
        <li style="margin-bottom:8px;"><code style="background:#eef2f7; padding:2px 6px; border-radius:4px;">/api/capture</code> - triggers a new capture and returns the JPEG image.</li>
        <li style="margin-bottom:8px;"><code style="background:#eef2f7; padding:2px 6px; border-radius:4px;">/api/capture/latest</code> - returns the most recently captured image (no new capture).</li>
        <li style="margin-bottom:8px;">When this page loads, the virtual camera (Softcam) is initialized with a test pattern. Image captures will update the camera feed automatically.</li>
    </ul>
    <div style="font-size:0.8rem; color:#6c7a89;">Use these URLs directly in LightBurn's network camera source or any image viewer.</div>
</div>

</div>
</div>

<script>
var setupBtn = document.getElementById('setupBtn');
var setupMsg = document.getElementById('setupMsg');
var captureBtn = document.getElementById('captureBtn');
var autoStartBtn = document.getElementById('autoStartBtn');
var autoStopBtn = document.getElementById('autoStopBtn');
var autoStatus = document.getElementById('autoStatus');
var imageContainer = document.getElementById('imageContainer');
var errorDiv = document.getElementById('errorMsg');

var autoInterval = null;
var isCapturing = false;
var currentImageUrl = null;

function showMessage(div, msg, isError) {
    div.textContent = msg;
    div.className = isError ? 'error' : 'success';
    div.style.display = 'block';
    setTimeout(function() { div.style.display = 'none'; }, 5000);
}

function displayImage(blob) {
    if (currentImageUrl) {
        URL.revokeObjectURL(currentImageUrl);
        currentImageUrl = null;
    }
    var imgUrl = URL.createObjectURL(blob);
    currentImageUrl = imgUrl;
    var img = document.createElement('img');
    img.src = imgUrl;
    img.alt = 'Captured image';
    img.onload = function() {
        imageContainer.innerHTML = '';
        imageContainer.appendChild(img);
    };
    img.onerror = function() {
        console.error('Image failed to load');
        imageContainer.innerHTML = '<div class="placeholder">Failed to load image</div>';
        showMessage(errorDiv, 'Failed to load captured image', true);
    };
}

function captureOne() {
    if (isCapturing) return Promise.resolve(false);
    isCapturing = true;
    imageContainer.innerHTML = '<div class="spinner" style="width:40px;height:40px;"></div>';
    return fetch('/api/capture')
        .then(function(resp) {
            if (!resp.ok) throw new Error('HTTP ' + resp.status);
            return resp.blob();
        })
        .then(function(blob) {
            if (blob.size < 100) throw new Error('Image too small (maybe empty)');
            displayImage(blob);
            return true;
        })
        .catch(function(err) {
            console.error('Capture error:', err);
            imageContainer.innerHTML = '<div class="placeholder">Capture failed: ' + err.message + '</div>';
            showMessage(errorDiv, 'Capture failed: ' + err.message, true);
            return false;
        })
        .finally(function() {
            isCapturing = false;
        });
}

function startAutoCapture() {
    if (autoInterval) return;
    autoStartBtn.disabled = true;
    autoStopBtn.disabled = false;
    captureBtn.disabled = true;               // Disable single capture button
    autoStatus.style.display = 'block';
    captureOne().then(function() {
        autoInterval = setInterval(function() {
            if (!isCapturing) captureOne();
        }, 2000);
    });
}

function stopAutoCapture() {
    if (autoInterval) {
        clearInterval(autoInterval);
        autoInterval = null;
    }
    autoStartBtn.disabled = false;
    autoStopBtn.disabled = true;
    captureBtn.disabled = false;              // Re-enable single capture button
    autoStatus.style.display = 'none';
}

setupBtn.onclick = function() {
    setupBtn.disabled = true;
    setupBtn.innerHTML = 'Setting up <span class="spinner"></span>';
    setupMsg.style.display = 'block';
    fetch('/api/machine/setup', { method: 'POST' })
        .then(function(resp) { return resp.json(); })
        .then(function(data) {
            if (data.status === 'ok') {
                showMessage(setupMsg, data.message, false);
            } else {
                showMessage(setupMsg, data.message || 'Setup failed', true);
            }
        })
        .catch(function(err) {
            showMessage(setupMsg, 'Network error: ' + err.message, true);
        })
        .finally(function() {
            setupBtn.disabled = false;
            setupBtn.innerHTML = 'Machine Setup (Home & Calibrate)';
        });
};

captureBtn.onclick = function() {
    if (captureBtn.disabled) return;
    captureBtn.disabled = true;
    captureBtn.innerHTML = 'Capturing <span class="spinner"></span>';
    captureOne().finally(function() {
        captureBtn.disabled = false;
        captureBtn.innerHTML = 'Capture Single Image';
    });
};

autoStartBtn.onclick = startAutoCapture;
autoStopBtn.onclick = stopAutoCapture;
</script>
</body></html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeThicknessPage(SOCKET clientSocket) {
    std::string html = R"html(
<!DOCTYPE html>
<html>
<head><title>Snapmaker Measure Thickness</title><meta charset="UTF-8"><style>
*{box-sizing:border-box}
body{font-family:Segoe UI,sans-serif;background:#f0f4f9;margin:0;padding:20px}
.container{max-width:650px;margin:auto}
.card{background:white;border-radius:28px;box-shadow:0 10px 30px rgba(0,0,0,0.1);padding:2rem}
.header{display:flex;align-items:center;gap:16px;margin-bottom:24px}
.header svg{width:52px;height:52px}
.header h1{color:#0a1c2f;margin:0}
button{background:#0a1c2f;color:white;border:none;padding:12px 28px;font-size:1rem;font-weight:bold;border-radius:40px;cursor:pointer;width:100%;margin-top:10px}
button:hover{background:#1e4a76}
button:disabled{background:#6c7a89}
.setup-btn{background:#e67e22}
.setup-btn:hover{background:#d35400}
.radio-group{display:flex;gap:15px;margin:20px 0;flex-wrap:wrap}
.radio-label{background:#eef2f7;padding:10px 16px;border-radius:40px;cursor:pointer;font-weight:500;border:2px solid #cbd5e1}
.radio-label.selected{background:#3498db;color:white;border-color:#3498db}
.radio-label input{display:none}
.custom-panel{background:#f8fafc;border-radius:20px;padding:15px;margin:10px 0 20px;border:1px solid #e2edf7}
.coord-row{display:flex;gap:20px;align-items:center;margin-bottom:12px;flex-wrap:wrap}
.coord-row label{width:60px;font-weight:600}
.coord-row input{padding:8px;border-radius:8px;border:1px solid #ccc;width:100px;text-align:center}
.result{background:#f8fafc;border-radius:20px;padding:1rem;margin-top:20px;border:1px solid #e2edf7}
.thickness-value{font-size:2rem;font-weight:bold;color:#2ecc71}
.error{color:#e74c3c;font-weight:bold}
.success{color:#27ae60;font-weight:bold}
.spinner{display:inline-block;width:20px;height:20px;border:3px solid #ddd;border-top-color:#3498db;border-radius:50%;animation:spin 1s linear infinite;vertical-align:middle;margin-left:10px}
@keyframes spin{to{transform:rotate(360deg)}}
.note{background:#fff3cd;border-left:4px solid #ffc107;padding:12px;margin:15px 0;border-radius:8px;font-size:0.9rem}
hr{margin:20px 0;border:0;border-top:1px solid #e2edf7}
.info{font-size:0.8rem;color:#6c7a89;margin-top:8px}
</style>
</head>
<body>
<div class="container">
<div class="card">
<div class="header">
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="52" height="52">
    <circle cx="50" cy="50" r="46" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
    <text x="50" y="60" font-family="Arial,sans-serif" font-size="32" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
    <rect x="20" y="18" width="60" height="6" rx="1" fill="#3498db"/>
    <rect x="26" y="18" width="1.5" height="3" fill="#fff"/>
    <rect x="34" y="18" width="1.5" height="4" fill="#fff"/>
    <rect x="42" y="18" width="1.5" height="3" fill="#fff"/>
    <rect x="50" y="18" width="1.5" height="4" fill="#fff"/>
    <rect x="58" y="18" width="1.5" height="3" fill="#fff"/>
    <rect x="66" y="18" width="1.5" height="4" fill="#fff"/>
    <rect x="74" y="18" width="1.5" height="3" fill="#fff"/>
    <rect x="20" y="76" width="60" height="6" rx="1" fill="#3498db"/>
    <rect x="26" y="79" width="1.5" height="3" fill="#fff"/>
    <rect x="34" y="78" width="1.5" height="4" fill="#fff"/>
    <rect x="42" y="79" width="1.5" height="3" fill="#fff"/>
    <rect x="50" y="78" width="1.5" height="4" fill="#fff"/>
    <rect x="58" y="79" width="1.5" height="3" fill="#fff"/>
    <rect x="66" y="78" width="1.5" height="4" fill="#fff"/>
    <rect x="74" y="79" width="1.5" height="3" fill="#fff"/>
    <rect x="18" y="20" width="6" height="60" rx="1" fill="#3498db"/>
    <rect x="18" y="26" width="3" height="1.5" fill="#fff"/>
    <rect x="18" y="34" width="4" height="1.5" fill="#fff"/>
    <rect x="18" y="42" width="3" height="1.5" fill="#fff"/>
    <rect x="18" y="50" width="4" height="1.5" fill="#fff"/>
    <rect x="18" y="58" width="3" height="1.5" fill="#fff"/>
    <rect x="18" y="66" width="4" height="1.5" fill="#fff"/>
    <rect x="18" y="74" width="3" height="1.5" fill="#fff"/>
    <rect x="76" y="20" width="6" height="60" rx="1" fill="#3498db"/>
    <rect x="79" y="26" width="3" height="1.5" fill="#fff"/>
    <rect x="78" y="34" width="4" height="1.5" fill="#fff"/>
    <rect x="79" y="42" width="3" height="1.5" fill="#fff"/>
    <rect x="78" y="50" width="4" height="1.5" fill="#fff"/>
    <rect x="79" y="58" width="3" height="1.5" fill="#fff"/>
    <rect x="78" y="66" width="4" height="1.5" fill="#fff"/>
    <rect x="79" y="74" width="3" height="1.5" fill="#fff"/>
    <rect x="26" y="24" width="14" height="14" fill="none" stroke="#e74c3c" stroke-width="2" opacity="0.8"/>
    <rect x="60" y="62" width="14" height="14" fill="none" stroke="#2ecc71" stroke-width="2" opacity="0.8"/>
    <ellipse cx="32" cy="32" rx="10" ry="6" fill="rgba(255,255,255,0.06)" transform="rotate(-25 32 32)"/>
  </svg>
  <h1>Material Thickness</h1>
</div>

<div class="note">
  <strong>IMPORTANT:</strong> Run <strong>Machine Setup</strong> first to home the machine.
</div>

<button id="setupBtn" class="setup-btn">Machine Setup (Home and Calibrate)</button>
<div id="setupMsg" style="margin-top:8px; font-size:0.85rem;"></div>
<hr>

<div class="radio-group">
  <label class="radio-label selected" id="labelDefault">
    <input type="radio" name="mode" value="default" checked> Default (Center)
  </label>
  <label class="radio-label" id="labelFrontLeft">
    <input type="radio" name="mode" value="frontleft"> Front-Left (Preset)
  </label>
  <label class="radio-label" id="labelCustom">
    <input type="radio" name="mode" value="custom"> Custom X/Y
  </label>
</div>

<div id="customPanel" class="custom-panel" style="display:none">
  <div class="coord-row"><label>X (mm):</label><input type="number" id="customX" step="0.1" value="99.0" placeholder="X"></div>
  <div class="coord-row"><label>Y (mm):</label><input type="number" id="customY" step="0.1" value="22.0" placeholder="Y"></div>
  <div class="info">A350 front-left approx 99,22 ; A250 front-left approx 66,15</div>
</div>

<button id="measureBtn">Measure Thickness</button>
<div id="result" class="result" style="display:none"><div><span class="thickness-value" id="thicknessVal">--</span> mm</div></div>
<div id="errorMsg" class="error" style="display:none"></div>
</div>
</div>

<script>
var setupBtn = document.getElementById('setupBtn');
var setupMsg = document.getElementById('setupMsg');
var measureBtn = document.getElementById('measureBtn');
var thicknessSpan = document.getElementById('thicknessVal');
var errorDiv = document.getElementById('errorMsg');
var resultDiv = document.getElementById('result');
var radios = document.querySelectorAll('input[name="mode"]');
var labels = document.querySelectorAll('.radio-label');
var customPanel = document.getElementById('customPanel');
var selectedMode = 'default';

function updateUIMode() {
    for (var i = 0; i < labels.length; i++) labels[i].classList.remove('selected');
    if (selectedMode === 'default') document.getElementById('labelDefault').classList.add('selected');
    else if (selectedMode === 'frontleft') document.getElementById('labelFrontLeft').classList.add('selected');
    else document.getElementById('labelCustom').classList.add('selected');
    customPanel.style.display = (selectedMode === 'custom') ? 'block' : 'none';
}

for (var i = 0; i < radios.length; i++) {
    radios[i].addEventListener('change', function(e) {
        if (e.target.checked) selectedMode = e.target.value;
        updateUIMode();
    });
}
updateUIMode();

function showMessage(div, msg, isError) {
    div.textContent = msg;
    div.className = isError ? 'error' : 'success';
    div.style.display = 'block';
    setTimeout(function() { div.style.display = 'none'; }, 5000);
}

setupBtn.onclick = async function() {
    setupBtn.disabled = true;
    setupBtn.innerHTML = 'Setting up <span class="spinner"></span>';
    setupMsg.style.display = 'block';
    try {
        var resp = await fetch('/api/machine/setup', { method: 'POST' });
        var data = await resp.json();
        if (resp.ok && data.status === 'ok') {
            showMessage(setupMsg, data.message, false);
        } else {
            showMessage(setupMsg, data.message || 'Setup failed', true);
        }
    } catch (err) {
        showMessage(setupMsg, 'Network error: ' + err.message, true);
    } finally {
        setupBtn.disabled = false;
        setupBtn.innerHTML = 'Machine Setup (Home and Calibrate)';
    }
};

measureBtn.onclick = async function() {
    measureBtn.disabled = true;
    measureBtn.innerHTML = 'Measuring <span class="spinner"></span>';
    resultDiv.style.display = 'none';
    errorDiv.style.display = 'none';

    var url = '/api/thickness';
    if (selectedMode === 'frontleft') {
        url += '?x=99.0&y=22.0';
    } else if (selectedMode === 'custom') {
        var x = parseFloat(document.getElementById('customX').value);
        var y = parseFloat(document.getElementById('customY').value);
        if (isNaN(x) || isNaN(y)) {
            showMessage(errorDiv, 'Invalid X or Y coordinates', true);
            measureBtn.disabled = false;
            measureBtn.innerHTML = 'Measure Thickness';
            return;
        }
        url += '?x=' + x + '&y=' + y;
    }

    try {
        var resp = await fetch(url);
        var data = await resp.json();
        if (resp.ok && data.status === 'ok') {
            thicknessSpan.textContent = data.thickness_mm.toFixed(1);
            resultDiv.style.display = 'block';
        } else {
            showMessage(errorDiv, data.message || 'Measurement failed', true);
        }
    } catch (err) {
        showMessage(errorDiv, 'Network error: ' + err.message, true);
    } finally {
        measureBtn.disabled = false;
        measureBtn.innerHTML = 'Measure Thickness';
    }
};
</script>
</body></html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeAboutPage(SOCKET clientSocket) {
    // Read README.txt file
    std::string readmeContent;
    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(NULL, exePathW, MAX_PATH);
    fs::path exeDir = fs::path(exePathW).parent_path();
    fs::path readmePath = exeDir / "README.txt";

    if (!fs::exists(readmePath)) {
        readmePath = exeDir / "README.md";
    }

    if (fs::exists(readmePath)) {
        std::ifstream file(readmePath);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            readmeContent = ss.str();
            file.close();
        }
    }

    if (readmeContent.empty()) {
        readmeContent = "README file not found. Please ensure README.txt is in the application directory.";
    }

    // Escape HTML special characters
    std::string escapedContent;
    for (char c : readmeContent) {
        if (c == '<') escapedContent += "&lt;";
        else if (c == '>') escapedContent += "&gt;";
        else if (c == '&') escapedContent += "&amp;";
        else escapedContent += c;
    }

    std::string html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snapmaker LightBurn Host - About</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: linear-gradient(135deg, #eef2f7 0%, #d9e2ec 100%);
            font-family: 'Segoe UI', 'Inter', system-ui, sans-serif;
            padding: 2rem;
        }
        .container { max-width: 1000px; margin: 0 auto; }
        .card {
            background: #ffffff;
            border-radius: 2rem;
            box-shadow: 0 20px 35px -12px rgba(0,0,0,0.15);
            overflow: hidden;
        }
        .header {
            background: #0a1c2f;
            padding: 1.5rem 2rem;
            display: flex;
            align-items: center;
            gap: 1rem;
            border-bottom: 3px solid #3498db;
        }
        .logo { width: 60px; height: 60px; }
        .logo svg { width: 100%; height: 100%; }
        .title h1 { color: white; font-size: 1.8rem; font-weight: 600; }
        .title p { color: #8aa9c9; font-size: 0.85rem; }
        .content { padding: 2rem; }
        .readme-content {
            background: #f8fafc;
            border-radius: 1rem;
            padding: 1.5rem;
            border: 1px solid #e2edf7;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 0.85rem;
            line-height: 1.5;
            color: #1e4663;
            white-space: pre-wrap;
            word-wrap: break-word;
            overflow-x: auto;
        }
        .readme-content h1, .readme-content h2, .readme-content h3 { color: #0a1c2f; margin-top: 1rem; margin-bottom: 0.5rem; }
        .readme-content h1 { font-size: 1.5rem; }
        .readme-content h2 { font-size: 1.3rem; border-bottom: 1px solid #e2edf4; padding-bottom: 0.3rem; }
        .readme-content h3 { font-size: 1.1rem; }
        .readme-content code {
            background: #eef3fc;
            padding: 0.2rem 0.4rem;
            border-radius: 4px;
            font-family: monospace;
        }
        .readme-content pre {
            background: #0a1c2f;
            color: #2ecc71;
            padding: 1rem;
            border-radius: 8px;
            overflow-x: auto;
            margin: 1rem 0;
        }
        .readme-content pre code { background: none; padding: 0; color: inherit; }
        .readme-content a { color: #3498db; text-decoration: none; }
        .readme-content a:hover { text-decoration: underline; }
        .readme-content hr { border: none; border-top: 1px solid #e2edf4; margin: 1rem 0; }
        .footer {
            margin-top: 1.5rem;
            text-align: center;
            font-size: 0.7rem;
            color: #6c7a89;
            border-top: 1px solid #e2e8f0;
            padding-top: 1rem;
        }
        @media (max-width: 550px) {
            .header { flex-direction: column; text-align: center; }
            .content { padding: 1rem; }
        }
    </style>
</head>
<body>
<div class="container">
    <div class="card">
        <div class="header">
            <div class="logo">
                <svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                    <circle cx="50" cy="50" r="46" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
                    <circle cx="50" cy="50" r="38" fill="none" stroke="#3498db" stroke-width="1.5" stroke-dasharray="6 4"/>
                    <text x="52" y="82" font-family="Arial, sans-serif" font-size="80" font-weight="bold" fill="#3498db" text-anchor="middle" opacity="0.35">?</text>
                    <text x="50" y="62" font-family="Arial, sans-serif" font-size="34" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                    <circle cx="50" cy="14" r="3" fill="#2ecc71"/>
                    <circle cx="50" cy="86" r="3" fill="#e74c3c"/>
                    <circle cx="14" cy="50" r="3" fill="#f1c40f"/>
                    <circle cx="86" cy="50" r="3" fill="#3498db"/>
                    <circle cx="24" cy="24" r="2" fill="#3498db" opacity="0.6"/>
                    <circle cx="76" cy="24" r="2" fill="#3498db" opacity="0.6"/>
                    <circle cx="24" cy="76" r="2" fill="#3498db" opacity="0.6"/>
                    <circle cx="76" cy="76" r="2" fill="#3498db" opacity="0.6"/>
                    <circle cx="50" cy="50" r="46" fill="none" stroke="#2ecc71" stroke-width="1" opacity="0.3"/>
                </svg>
            </div>
            <div class="title">
                <h1>About</h1>
            </div>
        </div>
        <div class="content">
            <div class="readme-content">
)html" + escapedContent + R"html(
            </div>
        </div>
        <div class="footer">
            <p>Bridge running on port 8081</p>
        </div>
    </div>
</div>
</body>
</html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeGCodePage(SOCKET clientSocket) {
    std::string html = R"html(
<!DOCTYPE html>
<html>
<head><title>Snapmaker G-code Console</title><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>
body{font-family:'Segoe UI',monospace;background:#0a1c2f;margin:0;padding:20px}
.container{max-width:900px;margin:auto}
.card{background:white;border-radius:20px;box-shadow:0 10px 30px rgba(0,0,0,0.3);padding:1.8rem}
.header{display:flex;align-items:center;gap:16px;margin-bottom:24px}
.header svg{width:52px;height:52px}
.header h1{color:#0a1c2f;margin:0;font-size:1.8rem}
.input-group{display:flex;gap:12px;margin-bottom:20px;flex-wrap:wrap}
.command-input{flex:1;padding:12px 16px;font-size:1rem;font-family:monospace;border:2px solid #3498db;border-radius:40px;outline:none}
.command-input:focus{border-color:#2ecc71}
button{background:#0a1c2f;color:white;border:none;padding:12px 28px;font-size:1rem;font-weight:bold;border-radius:40px;cursor:pointer}
button:hover{background:#1e4a76}
button:disabled{background:#6c7a89;cursor:not-allowed}
.quick-commands{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:20px}
.quick-btn{background:#3498db;padding:8px 16px;font-size:0.85rem}
.quick-btn:hover{background:#2980b9}
.response-area{background:#0a1c2f;color:#2ecc71;padding:16px;border-radius:16px;font-family:monospace;font-size:0.9rem;margin-top:20px;max-height:300px;overflow:auto}
.response-line{margin:4px 0;border-bottom:1px solid #1e4663;padding:4px}
.history-area{background:#f0f4f9;border-radius:16px;padding:12px;margin-top:16px;max-height:200px;overflow:auto}
.history-title{font-weight:bold;margin-bottom:8px;color:#0a1c2f}
.history-item{font-family:monospace;padding:4px 8px;margin:2px 0;background:#e2edf7;border-radius:8px;cursor:pointer}
.history-item:hover{background:#3498db;color:white}
.macro-area{background:#f0f4f9;border-radius:16px;padding:12px;margin-top:16px;border:1px solid #e2edf4}
.macro-area h3{color:#0a1c2f;margin-bottom:10px;font-size:1rem}
.macro-row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}
.macro-row select{flex:1;min-width:150px;padding:10px 14px;border-radius:40px;border:2px solid #3498db;font-size:1rem;background:white}
.macro-row button{background:#2ecc71;padding:10px 20px;font-size:0.9rem}
.macro-row button:hover{background:#27ae60}
.macro-output{background:#0a1c2f;color:#2ecc71;padding:12px;border-radius:12px;margin-top:10px;font-family:monospace;font-size:0.85rem;max-height:200px;overflow:auto;white-space:pre-wrap}
.macro-line-ok{color:#2ecc71}
.macro-line-fail{color:#e74c3c}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid #ddd;border-top-color:#3498db;border-radius:50%;animation:spin 0.8s linear infinite;vertical-align:middle;margin-left:8px}
@keyframes spin{to{transform:rotate(360deg)}}
.error{color:#e74c3c}
.success{color:#2ecc71}
.macro-status{font-size:0.8rem;color:#6c7a89;margin-top:6px}
</style></head>
<body>
<div class="container">
<div class="card">
<div class="header">
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="48" height="48">
  <circle cx="50" cy="50" r="46" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
  <rect x="28" y="32" width="44" height="36" rx="4" fill="#3498db"/>
  <text x="50" y="56" font-family="monospace" font-size="16" font-weight="bold" fill="#ffffff" text-anchor="middle">&lt;S/&gt;</text>
  <circle cx="38" cy="42" r="2" fill="#2ecc71"/>
  <circle cx="62" cy="42" r="2" fill="#e74c3c"/>
  <line x1="34" y1="62" x2="66" y2="62" stroke="#2ecc71" stroke-width="1.5" stroke-dasharray="3,2"/>
  <ellipse cx="32" cy="34" rx="10" ry="6" fill="rgba(255,255,255,0.06)" transform="rotate(-20 32 34)"/>
</svg>
  <h1>G-code Console</h1>
</div>
<p>Send raw G-code commands directly to your Snapmaker.</p>

<div class="quick-commands">
  <button class="quick-btn" data-cmd="G28">Home (G28)</button>
  <button class="quick-btn" data-cmd="G90">Absolute Pos (G90)</button>
  <button class="quick-btn" data-cmd="G91">Relative Pos (G91)</button>
  <button class="quick-btn" data-cmd="G53">Machine Coord (G53)</button>
  <button class="quick-btn" data-cmd="G54">Work Coord (G54)</button>
</div>

<div class="input-group">
  <input type="text" id="gcodeInput" class="command-input" placeholder="Enter G-code command (e.g., G28, G1 X10 Y20, M105)" autocomplete="off">
  <button id="sendBtn">Send Command</button>
</div>

<div class="response-area" id="responseArea">
  <div class="response-line">Ready. Enter a G-code command above.</div>
</div>

<div class="history-area">
  <div class="history-title">Command History (click to reuse)</div>
  <div id="historyList">No commands sent yet.</div>
</div>

<!-- Macro Section -->
<div class="macro-area">
  <h3>Macros</h3>
  <p style="font-size:0.8rem;color:#6c7a89;margin-bottom:10px;">Run .txt macro files from the macros folder. Each line is sent as a G-code command.</p>
  <div class="macro-row">
    <select id="macroSelect">
      <option value="">-- Select a macro --</option>
    </select>
    <button id="runMacroBtn">Run Macro</button>
    <span id="macroStatus" class="macro-status"></span>
  </div>
  <div id="macroOutput" class="macro-output">Select a macro and click Run to execute.</div>
</div>

</div>
</div>
<script>
const gcodeInput=document.getElementById('gcodeInput');
const sendBtn=document.getElementById('sendBtn');
const responseArea=document.getElementById('responseArea');
const historyList=document.getElementById('historyList');
const macroSelect=document.getElementById('macroSelect');
const runMacroBtn=document.getElementById('runMacroBtn');
const macroOutput=document.getElementById('macroOutput');
const macroStatus=document.getElementById('macroStatus');
let history=[];

function addResponse(text,isError){
    const line=document.createElement('div');
    line.className='response-line';
    line.style.color=isError?'#e74c3c':'#2ecc71';
    line.textContent='> '+text;
    responseArea.appendChild(line);
    line.scrollIntoView({behavior:'smooth',block:'nearest'});
}

function addToHistory(cmd){
    if(history.length===0 || history[history.length-1]!==cmd){
        history.push(cmd);
        if(history.length>20) history.shift();
        renderHistory();
    }
}

function renderHistory(){
    if(history.length===0){
        historyList.innerHTML='No commands sent yet.';
        return;
    }
    let html='';
    for(let i=history.length-1;i>=0;i--){
        html+=`<div class="history-item" data-cmd="${history[i].replace(/"/g,'&quot;')}">${history[i]}</div>`;
    }
    historyList.innerHTML=html;
    document.querySelectorAll('.history-item').forEach(item=>{
        item.addEventListener('click',()=>{
            gcodeInput.value=item.dataset.cmd;
            gcodeInput.focus();
        });
    });
}

async function sendGcode(gcode){
    sendBtn.disabled=true;
    sendBtn.innerHTML='Sending <span class="spinner"></span>';
    addResponse('Sending: '+gcode,false);
    try{
        const resp=await fetch('/api/gcode/send',{
            method:'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({gcode:gcode})
        });
        const text=await resp.text();
        if(resp.ok){
            addResponse('Response: '+text,false);
            addToHistory(gcode);
        }else{
            addResponse('Error: ('+resp.status+') '+text,true);
        }
    }catch(err){
        addResponse('Network error: '+err.message,true);
    }finally{
        sendBtn.disabled=false;
        sendBtn.innerHTML='Send Command';
        gcodeInput.value='';
        gcodeInput.focus();
    }
}

sendBtn.addEventListener('click',()=>{
    const cmd=gcodeInput.value.trim().toUpperCase();
    if(!cmd){
        addResponse('Please enter a G-code command.',true);
        return;
    }
    sendGcode(cmd);
});

gcodeInput.addEventListener('keypress',(e)=>{
    if(e.key==='Enter'){
        const cmd=gcodeInput.value.trim().toUpperCase();
        if(cmd) sendGcode(cmd);
    }
});

document.querySelectorAll('.quick-btn').forEach(btn=>{
    btn.addEventListener('click',()=>{
        const cmd=btn.dataset.cmd;
        gcodeInput.value=cmd;
        sendGcode(cmd);
    });
});

// ===== MACRO FUNCTIONS =====
async function loadMacros() {
    try {
        const resp = await fetch('/api/macros');
        if (!resp.ok) throw new Error('Failed to load macros');
        const data = await resp.json();
        macroSelect.innerHTML = '<option value="">-- Select a macro --</option>';
        data.forEach(macro => {
            const opt = document.createElement('option');
            opt.value = macro.name;
            opt.textContent = macro.name;
            macroSelect.appendChild(opt);
        });
        if (data.length === 0) {
            const opt = document.createElement('option');
            opt.value = '';
            opt.textContent = 'No macros found';
            opt.disabled = true;
            macroSelect.appendChild(opt);
        }
    } catch (err) {
        console.error('Failed to load macros:', err);
        macroSelect.innerHTML = '<option value="">Error loading macros</option>';
    }
}

async function runMacro(name) {
    runMacroBtn.disabled = true;
    runMacroBtn.innerHTML = 'Running <span class="spinner"></span>';
    macroStatus.textContent = 'Executing...';
    macroOutput.innerHTML = 'Running macro...';
    try {
        const resp = await fetch('/api/macros/run', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ macro: name })
        });
        const data = await resp.json();
        if (data.success) {
            let html = '';
            let okCount = 0;
            let failCount = 0;
            data.results.forEach(r => {
                // FIX: treat any status that does NOT start with "FAIL:" as success
                const isOk = !r.status.startsWith('FAIL:');
                if (isOk) okCount++; else failCount++;
                const cls = isOk ? 'macro-line-ok' : 'macro-line-fail';
                html += `<div class="${cls}">${r.command} -> ${r.status}</div>`;
            });
            macroOutput.innerHTML = html;
            macroStatus.textContent = 'Done: ' + okCount + ' OK, ' + failCount + ' failed.';
        } else {
            macroOutput.innerHTML = 'Error: ' + (data.message || 'Unknown error');
            macroStatus.textContent = 'Error';
        }
    } catch (err) {
        macroOutput.innerHTML = 'Network error: ' + err.message;
        macroStatus.textContent = 'Error';
    } finally {
        runMacroBtn.disabled = false;
        runMacroBtn.innerHTML = 'Run Macro';
    }
}

runMacroBtn.addEventListener('click', () => {
    const name = macroSelect.value;
    if (!name) {
        macroOutput.innerHTML = 'Please select a macro.';
        return;
    }
    runMacro(name);
});

loadMacros();

gcodeInput.focus();
</script>
</body></html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeRootPage(SOCKET clientSocket, const std::string& lastUploadedFilename, const std::string& ipAddress) {
    std::string html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="2">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snapmaker Bridge</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: linear-gradient(135deg, #eef2f7 0%, #d9e2ec 100%);
            font-family: 'Segoe UI', 'Inter', system-ui, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 2rem;
        }
        .card {
            max-width: 700px;
            width: 100%;
            background: #ffffff;
            border-radius: 2rem;
            box-shadow: 0 20px 35px -12px rgba(0,0,0,0.15);
            overflow: hidden;
        }
        .header {
            background: #0a1c2f;
            padding: 1.2rem 2rem;
            display: flex;
            align-items: center;
            gap: 1rem;
            border-bottom: 3px solid #3498db;
        }
        .logo { width: 50px; height: 50px; }
        .logo svg { width: 100%; height: 100%; }
        .title h1 { color: white; font-size: 1.5rem; font-weight: 600; }
        .title p { color: #8aa9c9; font-size: 0.75rem; }
        .content { padding: 1.8rem; }
        .status-grid {
            background: #f8fafc;
            border-radius: 1.2rem;
            padding: 1rem 1.3rem;
            border: 1px solid #e2edf7;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            padding: 0.6rem 0;
            border-bottom: 1px solid #e2edf4;
            flex-wrap: wrap;
            gap: 0.5rem;
        }
        .info-row:last-child { border-bottom: none; }
        .label { font-weight: 600; color: #1e4663; }
        .value {
            font-family: monospace;
            background: #eef3fc;
            padding: 0.2rem 0.7rem;
            border-radius: 20px;
            color: #0a2e4a;
            word-break: break-all;
        }
        .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 0.4rem;
            background: #e6f7e6;
            padding: 0.2rem 0.8rem;
            border-radius: 40px;
            color: #2c6e2c;
            font-weight: 500;
        }
        .status-badge::before { content: "*"; color: #2ecc71; margin-right: 4px; }
        .button-group {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 0.8rem;
            margin-top: 1rem;
        }
        button, .button-link {
            background: #0a1c2f;
            color: white;
            border: none;
            padding: 0.5rem 1.2rem;
            border-radius: 2rem;
            cursor: pointer;
            font-weight: 500;
            display: inline-flex;
            align-items: center;
            gap: 8px;
            text-decoration: none;
            font-size: 1rem;
        }
        button:hover, .button-link:hover { background: #1e4a76; }
        .footer {
            margin-top: 1rem;
            text-align: center;
            font-size: 0.7rem;
            color: #6c7a89;
            border-top: 1px solid #e2e8f0;
            padding-top: 1rem;
        }
        @media (max-width: 550px) {
            .info-row { flex-direction: column; align-items: flex-start; }
            .value { max-width: 100%; }
            .header { flex-direction: column; text-align: center; }
        }
    </style>
</head>
<body>
<div class="card">
    <div class="header">
        <div class="logo">
            <svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                <rect x="8" y="5" width="84" height="90" rx="4" fill="#000000"/>
                <rect x="4" y="15" width="6" height="20" rx="1" fill="#000000"/>
                <circle cx="7" cy="19" r="2" fill="#3498DB"/>
                <circle cx="7" cy="31" r="2" fill="#3498DB"/>
                <rect x="90" y="15" width="6" height="20" rx="1" fill="#000000"/>
                <circle cx="93" cy="19" r="2" fill="#3498DB"/>
                <circle cx="93" cy="31" r="2" fill="#3498DB"/>
                <rect x="4" y="55" width="6" height="20" rx="1" fill="#000000"/>
                <circle cx="7" cy="59" r="2" fill="#3498DB"/>
                <circle cx="7" cy="71" r="2" fill="#3498DB"/>
                <rect x="90" y="55" width="6" height="20" rx="1" fill="#000000"/>
                <circle cx="93" cy="59" r="2" fill="#3498DB"/>
                <circle cx="93" cy="71" r="2" fill="#3498DB"/>
                <rect x="20" y="10" width="60" height="3" rx="1" fill="#2980B9"/>
                <rect x="20" y="15" width="60" height="3" rx="1" fill="#2980B9"/>
                <rect x="14" y="20" width="72" height="56" rx="3" fill="#3498DB"/>
                <rect x="18" y="24" width="64" height="48" rx="2" fill="DarkBlue"/>
                <circle cx="50" cy="50" r="11" fill="none" stroke="#3498DB" stroke-width="2.1"/>
                <circle cx="50" cy="50" r="9" fill="none" stroke="#2980B9" stroke-width="2.1"/>
                <text x="50" y="55" font-family="Arial, sans-serif" font-size="14" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                <circle cx="26" cy="33" r="2.5" fill="#2ECC71"/>
                <circle cx="34" cy="33" r="2.5" fill="#3498DB"/>
                <circle cx="42" cy="33" r="2.5" fill="#F1C40F"/>
                <circle cx="50" cy="33" r="2.5" fill="#E74C3C"/>
                <rect x="22" y="44" width="10" height="7" rx="1" fill="#000000" stroke="#2980B9" stroke-width="0.8"/>
                <rect x="22" y="54" width="10" height="7" rx="1" fill="#000000" stroke="#2980B9" stroke-width="0.8"/>
                <rect x="70" y="45" width="10" height="14" rx="2" fill="#000000" stroke="#3498DB" stroke-width="1"/>
                <rect x="20" y="78" width="60" height="3" rx="1" fill="#2980B9"/>
                <rect x="20" y="83" width="60" height="3" rx="1" fill="#2980B9"/>
            </svg>
        </div>
        <div class="title">
            <h1>Snapmaker Bridge</h1>
            <p>OctoPrint Bridge - Remote printing & monitoring</p>
        </div>
    </div>
    <div class="content">
        <div class="status-grid">
            <div class="info-row">
                <span class="label">HTTP Upload Server Status</span>
                <span class="status-badge">Running</span>
            </div>
            <div class="info-row">
                <span class="label">Last Uploaded File</span>
                <span class="value">)html" + lastUploadedFilename + R"html(</span>
            </div>
            <div class="info-row">
                <span class="label">Snapmaker IP</span>
                <span class="value">)html" + ipAddress + R"html(</span>
            </div>
        </div>
        <div class="button-group">
            <!-- Console monitor button -->
            <button id="monitorBtn">
                <svg width="20" height="20" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg" style="vertical-align: middle;">
                    <rect x="12" y="15" width="76" height="52" rx="6" fill="#FFFFFF"/>
                    <rect x="16" y="19" width="68" height="44" rx="3" fill="#3498DB"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="DarkBlue"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="none" stroke="#2980B9" stroke-width="2.5"/>
                    <polyline points="22,48 30,48 34,38 38,55 42,42 44,48 56,48 58,42 62,55 66,38 70,48 78,48" fill="none" stroke="#E74C3C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                    <text x="50" y="47" font-family="Arial" font-size="18" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                    <rect x="35" y="67" width="30" height="6" rx="2" fill="#FFFFFF"/>
                    <rect x="40" y="73" width="20" height="6" rx="2" fill="#FFFFFF"/>
                    <circle cx="84" cy="68" r="2.5" fill="#2ECC71"/>
                    <ellipse cx="32" cy="28" rx="12" ry="4" fill="rgba(255,255,255,0.06)" transform="rotate(-15 32 28)"/>
                </svg>
                Launch Print Monitor (Console)
            </button>

            <!-- Web Monitor button -->
            <button id="webMonitorBtn" class="button-link">
                <svg width="20" height="20" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg" style="vertical-align: middle;">
                    <rect x="12" y="15" width="76" height="52" rx="6" fill="#FFFFFF"/>
                    <rect x="16" y="19" width="68" height="44" rx="3" fill="#3498DB"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="DarkBlue"/>
                    <rect x="20" y="22" width="60" height="38" rx="2" fill="none" stroke="#2980B9" stroke-width="2.5"/>
                    <polyline points="22,48 30,48 34,38 38,55 42,42 44,48 56,48 58,42 62,55 66,38 70,48 78,48" fill="none" stroke="#E74C3C" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                    <text x="50" y="47" font-family="Arial" font-size="18" font-weight="bold" fill="#FFFFFF" text-anchor="middle">S</text>
                    <rect x="35" y="67" width="30" height="6" rx="2" fill="#FFFFFF"/>
                    <rect x="40" y="73" width="20" height="6" rx="2" fill="#FFFFFF"/>
                    <circle cx="84" cy="68" r="2.5" fill="#2ECC71"/>
                    <ellipse cx="32" cy="28" rx="12" ry="4" fill="rgba(255,255,255,0.06)" transform="rotate(-15 32 28)"/>
                </svg>
                Launch Web Monitor
            </button>

            <!-- Mobile Monitor button -->
            <button id="mobileMonitorBtn">
                <svg width="20" height="20" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg" style="vertical-align: middle;">
                    <rect x="18" y="4" width="64" height="92" rx="14" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
                    <rect x="24" y="12" width="52" height="68" rx="6" fill="#1e3a5f"/>
                    <circle cx="50" cy="46" r="18" fill="none" stroke="#3498db" stroke-width="2" opacity="0.5"/>
                    <text x="50" y="56" font-family="Arial" font-size="28" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
                    <circle cx="50" cy="89" r="4" fill="none" stroke="#3498db" stroke-width="2"/>
                    <circle cx="46" cy="9" r="2.5" fill="#2ecc71"/>
                    <circle cx="56" cy="9" r="2.5" fill="#e74c3c"/>
                    <rect x="40" y="81" width="20" height="2" rx="1" fill="#3498db" opacity="0.6"/>
                </svg>
                Launch Mobile Monitor
            </button>
        </div>
        <div class="footer">
            Auto-detects OctoPrint uploads - Supports "Upload" and "Upload & Print"<br>
            Bridge running on port 8081<br>
            This page auto-refreshes every 2 seconds.<br>
            <span style="color: #e74c3c;">Do not expose this interface directly to the internet. Use on <strong>private, trusted networks</strong> only. If you need remote access, use a VPN.</span>
        </div>
    </div>
</div>
<script>
    // Console monitor button – launches new console window
    document.getElementById('monitorBtn').addEventListener('click', function() {
        fetch('/launch_monitor').catch(e => console.warn(e));
    });

    // Web monitor button – tries new window, falls back to same window if blocked
    document.getElementById('webMonitorBtn').addEventListener('click', function() {
        var newWin = window.open('/monitor', '_blank');
        if (!newWin || newWin.closed || typeof newWin.closed == 'undefined') {
            window.location.href = '/monitor';
        }
    });

    // Mobile monitor button – opens mobile page in new tab
    document.getElementById('mobileMonitorBtn').addEventListener('click', function() {
        window.open('/mobile', '_blank');
    });
</script>
</body>
</html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeMobilePage(SOCKET clientSocket) {
    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::string rtspUrl = cfg.rtspCamera.rtspUrl;

    std::string html;

    // ---------- Part 1: Head and Styles ----------
    html += R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Snapmaker Mobile</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', sans-serif; background: #0a1c2f; color: #fff; padding: 16px; min-height: 100vh; }
        .card { background: #1e3a5f; border-radius: 16px; padding: 16px; margin-bottom: 16px; border-left: 4px solid #3498db; }
        .card .title { font-size: 0.8rem; color: #8aa9c9; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 4px; }
        .card .value { font-size: 1.4rem; font-weight: bold; }
        .flex { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 12px; }
        .status-badge { padding: 4px 12px; border-radius: 20px; font-size: 0.9rem; font-weight: 600; }
        .badge-printing { background: #2ecc71; color: #0a1c2f; }
        .badge-paused { background: #f39c12; color: #0a1c2f; }
        .badge-idle { background: #3498db; color: #fff; }
        .badge-offline { background: #e74c3c; color: #fff; }
        .badge-on { background: #2ecc71; color: #0a1c2f; }
        .badge-off { background: #e74c3c; color: #fff; }
        .badge-unknown { background: #7f8c8d; color: #fff; }
        .progress-bar { width: 100%; height: 8px; background: #2c3e50; border-radius: 4px; margin: 8px 0; overflow: hidden; }
        .progress-fill { height: 100%; background: #2ecc71; width: 0%; transition: width 0.3s; }
        .button-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-top: 12px; }
        .btn { padding: 12px; border: none; border-radius: 40px; font-weight: bold; font-size: 0.9rem; cursor: pointer; background: #0a1c2f; color: #fff; transition: 0.2s; }
        .btn:active { transform: scale(0.96); }
        .btn-pause { background: #f39c12; }
        .btn-resume { background: #2ecc71; }
        .btn-stop { background: #e74c3c; }
        .btn-camera { background: #3498db; margin-top: 8px; width: 100%; }
        .btn:disabled { opacity: 0.4; pointer-events: none; }
        .btn-kasa-on { background: #2ecc71; }
        .btn-kasa-off { background: #e74c3c; }
        .footer { text-align: center; font-size: 0.7rem; color: #6c7a89; margin-top: 20px; border-top: 1px solid #1e3a5f; padding-top: 12px; }
        .camera-note { font-size: 0.8rem; color: #8aa9c9; margin-top: 8px; text-align: center; }
        .module-detail { font-size: 0.9rem; margin: 4px 0; }
        .kasa-row { display: flex; gap: 12px; align-items: center; margin-top: 8px; flex-wrap: wrap; }
        .hidden { display: none; }
        .status { font-size: 0.85rem; margin-top: 8px; }
        .success { color: #2ecc71; }
        .error { color: #e74c3c; }
        .header { display: flex; align-items: center; gap: 16px; margin-bottom: 20px; }
        .header svg { flex-shrink: 0; }
        .header h1 { font-size: 1.6rem; color: #fff; margin: 0; }
        .header p { color: #8aa9c9; font-size: 0.8rem; margin: 0; }
    </style>
</head>
<body>
<div id="app">
    <!-- Header with phone icon and title -->
    <div class="header">
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="64" height="64">
            <rect x="18" y="4" width="64" height="92" rx="14" fill="#0a1c2f" stroke="#3498db" stroke-width="2.5"/>
            <rect x="24" y="12" width="52" height="68" rx="6" fill="#1e3a5f"/>
            <circle cx="50" cy="46" r="18" fill="none" stroke="#3498db" stroke-width="2" opacity="0.5"/>
            <text x="50" y="56" font-family="Arial, Helvetica, sans-serif" font-size="28" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
            <circle cx="50" cy="89" r="4" fill="none" stroke="#3498db" stroke-width="2"/>
            <circle cx="46" cy="9" r="2.5" fill="#2ecc71"/>
            <circle cx="56" cy="9" r="2.5" fill="#e74c3c"/>
            <rect x="40" y="81" width="20" height="2" rx="1" fill="#3498db" opacity="0.6"/>
        </svg>
        <div>
            <h1>Snapmaker Mobile</h1>
            <p>Monitor & control your printer</p>
        </div>
    </div>
)html";

    // ---------- Part 2: Cards (Status, Temperatures, Module Details, Controls, Kasa, Camera) ----------
    html += R"html(
    <!-- Machine Status -->
    <div class="card">
        <div class="flex">
            <span class="title">Status</span>
            <span id="statusBadge" class="status-badge">--</span>
        </div>
        <div style="margin-top: 6px;">
            <span id="toolHead" class="value" style="font-size:1.2rem;">--</span>
            <span style="margin-left:12px; font-size:0.9rem; color:#8aa9c9;" id="fileName">--</span>
        </div>
        <div class="progress-bar"><div class="progress-fill" id="progressFill"></div></div>
        <div class="flex" style="font-size:0.9rem;">
            <span id="progressText">0%</span>
            <span id="timeInfo">-- / --</span>
        </div>
    </div>

    <!-- Temperatures (hidden for Laser, CNC, and Offline) -->
    <div class="card" id="tempCard">
        <div class="title">Temperatures</div>
        <div class="flex" id="tempDisplay">
            <span>Nozzle 1: <span id="nozzleTemp">--</span> C</span>
            <span id="nozzle2Wrap" style="display:none;">Nozzle 2: <span id="nozzle2Temp">--</span> C</span>
            <span>Bed: <span id="bedTemp">--</span> C</span>
        </div>
    </div>

    <!-- Module Details (Filament, Laser, CNC specifics) -->
    <div class="card" id="moduleCard" style="display:none;">
        <div class="title">Module Details</div>
        <div id="moduleDetails"></div>
    </div>

    <!-- Control Buttons -->
    <div class="card">
        <div class="button-grid">
            <button class="btn btn-pause" id="pauseBtn">Pause</button>
            <button class="btn btn-resume" id="resumeBtn">Resume</button>
            <button class="btn btn-stop" id="stopBtn">Stop</button>
        </div>
    </div>

    <!-- Kasa Smart Plug (hidden if not configured) -->
    <div class="card hidden" id="kasaCard">
        <div class="flex">
            <span class="title">Kasa Smart Plug</span>
            <span id="kasaStatusBadge" class="status-badge badge-unknown">Unknown</span>
        </div>
        <div class="kasa-row">
            <button class="btn btn-kasa-on" id="kasaOnBtn">Turn On</button>
            <button class="btn btn-kasa-off" id="kasaOffBtn">Turn Off</button>
            <span style="font-size:0.8rem; color:#8aa9c9;" id="kasaIpDisplay"></span>
        </div>
        <div id="kasaStatus" class="status"></div>
    </div>

    <!-- RTSP Camera -->
    <div class="card">
        <div class="title">RTSP Camera</div>
        <button class="btn btn-camera" id="cameraBtn">Open RTSP Camera</button>
        <div class="camera-note">Requires VLC or an RTSP player.</div>
    </div>

<div class="footer">Snapmaker Bridge - Polling every 2s<br>            <span style="color: #e74c3c;">Do not expose this interface directly to the internet. Use on <strong>private, trusted networks</strong> only. If you need remote access, use a VPN.</span></div>
</div>

)html";

    // ---------- Part 3: JavaScript ----------
    html += R"html(
<script>
    let jobInterval = null;
    const statusBadge = document.getElementById('statusBadge');
    const toolHeadSpan = document.getElementById('toolHead');
    const fileNameSpan = document.getElementById('fileName');
    const progressFill = document.getElementById('progressFill');
    const progressText = document.getElementById('progressText');
    const timeInfo = document.getElementById('timeInfo');
    const nozzleTemp = document.getElementById('nozzleTemp');
    const nozzle2Temp = document.getElementById('nozzle2Temp');
    const nozzle2Wrap = document.getElementById('nozzle2Wrap');
    const bedTemp = document.getElementById('bedTemp');
    const moduleCard = document.getElementById('moduleCard');
    const moduleDetails = document.getElementById('moduleDetails');
    const pauseBtn = document.getElementById('pauseBtn');
    const resumeBtn = document.getElementById('resumeBtn');
    const stopBtn = document.getElementById('stopBtn');
    const cameraBtn = document.getElementById('cameraBtn');
    const kasaCard = document.getElementById('kasaCard');
    const kasaStatusBadge = document.getElementById('kasaStatusBadge');
    const kasaOnBtn = document.getElementById('kasaOnBtn');
    const kasaOffBtn = document.getElementById('kasaOffBtn');
    const kasaStatus = document.getElementById('kasaStatus');

    let kasaConfigured = false;

    function formatTime(sec) {
        if (!sec || sec < 0) return '--';
        let h = Math.floor(sec / 3600);
        let m = Math.floor((sec % 3600) / 60);
        let s = Math.floor(sec % 60);
        if (h > 0) return h + 'h ' + m + 'm ' + s + 's';
        if (m > 0) return m + 'm ' + s + 's';
        return s + 's';
    }

    function updateJob(data) {
        let progress = data.progress || 0;
        let fileName = data.fileName || 'No file';
        let elapsed = data.elapsedTime || 0;
        let remaining = data.remainingTime || 0;
        let state = data.state || 'Unknown';
        fileNameSpan.textContent = fileName;
        let percent = Math.round(progress * 100);
        progressFill.style.width = percent + '%';
        progressText.textContent = percent + '%';
        timeInfo.textContent = formatTime(elapsed) + ' / ' + (remaining > 0 ? formatTime(remaining) : '--');
        let badgeClass = 'badge-';
        let label = state;
        if (state === 'RUNNING' || state === 'Printing') { badgeClass += 'printing'; label = 'Printing'; }
        else if (state === 'PAUSED' || state === 'Paused') { badgeClass += 'paused'; label = 'Paused'; }
        else if (state === 'IDLE' || state === 'Idle' || state === 'Operational') { badgeClass += 'idle'; label = 'Idle'; }
        else { badgeClass += 'offline'; label = 'Offline'; }
        statusBadge.textContent = label;
        statusBadge.className = 'status-badge ' + badgeClass;
        // Disable buttons when offline
        pauseBtn.disabled = (state !== 'RUNNING' && state !== 'Printing');
        resumeBtn.disabled = (state !== 'PAUSED' && state !== 'Paused');
        stopBtn.disabled = (state !== 'RUNNING' && state !== 'Printing' && state !== 'PAUSED' && state !== 'Paused');
    }

    function updatePrinter(data) {
        let tool = data.toolHead || 'Unknown';
        toolHeadSpan.textContent = tool;

        // Hide temperature card for Laser, CNC, or Offline
        let tempCard = document.getElementById('tempCard');
        if (tempCard) {
            if (tool === 'Laser' || tool === 'CNC' || tool === 'Offline') {
                tempCard.style.display = 'none';
            } else {
                tempCard.style.display = 'block';
            }
        }

        let temp = data.temperature || {};
        let custom = data.custom || {};
        let nozzle = (temp.tool0 && temp.tool0.actual) || 0;
        let bed = (temp.bed && temp.bed.actual) || 0;
        nozzleTemp.textContent = nozzle.toFixed(1);
        bedTemp.textContent = bed.toFixed(1);

        let nozzle2 = 0;
        if (custom['3dp'] && custom['3dp'].nozzle2) {
            nozzle2 = custom['3dp'].nozzle2.actual || 0;
        } else if (temp.tool1) {
            nozzle2 = temp.tool1.actual || 0;
        }
        if (nozzle2 > 0) {
            nozzle2Wrap.style.display = 'inline';
            nozzle2Temp.textContent = nozzle2.toFixed(1);
        } else {
            nozzle2Wrap.style.display = 'none';
        }

        let html = '';
        if (custom['3dp']) {
            let dp = custom['3dp'];
            if (dp.filamentOut !== undefined) {
                html += '<div class="module-detail">Filament: ' + (dp.filamentOut ? 'Out' : 'Loaded') + '</div>';
            }
        } else if (custom.laser) {
            let ls = custom.laser;
            html += '<div class="module-detail">Power: ' + ls.power + '%</div>';
            if (ls.focalLength) html += '<div class="module-detail">Focal Length: ' + ls.focalLength + ' mm</div>';
            if (ls.workSpeed) html += '<div class="module-detail">Work Speed: ' + ls.workSpeed + ' mm/min</div>';
        } else if (custom.cnc) {
            let cnc = custom.cnc;
            if (cnc.spindleSpeed) html += '<div class="module-detail">Spindle Speed: ' + cnc.spindleSpeed + ' RPM</div>';
            if (cnc.workSpeed) html += '<div class="module-detail">Work Speed: ' + cnc.workSpeed + ' mm/min</div>';
        }
        if (html) {
            moduleDetails.innerHTML = html;
            moduleCard.style.display = 'block';
        } else {
            moduleCard.style.display = 'none';
        }
    }

    function updateKasaStatus(data) {
        let badge = kasaStatusBadge;
        let statusMsg = '';
        if (data && data.status === 'ok') {
            let isOn = data.state === true || data.state === 1;
            if (isOn) {
                badge.textContent = 'ON';
                badge.className = 'status-badge badge-on';
                statusMsg = 'Plug is ON';
                kasaOnBtn.disabled = true;
                kasaOffBtn.disabled = false;
            } else {
                badge.textContent = 'OFF';
                badge.className = 'status-badge badge-off';
                statusMsg = 'Plug is OFF';
                kasaOnBtn.disabled = false;
                kasaOffBtn.disabled = true;
            }
            kasaStatus.innerHTML = '<span class="success">' + statusMsg + '</span>';
        } else {
            badge.textContent = 'Unknown';
            badge.className = 'status-badge badge-unknown';
            let msg = (data && data.message) ? data.message : 'Cannot read status';
            kasaStatus.innerHTML = '<span class="error">' + msg + '</span>';
            kasaOnBtn.disabled = false;
            kasaOffBtn.disabled = false;
        }
    }

    function fetchStatus() {
        fetch('/api/live/job')
            .then(res => res.json())
            .then(updateJob)
            .catch(() => {});
        fetch('/api/live/printer')
            .then(res => res.json())
            .then(updatePrinter)
            .catch(() => {});
        if (kasaConfigured) {
            fetch('/api/kasa/status')
                .then(res => {
                    if (!res.ok) throw new Error('HTTP ' + res.status);
                    return res.json();
                })
                .then(updateKasaStatus)
                .catch(err => {
                    kasaStatus.innerHTML = '<span class="error">Status error: ' + err.message + '</span>';
                    kasaStatusBadge.textContent = 'Unknown';
                    kasaStatusBadge.className = 'status-badge badge-unknown';
                    kasaOnBtn.disabled = false;
                    kasaOffBtn.disabled = false;
                });
        }
    }

    function sendCommand(url, btn) {
        if (btn) btn.disabled = true;
        fetch(url, { method: 'POST' })
            .then(() => {
                setTimeout(() => { if (btn) btn.disabled = false; }, 2000);
                fetchStatus();
            })
            .catch(() => {
                if (btn) btn.disabled = false;
            });
    }

    pauseBtn.addEventListener('click', function() { sendCommand('/api/pause', this); });
    resumeBtn.addEventListener('click', function() { sendCommand('/api/resume', this); });
    stopBtn.addEventListener('click', function() {
        if (confirm('Stop current print?')) {
            sendCommand('/api/stop', this);
        }
    });

    cameraBtn.addEventListener('click', function() {
        var rtspUrl = '##RTSP_URL##';
        if (!rtspUrl || rtspUrl === '') {
            alert('RTSP camera not configured.');
            return;
        }
        window.open(rtspUrl, '_blank');
    });

    kasaOnBtn.addEventListener('click', function() { sendCommand('/api/kasa/on', this); });
    kasaOffBtn.addEventListener('click', function() { sendCommand('/api/kasa/off', this); });

    fetch('/config.json')
        .then(resp => resp.json())
        .then(cfg => {
            if (cfg.kasa && cfg.kasa.ipAddress && cfg.kasa.ipAddress !== '') {
                kasaConfigured = true;
                kasaCard.classList.remove('hidden');
                document.getElementById('kasaIpDisplay').textContent = cfg.kasa.ipAddress;
                fetch('/api/kasa/status')
                    .then(res => {
                        if (!res.ok) throw new Error('HTTP ' + res.status);
                        return res.json();
                    })
                    .then(updateKasaStatus)
                    .catch(err => {
                        kasaStatus.innerHTML = '<span class="error">Initial status error: ' + err.message + '</span>';
                        kasaStatusBadge.textContent = 'Unknown';
                        kasaStatusBadge.className = 'status-badge badge-unknown';
                    });
            }
        })
        .catch(err => console.error('Failed to load config:', err));

    fetchStatus();
    jobInterval = setInterval(fetchStatus, 2000);

    window.addEventListener('beforeunload', function() {
        if (jobInterval) clearInterval(jobInterval);
    });
</script>
</body>
</html>
)html";

    size_t pos = html.find("##RTSP_URL##");
    if (pos != std::string::npos) {
        html.replace(pos, 12, rtspUrl);
    }

    SendHttpResponse(clientSocket, 200, "text/html", html);
}

void ServeDiscoverPage(SOCKET clientSocket) {
    std::string html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snapmaker Network Scanner</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: linear-gradient(135deg, #eef2f7 0%, #d9e2ec 100%);
            font-family: 'Segoe UI', 'Inter', system-ui, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 2rem;
        }
        .card {
            max-width: 750px;
            width: 100%;
            background: #ffffff;
            border-radius: 2rem;
            box-shadow: 0 20px 35px -12px rgba(0,0,0,0.15);
            overflow: hidden;
        }
        .header {
            background: #0a1c2f;
            padding: 1.2rem 2rem;
            display: flex;
            align-items: center;
            gap: 1rem;
            border-bottom: 3px solid #3498db;
        }
        .logo { width: 50px; height: 50px; }
        .logo svg { width: 100%; height: 100%; }
        .title h1 {
            color: white;
            font-size: 1.5rem;
            font-weight: 600;
        }
        .title p {
            color: #8aa9c9;
            font-size: 0.75rem;
        }
        .content {
            padding: 1.8rem;
        }
        .subtitle {
            color: #1e4663;
            font-size: 0.95rem;
            margin-bottom: 1.5rem;
            border-bottom: 1px solid #e2edf4;
            padding-bottom: 0.8rem;
        }
        .scan-btn {
            background: #238636;
            color: white;
            border: none;
            padding: 14px 32px;
            font-size: 1.1rem;
            font-weight: 600;
            border-radius: 40px;
            cursor: pointer;
            transition: all 0.2s ease;
            width: 100%;
            text-align: center;
        }
        .scan-btn:hover { background: #2ea043; }
        .scan-btn:disabled {
            background: #6c7a89;
            cursor: not-allowed;
        }
        .scan-btn .spinner {
            display: none;
            width: 20px;
            height: 20px;
            border: 3px solid rgba(255,255,255,0.2);
            border-top: 3px solid #ffffff;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
            margin: 0 auto;
        }
        .scan-btn.loading .spinner { display: inline-block; }
        .scan-btn.loading .btn-text { opacity: 0.7; }
        @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }

        #statusBox {
            margin-top: 1.5rem;
            padding: 1rem 1.2rem;
            border-radius: 1rem;
            background: #f8fafc;
            border-left: 4px solid #8b949e;
            display: none;
            font-size: 0.95rem;
        }
        #statusBox.visible { display: block; }
        #statusBox.idle { border-left-color: #8b949e; }
        #statusBox.scanning { border-left-color: #d29922; }
        #statusBox.success { border-left-color: #2ecc71; }
        #statusBox.error { border-left-color: #e74c3c; }

        .results-area {
            margin-top: 1.5rem;
            display: none;
        }
        .results-area.visible { display: block; }

        .device-card {
            background: #f8fafc;
            border: 1px solid #e2edf4;
            border-radius: 1rem;
            padding: 1rem 1.2rem;
            margin-bottom: 0.8rem;
            display: flex;
            flex-wrap: wrap;
            align-items: center;
            justify-content: space-between;
            transition: border-color 0.2s;
        }
        .device-card:hover { border-color: #3498db; }
        .device-info {
            display: flex;
            flex-wrap: wrap;
            align-items: baseline;
            gap: 0.6rem 1.2rem;
        }
        .device-ip {
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 1.1rem;
            font-weight: 600;
            color: #0a1c2f;
        }
        .device-model {
            background: #e2edf4;
            padding: 0.2rem 0.8rem;
            border-radius: 20px;
            font-size: 0.8rem;
            color: #1e4663;
        }
        .device-status {
            font-weight: 600;
            font-size: 0.8rem;
            padding: 0.2rem 0.8rem;
            border-radius: 20px;
            text-transform: uppercase;
            letter-spacing: 0.3px;
        }
        .status-idle { background: #d4edda; color: #155724; }
        .status-running { background: #fff3cd; color: #856404; }
        .status-paused { background: #d1ecf1; color: #0c5460; }
        .status-offline { background: #f8d7da; color: #721c24; }
        .status-unknown { background: #e2e8f0; color: #2d3748; }

        .device-actions {
            display: flex;
            gap: 0.5rem;
        }
        .btn-copy {
            background: #3498db;
            color: white;
            border: none;
            padding: 0.3rem 1rem;
            border-radius: 20px;
            font-size: 0.8rem;
            font-weight: 500;
            cursor: pointer;
            transition: background 0.15s;
        }
        .btn-copy:hover { background: #2980b9; }
        .btn-copy:active { transform: scale(0.96); }
        .btn-copy.copied {
            background: #2ecc71;
        }

        .toast {
            position: fixed;
            bottom: 30px;
            right: 30px;
            background: #2ecc71;
            color: white;
            padding: 0.8rem 1.5rem;
            border-radius: 10px;
            font-weight: 500;
            box-shadow: 0 4px 16px rgba(0,0,0,0.2);
            opacity: 0;
            transform: translateY(20px);
            transition: opacity 0.3s, transform 0.3s;
            pointer-events: none;
        }
        .toast.show {
            opacity: 1;
            transform: translateY(0);
        }
        .toast.error {
            background: #e74c3c;
        }

        .footer-actions {
            margin-top: 1.5rem;
            display: flex;
            gap: 1rem;
            flex-wrap: wrap;
            border-top: 1px solid #e2e8f0;
            padding-top: 1.2rem;
        }
        .footer-actions a {
            color: #3498db;
            text-decoration: none;
            font-size: 0.9rem;
        }
        .footer-actions a:hover { text-decoration: underline; }

        .empty-state {
            text-align: center;
            padding: 2rem 1rem;
            color: #6c7a89;
        }
        .empty-state .big-icon { font-size: 3rem; margin-bottom: 0.5rem; display: block; }
        .hint {
            font-size: 0.85rem;
            color: #6c7a89;
            margin-top: 0.5rem;
        }

        @media (max-width: 550px) {
            .header { flex-direction: column; text-align: center; }
            .content { padding: 1rem; }
            .device-card { flex-direction: column; align-items: stretch; gap: 0.8rem; }
            .device-actions { justify-content: flex-end; }
        }
    </style>
</head>
<body>

<div class="card">
    <div class="header">
        <div class="logo">
            <!-- === FINAL SVG (your provided code) === -->
            <svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                <!-- Magnifying glass lens (dark blue) -->
                <circle cx="48" cy="44" r="26" fill="#0a1c2f"/>
                <!-- Lighter blue ring inside the lens -->
                <circle cx="48" cy="44" r="20" fill="none" stroke="#2980B9" stroke-width="9.5" opacity="1.0"/>
                <!-- White S in the centre -->
                <text x="48" y="55" font-family="Arial, Helvetica, sans-serif" font-size="28" font-weight="bold" fill="#ffffff" text-anchor="middle">S</text>
                <!-- Rectangular handle (lighter blue) -->
                <rect x="43" y="65" width="10" height="25" rx="3" fill="#2980B9"/>
                <!-- Network nodes -->
                <circle cx="30" cy="32" r="3.5" fill="#2ecc71"/>   <!-- Green -->
                <circle cx="66" cy="32" r="3.5" fill="#e74c3c"/>   <!-- Red -->
                <circle cx="48" cy="67" r="3.5" fill="#f1c40f"/>   <!-- Amber -->
                <!-- Triangle lines (white, subtle) -->
                <line x1="30" y1="32" x2="66" y2="32" stroke="#ffffff" stroke-width="1.5" opacity="0.6"/>
                <line x1="66" y1="32" x2="48" y2="67" stroke="#ffffff" stroke-width="1.5" opacity="0.6"/>
                <line x1="48" y1="67" x2="30" y2="32" stroke="#ffffff" stroke-width="1.5" opacity="0.6"/>
            </svg>
        </div>
        <div class="title">
            <h1>Snapmaker Network Scanner</h1>
            <p>UDP Discovery on port 20054</p>
        </div>
    </div>
    <div class="content">
        <div class="subtitle">Find all Snapmaker 2.0 devices on your local network</div>

        <button id="scanBtn" class="scan-btn" onclick="startScan()">
            <span class="spinner"></span>
            <span class="btn-text">Scan Network</span>
        </button>

        <div id="statusBox" class="idle">
            <span id="statusMessage">Ready to scan. Make sure your printer is powered on.</span>
        </div>

        <div id="resultsArea" class="results-area">
            <div id="deviceList"></div>
        </div>

        <div class="footer-actions">
            <!-- Only "Go to Config" remains; "Home" link removed -->
            <a href="/config">Go to Config</a>
            <span style="flex:1;"></span>
            <span style="color:#6c7a89; font-size:0.8rem;">Broadcasts to 255.255.255.255:20054</span>
        </div>
    </div>
</div>

<div id="toast" class="toast">IP copied!</div>

<script>
    var scanBtn = document.getElementById('scanBtn');
    var statusBox = document.getElementById('statusBox');
    var statusMsg = document.getElementById('statusMessage');
    var resultsArea = document.getElementById('resultsArea');
    var deviceList = document.getElementById('deviceList');

    function setStatus(message, type) {
        type = type || 'idle';
        statusBox.className = 'visible ' + type;
        statusMsg.textContent = message;
    }

    function showToast(message, isError) {
        var toast = document.getElementById('toast');
        toast.textContent = message;
        toast.className = 'toast' + (isError ? ' error' : '');
        toast.classList.add('show');
        clearTimeout(toast._timeout);
        toast._timeout = setTimeout(function() { toast.classList.remove('show'); }, 3000);
    }

    function copyToClipboard(text) {
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text)
                .then(function() {
                    showToast('Copied: ' + text);
                })
                .catch(function(err) {
                    fallbackCopy(text);
                });
        } else {
            fallbackCopy(text);
        }
    }

    function fallbackCopy(text) {
        var textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.select();
        try {
            document.execCommand('copy');
            showToast('Copied: ' + text);
        } catch (err) {
            showToast('Failed to copy. Select the IP manually.', true);
        }
        document.body.removeChild(textarea);
    }

    function renderDevices(devices) {
        deviceList.innerHTML = '';
        resultsArea.classList.add('visible');

        if (devices.length === 0) {
            deviceList.innerHTML = 
                '<div class="empty-state">' +
                '<span class="big-icon">[-]</span>' +
                '<strong>No devices found</strong>' +
                '<p class="hint">Ensure your Snapmaker is powered on and connected to the same network.<br>' +
                'Try disabling your firewall temporarily for UDP port 20054.</p>' +
                '</div>';
            return;
        }

        for (var i = 0; i < devices.length; i++) {
            var dev = devices[i];
            var statusClass = 'status-unknown';
            var st = dev.status.toLowerCase();
            if (st === 'idle') statusClass = 'status-idle';
            else if (st === 'running') statusClass = 'status-running';
            else if (st === 'paused') statusClass = 'status-paused';
            else if (st === 'offline') statusClass = 'status-offline';

            var card = document.createElement('div');
            card.className = 'device-card';
            card.innerHTML = 
                '<div class="device-info">' +
                '<span class="device-ip">' + dev.ip + '</span>' +
                '<span class="device-model">' + dev.model + '</span>' +
                '<span class="device-status ' + statusClass + '">' + dev.status + '</span>' +
                '</div>' +
                '<div class="device-actions">' +
                '<button class="btn-copy" data-ip="' + dev.ip + '" onclick="copyIP(this)">Copy IP</button>' +
                '</div>';
            deviceList.appendChild(card);
        }
    }

    function copyIP(btn) {
        var ip = btn.getAttribute('data-ip');
        copyToClipboard(ip);
        var originalText = btn.textContent;
        btn.textContent = 'Copied!';
        btn.classList.add('copied');
        setTimeout(function() {
            btn.textContent = originalText;
            btn.classList.remove('copied');
        }, 2000);
    }

    function startScan() {
        scanBtn.disabled = true;
        scanBtn.classList.add('loading');
        setStatus('Scanning network for Snapmaker devices... (takes ~2-3 seconds)', 'scanning');
        resultsArea.classList.remove('visible');
        deviceList.innerHTML = '';

        fetch('/api/discover')
            .then(function(response) {
                if (!response.ok) throw new Error('HTTP ' + response.status);
                return response.json();
            })
            .then(function(devices) {
                if (!Array.isArray(devices)) {
                    devices = devices.ip ? [devices] : [];
                }
                if (devices.length === 0) {
                    setStatus('No Snapmaker found on the network.', 'error');
                } else {
                    setStatus('Found ' + devices.length + ' device(s).', 'success');
                }
                renderDevices(devices);
            })
            .catch(function(err) {
                setStatus('Error: ' + err.message + '. Is the server running?', 'error');
                resultsArea.classList.add('visible');
                deviceList.innerHTML = 
                    '<div class="empty-state">' +
                    '<span class="big-icon">[X]</span>' +
                    '<strong>API Error</strong>' +
                    '<p class="hint">Failed to reach the backend discovery service.<br>' +
                    'Make sure you are connected to localhost:8081.</p>' +
                    '</div>';
            })
            .finally(function() {
                scanBtn.disabled = false;
                scanBtn.classList.remove('loading');
            });
    }
</script>
</body>
</html>
)html";
    SendHttpResponse(clientSocket, 200, "text/html", html);
}