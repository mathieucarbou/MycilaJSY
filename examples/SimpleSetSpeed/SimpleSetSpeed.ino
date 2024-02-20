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

// Switch to 38400 bauds
// const uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x08, 0xAA, 0x12};

// Switch to 19200 bauds
// const uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x07, 0xEA, 0x16};

// Switch to 9600 bauds
// const uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x06, 0x2B, 0xD6};

// Switch to 4800 bauds
const uint8_t data[] = {0x00, 0x10, 0x00, 0x04, 0x00, 0x01, 0x02, 0x01, 0x05, 0x6B, 0xD7};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;
  Serial.printf("Opening connection with JSY at %d bauds\n", BAUDS);
  Serial2.begin(BAUDS, SERIAL_8N1, JSY_TX, JSY_RX);
  Serial.print("Set new speed");
}

void loop() {
  Serial.print(".");
  Serial2.write(data, 11);
  Serial2.flush();
  while (Serial2.available())
    Serial2.read();
  delay(30);
}
