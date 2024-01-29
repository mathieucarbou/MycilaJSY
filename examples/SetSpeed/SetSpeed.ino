#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  Mycila::JSY.begin(17, 16);

  if (Mycila::JSY.isEnabled()) {
    Mycila::JSYBaudRate target = Mycila::JSY.getBaudRate() == Mycila::JSYBaudRate::BAUD_38400 ? Mycila::JSYBaudRate::BAUD_4800 : Mycila::JSYBaudRate::BAUD_38400;

    if (Mycila::JSY.updateBaudRate(target)) {
      Serial.println("JSY baud rate updated");

    } else {
      Serial.println("JSY baud rate update failed");
    }

  } else {
    Serial.println("JSY is disabled");
  }
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
