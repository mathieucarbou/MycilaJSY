#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  jsy.begin(&Serial2, 17, 16, true);
}

void loop() {
  delay(1000);

  if (jsy.isEnabled()) {

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();

    if (jsy.getEnergy1() > 0 || jsy.getEnergy2() > 0 || jsy.getEnergyReturned1() > 0 || jsy.getEnergyReturned2() > 0)
      jsy.resetEnergy();
  }
}
