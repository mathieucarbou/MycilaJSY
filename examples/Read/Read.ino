#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  jsy.begin(&Serial2, 17, 16);
}

void loop() {
  if (jsy.read()) {
    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}
