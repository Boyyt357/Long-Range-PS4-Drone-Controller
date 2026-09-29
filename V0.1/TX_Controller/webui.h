#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

/* ================== EMBEDDED DASHBOARD WEBUI ================== */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Game Controller</title>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-dark: #070a12;
      --panel-bg: rgba(17, 24, 39, 0.65);
      --panel-border: rgba(255, 255, 255, 0.08);
      --accent-cyan: #06b6d4;
      --accent-violet: #818cf8;
      --accent-emerald: #10b981;
      --accent-rose: #f43f5e;
      --accent-amber: #f59e0b;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background-color: var(--bg-dark);
      background-image: radial-gradient(circle at 15% 15%, rgba(6, 182, 212, 0.08) 0%, transparent 45%),
                        radial-gradient(circle at 85% 85%, rgba(129, 140, 248, 0.08) 0%, transparent 45%);
      font-family: 'Outfit', -apple-system, sans-serif;
      color: var(--text-main);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 16px;
    }
    header {
      margin-bottom: 20px;
      text-align: center;
    }
    h1 {
      font-size: 2.1rem;
      font-weight: 800;
      background: linear-gradient(135deg, var(--accent-cyan), var(--accent-violet));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-bottom: 6px;
      letter-spacing: -0.5px;
    }
    .header-badges {
      display: flex;
      gap: 10px;
      justify-content: center;
      flex-wrap: wrap;
    }
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 7px;
      font-size: 0.85rem;
      background: rgba(255,255,255,0.04);
      padding: 5px 14px;
      border-radius: 24px;
      border: 1px solid var(--panel-border);
      color: var(--text-muted);
    }
    .status-dot {
      width: 9px;
      height: 9px;
      border-radius: 50%;
      background-color: var(--accent-emerald);
      box-shadow: 0 0 10px var(--accent-emerald);
    }
    .status-dot.warning {
      background-color: var(--accent-amber);
      box-shadow: 0 0 10px var(--accent-amber);
      animation: pulse 1.2s infinite;
    }
    @keyframes pulse {
      0% { opacity: 0.3; } 50% { opacity: 1; } 100% { opacity: 0.3; }
    }
    .container {
      width: 100%;
      max-width: 950px;
      display: grid;
      grid-template-columns: 1fr;
      gap: 18px;
    }
    @media (min-width: 768px) {
      .container { grid-template-columns: 1fr 1fr; }
      .full-width { grid-column: span 2; }
    }
    .card {
      background: var(--panel-bg);
      border: 1px solid var(--panel-border);
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      border-radius: 20px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.25);
    }
    h2 {
      font-size: 1.18rem;
      font-weight: 600;
      margin-bottom: 16px;
      display: flex;
      align-items: center;
      gap: 9px;
      color: var(--accent-cyan);
    }

    /* LoRa Data Rate Hero */
    .rate-hero {
      background: linear-gradient(135deg, rgba(6, 182, 212, 0.12), rgba(129, 140, 248, 0.12));
      border: 1px solid rgba(6, 182, 212, 0.25);
      border-radius: 16px;
      padding: 18px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }
    .rate-title {
      font-size: 0.82rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .rate-val {
      font-size: 2.6rem;
      font-weight: 800;
      color: #fff;
      font-family: monospace;
      letter-spacing: -1px;
    }
    .rate-sub {
      font-size: 0.8rem;
      color: var(--accent-cyan);
      margin-top: 3px;
    }



    /* Arm & Speed status */
    .arm-banner {
      border-radius: 12px;
      padding: 12px 16px;
      text-align: center;
      font-weight: 700;
      font-size: 0.95rem;
      letter-spacing: 0.5px;
      margin-bottom: 14px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .arm-banner.armed {
      background: rgba(16, 185, 129, 0.15);
      border: 1px solid var(--accent-emerald);
      color: var(--accent-emerald);
    }
    .arm-banner.disarmed {
      background: rgba(244, 63, 94, 0.15);
      border: 1px solid var(--accent-rose);
      color: var(--accent-rose);
    }
    .speed-pills {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 8px;
      margin-top: 6px;
    }
    .speed-pill {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--panel-border);
      padding: 8px;
      text-align: center;
      border-radius: 10px;
      font-weight: 700;
      font-size: 0.85rem;
      color: var(--text-muted);
      transition: all 0.2s ease;
    }
    .speed-pill.active {
      background: rgba(6, 182, 212, 0.15);
      border-color: var(--accent-cyan);
      color: var(--accent-cyan);
    }

    /* Gamepad Hub */
    .bt-hub {
      background: rgba(10, 15, 30, 0.65);
      border: 1px solid var(--panel-border);
      border-radius: 14px;
      padding: 14px 16px;
      margin-bottom: 16px;
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .bt-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 0.88rem;
    }
    .bt-tag {
      font-size: 0.75rem;
      font-weight: 700;
      padding: 3px 8px;
      border-radius: 6px;
      text-transform: uppercase;
    }
    .bt-tag.connected {
      background: rgba(16, 185, 129, 0.2);
      color: var(--accent-emerald);
      border: 1px solid var(--accent-emerald);
    }
    .bt-tag.searching {
      background: rgba(245, 158, 11, 0.2);
      color: var(--accent-amber);
      border: 1px solid var(--accent-amber);
    }

    /* Virtual Sticks */
    .joysticks-wrapper {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 16px;
      margin-top: 10px;
    }
    .joystick-box {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 8px;
    }
    .joystick-label {
      font-size: 0.78rem;
      color: var(--text-muted);
      font-weight: 600;
    }
    .joystick {
      width: 120px;
      height: 120px;
      background: rgba(10, 15, 30, 0.85);
      border: 2px solid var(--panel-border);
      border-radius: 50%;
      position: relative;
      box-shadow: inset 0 0 16px rgba(0,0,0,0.5);
    }
    .joystick::before {
      content: '';
      position: absolute;
      top: 0; left: 50%; width: 1px; height: 100%;
      background: rgba(6, 182, 212, 0.15);
    }
    .joystick::after {
      content: '';
      position: absolute;
      left: 0; top: 50%; width: 100%; height: 1px;
      background: rgba(6, 182, 212, 0.15);
    }
    .joystick-center-hover {
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      width: 24px; height: 24px;
      border: 1px dashed rgba(16, 185, 129, 0.4);
      border-radius: 50%;
      pointer-events: none;
    }
    .joystick-handle {
      position: absolute;
      width: 28px;
      height: 28px;
      background: radial-gradient(circle at 35% 35%, var(--accent-cyan), var(--accent-violet));
      border-radius: 50%;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      box-shadow: 0 4px 12px rgba(6, 182, 212, 0.45);
      z-index: 2;
      will-change: transform;
    }

    /* Channels */
    .channel-row { margin-bottom: 10px; }
    .channel-header {
      display: flex;
      justify-content: space-between;
      font-size: 0.82rem;
      margin-bottom: 3px;
    }
    .channel-name { font-weight: 600; }
    .channel-value { font-family: monospace; color: var(--accent-cyan); }
    .channel-bar-bg {
      height: 8px;
      background: rgba(255, 255, 255, 0.06);
      border-radius: 4px;
      overflow: hidden;
    }
    .channel-bar-fill {
      height: 100%;
      background: linear-gradient(90deg, var(--accent-violet), var(--accent-cyan));
      border-radius: 4px;
      width: 50%;
      transition: width 0.06s ease-out;
    }

    /* Forms */
    .form-group { margin-bottom: 14px; }
    .form-label-row {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      margin-bottom: 5px;
    }
    .val-display { font-family: monospace; color: var(--accent-cyan); font-weight: 600; }
    input[type="range"] {
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: rgba(255, 255, 255, 0.1);
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: var(--accent-cyan);
      cursor: pointer;
      box-shadow: 0 0 10px var(--accent-cyan);
    }
    .checkbox-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
      font-size: 0.86rem;
    }
    .toggle-switch {
      position: relative;
      display: inline-block;
      width: 44px;
      height: 24px;
    }
    .toggle-switch input { opacity: 0; width: 0; height: 0; }
    .toggle-slider {
      position: absolute; cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: rgba(255,255,255,0.1);
      border-radius: 24px;
      transition: .3s;
    }
    .toggle-slider:before {
      position: absolute; content: "";
      height: 18px; width: 18px;
      left: 3px; bottom: 3px;
      background-color: white;
      border-radius: 50%;
      transition: .3s;
    }
    input:checked + .toggle-slider { background-color: var(--accent-cyan); }
    input:checked + .toggle-slider:before { transform: translateX(20px); }

    .btn-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 18px;
    }
    .btn {
      padding: 12px;
      border-radius: 12px;
      border: none;
      font-weight: 700;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.9rem;
      transition: all 0.2s ease;
    }
    .btn-primary {
      background: linear-gradient(135deg, var(--accent-cyan), var(--accent-violet));
      color: #fff;
      box-shadow: 0 4px 14px rgba(6, 182, 212, 0.3);
    }
    .btn-primary:hover { opacity: 0.92; transform: translateY(-1px); }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.06);
      color: var(--text-main);
      border: 1px solid var(--panel-border);
    }
    .btn-secondary:hover { background: rgba(255, 255, 255, 0.1); }
  </style>
