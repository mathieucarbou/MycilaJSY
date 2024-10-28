#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
  #define RX2 RX1
  #define TX2 TX1
#endif

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  Mycila::JSY jsy;

  // With auto-detection
  jsy.begin(Serial2, RX2, TX2);

  // By forcing a baud rate you know you have set to JSY
  // jsy.begin(Serial2, RX2, TX2, Mycila::JSY::BaudRate::BAUD_38400);

  // Try change speed until success
  uint32_t start = millis();
  Mycila::JSY::BaudRate baudRate = jsy.getBaudRate() == Mycila::JSY::BaudRate::BAUD_38400 ? Mycila::JSY::BaudRate::BAUD_1200 : Mycila::JSY::BaudRate::BAUD_38400;
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
  serializeJsonPretty(doc, Serial);
  Serial.println();

  jsy.end();
}

void loop() {
  delay(1000);
}
