#ifndef CRSF_H
#define CRSF_H

#include <Arduino.h>

#define CRSF_ADDRESS_FLIGHT_CONTROLLER    0xC8
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED 0x16

// CRSF CRC8 with DVB-S2 polynomial 0xD5
inline uint8_t crsf_crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0xD5;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// Builds a standard 26-byte CRSF RC packet for F4/F7 Flight Controllers
inline void sendCRSFFrame(HardwareSerial &serialPort, const uint16_t channels[10]) {
  uint8_t packet[26];
  packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER; // 0xC8
  packet[1] = 24;                             // Length: Type(1) + Payload(22) + CRC(1)
  packet[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED; // 0x16

  uint16_t crsfCh[16];
  // Map 1000-2000us to CRSF 11-bit scale (1000us -> 191, 1500us -> 992, 2000us -> 1792)
  for (int i = 0; i < 10; i++) {
    crsfCh[i] = map(constrain(channels[i], 1000, 2000), 1000, 2000, 191, 1792);
  }
  // Channels 11 to 16 default to center (992)
  for (int i = 10; i < 16; i++) {
    crsfCh[i] = 992;
  }

  // Bit-pack 16 channels x 11-bit into 22 payload bytes
  uint8_t *p = &packet[3];
  p[0]  = (uint8_t)(crsfCh[0]);
  p[1]  = (uint8_t)((crsfCh[0] >> 8) | (crsfCh[1] << 3));
  p[2]  = (uint8_t)((crsfCh[1] >> 5) | (crsfCh[2] << 6));
  p[3]  = (uint8_t)(crsfCh[2] >> 2);
  p[4]  = (uint8_t)((crsfCh[2] >> 10) | (crsfCh[3] << 1));
  p[5]  = (uint8_t)((crsfCh[3] >> 7) | (crsfCh[4] << 4));
  p[6]  = (uint8_t)((crsfCh[4] >> 4) | (crsfCh[5] << 7));
  p[7]  = (uint8_t)(crsfCh[5] >> 1);
  p[8]  = (uint8_t)((crsfCh[5] >> 9) | (crsfCh[6] << 2));
  p[9]  = (uint8_t)((crsfCh[6] >> 6) | (crsfCh[7] << 5));
  p[10] = (uint8_t)(crsfCh[7] >> 3);
  p[11] = (uint8_t)(crsfCh[8]);
  p[12] = (uint8_t)((crsfCh[8] >> 8) | (crsfCh[9] << 3));
  p[13] = (uint8_t)((crsfCh[9] >> 5) | (crsfCh[10] << 6));
  p[14] = (uint8_t)(crsfCh[10] >> 2);
  p[15] = (uint8_t)((crsfCh[10] >> 10) | (crsfCh[11] << 1));
  p[16] = (uint8_t)((crsfCh[11] >> 7) | (crsfCh[12] << 4));
  p[17] = (uint8_t)((crsfCh[12] >> 4) | (crsfCh[13] << 7));
  p[18] = (uint8_t)(crsfCh[13] >> 1);
  p[19] = (uint8_t)((crsfCh[13] >> 9) | (crsfCh[14] << 2));
  p[20] = (uint8_t)((crsfCh[14] >> 6) | (crsfCh[15] << 5));
  p[21] = (uint8_t)(crsfCh[15] >> 3);

  // CRC8 computed over Type (0x16) and 22 payload bytes (23 bytes total)
  packet[25] = crsf_crc8(&packet[2], 23);

  serialPort.write(packet, 26);
}

#endif // CRSF_H
