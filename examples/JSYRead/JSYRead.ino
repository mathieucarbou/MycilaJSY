#include <MycilaJSY.h>

#define KEY_JSY_RX_PIN

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::JSY.begin(17, 16);
}

void loop() {
  Mycila::JSY.read();

  JsonDocument doc;
  Mycila::JSY.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  delay(5000);
}
