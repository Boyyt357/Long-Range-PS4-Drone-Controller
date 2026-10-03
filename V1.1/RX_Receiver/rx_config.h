#ifndef RX_CONFIG_H
#define RX_CONFIG_H

#include <Arduino.h>

/* ================== FLIGHT CONTROLLER PROTOCOL SELECTION ================== */
#define USE_CRSF          1 // 1: TBS Crossfire (CRSF @ 420000 Baud), 0: FlySky iBUS (@ 115200 Baud)

/* ================== HARDWARE PIN DEFINITIONS ================== */
#define PIN_SERIAL_TX     17 // Output to FC UART RX

#define SCK_PIN           5
#define MISO_PIN          19
#define MOSI_PIN          27
#define SS_PIN            18
#define RST_PIN           14
#define DIO0_PIN          26

#define PIN_STATUS_LED    2

/* ================== RF & TIMING PARAMETERS ================== */
#define LORA_FREQUENCY    433E6
#define LORA_BANDWIDTH    125E3
#define FAILSAFE_TIMEOUT  1500 // 1.5s Failsafe Timeout for robust link retention

/* ================== CLEAN 20-BYTE RC PACKET ================== */
typedef struct __attribute__((packed)) {
  uint16_t ch[10]; // 10 Channels (1000 - 2000us)
} RCPacket;

/* ================== SIMPLE DIRECT CHANNEL UNPACKING ================== */
inline bool unpackRCPacket(const RCPacket &pkt, uint16_t channels[10]) {
  // Sanity check: All RC channels must be within realistic 800 - 2200us pulse widths
  for (int i = 0; i < 10; i++) {
    if (pkt.ch[i] < 800 || pkt.ch[i] > 2200) {
      return false; // Rejects corrupted packets or noise
    }
  }
  for (int i = 0; i < 10; i++) {
    channels[i] = pkt.ch[i];
  }
  return true;
}

#endif // RX_CONFIG_H