</head>
<body>
  <header>
    <h1>Game Controller</h1>
    <div class="header-badges">
      <div class="status-badge">
        <div class="status-dot"></div>
        <span id="lora-link-badge">Radio Link</span>
      </div>
      <div class="status-badge">
        <div class="status-dot warning" id="bt-dot"></div>
        <span id="bt-status-text">Gamepad: Disconnected</span>
      </div>
      <div class="status-badge">
        <span style="color:var(--accent-cyan);">WebUI: 192.168.4.1</span>
      </div>
    </div>
  </header>

  <div class="container">
    <!-- Card 1: LoRa Live Data Rate & Link Status -->
    <div class="card">
      <h2>Radio Link Status</h2>

      <!-- Data Rate Hero Box -->
      <div class="rate-hero">
        <div>
          <div class="rate-title">Live Transmission Rate</div>
          <div class="rate-sub">433.0 MHz | Fixed SF7 | BW 125 kHz</div>
        </div>
        <div class="rate-val"><span id="data-rate">--</span> <span style="font-size:1.1rem; color:var(--text-muted); font-weight:normal;">Hz</span></div>
      </div>

      <!-- Arm Status Banner -->
      <div class="arm-banner disarmed" id="arm-banner">
        <span id="arm-label">STATUS: DISARMED</span>
        <span style="font-size:0.8rem; font-weight:normal;">Click 'X' to Toggle</span>
      </div>

      <!-- Speed Mode Pill Selector -->
      <div>
        <div style="font-size:0.82rem; color:var(--text-muted); font-weight:600;">ACTIVE SPEED MODE (Cycle with L1)</div>
        <div class="speed-pills">
          <div class="speed-pill" id="pill-mode-0">LOW<br><span style="font-size:0.75rem;" id="pill-pct-0">40%</span></div>
          <div class="speed-pill" id="pill-mode-1">MID<br><span style="font-size:0.75rem;" id="pill-pct-1">70%</span></div>
          <div class="speed-pill" id="pill-mode-2">FAST<br><span style="font-size:0.75rem;" id="pill-pct-2">100%</span></div>
        </div>
      </div>
    </div>

    <!-- Card 2: PS4 / EvoFox Gamepad Status & Sticks -->
    <div class="card">
      <h2>Gamepad & Virtual Sticks</h2>

      <div class="bt-hub">
        <div class="bt-row">
          <span>Connected Controller</span>
          <span class="bt-tag searching" id="bt-hub-tag">Waiting for Gamepad</span>
        </div>
        <div class="bt-row" style="color:var(--text-muted);">
          <span>Device Name</span>
          <span id="bt-device-name" style="font-family:monospace; color:#fff;">--</span>
        </div>
        <div class="bt-row" style="color:var(--text-muted);">
          <span>Gamepad Battery</span>
          <span id="bt-battery-val" style="font-family:monospace; color:var(--accent-emerald);">-- %</span>
        </div>
      </div>

      <!-- Virtual Sticks -->
      <div class="joysticks-wrapper">
        <div class="joystick-box">
          <div class="joystick">
            <div class="joystick-center-hover" title="1500us Hover Target"></div>
            <div class="joystick-handle" id="left-handle"></div>
          </div>
          <div class="joystick-label">Left Stick (Yaw / Throttle Hover)</div>
        </div>
        <div class="joystick-box">
          <div class="joystick">
            <div class="joystick-handle" id="right-handle"></div>
          </div>
          <div class="joystick-label">Right Stick (Roll / Pitch)</div>
        </div>
      </div>
    </div>

    <!-- Card 3: Live Channels Output -->
    <div class="card">
      <h2>FC Output Channels (1000 - 2000 &mu;s)</h2>
      <div id="channels-container"></div>
    </div>

    <!-- Card 4: Control & Flight Parameters -->
    <div class="card">
      <h2>Flight & Rate Parameters</h2>

      <div class="form-group">
        <div class="form-label-row">
          <label for="altHoldDeadband">Alt-Hold Throttle Deadband (&plusmn;&mu;s at 1500)</label>
          <span class="val-display" id="altHoldDeadband-val">25</span>
        </div>
        <input type="range" id="altHoldDeadband" min="5" max="60" value="25">
      </div>

      <div class="form-group">
        <div class="form-label-row">
          <label for="stickDeadband">Roll / Pitch / Yaw Deadband (&plusmn;&mu;s)</label>
          <span class="val-display" id="stickDeadband-val">15</span>
        </div>
        <input type="range" id="stickDeadband" min="0" max="40" value="15">
      </div>

      <div class="form-group">
        <div class="form-label-row">
          <label for="crossDeadband">Cross / Axis Isolation Gate (Anti-Wiggle)</label>
          <span class="val-display" id="crossDeadband-val">35</span>
        </div>
        <input type="range" id="crossDeadband" min="0" max="80" value="35">
      </div>

      <div class="form-group">
        <div class="form-label-row">
          <label for="speedLowPct">Low Speed Mode Max Rate</label>
          <span class="val-display" id="speedLowPct-val">40%</span>
        </div>
        <input type="range" id="speedLowPct" min="20" max="60" value="40">
      </div>

      <div class="form-group">
        <div class="form-label-row">
          <label for="speedMidPct">Mid Speed Mode Max Rate</label>
          <span class="val-display" id="speedMidPct-val">70%</span>
        </div>
        <input type="range" id="speedMidPct" min="50" max="90" value="70">
      </div>

      <div class="checkbox-row" style="margin-top: 18px;">
        <span>Apply Speed Mode to Throttle (DJI Style)</span>
        <label class="toggle-switch">
          <input type="checkbox" id="throttleSpeedMode" onchange="saveSettings()">
          <span class="toggle-slider"></span>
        </label>
      </div>


      <div class="btn-group">
        <button class="btn btn-secondary" onclick="loadSettings()">Reset</button>
        <button class="btn btn-primary" onclick="saveSettings()">Save Configuration</button>
      </div>
    </div>

    <!-- Card 5: Channel Inversion -->
    <div class="card full-width">
      <h2>Channel Inversion (Mirroring)</h2>
      <p style="font-size:0.85rem; color:var(--text-muted); margin-bottom:12px;">
        Reverse stick direction in software to match your Flight Controller channel map.
      </p>
      <div id="mirror-container" style="display:grid; grid-template-columns:1fr 1fr; gap:10px;"></div>
    </div>
  </div>

  <footer style="margin-top: 30px; font-size: 0.8rem; color: var(--text-muted); text-align: center;">
    PS4 Drone Controller | Fixed SF7 433MHz | 10-Channel Direct Link
  </footer>

  <script>
    const chNames = [
      "CH1: Roll (Right Stick X)", 
      "CH2: Pitch (Right Stick Y)", 
      "CH3: Throttle (Left Stick Y)", 
      "CH4: Yaw (Left Stick X)",
      "AUX1 (CH5): Cross / X Button",
      "AUX2 (CH6): R1 Button",
      "AUX3 (CH7): L1 Button / Speed",
      "AUX4 (CH8): Triangle Button",
      "AUX5 (CH9): Square Button",
      "AUX6 (CH10): Circle Button"
    ];

    document.addEventListener("DOMContentLoaded", () => {
      // Build channel meters
      const container = document.getElementById("channels-container");
      chNames.forEach((name, i) => {
        container.innerHTML += `
          <div class="channel-row">
            <div class="channel-header">
              <span class="channel-name">${name}</span>
              <span class="channel-value" id="ch-val-${i}">1500 &mu;s</span>
            </div>
            <div class="channel-bar-bg">
              <div class="channel-bar-fill" id="ch-fill-${i}"></div>
            </div>
          </div>
        `;
      });

      // Build Invert toggles for primary 4 channels
      const mirrorContainer = document.getElementById("mirror-container");
      ["Roll (CH1)", "Pitch (CH2)", "Throttle (CH3)", "Yaw (CH4)"].forEach((name, i) => {
        mirrorContainer.innerHTML += `
          <div class="checkbox-row">
            <span>Invert ${name}</span>
            <label class="toggle-switch">
              <input type="checkbox" id="invert-${i}" onchange="updateInvert(${i})">
              <span class="toggle-slider"></span>
            </label>
          </div>
        `;
      });

      setupSlider("altHoldDeadband", " \u03BCs");
      setupSlider("stickDeadband", " \u03BCs");
      setupSlider("crossDeadband", "");
      setupSlider("speedLowPct", "%");
      setupSlider("speedMidPct", "%");

      loadSettings();
      requestAnimationFrame(renderSticks);
      pollTelemetry();
    });

    let targetLX = 0, targetLY = 0;
    let targetRX = 0, targetRY = 0;
    let curLX = 0, curLY = 0;
    let curRX = 0, curRY = 0;

    function renderSticks() {
      // 0.35 lerp factor: ultra-responsive (<30ms follow time), silky 60 FPS motion
      curLX += (targetLX - curLX) * 0.35;
      curLY += (targetLY - curLY) * 0.35;
      curRX += (targetRX - curRX) * 0.35;
      curRY += (targetRY - curRY) * 0.35;

      const lh = document.getElementById("left-handle");
      const rh = document.getElementById("right-handle");
      if (lh) lh.style.transform = `translate(calc(-50% + ${curLX.toFixed(1)}px), calc(-50% + ${curLY.toFixed(1)}px))`;
      if (rh) rh.style.transform = `translate(calc(-50% + ${curRX.toFixed(1)}px), calc(-50% + ${curRY.toFixed(1)}px))`;

      requestAnimationFrame(renderSticks);
    }

    let isFetching = false;
    function pollTelemetry() {
      if (isFetching) return;
      isFetching = true;
      fetch("/api/telemetry")
        .then(r => r.json())
        .then(data => {
          updateDashboard(data);
        })
        .catch(() => {})
        .finally(() => {
          isFetching = false;
          setTimeout(pollTelemetry, 50); // ~15 Hz adaptive polling without piling requests
        });
    }

    function updateDashboard(data) {
      // 1. Data Rate in Hz
      const drEl = document.getElementById("data-rate");
      if (drEl && drEl._val !== data.dataRateHz) {
        drEl._val = data.dataRateHz;
        drEl.innerText = data.dataRateHz;
      }

      // 2. Gamepad Status
      const btDot = document.getElementById("bt-dot");
      const btText = document.getElementById("bt-status-text");
      const btHubTag = document.getElementById("bt-hub-tag");
      if (data.btConnected) {
        if (btDot.className !== "status-dot") btDot.className = "status-dot";
        btText.innerText = "Gamepad: Connected";
        if (btHubTag.className !== "bt-tag connected") {
          btHubTag.className = "bt-tag connected";
          btHubTag.innerText = "CONNECTED";
        }
        document.getElementById("bt-device-name").innerText = data.btName + (data.btMac ? " [" + data.btMac + "]" : "");
        document.getElementById("bt-battery-val").innerText = data.btBattery + "%";
      } else {
        if (btDot.className !== "status-dot warning") btDot.className = "status-dot warning";
        if (btHubTag.className !== "bt-tag searching") {
          btHubTag.className = "bt-tag searching";
          btHubTag.innerText = data.btName && data.btName !== "None" ? "READY TO RECONNECT" : "WAITING PAIRING";
        }
        btText.innerText = (data.btName && data.btName !== "None") ? "Gamepad: Tap PS button" : "Gamepad: Pair SHARE+PS";
        document.getElementById("bt-device-name").innerText = (data.btName && data.btName !== "None") ? "Recent: " + data.btName + " (Tap PS button)" : "Hold SHARE + PS to pair new controller";
        document.getElementById("bt-battery-val").innerText = "--";
      }

      // 3. Arm Status Banner
      const armBanner = document.getElementById("arm-banner");
      const armLabel = document.getElementById("arm-label");
      if (data.isArmed) {
        if (armBanner.className !== "arm-banner armed") {
          armBanner.className = "arm-banner armed";
          armLabel.innerText = "STATUS: ARMED (\u2713 MOTORS ACTIVE)";
        }
      } else {
        if (armBanner.className !== "arm-banner disarmed") {
          armBanner.className = "arm-banner disarmed";
          armLabel.innerText = "STATUS: DISARMED";
        }
      }

      // 4. Speed Mode Pills
      [0, 1, 2].forEach(m => {
        const el = document.getElementById("pill-mode-" + m);
        if (el) {
          if (data.speedMode === m) el.classList.add("active");
          else el.classList.remove("active");
        }
      });

      // 5. Channels Output Display (dirty checked to avoid DOM reflows)
      data.ch.forEach((val, i) => {
        const chValEl = document.getElementById("ch-val-" + i);
        const chFillEl = document.getElementById("ch-fill-" + i);
        if (chValEl && chFillEl && chValEl._val !== val) {
          chValEl._val = val;
          chValEl.innerText = val + " \u03BCs";
          const pct = Math.min(100, Math.max(0, (val - 1000) / 10));
          chFillEl.style.width = pct + "%";
        }
      });

      // 6. Virtual Sticks Target Update (scaled to max deflection 44px)
      targetLX = ((data.ch[3] - 1500) / 500) * 44;
      targetLY = -((data.ch[2] - 1500) / 500) * 44;
      targetRX = ((data.ch[0] - 1500) / 500) * 44;
      targetRY = -((data.ch[1] - 1500) / 500) * 44;
    }

    function setupSlider(id, suffix) {
      const slider = document.getElementById(id);
      const val = document.getElementById(id + "-val");
      slider.oninput = () => { val.innerText = slider.value + suffix; };
    }

    function loadSettings() {
      fetch("/api/settings")
        .then(r => r.json())
        .then(data => {
          document.getElementById("altHoldDeadband").value = data.altHoldDeadband;
          document.getElementById("altHoldDeadband-val").innerText = data.altHoldDeadband + " \u03BCs";
          document.getElementById("stickDeadband").value = data.stickDeadband;
          document.getElementById("stickDeadband-val").innerText = data.stickDeadband + " \u03BCs";
          if (data.crossDeadband !== undefined) {
            document.getElementById("crossDeadband").value = data.crossDeadband;
            document.getElementById("crossDeadband-val").innerText = data.crossDeadband;
          }
          document.getElementById("speedLowPct").value = data.speedLowPct;
          document.getElementById("speedLowPct-val").innerText = data.speedLowPct + "%";
          document.getElementById("speedMidPct").value = data.speedMidPct;
          document.getElementById("speedMidPct-val").innerText = data.speedMidPct + "%";
          document.getElementById("pill-pct-0").innerText = data.speedLowPct + "%";
          document.getElementById("pill-pct-1").innerText = data.speedMidPct + "%";

          for (let i = 0; i < 4; i++) {
            document.getElementById("invert-" + i).checked = data.invert[i];
          }
          if (data.throttleSpeedMode !== undefined) {
            document.getElementById("throttleSpeedMode").checked = data.throttleSpeedMode;
          }
        })
        .catch(() => {});
    }

    function saveSettings() {
      const p = new URLSearchParams({
        altHoldDeadband: document.getElementById("altHoldDeadband").value,
        stickDeadband: document.getElementById("stickDeadband").value,
        crossDeadband: document.getElementById("crossDeadband").value,
        speedLowPct: document.getElementById("speedLowPct").value,
        speedMidPct: document.getElementById("speedMidPct").value,
        throttleSpeedMode: document.getElementById("throttleSpeedMode").checked ? 1 : 0
      });

      fetch("/api/set_settings?" + p.toString())
        .then(r => r.text())
        .then(res => {
          if (res === "OK") alert("Settings successfully saved to Flash!");
          loadSettings();
        });
    }

    function updateInvert(chIdx) {
      const val = document.getElementById("invert-" + chIdx).checked ? 1 : 0;
      fetch(`/api/set_invert?ch=${chIdx}&val=${val}`);
    }
  </script>
</body>
</html>
)rawliteral";

#endif // WEBUI_H
