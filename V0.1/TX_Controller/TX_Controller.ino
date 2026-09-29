/*
 * ESP32 PS4 Drone Controller Transmitter (TX) - Pure LoRa Ground Station
 * 
 * Hardware Architecture:
 * - Bluetooth Classic Gamepad Host (Bluepad32: PS4 / EvoFox Play 2 Gamepad)
 * - Wi-Fi SoftAP WebUI (192.168.4.1) on fixed Channel 1 (Pure AP mode)
 * - 100% Pure LoRa SX1278 (433MHz) Asynchronous Non-Blocking Transmission
 * - Dynamic Spreading Factor (SF7 - SF10) switchable from WebUI with Auto-Sync to Receiver
 * - Real-time Data Rate (Hz) measurement and display
 * 
 * Flight Controller Features:
 * - INAV Alt-Hold Optimized: Center stick = 1500us (Drone holds altitude / zero vertical speed)
 * - INAV Low-Throttle Arming Bypass: Outputs 1000us when disarmed, satisfies min_check (<1100us)
 * - 3 Cyclable Speed Modes (Low 40%, Mid 70%, Fast 100%) via L1 for Roll/Pitch/Yaw
 * - 'X' Button Toggle Arming (AUX1: 1000 / 2000us) with Center-Throttle Safety Interlock
 * - 10-Byte Compressed Packet with CRC16-CCITT
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Bluetooth Gamepad Driver via Bluepad32
#include <Bluepad32.h>

#include "config.h"
#include "webui.h"

/* ================== GLOBAL INSTANCES ================== */
Preferences preferences;
WebServer server(80);

// Default Settings
Settings settings = {
  25,     // altHoldDeadband (±25us at 1500us center throttle)
  15,     // stickDeadband (±15us for Roll/Pitch/Yaw)
  35,     // crossDeadband (Axis Isolation / Cross Gate Deadband)
  100,    // filterBeta (100% sensitivity)
  20,     // loraPower (20 dBm)
  {false, false, false, false}, // Invert channels: Roll, Pitch, Thr, Yaw
  40,     // speedLowPct (40% rate)
  70,     // speedMidPct (70% rate)
  100,    // speedFastPct (100% rate)
  false   // throttleSpeedMode
};

// 10-Byte Packet Buffers
RCPacket txPacket;

// Channels: [0]=Roll, [1]=Pitch, [2]=Throttle (1000 disarmed, 1500 hover), [3]=Yaw
uint16_t primaryChannels[4] = {1500, 1500, 1000, 1500};
uint16_t allChannels[10]    = {1500, 1500, 1000, 1500, 1000, 1000, 1500, 1000, 1000, 1000};

// State Variables
bool isArmed = false;
uint8_t speedMode = 0; // 0=Low, 1=Mid, 2=Fast
uint8_t altHoldMode = 1; // 1 = Active (1500us hover center)
bool r1Toggle = false;
bool triToggle = false;
bool sqToggle = false;
bool circToggle = false;
bool throttleOverride = false;
uint16_t lastRawThrottle = 1000;

// Bluepad32 Gamepad Reference & Recent Memory
ControllerPtr activeController = nullptr;
String recentControllerName = "None";
String recentControllerMac  = "";
bool prevXButton = false;
bool prevL1Button = false;
bool prevR1Button = false;
bool prevTriButton = false;
bool prevSqButton = false;
bool prevCircButton = false;

bool loraInitialized = false;
unsigned long armTransitionTime = 0;
uint32_t txPacketCounter = 0;
uint16_t txRateHz = 0;

// Removed dynamic SF variables



/* ================== PERSISTENCE ================== */
void saveSettings() {
  preferences.begin("drone_tx", false);
  preferences.putBytes("cfg_v11", &settings, sizeof(Settings));
  preferences.end();
}

