#include <MycilaJSY.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_38400, &Serial2, true, 60, 0);
}

void loop() {
  JsonDocument doc;
  Mycila::JSY.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  delay(5000);
}
