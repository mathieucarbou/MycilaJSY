#include <Arduino.h>
#include <HardwareSerial.h>

// Pin: RX (ESP32) -> TX (JSY)
#define JSY_TX 16

// Pin: TX (ESP32) -> RX (JSY)
#define JSY_RX 17

// Choose the speed that you have configured for your JSY
#define BAUDS 38400
// #define BAUDS 19200
// #define BAUDS 9600
// #define BAUDS 4800

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;
  Serial.printf("Opening connection with JSY at %d bauds\n", BAUDS);
  Serial2.begin(BAUDS, SERIAL_8N1, JSY_TX, JSY_RX);
  Serial.print("Reset energy data");
}

void loop() {
  Serial.print(".");
  const uint8_t data[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xFA};
  Serial2.write(data, 13);
  Serial2.flush();
  while (Serial2.available())
    Serial2.read();
  delay(30);
}
