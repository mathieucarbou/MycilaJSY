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
Mycila::JSY::Data jsyData;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, RX2, TX2, true);
  jsy.setCallback([](Mycila::JSY::EventType eventType, const Mycila::JSY::Data& data) {
    if (eventType == Mycila::JSY::EventType::EVT_READ) {
      jsyData = data;
    }
  });
}

void loop() {
  delay(1000);

  if (jsy.isEnabled()) {

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();

    if (jsyData.aggregate.activeEnergyImported > 0 || jsyData.aggregate.activeEnergyReturned > 0)
      jsy.resetEnergy();
  }
}
