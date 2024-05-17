#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX / Serial TX) and 16 (JSY TX / Serial RX)
  jsy.begin(Serial2, 16, 17);
}

void loop() {
  if (jsy.read()) {

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();

    if (jsy.getEnergy1() > 0 || jsy.getEnergy2() > 0 || jsy.getEnergyReturned1() > 0 || jsy.getEnergyReturned2() > 0)
      jsy.resetEnergy();
  }
  delay(1000);
}
