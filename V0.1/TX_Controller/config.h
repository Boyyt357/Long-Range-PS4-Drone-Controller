#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ================== HARDWARE PIN DEFINITIONS ================== */
// LoRa SX1278 SPI Connections (Standard ESP32 VSPI)
#define SCK_PIN       5
#define MISO_PIN      19
#define MOSI_PIN      27
#define SS_PIN        18
#define RST_PIN       14
#define DIO0_PIN      26

#define PIN_STATUS_LED 2

/* ================== RF CONFIGURATION ================== */
#define LORA_FREQUENCY  433E6
#define LORA_BANDWIDTH  125E3

/* ================== CLEAN 20-BYTE RC PACKET ================== */
typedef struct __attribute__((packed)) {
  uint16_t ch[10]; // 10 Channels (1000 - 2000us)
} RCPacket;

/* ================== SETTINGS & NON-VOLATILE STORAGE ================== */
struct Settings {
  int      altHoldDeadband;
  int      stickDeadband;
  int      crossDeadband;  // Axis Isolation / Cross Gate Deadband
  int      filterBeta;
  uint8_t  loraPower;     // Tx Power (20 dBm)
  bool     invert[4];     // Invert channels: Roll, Pitch, Thr, Yaw
  uint8_t  speedLowPct;
  uint8_t  speedMidPct;
  uint8_t  speedFastPct;
  bool     throttleSpeedMode; // Apply speed multiplier to throttle
};

extern Settings settings;

/* ================== SIMPLE DIRECT CHANNEL PACKING ================== */
inline void packRCPacket(RCPacket &pkt, const uint16_t channels[10]) {
  for (int i = 0; i < 10; i++) {
    pkt.ch[i] = channels[i];
  }
}

#endif // CONFIG_H