void loadSettings() {
  preferences.begin("drone_tx", false);
  if (preferences.isKey("cfg_v11")) {
    preferences.getBytes("cfg_v11", &settings, sizeof(Settings));
  } else {
    settings.altHoldDeadband = 25;
    settings.stickDeadband = 15;
    settings.crossDeadband = 35; // Default 35 units cross gate isolation
    settings.filterBeta = 100;
    settings.loraPower = 20;
    settings.invert[0] = false;
    settings.invert[1] = false;
    settings.invert[2] = false;
    settings.invert[3] = false;
    settings.speedLowPct = 40;
    settings.speedMidPct = 70;
    settings.speedFastPct = 100;
    settings.throttleSpeedMode = false;
    saveSettings();
  }
  preferences.end();

  // Sanity check for valid ranges across firmware upgrades
  if (settings.crossDeadband < 0 || settings.crossDeadband > 80) settings.crossDeadband = 35;
  settings.loraPower = 20; // Guarantee Max 20 dBm
}

/* ================== BLUEPAD32 GAMEPAD CALLBACKS ================== */
void updateGamepadFeedback() {
  if (!activeController || !activeController->isConnected()) return;

  if (isArmed) {
    // Armed LED Indicator
    if (speedMode == 0) {
      activeController->setColorLED(0, 255, 0);     // Low: Bright Green
    } else if (speedMode == 1) {
      activeController->setColorLED(0, 180, 255);   // Mid: Cyan
    } else {
      activeController->setColorLED(255, 0, 128);   // Fast: Magenta
    }
  } else {
    // Disarmed: Dim Blue pulsing / ready
    activeController->setColorLED(20, 40, 120);
  }
}

