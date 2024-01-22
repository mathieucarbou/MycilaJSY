#define MYCILA_JSY_ASYNC_READ_PAUSE_INTERVAL_MS 60
#define MYCILA_JSY_ASYNC_CORE 0

#include <MycilaJSY.h>

#define KEY_JSY_RX_PIN

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::JSY.begin(17, 16, true);
}

void loop() {
  JsonDocument doc;
  Mycila::JSY.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  delay(5000);
}
