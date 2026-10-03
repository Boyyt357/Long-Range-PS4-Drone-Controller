#ifndef IBUS_H
#define IBUS_H

#include <Arduino.h>

// Builds a standard 32-byte FlySky iBUS frame (14 channels @ 115200 Baud)
inline void sendIBUSFrame(HardwareSerial &serialPort, const uint16_t channels[10]) {
  uint8_t packet[32];
  packet[0] = 0x20; // 32 bytes frame length
  packet[1] = 0x40; // Command: Servo channels

  uint16_t ibusChannels[14];
  for (int i = 0; i < 10; i++) {
    ibusChannels[i] = channels[i];
  }
  // Channels 11 to 14 default to 1500us
  for (int i = 10; i < 14; i++) {
    ibusChannels[i] = 1500;
  }

  // Pack 14 channels (2 bytes each, little endian)
  for (int i = 0; i < 14; i++) {
    packet[2 + i * 2]     = ibusChannels[i] & 0xFF;
    packet[3 + i * 2] = (ibusChannels[i] >> 8) & 0xFF;
  }

  // 16-bit Checksum (0xFFFF - sum of bytes 0 to 29)
  uint16_t chksum = 0xFFFF;
  for (int i = 0; i < 30; i++) {
    chksum -= packet[i];
  }

  packet[30] = chksum & 0xFF;
  packet[31] = (chksum >> 8) & 0xFF;

  serialPort.write(packet, 32);
}

#endif // IBUS_H