void onConnectedController(ControllerPtr ctl) {
  if (activeController == nullptr) {
    activeController = ctl;
    recentControllerName = ctl->getModelName().c_str();

    ControllerProperties properties = ctl->getProperties();
    const uint8_t* bssid = properties.btaddr;
    char macBuf[20];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    recentControllerMac = macBuf;

    Serial.printf("[Bluetooth] Controller Connected: %s [%s]\n", 
                  recentControllerName.c_str(), recentControllerMac.c_str());

    ctl->playDualRumble(0, 250, 0x80, 0x80);
    updateGamepadFeedback();
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  if (activeController == ctl) {
    Serial.printf("[Bluetooth] Controller Disconnected: %s\n", recentControllerName.c_str());
    activeController = nullptr;
    isArmed = false; // Safety disarm on gamepad loss!
  }
}

/* ================== WEB SERVER ROUTING HANDLERS ================== */
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleTelemetry() {
  String json;
  json.reserve(256);
  json = "{\"ch\":[";
  for (int i = 0; i < 10; i++) {
    if (i == 2) {
      json += String(lastRawThrottle); // Send raw stick position for UI visualizer
    } else {
      json += String(allChannels[i]);
    }
    if (i < 9) json += ",";
  }
  json += "],\"isArmed\":";
  json += isArmed ? "true" : "false";
  json += ",\"speedMode\":";
  json += String(speedMode);
  json += ",\"altHold\":";
  json += String(altHoldMode);
  json += ",\"beeper\":";
  json += triToggle ? "1" : "0";
  json += ",\"dataRateHz\":";
  json += String(txRateHz);

  // Bluetooth Gamepad status
  bool btConn = (activeController && activeController->isConnected());
  json += ",\"btConnected\":";
  json += btConn ? "true" : "false";
  json += ",\"btName\":\"" + recentControllerName + "\",";
  json += "\"btMac\":\"" + recentControllerMac + "\",";
  if (btConn) {
    int battPct = map(constrain(activeController->battery(), 0, 255), 0, 255, 0, 100);
    json += "\"btBattery\":" + String(battPct);
  } else {
    json += "\"btBattery\":0";
  }

  json += "}";
  server.send(200, "application/json", json);
}

void handleGetSettings() {
  String json = "{";
  json += "\"altHoldDeadband\":" + String(settings.altHoldDeadband) + ",";
  json += "\"stickDeadband\":" + String(settings.stickDeadband) + ",";
  json += "\"crossDeadband\":" + String(settings.crossDeadband) + ",";
  json += "\"speedLowPct\":" + String(settings.speedLowPct) + ",";
  json += "\"speedMidPct\":" + String(settings.speedMidPct) + ",";
  json += "\"speedFastPct\":" + String(settings.speedFastPct) + ",";
  json += "\"throttleSpeedMode\":" + String(settings.throttleSpeedMode ? "true" : "false") + ",";
  json += "\"invert\":[";
  for (int i = 0; i < 4; i++) {
    json += String(settings.invert[i] ? "true" : "false");
    if (i < 3) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSetSettings() {
  if (server.hasArg("altHoldDeadband")) 
    settings.altHoldDeadband = constrain(server.arg("altHoldDeadband").toInt(), 5, 60);
  if (server.hasArg("stickDeadband")) 
    settings.stickDeadband = constrain(server.arg("stickDeadband").toInt(), 0, 40);
  if (server.hasArg("crossDeadband")) 
    settings.crossDeadband = constrain(server.arg("crossDeadband").toInt(), 0, 80);
  if (server.hasArg("speedLowPct")) 
    settings.speedLowPct = constrain(server.arg("speedLowPct").toInt(), 20, 60);
  if (server.hasArg("speedMidPct")) 
    settings.speedMidPct = constrain(server.arg("speedMidPct").toInt(), 50, 90);
  if (server.hasArg("throttleSpeedMode"))
    settings.throttleSpeedMode = (server.arg("throttleSpeedMode").toInt() == 1);

  saveSettings();
  server.send(200, "text/plain", "OK");
}

void handleSetInvert() {
  if (server.hasArg("ch") && server.hasArg("val")) {
    int ch = server.arg("ch").toInt();
    int val = server.arg("val").toInt();
    if (ch >= 0 && ch < 4) {
      settings.invert[ch] = (val == 1);
      saveSettings();
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Bad Request");
}

/* ================== WIFI SOFTAP SETUP (PURE AP MODE) ================== */
void configureWireless() {
  WiFi.mode(WIFI_AP);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  // Broadcast SoftAP on fixed Channel 1 (visible to all phones/devices, 4 max clients)
  bool apSuccess = WiFi.softAP("PS4_Drone_Transmitter", "12345678", 1, 0, 4);

  if (apSuccess) {
    Serial.printf("[Wi-Fi AP] Broadcasting SSID: 'PS4_Drone_Transmitter' on Ch 1, IP: %s\n", 
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[Wi-Fi AP] Failed to start SoftAP!");
  }
}

/* ================== SETUP ================== */
void setup() {
  Serial.begin(115200);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  // 1. Load configuration from Flash NVS
  loadSettings();

  // 2. Configure Wi-Fi SoftAP
  configureWireless();

  // 3. Setup Web Server Handlers
  server.on("/", handleRoot);
  server.on("/api/telemetry", handleTelemetry);
  server.on("/api/settings", handleGetSettings);
  server.on("/api/set_settings", handleSetSettings);
  server.on("/api/set_invert", handleSetInvert);
  server.begin();
  Serial.println("[WebUI] Server Started at http://192.168.4.1");

  // 4. Initialize LoRa SX1278 SPI
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (LoRa.begin(LORA_FREQUENCY)) {
    loraInitialized = true;
    LoRa.setSpreadingFactor(7); // Fixed SF7
    LoRa.setTxPower(settings.loraPower);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    Serial.printf("[LoRa] Transmitter Initialized (Fixed SF7, BW 125k, Power %ddBm)\n", 
                  settings.loraPower);
  } else {
    loraInitialized = false;
    Serial.println("[LoRa ERROR] SX1278 not found. Check SPI wiring!");
  }

  // 5. Initialize Bluepad32 Bluetooth Gamepad Host
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);
  BP32.enableNewBluetoothConnections(true);
  if (recentControllerMac.length() > 0) {
    Serial.printf("[Bluetooth] Ready. Last Paired: %s [%s] (Press PS to auto-reconnect)\n", 
                  recentControllerName.c_str(), recentControllerMac.c_str());
  } else {
    Serial.println("[Bluetooth] Ready. Put controller in pairing mode (SHARE + PS).");
  }
}

/* ================== AXIAL / CROSS DEADBAND HELPER ================== */
inline int applyAxialDeadband(int val, int deadband) {
  if (abs(val) <= deadband) {
    return 0;
  }
  if (val > 0) {
    return map(val, deadband, 511, 0, 511);
  } else {
    return map(val, -512, -deadband, -512, 0);
  }
}

/* ================== MAIN CONTROL LOOP ================== */
void loop() {
  // 1. Handle WebUI client requests
  server.handleClient();

  // 2. Update Bluepad32 Bluetooth stack
  BP32.update();

  // 3. Process Controller Inputs if connected
  int rawRoll = 0, rawPitch = 0, rawThrottle = 0, rawYaw = 0;

  if (activeController && activeController->isConnected()) {
    digitalWrite(PIN_STATUS_LED, HIGH);

    int stickYaw  = activeController->axisX();
    int stickThr  = -activeController->axisY();
    int stickRoll = activeController->axisRX();
    int stickPit  = -activeController->axisRY();

    // --- ARM / DISARM TOGGLE via 'X' (Cross) Button ---
    // Note: Bluepad32 maps the PlayStation Cross (X) button to a(), and Square to x()
    bool curXButton = activeController->a();
    if (curXButton && !prevXButton) {
      if (!loraInitialized) {
        activeController->playDualRumble(0, 1000, 0xFF, 0x00);
        Serial.println("[ERROR] Cannot Arm: LoRa Radio Disconnected or Failed!");
      } else if (!isArmed) {
        if (stickThr < -400) { // Require stick to be fully down to arm safely
          isArmed = true;
          throttleOverride = true; // Lock throttle low to satisfy INAV until pilot raises stick
          activeController->playDualRumble(0, 300, 0x80, 0x80);
          updateGamepadFeedback();
          Serial.println("[ARMED] Motors Activated! (Throttle locked low until stick raised)");
        } else {
          activeController->playDualRumble(0, 500, 0xFF, 0x00);
          Serial.println("[WARNING] Cannot Arm: Pull Left Stick completely down first!");
        }
      } else {
        isArmed = false;
        throttleOverride = false;
        activeController->playDualRumble(0, 200, 0x40, 0x40);
        updateGamepadFeedback();
        Serial.println("[DISARMED] Motors Stopped");
      }
    }
    prevXButton = curXButton;

    // --- Emergency Disarm via PS Button or L1+R1+Cross ---
    if ((activeController->miscButtons() & 0x01) || 
        (activeController->l1() && activeController->r1() && activeController->a())) {
      if (isArmed) {
        isArmed = false;
        activeController->playDualRumble(0, 400, 0xFF, 0xFF);
        updateGamepadFeedback();
        Serial.println("[EMERGENCY DISARM] Triggered by Pilot!");
      }
    }

    // --- Speed Mode Cycling via 'L1' Button ---
    bool curL1Button = activeController->l1();
    if (curL1Button && !prevL1Button) {
      speedMode = (speedMode + 1) % 3;
      if (speedMode == 0) {
        activeController->playDualRumble(0, 120, 0x30, 0x20);
      } else if (speedMode == 1) {
        activeController->playDualRumble(0, 200, 0x60, 0x40);
      } else {
        activeController->playDualRumble(0, 300, 0xA0, 0x80);
      }
      updateGamepadFeedback();
      Serial.printf("[SPEED MODE] Switched to: %s\n", 
                    (speedMode == 0) ? "LOW" : ((speedMode == 1) ? "MID" : "FAST"));
    }
    prevL1Button = curL1Button;

    // --- RTL / GPS Toggle via 'R1' Button ---
    bool curR1Button = activeController->r1();
    if (curR1Button && !prevR1Button) {
      r1Toggle = !r1Toggle;
      if (r1Toggle) {
        activeController->playDualRumble(0, 150, 0x40, 0x40);
        Serial.println("[AUX2] R1 Toggle: ON (2000us)");
      } else {
        activeController->playDualRumble(0, 150, 0x10, 0x10);
        Serial.println("[AUX2] R1 Toggle: OFF (1000us)");
      }
    }
    prevR1Button = curR1Button;

    // --- Triangle Toggle via 'y()' Button ---
    bool curTriButton = activeController->y();
    if (curTriButton && !prevTriButton) {
      triToggle = !triToggle;
      if (triToggle) {
        activeController->playDualRumble(0, 150, 0x40, 0x40);
        Serial.println("[AUX4] Triangle Toggle: ON (2000us)");
      } else {
        activeController->playDualRumble(0, 150, 0x10, 0x10);
        Serial.println("[AUX4] Triangle Toggle: OFF (1000us)");
      }
    }
    prevTriButton = curTriButton;

    // --- Square (Box) Toggle via 'x()' Button ---
    bool curSqButton = activeController->x();
    if (curSqButton && !prevSqButton) {
      sqToggle = !sqToggle;
      if (sqToggle) {
        activeController->playDualRumble(0, 150, 0x40, 0x40);
        Serial.println("[AUX5] Square Toggle: ON (2000us)");
      } else {
        activeController->playDualRumble(0, 150, 0x10, 0x10);
        Serial.println("[AUX5] Square Toggle: OFF (1000us)");
      }
    }
    prevSqButton = curSqButton;

    // --- Circle Toggle via 'b()' Button ---
    bool curCircButton = activeController->b();
    if (curCircButton && !prevCircButton) {
      circToggle = !circToggle;
      if (circToggle) {
        activeController->playDualRumble(0, 150, 0x40, 0x40);
        Serial.println("[AUX6] Circle Toggle: ON (2000us)");
      } else {
        activeController->playDualRumble(0, 150, 0x10, 0x10);
        Serial.println("[AUX6] Circle Toggle: OFF (1000us)");
      }
    }
    prevCircButton = curCircButton;

    // --- Rate Multiplier based on Speed Mode (Roll, Pitch, Yaw ONLY) ---
    float speedMultiplier = 1.0f;
    if (speedMode == 0) {
      speedMultiplier = (float)settings.speedLowPct / 100.0f;
    } else if (speedMode == 1) {
      speedMultiplier = (float)settings.speedMidPct / 100.0f;
    } else {
      speedMultiplier = (float)settings.speedFastPct / 100.0f;
    }

    float thrSpeedMultiplier = settings.throttleSpeedMode ? speedMultiplier : 1.0f;

    // --- Throttle Control (Direct Pass-Through with Alt-Hold Deadband) ---
    if (abs(stickThr) < settings.altHoldDeadband) {
      rawThrottle = 1500;
    } else {
      int deltaThr = map(stickThr, -512, 511, -500, 500) * thrSpeedMultiplier;
      rawThrottle = 1500 + deltaThr;
    }

    // Release throttle override if user moves stick up out of the deadzone
    if (isArmed && throttleOverride) {
      if (stickThr > -350) {
        throttleOverride = false;
      }
    }

    // Force output to 1000us when disarmed or overridden to satisfy FC safety checks!
    uint16_t fcThrottle = (!isArmed || throttleOverride) ? 1000 : rawThrottle;

    // --- Axial / Cross Deadband (Axis Isolation Gate) & Center Deadband ---
    int activeDeadband = max(settings.stickDeadband, settings.crossDeadband);
    if (activeDeadband > 0) {
      stickRoll = applyAxialDeadband(stickRoll, activeDeadband);
      stickPit  = applyAxialDeadband(stickPit,  activeDeadband);
      stickYaw  = applyAxialDeadband(stickYaw,  activeDeadband);
    }

    int deltaRoll = map(stickRoll, -512, 511, -500, 500) * speedMultiplier;
    int deltaPit  = map(stickPit,  -512, 511, -500, 500) * speedMultiplier;
    int deltaYaw  = map(stickYaw,  -512, 511, -500, 500) * speedMultiplier;

    rawRoll  = 1500 + deltaRoll;
    rawPitch = 1500 + deltaPit;
    rawYaw   = 1500 + deltaYaw;
  } else {
    digitalWrite(PIN_STATUS_LED, LOW);
    rawRoll     = 1500;
    rawPitch    = 1500;
    rawThrottle = 1000;
    uint16_t fcThrottle = 1000;
    rawYaw      = 1500;
    isArmed     = false;
    throttleOverride = false;
  }

  uint16_t fcThrottleOut = (!activeController || !activeController->isConnected()) ? 1000 : ((!isArmed || throttleOverride) ? 1000 : rawThrottle);
  
  // Store the raw throttle globally so handleTelemetry can send it to the UI visualizer
  lastRawThrottle = constrain(rawThrottle, 1000, 2000);

  // --- Apply Channel Inversion if configured ---
  if (settings.invert[0]) rawRoll     = 3000 - rawRoll;
  if (settings.invert[1]) rawPitch    = 3000 - rawPitch;
  if (settings.invert[2]) fcThrottleOut = 3000 - fcThrottleOut; // Invert actual output
  if (settings.invert[3]) rawYaw      = 3000 - rawYaw;

  // Store primary channels
  primaryChannels[0] = constrain(rawRoll, 1000, 2000);
  primaryChannels[1] = constrain(rawPitch, 1000, 2000);
  primaryChannels[2] = constrain(fcThrottleOut, 1000, 2000);
  primaryChannels[3] = constrain(rawYaw, 1000, 2000);

  // Map to all 10 channels for FC:
  allChannels[0] = primaryChannels[0]; // CH1: Roll
  allChannels[1] = primaryChannels[1]; // CH2: Pitch
  allChannels[2] = primaryChannels[2]; // CH3: Throttle (1000 disarmed, 1500 hover armed)
  allChannels[3] = primaryChannels[3]; // CH4: Yaw
  allChannels[4] = isArmed ? 2000 : 1000; // CH5 (AUX1)
  allChannels[5] = r1Toggle ? 2000 : 1000; // CH6 (AUX2)
  allChannels[6] = 1500; // CH7 (AUX3)
  allChannels[7] = triToggle ? 2000 : 1000; // CH8 (AUX4)
  allChannels[8] = sqToggle ? 2000 : 1000; // CH9 (AUX5)
  allChannels[9] = circToggle ? 2000 : 1000; // CH10 (AUX6)

  // --- Direct 10-Channel Packet ---
  packRCPacket(txPacket, allChannels);

  // --- 4. PURE LORA ASYNCHRONOUS TRANSMISSION ---
  static unsigned long lastTxTime = 0;
  unsigned long now = millis();

  if (loraInitialized) {
    if (now - lastTxTime >= 52) {
      if (LoRa.beginPacket()) {
        LoRa.write((uint8_t*)&txPacket, sizeof(RCPacket));
        LoRa.endPacket(true); // Non-blocking async TX
        txPacketCounter++;
        lastTxTime = now;
      }
    }
  }

  // --- 5. Real-Time Data Rate (Hz) Calculation ---
  static unsigned long lastHzCalc = 0;
  static uint32_t lastTxCount = 0;
  if (now - lastHzCalc >= 1000) {
    txRateHz = txPacketCounter - lastTxCount;
    lastTxCount = txPacketCounter;
    lastHzCalc = now;
  }

  // Periodic Diagnostics Output to TX Serial Monitor (1 Hz)
  static unsigned long lastTxDebug = 0;
  if (now - lastTxDebug >= 1000) {
    lastTxDebug = now;
    Serial.printf("[TX | %u Hz] Pkts:%u | R:%d P:%d T:%d Y:%d | ARM:%s | AUX2:%d AUX4:%d AUX5:%d AUX6:%d\n",
                  txRateHz, txPacketCounter,
                  allChannels[0], allChannels[1], allChannels[2], allChannels[3],
                  isArmed ? "ARMED" : "DISARMED",
                  allChannels[5], allChannels[7], allChannels[8], allChannels[9]);
  }
}
