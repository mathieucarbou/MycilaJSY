#include <MycilaJSY.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_38400, &Serial2);
  // Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_19200, &Serial2);
  // Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_9600, &Serial2);
  // Mycila::JSY.begin(17, 16, Mycila::JSYBaudRate::BAUD_4800, &Serial2);
}

void loop() {
  Mycila::JSY.read();

  JsonDocument doc;
  Mycila::JSY.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();

  delay(1000);
}
