#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaJSY.h>

Mycila::JSY jsy;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  jsy.begin(Serial2, 16, 17);
}

uint32_t counts = 0;

void loop() {
  uint32_t now = millis();
  counts++;

  Serial.printf("JSY baud rate: %d\n", static_cast<int>(jsy.getBaudRate()));

  if (jsy.read()) {
    Serial.print("JSY read in ");
    Serial.print(millis() - now);
    Serial.print(" ms\n");

    JsonDocument doc;
    jsy.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
  }

  if (counts % 7 == 0) {
    if (jsy.setBaudRate(jsy.getBaudRate() == Mycila::JSYBaudRate::BAUD_38400 ? Mycila::JSYBaudRate::BAUD_1200 : Mycila::JSYBaudRate::BAUD_38400)) {
      Serial.printf("JSY baud rate updated to %d\n", static_cast<int>(jsy.getBaudRate()));
    } else {
      Serial.println("JSY baud rate update failed");
    }
  }

  delay(2000);
}
