#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

#ifndef SOC_UART_HP_NUM
  #define SOC_UART_HP_NUM SOC_UART_NUM
#endif
#if SOC_UART_HP_NUM < 3
  #define Serial2 Serial1
#endif

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, 16, 17, true);
}

void loop() {
  delay(1000);

  if (jsy.isEnabled()) {

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();

    if (jsy.data.aggregate.activeEnergyImported > 0 || jsy.data.aggregate.activeEnergyReturned > 0)
      jsy.resetEnergy();
  }
}
