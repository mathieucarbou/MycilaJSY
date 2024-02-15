#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // read JSY on pins 17 (JSY RX) and 16 (JSY TX)
  jsy.begin(17, 18);

  if (jsy.isEnabled()) {
    Mycila::JSYBaudRate target = jsy.getBaudRate() == Mycila::JSYBaudRate::BAUD_38400 ? Mycila::JSYBaudRate::BAUD_4800 : Mycila::JSYBaudRate::BAUD_38400;

    if (jsy.updateBaudRate(target)) {
      Serial.println("JSY baud rate updated");

    } else {
      Serial.println("JSY baud rate update failed");
    }

  } else {
    Serial.println("JSY is disabled");
  }
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
