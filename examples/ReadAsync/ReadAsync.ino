#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  jsy.begin(&Serial2, 17, 16, true, 0);
}

Mycila::JSYBaudRate target = Mycila::JSYBaudRate::BAUD_4800;

void loop() {
  if (!jsy.isEnabled()) {
    delay(1000);
    return;
  }

  delay(1000);

  JsonDocument doc;
  jsy.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  if (jsy.getBaudRate() != target) {
    jsy.setBaudRate(target);
  }
}
