TRANSMITTER (GROUND STATION)
<img width="963" height="482" alt="image" src="https://github.com/user-attachments/assets/79eee70c-53b3-4862-8b87-8542a96e1b03" />

  ESP32 Development Board wired to an SX1278 LoRa 433MHz module:
- 3V3 -> VCC
- GND -> GND
- GPIO 5 -> SCK
- GPIO 19 -> MISO
- GPIO 27 -> MOSI
- GPIO 18 -> NSS / CS
- GPIO 14 -> RST
- GPIO 26 -> DIO0

RECEIVER (ON DRONE)
<img width="960" height="437" alt="image" src="https://github.com/user-attachments/assets/2148b4c9-3c35-4589-b157-e46cb0a60838" />

<img width="966" height="220" alt="image" src="https://github.com/user-attachments/assets/5907648a-f93b-464d-b011-37c4a10c1407" />

  ESP32 Board wired to an SX1278 LoRa module with the same SPI connections
- GPIO 17 -> FC UART RX pad (CRSF Serial @ 420k Baud)
- 5V from FC -> ESP32 VIN
- GND from FC -> ESP32 GND

<img width="530" height="960" alt="image" src="https://github.com/user-attachments/assets/53e350a5-3fcd-4b86-8247-870109acc543" />
