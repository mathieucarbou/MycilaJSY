#include <MycilaJSY.h>
#include <MycilaLogger.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::Logger.forwardTo(&Serial);

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  Mycila::JSY.begin(17, 16, &Serial2, true, 60, 0);
}

void loop() {
  if (Mycila::JSY.isEnabled()) {
    JsonDocument doc;
    Mycila::JSY.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }
  delay(1000);
}
