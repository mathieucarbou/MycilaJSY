#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
  #define RX2     RX1
  #define TX2     TX1
#endif

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, RX2, TX2);

  if (jsy.resetEnergy()) {
    Serial.println("Energy counters reset");
  } else {
    Serial.println("Failed to reset energy counters");
  }
}

void loop() {
  if (jsy.read()) {
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }

  delay(1000);
}
