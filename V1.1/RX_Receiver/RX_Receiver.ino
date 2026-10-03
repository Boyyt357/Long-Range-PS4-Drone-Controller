/*
 * ESP32 PS4 Drone Controller Receiver (RX) - Pure LoRa 433MHz
 * 
 * Features:
 * - 100% Pure LoRa SX1278 (433MHz) Reception (Zero Wi-Fi / ESP-NOW RF noise)
 * - 0.5-Second Strict Failsafe Watchdog (cuts serial to trigger FC RX_LOSS)
 * - Auto-Sync Spreading Factor: In-flight target SF sync from TX
 * - Auto SF Hunting: When signal lost >800ms, cycles SF7-SF10 to find transmitter
 * - High-Speed TBS Crossfire (CRSF @ 420,000 Baud) or FlySky iBUS (@ 115,200 Baud)
 * - Live Data Rate (Hz), RSSI (dBm), and SNR (dB) Diagnostics
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Preferences.h>

#include "rx_config.h"
#include "crsf.h"
#include "ibus.h"

/* ================== GLOBAL VARIABLES ================== */
Preferences preferences;

// Channels: [0]=Roll, [1]=Pitch, [2]=Throttle (1000 disarmed, 1500 hover), [3]=Yaw, [4]=ARM, [5]=Speed, [6]=AltHold, [7]=Beeper, [8]=Square, [9]=Circle
uint16_t rcChannels[10] = {1500, 1500, 1000, 1500, 1000, 1000, 1500, 1000, 1000, 1000};

unsigned long lastRecvTime = 0;
bool isFailsafe = true;
bool loraAvailable = false;
// SF variables removed (hardcoded to SF7)

// Signal Metrics
int8_t lastLoraRssi = 0;
int8_t lastLoraSnr  = 0;
uint32_t rxPacketCount = 0;
uint16_t rxRateHz = 0;

// NVS Storage Removed

/* ================== SETUP ================== */
void setup() {
  Serial.begin(115200);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  // 1. Initialize Serial Output to Flight Controller UART RX
#if USE_CRSF
  // 420000 Baud, 8N1 for TBS Crossfire
  Serial1.begin(420000, SERIAL_8N1, -1, PIN_SERIAL_TX);
  Serial.println("[FC Serial] TBS Crossfire (CRSF) @ 420000 Baud Initialized");
#else
  // 115200 Baud, 8N1 for FlySky iBUS
  Serial1.begin(115200, SERIAL_8N1, -1, PIN_SERIAL_TX);
  Serial.println("[FC Serial] FlySky iBUS @ 115200 Baud Initialized");
#endif

  // 2. Load Spreading Factor from flash
  // 3. Initialize LoRa SX1278 SPI
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (LoRa.begin(LORA_FREQUENCY)) {
    loraAvailable = true;
    LoRa.setSpreadingFactor(7); // Fixed SF7
    LoRa.setSignalBandwidth(LORA_BANDWIDTH); // 125E3
    Serial.println("[LoRa] Receiver Ready (Fixed SF7, BW 125kHz)");
  } else {
    loraAvailable = false;
    Serial.println("[LoRa ERROR] SX1278 not detected. Check SPI wiring!");
  }
}

/* ================== MAIN LOOP ================== */
void loop() {
  // 1. Process incoming LoRa packets
  if (loraAvailable) {
    int packetSize = LoRa.parsePacket();
    if (packetSize == sizeof(RCPacket)) {
      RCPacket temp;
      LoRa.readBytes((uint8_t*)&temp, sizeof(RCPacket));
      // Re-arm receiver immediately so it is already listening for the next packet
      LoRa.parsePacket();

      if (unpackRCPacket(temp, rcChannels)) {
        lastRecvTime = millis();
        isFailsafe   = false;
        rxPacketCount++;
        lastLoraRssi = (int8_t)LoRa.packetRssi();
        lastLoraSnr  = (int8_t)LoRa.packetSnr();
        digitalWrite(PIN_STATUS_LED, HIGH);

        // Real-time packet telemetry log
        static unsigned long lastPktLog = 0;
        if (millis() - lastPktLog >= 250) { // Log 4 times a second max to avoid serial lag
          lastPktLog = millis();
          Serial.printf("[RX OK] #%-5u | RSSI:%4d dBm | SNR:%5.1fdB | R:%d P:%d T:%d Y:%d | ARM:%d\n",
                        rxPacketCount, lastLoraRssi, (float)lastLoraSnr,
                        rcChannels[0], rcChannels[1], rcChannels[2], rcChannels[3], rcChannels[4]);
        }
      }
    } else if (packetSize > 0) {
      // Discard unexpected packets (noise) immediately without serial lag
      while (LoRa.available()) LoRa.read();
      LoRa.parsePacket(); // Re-arm
    }
  }

  // 2. Failsafe Watchdog & Auto Modem Recovery
  unsigned long timeSinceLastPacket = millis() - lastRecvTime;
  static unsigned long lastHuntTime = 0;
  static unsigned long lastModemReset = 0;

  // Active Hardware Watchdog: If no packet for > 400ms, force-reset the SX1278 modem state machine.
  // Cycling through SLEEP completely flushes the internal state machine on cheap clone chips.
  if (timeSinceLastPacket > 400 && (millis() - lastModemReset > 400)) {
    lastModemReset = millis();
    LoRa.idle();
    LoRa.sleep();
    delayMicroseconds(200);
    LoRa.idle();
    LoRa.parsePacket(); // Fresh FIFO, re-arms single RX
  }

  if (timeSinceLastPacket > FAILSAFE_TIMEOUT) {
    if (!isFailsafe) {
      Serial.println("[FAILSAFE] RC Link Lost (>0.5s)! Cutting serial stream to trigger FC RX_LOSS.");
      isFailsafe = true;
      digitalWrite(PIN_STATUS_LED, LOW);
    }

    if (millis() - lastHuntTime > 1000) {
      Serial.printf("[RX Failsafe] Waiting on SF7... (No packet for %lu ms) | Total Received: %u\n", 
                    timeSinceLastPacket, rxPacketCount);
      lastHuntTime = millis();
    }
  } else {
    // Healthy signal: forward channels to Flight Controller
    static unsigned long lastFCFrame = 0;
    unsigned long now = millis();
#if USE_CRSF
    if (now - lastFCFrame >= 4) { // Match CRSF ~250Hz frame timing
      sendCRSFFrame(Serial1, rcChannels);
      lastFCFrame = now;
    }
#else
    if (now - lastFCFrame >= 7) { // Match standard iBUS packet intervals
      sendIBUSFrame(Serial1, rcChannels);
      lastFCFrame = now;
    }
#endif
  }

  // 3. Calculate Live Data Rate (Hz)
  static unsigned long lastHzCalc = 0;
  static uint32_t lastRxCount = 0;
  if (millis() - lastHzCalc >= 1000) {
    rxRateHz = rxPacketCount - lastRxCount;
    lastRxCount = rxPacketCount;
    lastHzCalc = millis();
  }

  // 4. Periodic Diagnostics to Serial Monitor (2 Hz)
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    if (!isFailsafe) {
      Serial.printf("[RX STATS | %2u Hz] Total:%-5u | RSSI:%4d dBm | SNR:%5.1fdB | Roll:%d Pit:%d Thr:%d Yaw:%d\n",
                        rxRateHz, rxPacketCount, lastLoraRssi, (float)lastLoraSnr,
                        rcChannels[0], rcChannels[1], rcChannels[2], rcChannels[3]);
    }
    lastDebug = millis();
  }
  
  yield();
}
