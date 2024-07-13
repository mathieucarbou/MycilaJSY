#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  Mycila::JSY jsy;

  // With auto-detection
  jsy.begin(Serial2, 16, 17);

  // By forcing a baud rate you know you have set to JSY
  // jsy.begin(Serial2, 16, 17, Mycila::JSYBaudRate::BAUD_38400);

  // Try change speed until success
  uint32_t start = millis();
  Mycila::JSYBaudRate baudRate = jsy.getBaudRate() == Mycila::JSYBaudRate::BAUD_38400 ? Mycila::JSYBaudRate::BAUD_1200 : Mycila::JSYBaudRate::BAUD_38400;
  while (!jsy.setBaudRate(baudRate) && millis() - start < 15000) {
    delay(500);
  }

  // Reset energy data until success
  start = millis();
  while (!jsy.resetEnergy() && millis() - start < 15000) {
    delay(500);
  }

  start = millis();
  while (!jsy.read() && millis() - start < 15000) {
    delay(500);
  }

  JsonDocument doc;
  jsy.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  jsy.end();
}

void loop() {
  delay(1000);
}
