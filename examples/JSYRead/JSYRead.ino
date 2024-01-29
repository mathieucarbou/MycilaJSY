#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  Mycila::JSY.begin(17, 16);
}

void loop() {
  if (Mycila::JSY.read()) {
    JsonDocument doc;
    Mycila::JSY.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}
